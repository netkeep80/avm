#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

inline constexpr const char *bootstrap_unit_symbol = "bootstrap_unit";
inline constexpr const char *bootstrap_nil_symbol = "bootstrap_nil";
inline constexpr const char *bootstrap_true_symbol = "bootstrap_true";
inline constexpr const char *bootstrap_false_symbol = "bootstrap_false";
inline constexpr const char *bootstrap_quote_symbol = "bootstrap_quote";
inline constexpr const char *bootstrap_sequence_symbol = "bootstrap_sequence";
inline constexpr const char *bootstrap_if_symbol = "bootstrap_if";
inline constexpr const char *semantic_commit_relation_state_symbol = "semantic_commit_relation_state";
inline constexpr const char *semantic_resolve_reference_symbol = "semantic_resolve_reference";
inline constexpr const char *semantic_apply_pure_relation_symbol = "semantic_apply_pure_relation";
inline constexpr const char *current_relation_state_reference_symbol = "current_relation_state_reference";
inline constexpr const char *current_object_reference_symbol = "current_object_reference";
inline constexpr const char *foreach_object_symbol = "foreach_object";

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

template <typename Json> Json symbol(const char *name)
{
	return tagged<Json>("$symbol", name);
}

template <typename Json> Json integer(std::int64_t value)
{
	return tagged<Json>("$integer", value);
}

template <typename Json> Json duplet(Json begin, Json end)
{
	Json result = Json::object();
	result["<<"] = std::move(begin);
	result[">>"] = std::move(end);
	return result;
}

template <typename Json> Json relation(Json relation_term, Json subject_term, Json object_term)
{
	return duplet(std::move(relation_term), duplet(std::move(subject_term), std::move(object_term)));
}

template <typename Json> Json list(std::vector<Json> items)
{
	Json tail = symbol<Json>(bootstrap_nil_symbol);
	for (auto it = items.rbegin(); it != items.rend(); ++it)
		tail = duplet(std::move(*it), std::move(tail));
	return tail;
}

template <typename Json> Json quote(Json value)
{
	return relation(symbol<Json>(bootstrap_quote_symbol), symbol<Json>(bootstrap_unit_symbol), std::move(value));
}

template <typename Json> Json quote_integer(std::int64_t value)
{
	return quote<Json>(integer<Json>(value));
}

template <typename Json> Json quote_boolean(bool value)
{
	return quote<Json>(symbol<Json>(value ? bootstrap_true_symbol : bootstrap_false_symbol));
}

template <typename Json> Json commit_relation_state(Json value_expression)
{
	return relation(symbol<Json>(semantic_commit_relation_state_symbol), symbol<Json>(bootstrap_unit_symbol),
	                std::move(value_expression));
}

template <typename Json> Json resolve_reference_symbol(const char *reference_symbol)
{
	return relation(symbol<Json>(semantic_resolve_reference_symbol), symbol<Json>(bootstrap_unit_symbol),
	                symbol<Json>(reference_symbol));
}

template <typename Json> Json resolve_current_relation_state()
{
	return resolve_reference_symbol<Json>(current_relation_state_reference_symbol);
}

template <typename Json> Json resolve_current_object()
{
	return resolve_reference_symbol<Json>(current_object_reference_symbol);
}

template <typename Json>
Json apply_pure_relation(const char *target_relation_symbol, Json subject_expression, Json object_expression)
{
	return relation(symbol<Json>(semantic_apply_pure_relation_symbol), symbol<Json>(target_relation_symbol),
	                duplet(std::move(subject_expression), std::move(object_expression)));
}

template <typename Json> Json conditional(Json condition, Json then_branch, Json else_branch)
{
	std::vector<Json> arguments;
	arguments.push_back(std::move(condition));
	arguments.push_back(std::move(then_branch));
	arguments.push_back(std::move(else_branch));
	return relation(symbol<Json>(bootstrap_if_symbol), symbol<Json>(bootstrap_unit_symbol), list<Json>(std::move(arguments)));
}

template <typename Json> std::string relation_name(const Json &relation_value, const std::string &path)
{
	if (!relation_value.is_object())
		throw MigrationError(path + ": relation must be an object");
	if (!relation_value.contains("$rel"))
		throw MigrationError(path + ": relation is missing $rel");
	const Json &encoded_relation = relation_value.at("$rel");
	if (!encoded_relation.is_string())
		throw MigrationError(path + ".$rel: relation must be a string");
	return encoded_relation.template get<std::string>();
}

template <typename Json>
const char *require_arithmetic_relation_symbol(const Json &relation_value, const std::string &path)
{
	if (!relation_value.is_object())
		throw MigrationError(path + ": arithmetic relation must be an object");
	if (!relation_value.contains("$rel") || !relation_value.contains("$sub") || !relation_value.contains("$obj") ||
	    relation_value.size() != 3)
		throw MigrationError(path + ": expected exactly $rel, $sub and $obj");

	const Json &encoded_relation = relation_value.at("$rel");
	if (!encoded_relation.is_string())
		throw MigrationError(path + ".$rel: arithmetic relation must be a string");
	return integer_relation_symbol(encoded_relation.template get<std::string>());
}

template <typename Json> Json migrate_direct_arithmetic_relation(const Json &relation_value)
{
	const char *relation_symbol = require_arithmetic_relation_symbol(relation_value, "$.$rel/result");
	const std::int64_t subject = require_integer_operand(relation_value.at("$sub"), "$.$rel/result.$sub");
	const std::int64_t object = require_integer_operand(relation_value.at("$obj"), "$.$rel/result.$obj");

	return relation(symbol<Json>(relation_symbol), integer<Json>(subject), integer<Json>(object));
}

template <typename Json> bool is_exact_current_role_reference(const Json &value, const char *legacy_role)
{
	return value.is_object() && value.size() == 1 && value.contains("$ref") && value.at("$ref").is_string() &&
	       value.at("$ref").template get<std::string>() == legacy_role;
}

template <typename Json> bool is_current_relation_state_reference(const Json &value)
{
	return is_exact_current_role_reference(value, "$rel");
}

template <typename Json> Json migrate_sequence_operand(const Json &value, const std::string &path)
{
	if (value.is_number_integer() || value.is_number_unsigned())
		return quote_integer<Json>(require_integer_operand(value, path));
	if (is_current_relation_state_reference(value))
		return resolve_current_relation_state<Json>();

	throw MigrationError(path + ": this migration gate supports only Integer or exact {$ref:$rel} operands");
}

template <typename Json> Json migrate_boolean_relation(const Json &relation_value, const std::string &path)
{
	if (!relation_value.is_object() || relation_value.size() != 3 || !relation_value.contains("$rel") ||
	    !relation_value.contains("$sub") || !relation_value.contains("$obj"))
		throw MigrationError(path + ": expected exactly $rel, $sub and $obj for frozen Boolean branch");
	if (!relation_value.at("$rel").is_string() ||
	    relation_value.at("$rel").template get<std::string>() != "if_rel_then_obj_else_sub")
		throw MigrationError(path + ".$rel: unsupported legacy Boolean relation");

	const std::int64_t false_branch = require_integer_operand(relation_value.at("$sub"), path + ".$sub");
	const std::int64_t true_branch = require_integer_operand(relation_value.at("$obj"), path + ".$obj");
	return commit_relation_state<Json>(conditional<Json>(resolve_current_relation_state<Json>(),
	                                                     quote_integer<Json>(true_branch),
	                                                     quote_integer<Json>(false_branch)));
}

template <typename Json> Json migrate_sequence_relation(const Json &relation_value, const std::string &path)
{
	const std::string name = relation_name(relation_value, path);
	if (name == "if_rel_then_obj_else_sub")
		return migrate_boolean_relation<Json>(relation_value, path);

	const char *relation_symbol = require_arithmetic_relation_symbol(relation_value, path);
	Json application =
	    apply_pure_relation<Json>(relation_symbol, migrate_sequence_operand(relation_value.at("$sub"), path + ".$sub"),
	                              migrate_sequence_operand(relation_value.at("$obj"), path + ".$obj"));
	return commit_relation_state<Json>(std::move(application));
}

template <typename Json> Json migrate_sequence_item(const Json &value, const std::string &path)
{
	if (value.is_boolean())
		return commit_relation_state<Json>(quote_boolean<Json>(value.template get<bool>()));
	if (value.is_number_integer() || value.is_number_unsigned())
		return commit_relation_state<Json>(quote_integer<Json>(require_integer_operand(value, path)));
	if (value.is_object())
		return migrate_sequence_relation<Json>(value, path);
	throw MigrationError(path + ": unsupported legacy sequence item");
}

template <typename Json> Json migrate_sequence(const Json &sequence)
{
	if (!sequence.is_array())
		throw MigrationError("$.$rel/result: expected legacy sequence array");
	if (sequence.empty())
		throw MigrationError("$.$rel/result: empty legacy sequence is not supported by this migration gate");

	std::vector<Json> expressions;
	expressions.reserve(sequence.size());
	for (std::size_t index = 0; index < sequence.size(); ++index)
	{
		expressions.push_back(
		    migrate_sequence_item<Json>(sequence.at(index), "$.$rel/result[" + std::to_string(index) + "]"));
	}

	return relation(symbol<Json>(bootstrap_sequence_symbol), symbol<Json>(bootstrap_unit_symbol),
	                list<Json>(std::move(expressions)));
}

template <typename Json> Json migrate_foreach_identity_body(const Json &body, const std::string &path)
{
	if (!body.is_object() || body.size() != 2 || !body.contains("$rel") || !body.contains("$obj"))
		throw MigrationError(path + ": expected exact frozen foreach body with $rel and $obj");
	if (!body.at("$rel").is_string() || body.at("$rel").template get<std::string>() != "=")
		throw MigrationError(path + ".$rel: only frozen '=' foreach body is supported");
	if (!is_exact_current_role_reference(body.at("$obj"), "$obj"))
		throw MigrationError(path + ".$obj: expected exact {$ref:$obj} frozen foreach body");
	return resolve_current_object<Json>();
}

template <typename Json> Json migrate_foreach_collection(const Json &collection, const std::string &path)
{
	if (!collection.is_array())
		throw MigrationError(path + ": foreach collection must be an array");
	if (collection.empty())
		throw MigrationError(path + ": empty foreach collection is not supported by this migration gate");

	std::vector<Json> items;
	items.reserve(collection.size());
	for (std::size_t index = 0; index < collection.size(); ++index)
	{
		const std::int64_t item =
		    require_integer_operand(collection.at(index), path + "[" + std::to_string(index) + "]");
		items.push_back(integer<Json>(item));
	}
	return list<Json>(std::move(items));
}

template <typename Json> Json migrate_foreach_object(const Json &relation_value)
{
	const std::string path = "$.$rel/result";
	if (!relation_value.is_object() || relation_value.size() != 3 || !relation_value.contains("$rel") ||
	    !relation_value.contains("$sub") || !relation_value.contains("$obj"))
		throw MigrationError(path + ": expected exactly $rel, $sub and $obj for frozen foreachobj");
	if (!relation_value.at("$rel").is_string() || relation_value.at("$rel").template get<std::string>() != "foreachobj")
		throw MigrationError(path + ".$rel: expected frozen foreachobj relation");

	Json body = migrate_foreach_identity_body<Json>(relation_value.at("$sub"), path + ".$sub");
	Json collection = migrate_foreach_collection<Json>(relation_value.at("$obj"), path + ".$obj");
	return relation(symbol<Json>(foreach_object_symbol), std::move(body), std::move(collection));
}

template <typename Json> Json migrate_relation(const Json &relation_value)
{
	const std::string name = relation_name(relation_value, "$.$rel/result");
	if (name == "foreachobj")
		return migrate_foreach_object<Json>(relation_value);
	if (name == "foreachsub")
		throw MigrationError(
		    "$.$rel/result.$rel: legacy foreachsub is not supported by the frozen compatibility corpus");
	return migrate_direct_arithmetic_relation<Json>(relation_value);
}

} // namespace detail

template <typename Json> MigrationResult<Json> migrate_program(const Json &legacy)
{
	if (!legacy.is_object())
		throw MigrationError("$: legacy jsonRVM program must be an object");
	if (!legacy.contains("$rel/result") || legacy.size() != 1)
		throw MigrationError("$: migration expects exactly the frozen $rel/result envelope");

	const Json &body = legacy.at("$rel/result");
	Json document = Json::object();
	document["$avm"] = "duplet-json/1";
	document["$root"] = body.is_array() ? detail::migrate_sequence<Json>(body) : detail::migrate_relation<Json>(body);
	return MigrationResult<Json>{std::move(document), "/result"};
}

} // namespace avm::jsonrvm_migration
