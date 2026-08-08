#include "json_duplet_projection.h"
#include "json_duplet_text.h"

#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

using Json = nlohmann::ordered_json;

Json anchor(avm::LinkId id)
{
	Json value = Json::object();
	value["$link"] = id;
	return value;
}

Json duplet(Json begin, Json end)
{
	Json value = Json::object();
	value["<<"] = std::move(begin);
	value[">>"] = std::move(end);
	return value;
}

Json relation_term(avm::LinkId relation, avm::LinkId subject, avm::LinkId object)
{
	return duplet(anchor(relation), duplet(anchor(subject), anchor(object)));
}

Json document(Json root)
{
	Json value = Json::object();
	value["$avm"] = "duplet-json/1";
	value["$root"] = std::move(root);
	return value;
}

std::string anchor_text(avm::LinkId id)
{
	return "{\"$link\":" + std::to_string(id) + "}";
}

std::string duplet_text(const std::string &begin, const std::string &end)
{
	return "{\"<<\":" + begin + ",\">>\":" + end + "}";
}

std::string relation_text(avm::LinkId relation, avm::LinkId subject, avm::LinkId object)
{
	return duplet_text(anchor_text(relation), duplet_text(anchor_text(subject), anchor_text(object)));
}

std::string document_text(const std::string &root)
{
	return "{\"$avm\":\"duplet-json/1\",\"$root\":" + root + "}";
}

bool projection_rejected(const Json &value, bool as_document = false)
{
	try
	{
		if (as_document)
			static_cast<void>(avm::json_duplet::project_duplet_document(value));
		else
			static_cast<void>(avm::json_duplet::project_duplet_term(value));
		return false;
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		return true;
	}
}

bool text_projection_rejected(const std::string &text, bool as_document = false)
{
	try
	{
		if (as_document)
			static_cast<void>(avm::json_duplet::project_duplet_document_text<Json>(text));
		else
			static_cast<void>(avm::json_duplet::project_duplet_term_text<Json>(text));
		return false;
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		return true;
	}
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();

	const Json term = relation_term(relation, subject, object);
	const avm::ProjectionDescription description = avm::json_duplet::project_duplet_term(term);
	assert(description.nodes.size() == 2);
	const avm::ProjectionNode expected_arguments{
	    avm::ProjectionRef::anchor(subject),
	    avm::ProjectionRef::anchor(object),
	};
	const avm::ProjectionNode expected_entity{
	    avm::ProjectionRef::anchor(relation),
	    avm::ProjectionRef::node(0),
	};
	assert(description.nodes[0] == expected_arguments);
	assert(description.nodes[1] == expected_entity);
	assert(description.root == avm::ProjectionRef::node(1));

	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, description).has_value());
	assert(store.size() == before_find);

	const avm::ProjectionResult realized = avm::realize_projection(store, description);
	assert(realized.nodes.size() == 2);
	assert(realized.root == realized.nodes[1]);
	const avm::RelationEntity decoded = avm::decode_relation_entity(store, realized.root);
	const avm::RelationEntity expected_relation{relation, subject, object};
	assert(decoded == expected_relation);

	const std::size_t before_repeat = store.size();
	const avm::ProjectionResult repeated = avm::realize_projection(store, description);
	assert(repeated.root == realized.root);
	assert(repeated.nodes == realized.nodes);
	assert(store.size() == before_repeat);

	const auto found = avm::find_projection(store, description);
	assert(found.has_value());
	assert(found->root == realized.root);

	const avm::ProjectionDescription documented = avm::json_duplet::project_duplet_document(document(term));
	assert(documented.nodes == description.nodes);
	assert(documented.root == description.root);

	const std::string raw_term = relation_text(relation, subject, object);
	const std::string raw_document = document_text(raw_term);
	const std::size_t before_text_projection = store.size();
	const avm::ProjectionDescription text_term = avm::json_duplet::project_duplet_term_text<Json>(raw_term);
	const avm::ProjectionDescription text_document = avm::json_duplet::project_duplet_document_text<Json>(raw_document);
	assert(text_term.nodes == description.nodes);
	assert(text_term.root == description.root);
	assert(text_document.nodes == description.nodes);
	assert(text_document.root == description.root);
	assert(store.size() == before_text_projection);

	const avm::ProjectionDescription anchor_only = avm::json_duplet::project_duplet_term(anchor(subject));
	assert(anchor_only.nodes.empty());
	assert(anchor_only.root == avm::ProjectionRef::anchor(subject));
	assert(avm::find_projection(store, anchor_only)->root == subject);

	const avm::LinkId missing_id = store.size() + 1000;
	const Json missing_term = duplet(anchor(relation), anchor(missing_id));
	const avm::ProjectionDescription missing_description = avm::json_duplet::project_duplet_term(missing_term);
	const std::size_t before_missing_find = store.size();
	assert(!avm::find_projection(store, missing_description).has_value());
	assert(store.size() == before_missing_find);

	bool missing_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, missing_description));
	}
	catch (const std::invalid_argument &)
	{
		missing_realize_rejected = true;
	}
	assert(missing_realize_rejected);
	assert(store.size() == before_missing_find);

	Json incomplete_pair = Json::object();
	incomplete_pair["<<"] = anchor(relation);
	assert(projection_rejected(incomplete_pair));

	Json mixed_pair = duplet(anchor(relation), anchor(subject));
	mixed_pair["extra"] = true;
	assert(projection_rejected(mixed_pair));

	Json zero_anchor = Json::object();
	zero_anchor["$link"] = 0;
	assert(projection_rejected(zero_anchor));

	Json negative_anchor = Json::object();
	negative_anchor["$link"] = -1;
	assert(projection_rejected(negative_anchor));

	Json text_anchor = Json::object();
	text_anchor["$link"] = "1";
	assert(projection_rejected(text_anchor));

	Json mixed_anchor = anchor(relation);
	mixed_anchor["extra"] = true;
	assert(projection_rejected(mixed_anchor));

	Json unknown_leaf = Json::object();
	unknown_leaf["$symbol"] = "R";
	assert(projection_rejected(unknown_leaf));
	assert(projection_rejected(Json::array({anchor(relation)})));
	assert(projection_rejected(Json(7)));
	assert(projection_rejected(Json("R")));

	Json bad_document = document(term);
	bad_document["$avm"] = "duplet-json/2";
	assert(projection_rejected(bad_document, true));

	Json extra_document = document(term);
	extra_document["extra"] = true;
	assert(projection_rejected(extra_document, true));

	std::string duplicate_avm = "{\"$avm\":\"duplet-json/1\",\"$avm\":\"duplet-json/1\",\"$root\":";
	duplicate_avm += raw_term;
	duplicate_avm += "}";
	assert(text_projection_rejected(duplicate_avm, true));

	std::string duplicate_root = "{\"$avm\":\"duplet-json/1\",\"$root\":";
	duplicate_root += raw_term;
	duplicate_root += ",\"$root\":";
	duplicate_root += raw_term;
	duplicate_root += "}";
	assert(text_projection_rejected(duplicate_root, true));

	std::string duplicate_begin = "{\"<<\":";
	duplicate_begin += anchor_text(relation);
	duplicate_begin += ",\"<<\":";
	duplicate_begin += anchor_text(subject);
	duplicate_begin += ",\">>\":";
	duplicate_begin += anchor_text(object);
	duplicate_begin += "}";
	assert(text_projection_rejected(duplicate_begin));

	std::string duplicate_end = "{\"<<\":";
	duplicate_end += anchor_text(relation);
	duplicate_end += ",\">>\":";
	duplicate_end += anchor_text(subject);
	duplicate_end += ",\">>\":";
	duplicate_end += anchor_text(object);
	duplicate_end += "}";
	assert(text_projection_rejected(duplicate_end));

	std::string duplicate_link = "{\"$link\":";
	duplicate_link += std::to_string(subject);
	duplicate_link += ",\"$link\":";
	duplicate_link += std::to_string(object);
	duplicate_link += "}";
	assert(text_projection_rejected(duplicate_link));

	const std::string sibling_anchor_pair = duplet_text(anchor_text(subject), anchor_text(object));
	assert(!text_projection_rejected(sibling_anchor_pair));
	assert(text_projection_rejected("{"));
	assert(text_projection_rejected("{\"$avm\":\"duplet-json/1\",\"$root\":}", true));

	const std::filesystem::path persistent_path =
	    std::filesystem::temp_directory_path() / "avm_duplet_json_projection_test.links";
	std::filesystem::remove(persistent_path);

	avm::LinkId persistent_relation = avm::invalid_link_id;
	avm::LinkId persistent_subject = avm::invalid_link_id;
	avm::LinkId persistent_object = avm::invalid_link_id;
	avm::LinkId persistent_root = avm::invalid_link_id;
	Json persistent_document;
	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		persistent_relation = persistent_store.create_point();
		persistent_subject = persistent_store.create_point();
		persistent_object = persistent_store.create_point();
		persistent_document = document(relation_term(persistent_relation, persistent_subject, persistent_object));
		const avm::ProjectionDescription persistent_description =
		    avm::json_duplet::project_duplet_document(persistent_document);
		persistent_root = avm::realize_projection(persistent_store, persistent_description).root;
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		const avm::ProjectionDescription persistent_description =
		    avm::json_duplet::project_duplet_document(persistent_document);
		const auto persistent_found = avm::find_projection(reopened, persistent_description);
		assert(persistent_found.has_value());
		assert(persistent_found->root == persistent_root);
		const avm::RelationEntity persistent_decoded = avm::decode_relation_entity(reopened, persistent_root);
		const avm::RelationEntity persistent_expected{persistent_relation, persistent_subject, persistent_object};
		assert(persistent_decoded == persistent_expected);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
