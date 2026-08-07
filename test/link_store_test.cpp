#include "avm/link_store.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace
{

bool contains_id(const std::vector<avm::LinkId> &ids, avm::LinkId id)
{
	return std::find(ids.begin(), ids.end(), id) != ids.end();
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;

	assert(store.size() == 0);

	const avm::LinkId a = store.create_point();
	const avm::LinkId b = store.create_point();

	assert(a != avm::invalid_link_id);
	assert(b != avm::invalid_link_id);
	assert(a != b);
	assert(store.get(a) == (avm::Link{a, a}));
	assert(store.get(b) == (avm::Link{b, b}));
	assert(store.find(a, a) == a);
	assert(store.find(b, b) == b);

	const std::size_t before_find = store.size();
	assert(!store.find(a, b).has_value());
	assert(store.size() == before_find);

	const avm::LinkId ab = store.intern(a, b);
	assert(store.get(ab) == (avm::Link{a, b}));
	assert(store.find(a, b) == ab);

	const std::size_t before_reintern = store.size();
	assert(store.intern(a, b) == ab);
	assert(store.size() == before_reintern);

	assert(contains_id(store.outgoing(a), a));
	assert(contains_id(store.outgoing(a), ab));
	assert(contains_id(store.incoming(b), b));
	assert(contains_id(store.incoming(b), ab));

	bool invalid_endpoint_rejected = false;
	try
	{
		static_cast<void>(store.intern(a, 999999));
	}
	catch (const std::invalid_argument &)
	{
		invalid_endpoint_rejected = true;
	}
	assert(invalid_endpoint_rejected);

	bool invalid_get_rejected = false;
	try
	{
		static_cast<void>(store.get(999999));
	}
	catch (const std::out_of_range &)
	{
		invalid_get_rejected = true;
	}
	assert(invalid_get_rejected);

	return 0;
}
