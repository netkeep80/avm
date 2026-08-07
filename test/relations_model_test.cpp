#include "avm/relations_model.h"

#include <cassert>

int main()
{
	avm::InMemoryLinkStore store;

	const avm::LinkId relation = store.create_point();
	const avm::LinkId relation2 = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();

	const avm::RelationEntity source{relation, subject, object};

	// A lookup for an absent triplet must not even create its inner (subject, object) dyad.
	const std::size_t before_find = store.size();
	assert(!avm::find_relation_entity(store, source).has_value());
	assert(store.size() == before_find);
	assert(!store.find(subject, object).has_value());

	// The same rule holds when the inner dyad already exists but the outer entity does not.
	const avm::LinkId subject_object_preexisting = store.intern(subject, object);
	const std::size_t before_outer_find = store.size();
	assert(!avm::find_relation_entity(store, source).has_value());
	assert(store.size() == before_outer_find);

	const avm::LinkId encoded = avm::encode_relation_entity(store, source);
	assert(avm::decode_relation_entity(store, encoded) == source);
	assert(avm::find_relation_entity(store, source) == encoded);

	const auto subject_object = store.find(subject, object);
	assert(subject_object == subject_object_preexisting);
	assert(store.get(encoded) == (avm::Link{relation, *subject_object}));

	const std::size_t before_reencode = store.size();
	assert(avm::encode_relation_entity(store, source) == encoded);
	assert(store.size() == before_reencode);

	const avm::RelationEntity same_so_other_relation{relation2, subject, object};
	const avm::LinkId encoded2 = avm::encode_relation_entity(store, same_so_other_relation);
	assert(encoded2 != encoded);
	assert(store.find(subject, object) == subject_object);
	assert(avm::decode_relation_entity(store, encoded2) == same_so_other_relation);

	const avm::RelationEntity self{subject, subject, subject};
	const avm::LinkId self_encoded = avm::encode_relation_entity(store, self);
	assert(self_encoded == subject);
	assert(avm::decode_relation_entity(store, self_encoded) == self);

	const avm::RelationEntity nested{encoded, encoded2, self_encoded};
	const avm::LinkId nested_encoded = avm::encode_relation_entity(store, nested);
	assert(avm::decode_relation_entity(store, nested_encoded) == nested);

	return 0;
}
