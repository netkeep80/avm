#pragma once

#include "avm/executor.h"
#include "avm/relations_model.h"

#include <initializer_list>
#include <optional>
#include <set>
#include <stdexcept>

namespace avm
{

struct DirectTriuneVocabulary
{
	LinkId subject_value_relation;
	LinkId pair_find_relation;
	LinkId pair_realize_relation;
	LinkId pair_target_begin_relation;
	LinkId pair_target_end_relation;

	static DirectTriuneVocabulary create(LinkStore &store)
	{
		return DirectTriuneVocabulary{
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(),
		};
	}

	void validate(const LinkStore &store) const
	{
		std::set<LinkId> unique;
		for (const LinkId id : {subject_value_relation, pair_find_relation, pair_realize_relation,
		                        pair_target_begin_relation, pair_target_end_relation})
		{
			if (!store.contains(id))
				throw std::invalid_argument("direct triune vocabulary contains an unknown LinkId");
			if (!unique.insert(id).second)
				throw std::invalid_argument("direct triune vocabulary identities must be distinct");
		}
	}
};

struct PairTarget
{
	LinkId descriptor;
	LinkId begin;
	LinkId end;

	bool operator==(const PairTarget &) const = default;
};

namespace detail
{

inline std::optional<LinkId> find_direct_role_value(const LinkStore &store, LinkId relation, LinkId subject)
{
	std::optional<LinkId> value;
	for (const LinkId candidate : store.outgoing(relation))
	{
		const RelationEntity decoded = decode_relation_entity(store, candidate);
		if (decoded.relation != relation || decoded.subject != subject)
			continue;

		if (value && *value != decoded.object)
			throw std::logic_error("direct triune role is not functional for the requested subject");
		value = decoded.object;
	}
	return value;
}

} // namespace detail

inline LinkId materialize_pair_target(LinkStore &store, const DirectTriuneVocabulary &vocabulary, LinkId begin,
                                      LinkId end)
{
	vocabulary.validate(store);
	if (!store.contains(begin))
		throw std::invalid_argument("pair target begin is not present in LinkStore");
	if (!store.contains(end))
		throw std::invalid_argument("pair target end is not present in LinkStore");

	const LinkId descriptor = store.create_point();
	encode_relation_entity(store, RelationEntity{vocabulary.pair_target_begin_relation, descriptor, begin});
	encode_relation_entity(store, RelationEntity{vocabulary.pair_target_end_relation, descriptor, end});
	return descriptor;
}

inline PairTarget decode_pair_target(const LinkStore &store, const DirectTriuneVocabulary &vocabulary,
                                     LinkId descriptor)
{
	vocabulary.validate(store);
	if (!store.contains(descriptor))
		throw std::invalid_argument("pair target descriptor is not present in LinkStore");

	const Link descriptor_link = store.get(descriptor);
	if (descriptor_link != Link{descriptor, descriptor})
		throw std::invalid_argument("pair target descriptor must be a point identity");

	const auto begin = detail::find_direct_role_value(store, vocabulary.pair_target_begin_relation, descriptor);
	const auto end = detail::find_direct_role_value(store, vocabulary.pair_target_end_relation, descriptor);
	if (!begin || !end)
		throw std::runtime_error("pair target descriptor is incomplete");

	return PairTarget{descriptor, *begin, *end};
}

inline void register_direct_triune_primitives(Executor &executor, const DirectTriuneVocabulary &vocabulary)
{
	vocabulary.validate(executor.store());

	executor.register_native(vocabulary.subject_value_relation,
	                         [](const ExecutionContext &context, Executor &) { return context.subject; });

	executor.register_native(vocabulary.pair_find_relation,
	                         [vocabulary](const ExecutionContext &context, Executor &current_executor)
	                         {
		                         const PairTarget target =
		                             decode_pair_target(current_executor.store(), vocabulary, context.object);
		                         const auto existing = current_executor.store().find(target.begin, target.end);
		                         if (!existing)
			                         throw std::runtime_error("pair target is not materialized");
		                         return *existing;
	                         });

	executor.register_native(vocabulary.pair_realize_relation,
	                         [vocabulary](const ExecutionContext &context, Executor &current_executor)
	                         {
		                         const PairTarget target =
		                             decode_pair_target(current_executor.store(), vocabulary, context.object);
		                         return current_executor.store().intern(target.begin, target.end);
	                         });
}

} // namespace avm
