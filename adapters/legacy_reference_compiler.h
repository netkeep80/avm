#pragma once

#include "avm/projection.h"
#include "avm/reference.h"

#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace avm::legacy_reference
{

class CompileError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

using NamedAnchors = std::map<std::string, LinkId>;

namespace detail
{

inline std::vector<std::string_view> split_path(std::string_view source)
{
	if (source.empty())
		throw CompileError("legacy reference is empty");

	std::vector<std::string_view> segments;
	std::size_t begin = 0;
	while (begin <= source.size())
	{
		const std::size_t slash = source.find('/', begin);
		const std::size_t end = slash == std::string_view::npos ? source.size() : slash;
		if (end == begin)
			throw CompileError("legacy reference contains an empty path segment");
		segments.push_back(source.substr(begin, end - begin));
		if (slash == std::string_view::npos)
			break;
		begin = slash + 1;
	}
	return segments;
}

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
	throw std::logic_error("unknown legacy reference role");
}

inline ReferenceRole parse_role(std::string_view role)
{
	if (role == "ent")
		return ReferenceRole::Entity;
	if (role == "rel")
		return ReferenceRole::RelationState;
	if (role == "sub")
		return ReferenceRole::Subject;
	if (role == "obj")
		return ReferenceRole::Object;
	throw CompileError("unknown legacy context role");
}

inline LinkId projection_marker(const ReferenceVocabulary &vocabulary, std::string_view segment)
{
	if (segment == "begin")
		return vocabulary.begin_reference;
	if (segment == "end")
		return vocabulary.end_reference;
	if (segment == "relation")
		return vocabulary.relation_part_reference;
	if (segment == "subject")
		return vocabulary.subject_part_reference;
	if (segment == "object")
		return vocabulary.object_part_reference;
	throw CompileError("unsupported legacy path segment; JSON member traversal is not a canonical AVM reference");
}

class ProjectionBuilder
{
public:
	explicit ProjectionBuilder(const ReferenceVocabulary &vocabulary) : vocabulary_(vocabulary) {}

	ProjectionRef base_reference(std::string_view source, const NamedAnchors &names)
	{
		if (source.front() == '$')
			return context_reference(source);
		return named_reference(source, names);
	}

	ProjectionRef structural_projection(std::string_view segment, ProjectionRef inner)
	{
		return append(projection_marker(vocabulary_, segment), inner);
	}

	ProjectionDescription finish(ProjectionRef root)
	{
		description_.root = root;
		return description_;
	}

private:
	ProjectionRef context_reference(std::string_view source)
	{
		std::size_t dollars = 0;
		while (dollars < source.size() && source[dollars] == '$')
			++dollars;
		if (dollars == 0 || dollars == source.size())
			throw CompileError("malformed legacy context reference");

		const ReferenceRole role = parse_role(source.substr(dollars));
		ProjectionRef selector = ProjectionRef::anchor(vocabulary_.current_context);
		for (std::size_t level = 1; level < dollars; ++level)
			selector = append(vocabulary_.parent_context, selector);
		return append(role_marker(vocabulary_, role), selector);
	}

	ProjectionRef named_reference(std::string_view name, const NamedAnchors &names)
	{
		const auto target = names.find(std::string(name));
		if (target == names.end())
			throw CompileError("unknown legacy absolute reference name");
		if (target->second == invalid_link_id)
			throw CompileError("legacy absolute reference resolves to invalid LinkId 0");
		return append(vocabulary_.named_reference, ProjectionRef::anchor(target->second));
	}

	ProjectionRef append(LinkId marker, ProjectionRef end)
	{
		const ProjectionNodeId node_id = description_.nodes.size();
		description_.nodes.push_back(ProjectionNode{ProjectionRef::anchor(marker), end});
		return ProjectionRef::node(node_id);
	}

	const ReferenceVocabulary &vocabulary_;
	ProjectionDescription description_{};
};

} // namespace detail

inline ProjectionDescription compile(std::string_view source, const ReferenceVocabulary &vocabulary,
                                     const NamedAnchors &names = {})
{
	const std::vector<std::string_view> segments = detail::split_path(source);
	detail::ProjectionBuilder builder(vocabulary);

	ProjectionRef reference = builder.base_reference(segments.front(), names);
	for (std::size_t index = 1; index < segments.size(); ++index)
		reference = builder.structural_projection(segments[index], reference);
	return builder.finish(reference);
}

} // namespace avm::legacy_reference
