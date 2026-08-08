#pragma once

#include "avm/relations_model.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

namespace avm
{

struct RelationQuery
{
	std::optional<LinkId> relation;
	std::optional<LinkId> subject;
	std::optional<LinkId> object;
};

struct RelationMatch
{
	LinkId entity_id;
	RelationEntity entity;

	bool operator==(const RelationMatch &) const = default;
};

namespace detail
{

inline bool relation_query_matches(const RelationQuery &query, const RelationEntity &entity)
{
	return (!query.relation || entity.relation == *query.relation) &&
	       (!query.subject || entity.subject == *query.subject) && (!query.object || entity.object == *query.object);
}

inline bool relation_query_constraints_exist(const LinkStore &store, const RelationQuery &query)
{
	if (query.relation && !store.contains(*query.relation))
		return false;
	if (query.subject && !store.contains(*query.subject))
		return false;
	if (query.object && !store.contains(*query.object))
		return false;
	return true;
}

inline void append_relation_candidate(const LinkStore &store, const RelationQuery &query, LinkId entity_id,
                                      std::vector<RelationMatch> &matches)
{
	const RelationEntity entity = decode_relation_entity(store, entity_id);
	if (relation_query_matches(query, entity))
		matches.push_back(RelationMatch{entity_id, entity});
}

inline void append_entities_for_pair(const LinkStore &store, const RelationQuery &query, LinkId pair_id,
                                     std::vector<RelationMatch> &matches)
{
	for (const LinkId entity_id : store.incoming(pair_id))
		detail::append_relation_candidate(store, query, entity_id, matches);
}

} // namespace detail

inline std::vector<RelationMatch> query_relation_entities(const LinkStore &store, const RelationQuery &query)
{
	if (!query.relation && !query.subject && !query.object)
		throw std::invalid_argument("Relations Model query requires at least one constraint");

	if (!detail::relation_query_constraints_exist(store, query))
		return {};

	std::vector<RelationMatch> matches;

	if (query.relation)
	{
		for (const LinkId entity_id : store.outgoing(*query.relation))
			detail::append_relation_candidate(store, query, entity_id, matches);
	}
	else if (query.subject && query.object)
	{
		const std::optional<LinkId> pair_id = store.find(*query.subject, *query.object);
		if (pair_id)
			detail::append_entities_for_pair(store, query, *pair_id, matches);
	}
	else if (query.subject)
	{
		for (const LinkId pair_id : store.outgoing(*query.subject))
			detail::append_entities_for_pair(store, query, pair_id, matches);
	}
	else
	{
		for (const LinkId pair_id : store.incoming(*query.object))
			detail::append_entities_for_pair(store, query, pair_id, matches);
	}

	std::sort(matches.begin(), matches.end(),
	          [](const RelationMatch &left, const RelationMatch &right) { return left.entity_id < right.entity_id; });
	matches.erase(std::unique(matches.begin(), matches.end(), [](const RelationMatch &left, const RelationMatch &right)
	                          { return left.entity_id == right.entity_id; }),
	              matches.end());
	return matches;
}

} // namespace avm
