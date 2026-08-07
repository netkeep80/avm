#pragma once

#include "avm/link_store.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

using ProjectionNodeId = std::size_t;

struct ProjectionRef
{
	enum class Kind
	{
		Anchor,
		Node,
	};

	Kind kind;
	LinkId anchor_id;
	ProjectionNodeId node_id;

	static ProjectionRef anchor(LinkId id)
	{
		return ProjectionRef{Kind::Anchor, id, 0};
	}

	static ProjectionRef node(ProjectionNodeId id)
	{
		return ProjectionRef{Kind::Node, invalid_link_id, id};
	}

	bool operator==(const ProjectionRef &) const = default;
};

struct ProjectionNode
{
	ProjectionRef begin;
	ProjectionRef end;

	bool operator==(const ProjectionNode &) const = default;
};

struct ProjectionDescription
{
	std::vector<ProjectionNode> nodes;
	ProjectionRef root;
};

struct ProjectionResult
{
	LinkId root;
	std::vector<LinkId> nodes;
};

namespace detail
{

inline void validate_projection_ref(const ProjectionRef &ref, ProjectionNodeId available_nodes)
{
	if (ref.kind == ProjectionRef::Kind::Anchor)
	{
		if (ref.anchor_id == invalid_link_id)
			throw std::invalid_argument("projection anchor cannot be invalid LinkId");
		return;
	}

	if (ref.node_id >= available_nodes)
		throw std::invalid_argument("projection node reference must target an earlier node");
}

inline void validate_projection_description(const ProjectionDescription &description)
{
	for (ProjectionNodeId index = 0; index < description.nodes.size(); ++index)
	{
		validate_projection_ref(description.nodes[index].begin, index);
		validate_projection_ref(description.nodes[index].end, index);
	}

	validate_projection_ref(description.root, description.nodes.size());
}

inline std::optional<LinkId> resolve_projection_ref(const LinkStore &store, const ProjectionRef &ref,
                                                    const std::vector<LinkId> &resolved_nodes)
{
	if (ref.kind == ProjectionRef::Kind::Anchor)
	{
		if (!store.contains(ref.anchor_id))
			return std::nullopt;
		return ref.anchor_id;
	}

	return resolved_nodes[ref.node_id];
}

inline void require_all_projection_anchors(const LinkStore &store, const ProjectionDescription &description)
{
	const auto require_anchor = [&store](const ProjectionRef &ref)
	{
		if (ref.kind == ProjectionRef::Kind::Anchor && !store.contains(ref.anchor_id))
			throw std::invalid_argument("projection anchor is not present in LinkStore");
	};

	for (const ProjectionNode &node : description.nodes)
	{
		require_anchor(node.begin);
		require_anchor(node.end);
	}
	require_anchor(description.root);
}

} // namespace detail

inline std::optional<ProjectionResult> find_projection(const LinkStore &store, const ProjectionDescription &description)
{
	detail::validate_projection_description(description);

	std::vector<LinkId> resolved_nodes;
	resolved_nodes.reserve(description.nodes.size());

	for (const ProjectionNode &node : description.nodes)
	{
		const auto begin = detail::resolve_projection_ref(store, node.begin, resolved_nodes);
		const auto end = detail::resolve_projection_ref(store, node.end, resolved_nodes);
		if (!begin || !end)
			return std::nullopt;

		const auto existing = store.find(*begin, *end);
		if (!existing)
			return std::nullopt;
		resolved_nodes.push_back(*existing);
	}

	const auto root = detail::resolve_projection_ref(store, description.root, resolved_nodes);
	if (!root)
		return std::nullopt;

	return ProjectionResult{*root, std::move(resolved_nodes)};
}

inline ProjectionResult realize_projection(LinkStore &store, const ProjectionDescription &description)
{
	detail::validate_projection_description(description);
	detail::require_all_projection_anchors(store, description);

	std::vector<LinkId> resolved_nodes;
	resolved_nodes.reserve(description.nodes.size());

	for (const ProjectionNode &node : description.nodes)
	{
		const LinkId begin = *detail::resolve_projection_ref(store, node.begin, resolved_nodes);
		const LinkId end = *detail::resolve_projection_ref(store, node.end, resolved_nodes);
		resolved_nodes.push_back(store.intern(begin, end));
	}

	const LinkId root = *detail::resolve_projection_ref(store, description.root, resolved_nodes);
	return ProjectionResult{root, std::move(resolved_nodes)};
}

} // namespace avm
