#include "anum_denotation_bridge.h"

#include "avm/link_store.h"
#include "avm/projection.h"
#include "nlohmann/json.hpp"

#include <cassert>
#include <cstddef>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace
{

json load_json(const char *path)
{
	std::ifstream input(path);
	if (!input)
		throw std::runtime_error(std::string("cannot open conformance file: ") + path);

	json value;
	input >> value;
	return value;
}

avm::AnumDenotationRef parse_ref(const json &value)
{
	if (!value.is_object())
		throw std::invalid_argument("denotation reference must be an object");
	if (value.size() == 1 && value.contains("anchor") && value["anchor"].is_string())
		return avm::AnumDenotationRef::anchor_ref(value["anchor"].get<std::string>());
	if (value.size() == 1 && value.contains("node") && value["node"].is_number_unsigned())
		return avm::AnumDenotationRef::node_ref(value["node"].get<avm::ProjectionNodeId>());
	throw std::invalid_argument("denotation reference must contain exactly anchor or node");
}

avm::CanonicalAnumDenotation parse_denotation(const json &value)
{
	if (!value.is_object() || !value.contains("kind") || !value["kind"].is_string())
		throw std::invalid_argument("denotation must contain string kind");

	const std::string kind = value["kind"].get<std::string>();
	if (kind == "raw" || kind == "quoted-raw")
	{
		if (value.size() != 2 || !value.contains("raw") || !value["raw"].is_string())
			throw std::invalid_argument("raw denotation must contain exactly kind and raw");
		if (kind == "raw")
			return avm::CanonicalAnumDenotation::raw_result(value["raw"].get<std::string>());
		return avm::CanonicalAnumDenotation::quoted_raw_result(value["raw"].get<std::string>());
	}

	if (kind != "structural" || value.size() != 4 || !value.contains("anchors") || !value.contains("nodes") ||
	    !value.contains("root") || !value["anchors"].is_array() || !value["nodes"].is_array())
		throw std::invalid_argument("structural denotation has invalid shape");

	avm::AnumStructuralDenotation structural;
	for (const json &anchor : value["anchors"])
	{
		if (!anchor.is_string())
			throw std::invalid_argument("structural anchor must be string");
		structural.anchors.push_back(anchor.get<std::string>());
	}

	for (const json &node : value["nodes"])
	{
		if (!node.is_object() || node.size() != 3 || !node.contains("id") || !node.contains("start") ||
		    !node.contains("end") || !node["id"].is_number_unsigned())
			throw std::invalid_argument("structural node has invalid shape");
		structural.nodes.push_back(avm::AnumDenotationNode{
			node["id"].get<avm::ProjectionNodeId>(),
			parse_ref(node["start"]),
			parse_ref(node["end"]),
		});
	}
	structural.root = parse_ref(value["root"]);
	return avm::CanonicalAnumDenotation::structural_result(std::move(structural));
}

std::map<std::string, avm::LinkId> create_anchor_points(avm::InMemoryLinkStore &store,
                                                       const avm::AnumStructuralDenotation &structural)
{
	std::map<std::string, avm::LinkId> result;
	for (const std::string &key : structural.anchors)
		result.emplace(key, store.create_point());
	return result;
}

avm::AnumAnchorResolver map_resolver(const std::map<std::string, avm::LinkId> &anchors)
{
	return [&anchors](std::string_view key) -> std::optional<avm::LinkId>
	{
		const auto it = anchors.find(std::string(key));
		if (it == anchors.end())
			return std::nullopt;
		return it->second;
	};
}

void assert_throws_invalid(const std::function<void()> &operation, std::string_view expected)
{
	bool thrown = false;
	try
	{
		operation();
	}
	catch (const std::invalid_argument &error)
	{
		thrown = true;
		assert(std::string_view(error.what()).find(expected) != std::string_view::npos);
	}
	assert(thrown);
}

void verify_structural_l4_lifecycle(const avm::CanonicalAnumDenotation &value)
{
	assert(value.structural.has_value());

	avm::InMemoryLinkStore store;
	const std::map<std::string, avm::LinkId> anchors = create_anchor_points(store, *value.structural);
	const std::size_t size_before_bridge = store.size();

	const auto projection = avm::bridge_anum_denotation(value, map_resolver(anchors));
	assert(projection.has_value());
	assert(store.size() == size_before_bridge);
	assert(projection->nodes.size() == value.structural->nodes.size());

	const std::size_t size_before_find = store.size();
	const auto found_before = avm::find_projection(store, *projection);
	assert(store.size() == size_before_find);
	if (projection->nodes.empty())
		assert(found_before.has_value());
	else
		assert(!found_before.has_value());

	const avm::ProjectionResult realized = avm::realize_projection(store, *projection);
	const auto found_after = avm::find_projection(store, *projection);
	assert(found_after.has_value());
	assert(found_after->root == realized.root);
	assert(found_after->nodes == realized.nodes);

	const std::size_t size_after_realize = store.size();
	const avm::ProjectionResult realized_again = avm::realize_projection(store, *projection);
	assert(realized_again.root == realized.root);
	assert(realized_again.nodes == realized.nodes);
	assert(store.size() == size_after_realize);
}

void verify_generic_denotation_corpus(const json &corpus)
{
	assert(corpus.at("schema") == "anum-denotation-conformance/v0.2");
	assert(corpus.at("contract") == "anum-denotation/v0.2");
	assert(corpus.at("status") == "accepted");

	for (const json &test_case : corpus.at("cases"))
	{
		const avm::CanonicalAnumDenotation value = parse_denotation(test_case.at("value"));
		if (value.kind == avm::AnumDenotationKind::Structural)
		{
			verify_structural_l4_lifecycle(value);
			continue;
		}

		std::size_t resolver_calls = 0;
		const avm::AnumAnchorResolver resolver = [&resolver_calls](std::string_view) -> std::optional<avm::LinkId>
		{
			++resolver_calls;
			return std::nullopt;
		};
		assert(!avm::bridge_anum_denotation(value, resolver).has_value());
		assert(resolver_calls == 0);
	}
}

void verify_recursive_denotation_corpus(const json &corpus)
{
	assert(corpus.at("schema") == "anum-recursive-denotation-conformance/v0.2");
	assert(corpus.at("contract") == "anum-recursive-denotation/v0.2");
	assert(corpus.at("status") == "accepted");

	for (const json &test_case : corpus.at("cases"))
	{
		const avm::CanonicalAnumDenotation value = parse_denotation(test_case.at("expected"));
		if (value.kind == avm::AnumDenotationKind::Structural)
			verify_structural_l4_lifecycle(value);
		else
			assert(!avm::bridge_anum_denotation(value, {}).has_value());
	}
}

void verify_shared_substructure_translation(const json &generic_corpus)
{
	for (const json &test_case : generic_corpus.at("cases"))
	{
		if (test_case.at("name") != "shared-substructure")
			continue;

		const avm::CanonicalAnumDenotation value = parse_denotation(test_case.at("value"));
		assert(value.structural.has_value());

		avm::InMemoryLinkStore store;
		const auto anchors = create_anchor_points(store, *value.structural);
		const auto projection = avm::bridge_anum_denotation(value, map_resolver(anchors));
		assert(projection.has_value());
		assert(projection->nodes.size() == 2);
		assert(projection->nodes[1].begin == avm::ProjectionRef::node(0));
		assert(projection->nodes[1].end == avm::ProjectionRef::node(0));
		assert(projection->root == avm::ProjectionRef::node(1));
		return;
	}
	assert(false && "shared-substructure conformance case is required");
}

void verify_missing_physical_anchor_is_non_mutating_until_realize()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId existing = store.create_point();
	const avm::LinkId missing = existing + 1000;
	assert(!store.contains(missing));

	const avm::CanonicalAnumDenotation value = avm::CanonicalAnumDenotation::structural_result(
		avm::AnumStructuralDenotation{{"missing"}, {}, avm::AnumDenotationRef::anchor_ref("missing")});
	const auto projection = avm::bridge_anum_denotation(
		value, [missing](std::string_view) -> std::optional<avm::LinkId> { return missing; });
	assert(projection.has_value());

	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, *projection).has_value());
	assert(store.size() == before_find);

	assert_throws_invalid([&] { (void)avm::realize_projection(store, *projection); }, "not present");
	assert(store.size() == before_find);
}

void verify_malformed_handoff_rejection()
{
	const avm::AnumAnchorResolver resolver = [](std::string_view) -> std::optional<avm::LinkId> { return 1; };

	assert_throws_invalid(
		[&]
		{
			(void)avm::bridge_anum_denotation(
				avm::CanonicalAnumDenotation::structural_result(
					avm::AnumStructuralDenotation{{"b", "a"}, {}, avm::AnumDenotationRef::anchor_ref("a")}),
				resolver);
		},
		"sorted");

	assert_throws_invalid(
		[&]
		{
			(void)avm::bridge_anum_denotation(
				avm::CanonicalAnumDenotation::structural_result(
					avm::AnumStructuralDenotation{{"a", "a"}, {}, avm::AnumDenotationRef::anchor_ref("a")}),
				resolver);
		},
		"unique");

	assert_throws_invalid(
		[&]
		{
			(void)avm::bridge_anum_denotation(
				avm::CanonicalAnumDenotation::structural_result(
					avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("missing")}),
				resolver);
		},
		"undeclared anchor");

	assert_throws_invalid(
		[&]
		{
			avm::AnumStructuralDenotation structural;
			structural.anchors = {"a"};
			structural.nodes.push_back(avm::AnumDenotationNode{
				0,
				avm::AnumDenotationRef::node_ref(0),
				avm::AnumDenotationRef::anchor_ref("a"),
			});
			structural.root = avm::AnumDenotationRef::node_ref(0);
			(void)avm::bridge_anum_denotation(avm::CanonicalAnumDenotation::structural_result(std::move(structural)),
			                                    resolver);
		},
		"earlier node");

	assert_throws_invalid(
		[&]
		{
			avm::AnumStructuralDenotation structural;
			structural.anchors = {"a"};
			structural.nodes.push_back(avm::AnumDenotationNode{
				1,
				avm::AnumDenotationRef::anchor_ref("a"),
				avm::AnumDenotationRef::anchor_ref("a"),
			});
			structural.root = avm::AnumDenotationRef::node_ref(0);
			(void)avm::bridge_anum_denotation(avm::CanonicalAnumDenotation::structural_result(std::move(structural)),
			                                    resolver);
		},
		"contiguous");

	assert_throws_invalid(
		[&]
		{
			const avm::CanonicalAnumDenotation value = avm::CanonicalAnumDenotation::structural_result(
				avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("a")});
			(void)avm::bridge_anum_denotation(value, {});
		},
		"resolver");

	assert_throws_invalid(
		[&]
		{
			const avm::CanonicalAnumDenotation value = avm::CanonicalAnumDenotation::structural_result(
				avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("a")});
			(void)avm::bridge_anum_denotation(
				value, [](std::string_view) -> std::optional<avm::LinkId> { return std::nullopt; });
		},
		"could not be resolved");

	assert_throws_invalid(
		[&]
		{
			const avm::CanonicalAnumDenotation value = avm::CanonicalAnumDenotation::structural_result(
				avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("a")});
			(void)avm::bridge_anum_denotation(
				value, [](std::string_view) -> std::optional<avm::LinkId> { return avm::invalid_link_id; });
		},
		"invalid LinkId");

	assert_throws_invalid(
		[&]
		{
			avm::CanonicalAnumDenotation mixed{
				avm::AnumDenotationKind::Structural,
				avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("a")},
				std::string("raw"),
			};
			(void)avm::bridge_anum_denotation(mixed, resolver);
		},
		"only structural payload");

	assert_throws_invalid(
		[&]
		{
			avm::CanonicalAnumDenotation mixed{
				avm::AnumDenotationKind::Raw,
				avm::AnumStructuralDenotation{{"a"}, {}, avm::AnumDenotationRef::anchor_ref("a")},
				std::string("raw"),
			};
			(void)avm::bridge_anum_denotation(mixed, resolver);
		},
		"only raw payload");
}

void verify_provenance(const json &provenance)
{
	assert(provenance.at("sourceRepository") == "netkeep80/anum_docs");
	assert(provenance.at("sourceCommit") == "62901cfc");
	assert(provenance.at("schemas").size() == 2);
}

} // namespace

int main(int argc, char **argv)
{
	assert(argc == 4);
	const json generic_corpus = load_json(argv[1]);
	const json recursive_corpus = load_json(argv[2]);
	const json provenance = load_json(argv[3]);

	verify_provenance(provenance);
	verify_generic_denotation_corpus(generic_corpus);
	verify_recursive_denotation_corpus(recursive_corpus);
	verify_shared_substructure_translation(generic_corpus);
	verify_missing_physical_anchor_is_non_mutating_until_realize();
	verify_malformed_handoff_rejection();
	return 0;
}
