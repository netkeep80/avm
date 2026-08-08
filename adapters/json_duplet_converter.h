#pragma once

#include "nlohmann/json.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace avm::json_duplet
{

using Json = nlohmann::ordered_json;

class ConversionError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

namespace detail
{

inline std::string object_path(const std::string &path, const std::string &key)
{
	return path + "." + key;
}

inline std::string array_path(const std::string &path, std::size_t index)
{
	return path + "[" + std::to_string(index) + "]";
}

inline Json triplet_to_duplet(const Json &value, const std::string &path)
{
	if (value.is_array())
	{
		Json result = Json::array();
		for (std::size_t index = 0; index < value.size(); ++index)
			result.push_back(triplet_to_duplet(value[index], array_path(path, index)));
		return result;
	}

	if (!value.is_object())
		return value;

	const bool has_relation = value.contains("$rel");
	const bool has_subject = value.contains("$sub");
	const bool has_object = value.contains("$obj");
	const unsigned relation_members = static_cast<unsigned>(has_relation) + static_cast<unsigned>(has_subject) +
	                                  static_cast<unsigned>(has_object);

	if (relation_members != 0)
	{
		if (relation_members != 3)
			throw ConversionError(path + ": incomplete legacy relation form: expected $rel, $sub and $obj");
		if (value.size() != 3)
			throw ConversionError(path + ": mixed legacy relation form: foreign members are not losslessly convertible");

		Json arguments = Json::object();
		arguments["<<"] = triplet_to_duplet(value.at("$sub"), object_path(path, "$sub"));
		arguments[">>"] = triplet_to_duplet(value.at("$obj"), object_path(path, "$obj"));

		Json result = Json::object();
		result["<<"] = triplet_to_duplet(value.at("$rel"), object_path(path, "$rel"));
		result[">>"] = std::move(arguments);
		return result;
	}

	Json result = Json::object();
	for (auto member = value.begin(); member != value.end(); ++member)
		result[member.key()] = triplet_to_duplet(member.value(), object_path(path, member.key()));
	return result;
}

inline Json duplet_to_triplet(const Json &value, const std::string &path)
{
	if (value.is_array())
	{
		Json result = Json::array();
		for (std::size_t index = 0; index < value.size(); ++index)
			result.push_back(duplet_to_triplet(value[index], array_path(path, index)));
		return result;
	}

	if (!value.is_object())
		return value;

	const bool has_begin = value.contains("<<");
	const bool has_end = value.contains(">>");
	if (has_begin || has_end)
	{
		if (!has_begin || !has_end)
			throw ConversionError(path + ": malformed duplet: fields << and >> must appear together");
		if (value.size() != 2)
			throw ConversionError(path + ": malformed duplet: foreign members are not allowed");

		const Json &arguments = value.at(">>");
		if (!arguments.is_object())
			throw ConversionError(path + ": standalone duplet has no explicit-triplet representation");

		const bool arguments_have_begin = arguments.contains("<<");
		const bool arguments_have_end = arguments.contains(">>");
		if (!arguments_have_begin || !arguments_have_end || arguments.size() != 2)
			throw ConversionError(object_path(path, ">>") +
			                      ": relation arguments must be an exact duplet with << and >>");

		Json result = Json::object();
		result["$rel"] = duplet_to_triplet(value.at("<<"), object_path(path, "<<"));
		result["$sub"] = duplet_to_triplet(arguments.at("<<"), object_path(object_path(path, ">>"), "<<"));
		result["$obj"] = duplet_to_triplet(arguments.at(">>"), object_path(object_path(path, ">>"), ">>"));
		return result;
	}

	Json result = Json::object();
	for (auto member = value.begin(); member != value.end(); ++member)
		result[member.key()] = duplet_to_triplet(member.value(), object_path(path, member.key()));
	return result;
}

} // namespace detail

inline Json convert_explicit_triplets_to_duplets(const Json &value)
{
	return detail::triplet_to_duplet(value, "$ ");
}

inline Json convert_relation_duplets_to_explicit_triplets(const Json &value)
{
	return detail::duplet_to_triplet(value, "$ ");
}

} // namespace avm::json_duplet
