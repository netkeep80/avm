#include "anum_denotation_bridge.h"
#include "json_duplet_values.h"

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/projection.h"
#include "avm/relations_model.h"
#include "avm/text_value.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using Json = nlohmann::ordered_json;
using AnchorMap = std::map<std::string, avm::LinkId>;

Json load_json(const char *path)
{
	std::ifstream input(path);
	if (!input)
		throw std::runtime_error(std::string("cannot open common-denotation corpus: ") + path);

	Json value;
	input >> value;
	return value;
}

std::size_t require_index(const Json &value, std::string_view role)
{
	if (value.is_number_unsigned())
		return value.get<std::size_t>();
	if (value.is_number_integer())
	{
		const std::int64_t index = value.get<std::int64_t>();
		if (index >= 0)
			return static_cast<std::size_t>(index);
	}
	throw std::invalid_argument(std::string(role) + " must be a non-negative integer");
}

std::vector<std::string> source_anchor_names(const Json &test_case)
{
	if (!test_case.contains("anchors") || !test_case.at("anchors").is_array())
		throw std::invalid_argument("common-denotation case must contain anchors array");

	std::vector<std::string> result;
	for (const Json &anchor : test_case.at("anchors"))
	{
		if (!anchor.is_string())
			throw std::invalid_argument("common-denotation anchor name must be string");
		result.push_back(anchor.get<std::string>());
	}
	return result;
}

avm::AnumDenotationRef parse_anum_ref(const Json &value, const std::vector<std::string> &source_anchors)
{
	if (!value.is_object() || value.size() != 1)
		throw std::invalid_argument("common-denotation Anum ref must contain exactly one member");

	if (value.contains("anchor"))
	{
		const std::size_t index = require_index(value.at("anchor"), "Anum anchor index");
		if (index >= source_anchors.size())
			throw std::invalid_argument("Anum anchor index is outside common-denotation anchor table");
		return avm::AnumDenotationRef::anchor_ref(source_anchors[index]);
	}
	if (value.contains("node"))
	{
		const std::size_t index = require_index(value.at("node"), "Anum node index");
		return avm::AnumDenotationRef::node_ref(static_cast<avm::ProjectionNodeId>(index));
	}
	throw std::invalid_argument("common-denotation Anum ref must contain anchor or node");
}

avm::CanonicalAnumDenotation parse_anum_denotation(const Json &test_case)
{
	const std::vector<std::string> source_anchors = source_anchor_names(test_case);
	const Json &encoded = test_case.at("anum");
	if (!encoded.is_object() || encoded.size() != 2 || !encoded.contains("nodes") || !encoded.contains("root") ||
	    !encoded.at("nodes").is_array())
		throw std::invalid_argument("common-denotation Anum payload has invalid shape");

	avm::AnumStructuralDenotation structural;
	structural.anchors = source_anchors;
	std::sort(structural.anchors.begin(), structural.anchors.end());
	if (std::adjacent_find(structural.anchors.begin(), structural.anchors.end()) != structural.anchors.end())
		throw std::invalid_argument("common-denotation anchors must be unique");

	for (std::size_t index = 0; index < encoded.at("nodes").size(); ++index)
	{
		const Json &node = encoded.at("nodes").at(index);
		if (!node.is_object() || node.size() != 2 || !node.contains("begin") || !node.contains("end"))
			throw std::invalid_argument("common-denotation Anum node must contain begin and end");
		structural.nodes.push_back(avm::AnumDenotationNode{
		    static_cast<avm::ProjectionNodeId>(index),
		    parse_anum_ref(node.at("begin"), source_anchors),
		    parse_anum_ref(node.at("end"), source_anchors),
		});
	}
	structural.root = parse_anum_ref(encoded.at("root"), source_anchors);
	return avm::CanonicalAnumDenotation::structural_result(std::move(structural));
}

struct Harness
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime;
	avm::IntegerVocabulary integers;
	avm::TextVocabulary text;

	Harness()
	    : runtime(store), integers(avm::IntegerVocabulary::create(store)), text(avm::TextVocabulary::create(store))
	{
	}
};

AnchorMap prepare_anchors(Harness &harness, const Json &test_case)
{
	AnchorMap result;
	for (const std::string &name : source_anchor_names(test_case))
	{
		avm::LinkId id = avm::invalid_link_id;
		if (name == "quote_relation")
			id = harness.runtime.vocabulary().quote_relation;
		else if (name == "unit")
			id = harness.runtime.vocabulary().unit;
		else
			id = harness.store.create_point();
		assert(result.emplace(name, id).second);
	}
	return result;
}

avm::AnumAnchorResolver anum_resolver(const AnchorMap &anchors)
{
	return [&anchors](std::string_view name) -> std::optional<avm::LinkId>
	{
		const auto it = anchors.find(std::string(name));
		if (it == anchors.end())
			return std::nullopt;
		return it->second;
	};
}

avm::ProjectionDescription json_projection(Harness &harness, const Json &test_case, const AnchorMap &anchors)
{
	avm::json_duplet::SymbolAnchors symbols(anchors.begin(), anchors.end());
	const avm::json_duplet::NativeLeafResolver resolver(harness.integers, harness.text, std::move(symbols));
	return avm::json_duplet::project_duplet_document(test_case.at("json"), resolver);
}

avm::ProjectionDescription anum_projection(const Json &test_case, const AnchorMap &anchors)
{
	const avm::CanonicalAnumDenotation denotation = parse_anum_denotation(test_case);
	const auto projection = avm::bridge_anum_denotation(denotation, anum_resolver(anchors));
	assert(projection.has_value());
	return *projection;
}

void verify_realized_semantics(Harness &harness, const Json &test_case, const AnchorMap &anchors, avm::LinkId root)
{
	if (test_case.value("assert_shared_root_ends", false))
	{
		const avm::Link root_link = harness.store.get(root);
		assert(root_link.begin == root_link.end);
		const avm::Link shared = harness.store.get(root_link.begin);
		assert(shared.begin == anchors.at("a"));
		assert(shared.end == anchors.at("b"));
	}

	if (test_case.contains("relation_entity"))
	{
		const Json &expected = test_case.at("relation_entity");
		assert(expected.is_array() && expected.size() == 3);
		const avm::RelationEntity entity = avm::decode_relation_entity(harness.store, root);
		assert(entity.relation == anchors.at(expected.at(0).get<std::string>()));
		assert(entity.subject == anchors.at(expected.at(1).get<std::string>()));
		assert(entity.object == anchors.at(expected.at(2).get<std::string>()));
	}

	if (test_case.contains("execute_returns"))
	{
		const std::string expected = test_case.at("execute_returns").get<std::string>();
		const std::size_t before_execution = harness.store.size();
		assert(harness.runtime.executor().execute(root) == anchors.at(expected));
		assert(harness.store.size() == before_execution);
	}
}

void verify_case_order(const Json &test_case, bool json_first)
{
	Harness harness;
	const AnchorMap anchors = prepare_anchors(harness, test_case);
	const std::size_t before_projection = harness.store.size();

	const avm::ProjectionDescription from_json = json_projection(harness, test_case, anchors);
	assert(harness.store.size() == before_projection);
	const avm::ProjectionDescription from_anum = anum_projection(test_case, anchors);
	assert(harness.store.size() == before_projection);

	if (test_case.at("id") == "shared")
		assert(from_json.nodes.size() > from_anum.nodes.size());

	const auto json_before = avm::find_projection(harness.store, from_json);
	const auto anum_before = avm::find_projection(harness.store, from_anum);
	assert(harness.store.size() == before_projection);
	if (from_anum.nodes.empty())
	{
		assert(json_before.has_value());
		assert(anum_before.has_value());
		assert(json_before->root == anum_before->root);
	}
	else
	{
		assert(!json_before.has_value());
		assert(!anum_before.has_value());
	}

	const avm::ProjectionDescription &first = json_first ? from_json : from_anum;
	const avm::ProjectionDescription &second = json_first ? from_anum : from_json;
	const avm::ProjectionResult first_realized = avm::realize_projection(harness.store, first);
	const std::size_t after_first_realize = harness.store.size();

	const auto second_found = avm::find_projection(harness.store, second);
	assert(second_found.has_value());
	assert(second_found->root == first_realized.root);
	assert(harness.store.size() == after_first_realize);

	const avm::ProjectionResult second_realized = avm::realize_projection(harness.store, second);
	assert(second_realized.root == first_realized.root);
	assert(harness.store.size() == after_first_realize);

	const avm::ProjectionResult first_repeated = avm::realize_projection(harness.store, first);
	assert(first_repeated.root == first_realized.root);
	assert(harness.store.size() == after_first_realize);

	const auto json_after = avm::find_projection(harness.store, from_json);
	const auto anum_after = avm::find_projection(harness.store, from_anum);
	assert(json_after.has_value() && anum_after.has_value());
	assert(json_after->root == first_realized.root);
	assert(anum_after->root == first_realized.root);

	verify_realized_semantics(harness, test_case, anchors, first_realized.root);
}

void verify_common_corpus(const Json &corpus)
{
	assert(corpus.at("$schema") == "avm/frontend-common-denotation/v1");
	assert(corpus.at("cases").is_array());
	assert(corpus.at("cases").size() == 6);

	std::set<std::string> ids;
	for (const Json &test_case : corpus.at("cases"))
	{
		const std::string id = test_case.at("id").get<std::string>();
		assert(ids.insert(id).second);
		verify_case_order(test_case, true);
		verify_case_order(test_case, false);
	}
	assert(ids == std::set<std::string>({"anchor", "executable-quote", "nested", "pair", "relation-entity", "shared"}));
}

void verify_failure_matrix()
{
	Harness harness;

	avm::json_duplet::SymbolAnchors no_symbols;
	const auto unresolved_json_resolver =
	    avm::json_duplet::NativeLeafResolver(harness.integers, harness.text, std::move(no_symbols));
	Json unresolved_document = Json::object();
	unresolved_document["$avm"] = "duplet-json/1";
	unresolved_document["$root"] = Json::object({{"$symbol", "missing"}});
	const std::size_t before_unresolved_json = harness.store.size();
	bool unresolved_json_rejected = false;
	try
	{
		static_cast<void>(avm::json_duplet::project_duplet_document(unresolved_document, unresolved_json_resolver));
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		unresolved_json_rejected = true;
	}
	assert(unresolved_json_rejected);
	assert(harness.store.size() == before_unresolved_json);

	const avm::CanonicalAnumDenotation unresolved_anum = avm::CanonicalAnumDenotation::structural_result(
	    avm::AnumStructuralDenotation{{"missing"}, {}, avm::AnumDenotationRef::anchor_ref("missing")});
	const std::size_t before_unresolved_anum = harness.store.size();
	bool unresolved_anum_rejected = false;
	try
	{
		static_cast<void>(avm::bridge_anum_denotation(
		    unresolved_anum, [](std::string_view) -> std::optional<avm::LinkId> { return std::nullopt; }));
	}
	catch (const std::invalid_argument &)
	{
		unresolved_anum_rejected = true;
	}
	assert(unresolved_anum_rejected);
	assert(harness.store.size() == before_unresolved_anum);

	const avm::LinkId missing = static_cast<avm::LinkId>(harness.store.size() + 1000);
	assert(!harness.store.contains(missing));
	avm::json_duplet::SymbolAnchors missing_symbol{{"missing", missing}};
	const auto missing_json_resolver =
	    avm::json_duplet::NativeLeafResolver(harness.integers, harness.text, std::move(missing_symbol));
	const avm::ProjectionDescription missing_json =
	    avm::json_duplet::project_duplet_document(unresolved_document, missing_json_resolver);
	const AnchorMap missing_anchors{{"missing", missing}};
	const auto missing_anum = avm::bridge_anum_denotation(unresolved_anum, anum_resolver(missing_anchors));
	assert(missing_anum.has_value());

	const std::size_t before_missing_anchor = harness.store.size();
	assert(!avm::find_projection(harness.store, missing_json).has_value());
	assert(!avm::find_projection(harness.store, *missing_anum).has_value());
	assert(harness.store.size() == before_missing_anchor);

	bool missing_json_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(harness.store, missing_json));
	}
	catch (const std::invalid_argument &)
	{
		missing_json_realize_rejected = true;
	}
	assert(missing_json_realize_rejected);
	assert(harness.store.size() == before_missing_anchor);

	bool missing_anum_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(harness.store, *missing_anum));
	}
	catch (const std::invalid_argument &)
	{
		missing_anum_realize_rejected = true;
	}
	assert(missing_anum_realize_rejected);
	assert(harness.store.size() == before_missing_anchor);

	const avm::LinkId existing = harness.store.create_point();
	avm::AnumStructuralDenotation malformed;
	malformed.anchors = {"a"};
	malformed.nodes.push_back(avm::AnumDenotationNode{
	    0,
	    avm::AnumDenotationRef::node_ref(0),
	    avm::AnumDenotationRef::anchor_ref("a"),
	});
	malformed.root = avm::AnumDenotationRef::node_ref(0);
	const std::size_t before_malformed_anum = harness.store.size();
	bool malformed_anum_rejected = false;
	try
	{
		static_cast<void>(avm::bridge_anum_denotation(
		    avm::CanonicalAnumDenotation::structural_result(std::move(malformed)),
		    [existing](std::string_view) -> std::optional<avm::LinkId> { return existing; }));
	}
	catch (const std::invalid_argument &)
	{
		malformed_anum_rejected = true;
	}
	assert(malformed_anum_rejected);
	assert(harness.store.size() == before_malformed_anum);

	avm::json_duplet::SymbolAnchors existing_symbol{{"a", existing}};
	const auto existing_json_resolver =
	    avm::json_duplet::NativeLeafResolver(harness.integers, harness.text, std::move(existing_symbol));
	Json malformed_document = Json::object();
	malformed_document["$avm"] = "duplet-json/1";
	malformed_document["$root"] = Json::object({{"<<", Json::object({{"$symbol", "a"}})}});
	const std::size_t before_malformed_json = harness.store.size();
	bool malformed_json_rejected = false;
	try
	{
		static_cast<void>(avm::json_duplet::project_duplet_document(malformed_document, existing_json_resolver));
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		malformed_json_rejected = true;
	}
	assert(malformed_json_rejected);
	assert(harness.store.size() == before_malformed_json);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
		throw std::runtime_error("expected frontend-common-denotation/v1 corpus path");

	verify_common_corpus(load_json(argv[1]));
	verify_failure_matrix();
	return 0;
}
