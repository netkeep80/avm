#include "avm/projection.h"
#include "avm/raw_carrier.h"
#include "avm/relations_model.h"

#include <cassert>
#include <stdexcept>

namespace
{

struct ToyContext
{
	avm::LinkId relation;
	avm::LinkId subject;
	avm::LinkId object;
};

class BinaryToyAdapter
{
public:
	avm::ProjectionDescription project(const avm::RawBytes &source, const ToyContext &context) const
	{
		if (source != avm::RawBytes{0xca, 0xfe, 0x01})
			throw std::invalid_argument("unexpected toy binary source");

		return avm::ProjectionDescription{
		    {
		        {avm::ProjectionRef::anchor(context.subject), avm::ProjectionRef::anchor(context.object)},
		        {avm::ProjectionRef::anchor(context.relation), avm::ProjectionRef::node(0)},
		    },
		    avm::ProjectionRef::node(1),
		};
	}
};

struct ToyAst
{
	bool relation_entity = false;
};

class AstToyAdapter
{
public:
	avm::ProjectionDescription project(const ToyAst &source, const ToyContext &context) const
	{
		if (!source.relation_entity)
			throw std::invalid_argument("toy AST does not describe a relation entity");

		avm::ProjectionDescription description;
		description.nodes.push_back(
		    {avm::ProjectionRef::anchor(context.subject), avm::ProjectionRef::anchor(context.object)});
		description.nodes.push_back(
		    {avm::ProjectionRef::anchor(context.relation), avm::ProjectionRef::node(0)});
		description.root = avm::ProjectionRef::node(1);
		return description;
	}
};

} // namespace

int main()
{
	avm::InMemoryRawCarrier raw;
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const ToyContext context{relation, subject, object};

	const std::size_t anchor_count = store.size();
	const avm::RawDocumentId raw_id = raw.put({0xca, 0xfe, 0x01});
	assert(store.size() == anchor_count);

	const auto loaded = raw.get(raw_id);
	assert(loaded.has_value());
	const BinaryToyAdapter binary_adapter;
	const avm::ProjectionDescription binary_projection = binary_adapter.project(*loaded, context);
	assert(store.size() == anchor_count);

	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, binary_projection).has_value());
	assert(store.size() == before_find);

	const avm::ProjectionResult realized = avm::realize_projection(store, binary_projection);
	assert(realized.nodes.size() == 2);
	assert(realized.root == realized.nodes[1]);
	assert(avm::decode_relation_entity(store, realized.root) ==
	       (avm::RelationEntity{relation, subject, object}));

	const std::size_t after_realize = store.size();
	const AstToyAdapter ast_adapter;
	const avm::ProjectionDescription ast_projection = ast_adapter.project(ToyAst{true}, context);
	assert(store.size() == after_realize);

	const auto found_via_other_adapter = avm::find_projection(store, ast_projection);
	assert(found_via_other_adapter.has_value());
	assert(found_via_other_adapter->root == realized.root);
	assert(found_via_other_adapter->nodes == realized.nodes);
	assert(store.size() == after_realize);

	const avm::ProjectionResult realized_again = avm::realize_projection(store, ast_projection);
	assert(realized_again.root == realized.root);
	assert(realized_again.nodes == realized.nodes);
	assert(store.size() == after_realize);

	assert(raw.erase(raw_id));
	assert(!raw.contains(raw_id));
	assert(store.size() == after_realize);
	assert(avm::find_projection(store, ast_projection)->root == realized.root);

	const avm::LinkId another_object = store.create_point();
	const ToyContext another_context{relation, subject, another_object};
	const avm::ProjectionDescription context_projection = ast_adapter.project(ToyAst{true}, another_context);
	const std::size_t before_context_find = store.size();
	assert(!avm::find_projection(store, context_projection).has_value());
	assert(store.size() == before_context_find);

	const avm::ProjectionResult context_realized = avm::realize_projection(store, context_projection);
	assert(context_realized.root != realized.root);
	assert(avm::decode_relation_entity(store, context_realized.root) ==
	       (avm::RelationEntity{relation, subject, another_object}));

	const ToyContext missing_context{relation, subject, 999999};
	const avm::ProjectionDescription missing_projection = ast_adapter.project(ToyAst{true}, missing_context);
	const std::size_t before_missing = store.size();
	assert(!avm::find_projection(store, missing_projection).has_value());
	assert(store.size() == before_missing);

	bool missing_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, missing_projection));
	}
	catch (const std::invalid_argument &)
	{
		missing_realize_rejected = true;
	}
	assert(missing_realize_rejected);
	assert(store.size() == before_missing);

	bool bad_binary_rejected = false;
	try
	{
		static_cast<void>(binary_adapter.project(avm::RawBytes{0x00}, context));
	}
	catch (const std::invalid_argument &)
	{
		bad_binary_rejected = true;
	}
	assert(bad_binary_rejected);
	assert(store.size() == before_missing);

	bool bad_ast_rejected = false;
	try
	{
		static_cast<void>(ast_adapter.project(ToyAst{false}, context));
	}
	catch (const std::invalid_argument &)
	{
		bad_ast_rejected = true;
	}
	assert(bad_ast_rejected);
	assert(store.size() == before_missing);

	return 0;
}
