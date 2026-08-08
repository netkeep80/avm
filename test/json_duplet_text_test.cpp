#include "json_duplet_text.h"

#include "avm/link_store.h"
#include "avm/projection.h"
#include "avm/relations_model.h"

#include <cassert>
#include <string>

namespace
{

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

bool text_rejected(const std::string &text, bool document)
{
	try
	{
		if (document)
			static_cast<void>(avm::json_duplet::project_duplet_document_text(text));
		else
			static_cast<void>(avm::json_duplet::project_duplet_term_text(text));
		return false;
	}
	catch (const std::exception &)
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

	const std::string relation_source = relation_text(relation, subject, object);
	const std::string document_source = document_text(relation_source);

	const avm::ProjectionDescription term_description = avm::json_duplet::project_duplet_term_text(relation_source);
	const avm::ProjectionDescription document_description =
	    avm::json_duplet::project_duplet_document_text(document_source);
	assert(term_description.nodes == document_description.nodes);
	assert(term_description.root == document_description.root);

	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, document_description).has_value());
	assert(store.size() == before_find);

	const avm::ProjectionResult realized = avm::realize_projection(store, document_description);
	const avm::RelationEntity decoded = avm::decode_relation_entity(store, realized.root);
	const avm::RelationEntity expected{relation, subject, object};
	assert(decoded == expected);

	const std::string duplicate_avm =
	    "{\"$avm\":\"duplet-json/1\",\"$avm\":\"duplet-json/1\",\"$root\":" + relation_source + "}";
	assert(text_rejected(duplicate_avm, true));

	const std::string duplicate_root =
	    "{\"$avm\":\"duplet-json/1\",\"$root\":" + relation_source + ",\"$root\":" + relation_source + "}";
	assert(text_rejected(duplicate_root, true));

	const std::string duplicate_begin = "{\"<<\":" + anchor_text(relation) + ",\"<<\":" +
	                                    anchor_text(subject) + ",\">>\":" + anchor_text(object) + "}";
	assert(text_rejected(duplicate_begin, false));

	const std::string duplicate_end = "{\"<<\":" + anchor_text(relation) + ",\">>\":" + anchor_text(subject) +
	                                  ",\">>\":" + anchor_text(object) + "}";
	assert(text_rejected(duplicate_end, false));

	const std::string duplicate_link = "{\"$link\":" + std::to_string(subject) + ",\"$link\":" +
	                                   std::to_string(object) + "}";
	assert(text_rejected(duplicate_link, false));

	const std::string sibling_same_keys = duplet_text(anchor_text(subject), anchor_text(object));
	const avm::ProjectionDescription sibling_description =
	    avm::json_duplet::project_duplet_term_text(sibling_same_keys);
	assert(sibling_description.nodes.size() == 1);

	assert(text_rejected("{", false));
	assert(text_rejected("{\"$avm\":\"duplet-json/1\",\"$root\":}", true));

	return 0;
}
