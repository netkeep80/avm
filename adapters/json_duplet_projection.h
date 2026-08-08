#pragma once

#include "avm/projection.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace avm::json_duplet
{

class ProjectionError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

namespace detail
{

inline std::string projection_object_path(const std::string &path, const std::string &key)
{
	return path + "." + key;
}

template <typename Json> LinkId decode_link_anchor(const Json &value, const std::string &path)
{
	if (!value.is_object() || !value.contains("$link") || value.size() != 1)
		throw ProjectionError(path + ": unsupported duplet JSON leaf; expected exact {$link: N} anchor");

	const Json &encoded_id = value.at("$link");
	std::uint64_t id = 0;
	if (encoded_id.is_number_unsigned())
	{
		id = encoded_id.template get<std::uint64_t>();
	}
	else if (encoded_id.is_number_integer())
	{
		const std::int64_t signed_id = encoded_id.template get<std::int64_t>();
		if (signed_id <= 0)
			throw ProjectionError(path + ".$link: LinkId must be a positive integer");
		id = static_cast<std::uint64_t>(signed_id);
	}
	else
	{
		throw ProjectionError(path + ".$link: LinkId must be an integer");
	}

	if (id == invalid_link_id)
		throw ProjectionError(path + ".$link: invalid LinkId 0 is not allowed");
	return static_cast<LinkId>(id);
}

template <typename Json> class ProjectionBuilder
{
public:
	ProjectionDescription build_term(const Json &term)
	{
		ProjectionDescription description;
		description_ = &description;
		description.root = project_term(term, "$ ");
		description_ = nullptr;
		return description;
	}

private:
	ProjectionRef project_term(const Json &term, const std::string &path)
	{
		if (!term.is_object())
			throw ProjectionError(path + ": unsupported duplet JSON leaf; expected pair or {$link: N}");

		const bool has_begin = term.contains("<<");
		const bool has_end = term.contains(">>");
		if (has_begin || has_end)
		{
			if (!has_begin || !has_end)
				throw ProjectionError(path + ": malformed duplet: fields << and >> must appear together");
			if (term.size() != 2)
				throw ProjectionError(path + ": malformed duplet: foreign members are not allowed");

			const ProjectionRef begin = project_term(term.at("<<"), projection_object_path(path, "<<"));
			const ProjectionRef end = project_term(term.at(">>"), projection_object_path(path, ">>"));
			const ProjectionNodeId node_id = description_->nodes.size();
			description_->nodes.push_back(ProjectionNode{begin, end});
			return ProjectionRef::node(node_id);
		}

		return ProjectionRef::anchor(decode_link_anchor(term, path));
	}

	ProjectionDescription *description_ = nullptr;
};

} // namespace detail

template <typename Json> ProjectionDescription project_duplet_term(const Json &term)
{
	detail::ProjectionBuilder<Json> builder;
	return builder.build_term(term);
}

template <typename Json> ProjectionDescription project_duplet_document(const Json &document)
{
	if (!document.is_object())
		throw ProjectionError("$: duplet-json/1 document must be an object");
	if (!document.contains("$avm") || !document.contains("$root") || document.size() != 2)
		throw ProjectionError("$: duplet-json/1 document must contain exactly $avm and $root");

	const Json &version = document.at("$avm");
	if (!version.is_string() || version.template get<std::string>() != "duplet-json/1")
		throw ProjectionError("$.$avm: expected version marker duplet-json/1");

	return project_duplet_term(document.at("$root"));
}

} // namespace avm::json_duplet
