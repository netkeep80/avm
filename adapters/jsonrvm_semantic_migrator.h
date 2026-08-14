#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace avm::jsonrvm_migration
{

class MigrationError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

template <typename Json> struct MigrationResult
{
	Json document;
	std::string observable_json_pointer;
};

namespace detail
{

inline const char *integer_relation_symbol(const std::string &legacy_operator)
{
	if (legacy_operator == "+")
		return "integer_add";
	if (legacy_operator == "-")
		return "integer_subtract";
	if (legacy_operator == "*")
		return "integer_multiply";
	if (legacy_operator == "/")
		return "integer_divide";
	throw MigrationError("unsupported legacy arithmetic relation: " + legacy_operator);
}

template <typename Json> std::int64_t require_integer_operand(const Json &value, const std::string &path)
{
	if (value.is_number_unsigned())
	{
		const std::uint64_t raw = value.template get<std::uint64_t>();
		const auto max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
		if (raw > max)
			throw MigrationError(path + ": integer operand exceeds int64 migration domain");
		return static_cast<std::int64_t>(raw);
	}

	if (value.is_number_integer())
		return value.template get<std::int64_t>();

	throw MigrationError(path + ": expected an integer operand");
}

template <typename Json> Json tagged(const char *marker, Json value)
{
	Json result = Json::object();
	result[marker] = std::move(value);
	return result;
}

template <typename Json> Json duplet(Json begin, Json end)
{
	Json result = Json::object();
	result["<<"] = std::move(begin);
	result[">>"] = std::move(end);
	return result;
}

template <typename Json> Json migrate_arithmetic_relation(const Json &relation)
{
	if (!relation.is_object())
		throw MigrationError("$.$rel/result: arithmetic relation must be an object");
	if (!relation.contains("$rel") || !relation.contains("$sub") || !relation.contains("$obj") || relation.size() != 3)
		throw MigrationError("$.$rel/result: expected exactly $rel, $sub and $obj");

	const Json &encoded_relation = relation.at("$rel");
	if (!encoded_relation.is_string())
		throw MigrationError("$.$rel/result.$rel: arithmetic relation must be a string");
	const std::string legacy_operator = encoded_relation.template get<std::string>();
	const char *symbol = integer_relation_symbol(legacy_operator);

	const std::int64_t subject = require_integer_operand(relation.at("$sub"), "$.$rel/result.$sub");
	const std::int64_t object = require_integer_operand(relation.at("$obj"), "$.$rel/result.$obj");

	return duplet(tagged<Json>("$symbol", symbol),
	              duplet(tagged<Json>("$integer", subject), tagged<Json>("$integer", object)));
}

} // namespace detail

template <typename Json> MigrationResult<Json> migrate_program(const Json &legacy)
{
	if (!legacy.is_object())
		throw MigrationError("$: legacy jsonRVM program must be an object");
	if (!legacy.contains("$rel/result") || legacy.size() != 1)
		throw MigrationError("$: first migration gate expects exactly the frozen $rel/result envelope");

	Json document = Json::object();
	document["$avm"] = "duplet-json/1";
	document["$root"] = detail::migrate_arithmetic_relation(legacy.at("$rel/result"));
	return MigrationResult<Json>{std::move(document), "/result"};
}

} // namespace avm::jsonrvm_migration
