#include "avm/projection.h"
#include "avm/raw_carrier.h"

#include <cassert>

int main()
{
	avm::InMemoryRawCarrier raw;
	avm::InMemoryLinkStore store;

	const std::size_t initial_link_count = store.size();
	const avm::RawBytes binary_payload{0x00, 0x01, 0x7f, 0x80, 0xff, 0x00};
	const avm::RawDocumentId first = raw.put(binary_payload);
	assert(first != avm::invalid_raw_document_id);
	assert(raw.contains(first));
	assert(raw.size() == 1);
	assert(raw.get(first).has_value());
	assert(*raw.get(first) == binary_payload);
	assert(store.size() == initial_link_count);

	const avm::RawDocumentId same_bytes = raw.put(binary_payload);
	assert(same_bytes != first);
	assert(raw.size() == 2);
	assert(*raw.get(same_bytes) == binary_payload);
	assert(store.size() == initial_link_count);

	const avm::RawDocumentId empty = raw.put({});
	assert(raw.contains(empty));
	assert(raw.get(empty).has_value());
	assert(raw.get(empty)->empty());
	assert(raw.size() == 3);
	assert(store.size() == initial_link_count);

	assert(!raw.contains(avm::invalid_raw_document_id));
	assert(!raw.get(avm::invalid_raw_document_id).has_value());
	assert(!raw.get(999999).has_value());
	assert(!raw.erase(999999));
	assert(store.size() == initial_link_count);

	const avm::LinkId a = store.create_point();
	const avm::LinkId b = store.create_point();
	const avm::ProjectionDescription description{
	    {{avm::ProjectionRef::anchor(a), avm::ProjectionRef::anchor(b)}},
	    avm::ProjectionRef::node(0),
	};

	const std::size_t before_projection = store.size();
	assert(!avm::find_projection(store, description).has_value());
	assert(store.size() == before_projection);

	const avm::ProjectionResult realized = avm::realize_projection(store, description);
	assert(store.contains(realized.root));
	assert(store.get(realized.root) == (avm::Link{a, b}));
	const std::size_t realized_link_count = store.size();

	assert(raw.erase(first));
	assert(!raw.contains(first));
	assert(!raw.get(first).has_value());
	assert(raw.size() == 2);
	assert(store.size() == realized_link_count);
	assert(store.contains(realized.root));
	assert(avm::find_projection(store, description)->root == realized.root);

	raw.erase(same_bytes);
	raw.erase(empty);
	assert(raw.size() == 0);
	assert(store.size() == realized_link_count);
	assert(store.contains(realized.root));

	const avm::RawDocumentId after_clear = raw.put({0x41, 0x00, 0x42});
	assert(after_clear != first);
	assert(after_clear != same_bytes);
	assert(after_clear != empty);
	assert(*raw.get(after_clear) == (avm::RawBytes{0x41, 0x00, 0x42}));
	assert(store.size() == realized_link_count);

	return 0;
}
