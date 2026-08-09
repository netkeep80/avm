#pragma once

#include "avm/relations_model.h"
#include "avm/semantic_context.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>

namespace avm
{

struct ReferenceVocabulary
{
	LinkId current_context;
	LinkId parent_context;
	LinkId entity_role;
	LinkId relation_state_role;
	LinkId subject_role;
	LinkId object_role;
	LinkId named_reference;
	LinkId begin_reference;
	LinkId end_reference;
	LinkId relation_part_reference;
	LinkId subject_part_reference;
	LinkId object_part_reference;

	static ReferenceVocabulary create(LinkStore &store)
	{
		ReferenceVocabulary vocabulary{};
		vocabulary.current_context = store.create_point();
		vocabulary.parent_context = store.create_point();
		vocabulary.entity_role = store.create_point();
		vocabulary.relation_state_role = store.create_point();
		vocabulary.subject_role = store.create_point();
		vocabulary.object_role = store.create_point();
		vocabulary.named_reference = store.create_point();
		vocabulary.begin_reference = store.create_point();
		vocabulary.end_reference = store.create_point();
		vocabulary.relation_part_reference = store.create_point();
		vocabulary.subject_part_reference = store.create_point();
		vocabulary.object_part_reference = store.create_point();
		return vocabulary;
	}
};

inline void validate_reference_vocabulary(const LinkStore &store, const ReferenceVocabulary &vocabulary)
{
	std::set<LinkId> unique;
	const auto require_identity = [&store, &unique](LinkId id)
	{
		if (!store.contains(id))
			throw std::invalid_argument("reference vocabulary contains an unknown LinkId");
		if (!unique.insert(id).second)
			throw std::invalid_argument("reference vocabulary identities must be distinct");
	};

	require_identity(vocabulary.current_context);
	require_identity(vocabulary.parent_context);
	require_identity(vocabulary.entity_role);
	require_identity(vocabulary.relation_state_role);
	require_identity(vocabulary.subject_role);
	require_identity(vocabulary.object_role);
	require_identity(vocabulary.named_reference);
	require_identity(vocabulary.begin_reference);
	require_identity(vocabulary.end_reference);
	require_identity(vocabulary.relation_part_reference);
	require_identity(vocabulary.subject_part_reference);
	require_identity(vocabulary.object_part_reference);
}

enum class ReferenceRole
{
	Entity,
	RelationState,
	Subject,
	Object,
};

enum class ReferenceProjection
{
	Begin,
	End,
	RelationPart,
	SubjectPart,
	ObjectPart,
};

namespace reference_detail
{

inline LinkId role_marker(const ReferenceVocabulary &vocabulary, ReferenceRole role)
{
	switch (role)
	{
	case ReferenceRole::Entity:
		return vocabulary.entity_role;
	case ReferenceRole::RelationState:
		return vocabulary.relation_state_role;
	case ReferenceRole::Subject:
		return vocabulary.subject_role;
	case ReferenceRole::Object:
		return vocabulary.object_role;
	}
	throw std::logic_error("unknown reference role");
}

inline SemanticContextRole semantic_role(ReferenceRole role)
{
	switch (role)
	{
	case ReferenceRole::Entity:
		return SemanticContextRole::Entity;
	case ReferenceRole::RelationState:
		return SemanticContextRole::RelationState;
	case ReferenceRole::Subject:
		return SemanticContextRole::Subject;
	case ReferenceRole::Object:
		return SemanticContextRole::Object;
	}
	throw std::logic_error("unknown reference role");
}

inline std::optional<ReferenceRole> marker_role(const ReferenceVocabulary &vocabulary, LinkId marker)
{
	if (marker == vocabulary.entity_role)
		return ReferenceRole::Entity;
	if (marker == vocabulary.relation_state_role)
		return ReferenceRole::RelationState;
	if (marker == vocabulary.subject_role)
		return ReferenceRole::Subject;
	if (marker == vocabulary.object_role)
		return ReferenceRole::Object;
	return std::nullopt;
}

inline LinkId projection_marker(const ReferenceVocabulary &vocabulary, ReferenceProjection projection)
{
	switch (projection)
	{
	case ReferenceProjection::Begin:
		return vocabulary.begin_reference;
	case ReferenceProjection::End:
		return vocabulary.end_reference;
	case ReferenceProjection::RelationPart:
		return vocabulary.relation_part_reference;
	case ReferenceProjection::SubjectPart:
		return vocabulary.subject_part_reference;
	case ReferenceProjection::ObjectPart:
		return vocabulary.object_part_reference;
	}
	throw std::logic_error("unknown reference projection");
}

inline std::size_t context_selector_depth(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                          LinkId selector, std::size_t max_depth)
{
	std::size_t depth = 0;
	LinkId current = selector;
	std::set<LinkId> visited;
	while (current != vocabulary.current_context)
	{
		if (depth >= max_depth)
			throw std::runtime_error("context selector exceeds configured depth limit");
		if (!store.contains(current))
			throw std::runtime_error("context selector references an unknown LinkId");
		if (!visited.insert(current).second)
			throw std::runtime_error("cycle detected in context selector");

		const Link step = store.get(current);
		if (step.begin != vocabulary.parent_context)
			throw std::runtime_error("context selector is not Current or Parent(selector)");
		current = step.end;
		++depth;
	}
	return depth;
}

} // namespace reference_detail

inline std::optional<LinkId> find_context_selector(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                                   std::size_t parent_levels)
{
	validate_reference_vocabulary(store, vocabulary);
	LinkId selector = vocabulary.current_context;
	for (std::size_t level = 0; level < parent_levels; ++level)
	{
		const auto parent = store.find(vocabulary.parent_context, selector);
		if (!parent)
			return std::nullopt;
		selector = *parent;
	}
	return selector;
}

inline LinkId realize_context_selector(LinkStore &store, const ReferenceVocabulary &vocabulary,
                                       std::size_t parent_levels)
{
	validate_reference_vocabulary(store, vocabulary);
	LinkId selector = vocabulary.current_context;
	for (std::size_t level = 0; level < parent_levels; ++level)
		selector = store.intern(vocabulary.parent_context, selector);
	return selector;
}

inline std::optional<LinkId> find_context_role_reference(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                                         ReferenceRole role, std::size_t parent_levels = 0)
{
	const auto selector = find_context_selector(store, vocabulary, parent_levels);
	if (!selector)
		return std::nullopt;
	return store.find(reference_detail::role_marker(vocabulary, role), *selector);
}

inline LinkId realize_context_role_reference(LinkStore &store, const ReferenceVocabulary &vocabulary,
                                             ReferenceRole role, std::size_t parent_levels = 0)
{
	const LinkId selector = realize_context_selector(store, vocabulary, parent_levels);
	return store.intern(reference_detail::role_marker(vocabulary, role), selector);
}

inline std::optional<LinkId> find_named_reference(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                                  LinkId target)
{
	validate_reference_vocabulary(store, vocabulary);
	if (!store.contains(target))
		return std::nullopt;
	return store.find(vocabulary.named_reference, target);
}

inline LinkId realize_named_reference(LinkStore &store, const ReferenceVocabulary &vocabulary, LinkId target)
{
	validate_reference_vocabulary(store, vocabulary);
	if (!store.contains(target))
		throw std::invalid_argument("named reference target is not present in LinkStore");
	return store.intern(vocabulary.named_reference, target);
}

inline std::optional<LinkId> find_reference_projection(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                                       ReferenceProjection projection, LinkId inner_reference)
{
	validate_reference_vocabulary(store, vocabulary);
	if (!store.contains(inner_reference))
		return std::nullopt;
	return store.find(reference_detail::projection_marker(vocabulary, projection), inner_reference);
}

inline LinkId realize_reference_projection(LinkStore &store, const ReferenceVocabulary &vocabulary,
                                           ReferenceProjection projection, LinkId inner_reference)
{
	validate_reference_vocabulary(store, vocabulary);
	if (!store.contains(inner_reference))
		throw std::invalid_argument("inner reference is not present in LinkStore");
	return store.intern(reference_detail::projection_marker(vocabulary, projection), inner_reference);
}

inline std::optional<LinkId> resolve_reference(const LinkStore &store, const ReferenceVocabulary &vocabulary,
                                               LinkId reference, const SemanticContextView &context,
                                               std::size_t max_depth = std::numeric_limits<std::size_t>::max())
{
	validate_reference_vocabulary(store, vocabulary);
	if (!store.contains(reference))
		return std::nullopt;

	std::size_t remaining_depth = max_depth;
	const auto resolve = [&](auto &&self, LinkId current) -> std::optional<LinkId>
	{
		if (remaining_depth == 0)
			throw std::runtime_error("reference expression exceeds configured depth limit");
		--remaining_depth;

		if (!store.contains(current))
			return std::nullopt;
		const Link expression = store.get(current);

		if (const auto role = reference_detail::marker_role(vocabulary, expression.begin))
		{
			const std::size_t parent_levels =
			    reference_detail::context_selector_depth(store, vocabulary, expression.end, max_depth);
			const LinkId value = context.ancestor(parent_levels).role(reference_detail::semantic_role(*role));
			if (!store.contains(value))
				return std::nullopt;
			return value;
		}

		if (expression.begin == vocabulary.named_reference)
		{
			if (!store.contains(expression.end))
				return std::nullopt;
			return expression.end;
		}

		const auto inner = self(self, expression.end);
		if (!inner)
			return std::nullopt;

		if (expression.begin == vocabulary.begin_reference)
			return store.get(*inner).begin;
		if (expression.begin == vocabulary.end_reference)
			return store.get(*inner).end;

		const bool relation_part = expression.begin == vocabulary.relation_part_reference;
		const bool subject_part = expression.begin == vocabulary.subject_part_reference;
		const bool object_part = expression.begin == vocabulary.object_part_reference;
		if (relation_part || subject_part || object_part)
		{
			const RelationEntity entity = decode_relation_entity(store, *inner);
			if (relation_part)
				return entity.relation;
			if (subject_part)
				return entity.subject;
			return entity.object;
		}

		throw std::runtime_error("LinkId is not a canonical reference expression");
	};

	return resolve(resolve, reference);
}

} // namespace avm
