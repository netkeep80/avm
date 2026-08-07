#include "avm/projection.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId a = store.create_point();
	const avm::LinkId b = store.create_point();
	const avm::LinkId c = store.create_point();

	const avm::ProjectionDescription single{
	    {{avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)}},
	    avm::ProjectionRef::node(0),
	};
	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, single).has_value());
	assert(store.size() == before_find);

	const avm::ProjectionResult single_realized = avm::realize_projection(store, single);
	assert(single_realized.nodes.size() == 1);
	assert(single_realized.root == single_realized.nodes[0]);
	assert(store.get(single_realized.root) == (avm::Link{a, b}));

	const std::size_t after_single_realize = store.size();
	const auto single_found = avm::find_projection(store, single);
	assert(single_found.has_value());
	assert(single_found->root == single_realized.root);
	assert(single_found->nodes == single_realized.nodes);
	assert(store.size() == after_single_realize);
	assert(avm::realize_projection(store, single).root == single_realized.root);
	assert(store.size() == after_single_realize);

	const avm::ProjectionDescription nested{
	    {
	        {avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)},
	        {avm::ProjectionRef::node(0), avm::ProjectionRef::anchor(c)},
	    },
	    avm::ProjectionRef::node(1),
	};
	const std::size_t before_nested_find = store.size();
	assert(!avm::find_projection(store, nested).has_value());
	assert(store.size() == before_nested_find);

	const avm::ProjectionResult nested_realized = avm::realize_projection(store, nested);
	assert(nested_realized.nodes.size() == 2);
	assert(nested_realized.nodes[0] == single_realized.root);
	assert(store.get(nested_realized.root) == (avm::Link{single_realized.root, c}));

	const std::size_t after_nested_realize = store.size();
	const auto nested_found = avm::find_projection(store, nested);
	assert(nested_found.has_value());
	assert(nested_found->root == nested_realized.root);
	assert(nested_found->nodes == nested_realized.nodes);
	assert(store.size() == after_nested_realize);

	const avm::ProjectionDescription duplicate_nodes{
	    {
	        {avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)},
	        {avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)},
	    },
	    avm::ProjectionRef::node(1),
	};
	const std::size_t before_duplicate = store.size();
	const avm::ProjectionResult duplicate_result = avm::realize_projection(store, duplicate_nodes);
	assert(duplicate_result.nodes.size() == 2);
	assert(duplicate_result.nodes[0] == duplicate_result.nodes[1]);
	assert(duplicate_result.root == single_realized.root);
	assert(store.size() == before_duplicate);

	const avm::ProjectionDescription anchor_root{{}, avm::ProjectionRef::anchor(c)};
	const std::size_t before_anchor_root = store.size();
	const auto anchor_found = avm::find_projection(store, anchor_root);
	assert(anchor_found.has_value());
	assert(anchor_found->root == c);
	assert(anchor_found->nodes.empty());
	assert(avm::realize_projection(store, anchor_root).root == c);
	assert(store.size() == before_anchor_root);

	const avm::LinkId missing_anchor = 999999;
	const avm::ProjectionDescription missing{
	    {{avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(missing_anchor)}},
	    avm::ProjectionRef::node(0),
	};
	const std::size_t before_missing_find = store.size();
	assert(!avm::find_projection(store, missing).has_value());
	assert(store.size() == before_missing_find);

	bool missing_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, missing));
	}
	catch (const std::invalid_argument &)
	{
		missing_realize_rejected = true;
	}
	assert(missing_realize_rejected);
	assert(store.size() == before_missing_find);

	const avm::LinkId d = store.create_point();
	const avm::LinkId e = store.create_point();
	const avm::ProjectionDescription no_partial_write{
	    {
	        {avm::ProjectionRef::anchor(d), avm::ProjectionRef::anchor(e)},
	        {avm::ProjectionRef::node(0), avm::ProjectionRef::anchor(missing_anchor)},
	    },
	    avm::ProjectionRef::node(1),
	};
	const std::size_t before_no_partial_write = store.size();
	bool no_partial_write_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, no_partial_write));
	}
	catch (const std::invalid_argument &)
	{
		no_partial_write_rejected = true;
	}
	assert(no_partial_write_rejected);
	assert(store.size() == before_no_partial_write);
	assert(!store.find(d, e).has_value());

	const std::vector<avm::ProjectionDescription> malformed{
	    {{{avm::ProjectionRef::node(0), avm::ProjectionRef::anchor(a)}}, avm::ProjectionRef::node(0)},
	    {{{avm::ProjectionRef::node(1), avm::ProjectionRef::anchor(a)}}, avm::ProjectionRef::node(0)},
	    {{{avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)}}, avm::ProjectionRef::node(1)},
	    {{{avm::ProjectionRef::anchor(avm::invalid_link_id), avm::ProjectionRef::anchor(a)}},
	     avm::ProjectionRef::node(0)},
	};

	for (const avm::ProjectionDescription &description : malformed)
	{
		const std::size_t before_invalid = store.size();
		bool find_rejected = false;
		try
		{
			static_cast<void>(avm::find_projection(store, description));
		}
		catch (const std::invalid_argument &)
		{
			find_rejected = true;
		}
		assert(find_rejected);
		assert(store.size() == before_invalid);

		bool realize_rejected = false;
		try
		{
			static_cast<void>(avm::realize_projection(store, description));
		}
		catch (const std::invalid_argument &)
		{
			realize_rejected = true;
		}
		assert(realize_rejected);
		assert(store.size() == before_invalid);
	}

	return 0;
}
