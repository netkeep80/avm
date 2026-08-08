#pragma once

#include "json_duplet_projection.h"

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace avm::json_duplet
{
namespace detail
{

template <typename Json> Json parse_unique_json(std::string_view text)
{
	std::vector<std::unordered_set<std::string>> keys_by_depth;
	typename Json::parser_callback_t reject_duplicate_keys =
	    [&keys_by_depth](int depth, typename Json::parse_event_t event, Json &parsed)
	{
		if (event == Json::parse_event_t::object_start)
		{
			const std::size_t key_depth = static_cast<std::size_t>(depth) + 1;
			if (keys_by_depth.size() <= key_depth)
				keys_by_depth.resize(key_depth + 1);
			keys_by_depth[key_depth].clear();
			return true;
		}

		if (event == Json::parse_event_t::key)
		{
			const std::size_t key_depth = static_cast<std::size_t>(depth);
			if (keys_by_depth.size() <= key_depth)
				keys_by_depth.resize(key_depth + 1);
			const std::string key = parsed.template get<std::string>();
			if (!keys_by_depth[key_depth].insert(key).second)
				throw ProjectionError("duplicate JSON object key: " + key);
		}

		return true;
	};

	try
	{
		return Json::parse(text.begin(), text.end(), reject_duplicate_keys);
	}
	catch (const ProjectionError &)
	{
		throw;
	}
	catch (const std::exception &error)
	{
		throw ProjectionError(std::string("invalid JSON: ") + error.what());
	}
}

} // namespace detail

template <typename Json> ProjectionDescription project_duplet_term_text(std::string_view text)
{
	return project_duplet_term(detail::parse_unique_json<Json>(text));
}

template <typename Json> ProjectionDescription project_duplet_document_text(std::string_view text)
{
	return project_duplet_document(detail::parse_unique_json<Json>(text));
}

} // namespace avm::json_duplet
