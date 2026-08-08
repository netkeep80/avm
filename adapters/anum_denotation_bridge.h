#pragma once

#include "avm/projection.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avm
{

enum class AnumDenotationKind
{
	Structural,
	Raw,
	QuotedRaw,
};

struct AnumDenotationRef
{
	enum class Kind
	{
		Anchor,
		Node,
	};

	Kind kind;
	std::string anchor;
	ProjectionNodeId node_id;

	static AnumDenotationRef anchor_ref(std::string key)
	{
		return AnumDenotationRef{Kind::Anchor, std::move(key), 0};
	}

	static AnumDenotationRef node_ref(ProjectionNodeId id)
	{
		return AnumDenotationRef{Kind::Node, {}, id};
	}

	bool operator==(const AnumDenotationRef &) const = default;
};

struct AnumDenotationNode
{
	ProjectionNodeId id;
	AnumDenotationRef start;
	AnumDenotationRef end;

	bool operator==(const AnumDenotationNode &) const = default;
};

struct AnumStructuralDenotation
{
	std::vector<std::string> anchors;
	std::vector<AnumDenotationNode> nodes;
	AnumDenotationRef root;
};

struct CanonicalAnumDenotation
{
	AnumDenotationKind kind;
	std::optional<AnumStructuralDenotation> structural;
	std::optional<std::string> raw;

	static CanonicalAnumDenotation structural_result(AnumStructuralDenotation value)
	{
		return CanonicalAnumDenotation{AnumDenotationKind::Structural, std::move(value), std::nullopt};
	}

	static CanonicalAnumDenotation raw_result(std::string value)
	{
		return CanonicalAnumDenotation{AnumDenotationKind::Raw, std::nullopt, std::move(value)};
	}

	static CanonicalAnumDenotation quoted_raw_result(std::string value)
	{
		return CanonicalAnumDenotation{AnumDenotationKind::QuotedRaw, std::nullopt, std::move(value)};
	}
};

using AnumAnchorResolver = std::function<std::optional<LinkId>(std::string_view)>;

namespace detail
{

inline void validate_anum_denotation_ref(const AnumDenotationRef &ref, const std::vector<std::string> &anchors,
                                         ProjectionNodeId available_nodes)
{
	if (ref.kind == AnumDenotationRef::Kind::Anchor)
	{
		if (ref.anchor.empty())
			throw std::invalid_argument("Anum denotation anchor reference cannot be empty");
		if (!std::binary_search(anchors.begin(), anchors.end(), ref.anchor))
			throw std::invalid_argument("Anum denotation reference uses an undeclared anchor");
		return;
	}

	if (!ref.anchor.empty())
		throw std::invalid_argument("Anum denotation node reference cannot contain an anchor key");
	if (ref.node_id >= available_nodes)
		throw std::invalid_argument("Anum denotation node reference must target an earlier node");
}

inline void validate_anum_structural_denotation(const AnumStructuralDenotation &value)
{
	if (!std::is_sorted(value.anchors.begin(), value.anchors.end()))
		throw std::invalid_argument("Anum denotation anchors must be sorted");
	if (std::adjacent_find(value.anchors.begin(), value.anchors.end()) != value.anchors.end())
		throw std::invalid_argument("Anum denotation anchors must be unique");
	if (std::any_of(value.anchors.begin(), value.anchors.end(), [](const std::string &anchor) { return anchor.empty(); }))
		throw std::invalid_argument("Anum denotation anchor keys must be non-empty");

	for (ProjectionNodeId index = 0; index < value.nodes.size(); ++index)
	{
		const AnumDenotationNode &node = value.nodes[index];
		if (node.id != index)
			throw std::invalid_argument("Anum denotation node ids must be contiguous and ordered");
		validate_anum_denotation_ref(node.start, value.anchors, index);
		validate_anum_denotation_ref(node.end, value.anchors, index);
	}

	validate_anum_denotation_ref(value.root, value.anchors, value.nodes.size());
}

inline const AnumStructuralDenotation &require_structural_anum_denotation(const CanonicalAnumDenotation &value)
{
	if (value.kind != AnumDenotationKind::Structural)
		throw std::invalid_argument("Anum denotation is not structural");
	if (!value.structural || value.raw)
		throw std::invalid_argument("structural Anum denotation must contain only structural payload");
	validate_anum_structural_denotation(*value.structural);
	return *value.structural;
}

inline void validate_non_structural_anum_denotation(const CanonicalAnumDenotation &value)
{
	if (value.kind == AnumDenotationKind::Structural)
		throw std::invalid_argument("structural Anum denotation requires structural payload");
	if (value.structural || !value.raw)
		throw std::invalid_argument("non-structural Anum denotation must contain only raw payload");
}

inline std::vector<LinkId> resolve_anum_anchors(const AnumStructuralDenotation &value,
                                                const AnumAnchorResolver &resolver)
{
	if (!resolver)
		throw std::invalid_argument("Anum anchor resolver is required for structural denotation");

	std::vector<LinkId> resolved;
	resolved.reserve(value.anchors.size());
	for (const std::string &key : value.anchors)
	{
		const std::optional<LinkId> id = resolver(key);
		if (!id)
			throw std::invalid_argument("Anum denotation anchor could not be resolved");
		if (*id == invalid_link_id)
			throw std::invalid_argument("Anum denotation anchor resolved to invalid LinkId");
		resolved.push_back(*id);
	}
	return resolved;
}

inline ProjectionRef translate_anum_ref(const AnumDenotationRef &ref, const std::vector<std::string> &anchors,
                                        const std::vector<LinkId> &resolved_anchors)
{
	if (ref.kind == AnumDenotationRef::Kind::Node)
		return ProjectionRef::node(ref.node_id);

	const auto it = std::lower_bound(anchors.begin(), anchors.end(), ref.anchor);
	if (it == anchors.end() || *it != ref.anchor)
		throw std::logic_error("validated Anum denotation anchor disappeared during translation");
	const std::size_t index = static_cast<std::size_t>(std::distance(anchors.begin(), it));
	return ProjectionRef::anchor(resolved_anchors[index]);
}

} // namespace detail

inline std::optional<ProjectionDescription> bridge_anum_denotation(const CanonicalAnumDenotation &value,
                                                                   const AnumAnchorResolver &resolver)
{
	if (value.kind != AnumDenotationKind::Structural)
	{
		detail::validate_non_structural_anum_denotation(value);
		return std::nullopt;
	}

	const AnumStructuralDenotation &structural = detail::require_structural_anum_denotation(value);
	const std::vector<LinkId> resolved_anchors = detail::resolve_anum_anchors(structural, resolver);

	ProjectionDescription result;
	result.nodes.reserve(structural.nodes.size());
	for (const AnumDenotationNode &node : structural.nodes)
	{
		result.nodes.push_back(ProjectionNode{
			detail::translate_anum_ref(node.start, structural.anchors, resolved_anchors),
			detail::translate_anum_ref(node.end, structural.anchors, resolved_anchors),
		});
	}
	result.root = detail::translate_anum_ref(structural.root, structural.anchors, resolved_anchors);
	return result;
}

} // namespace avm
