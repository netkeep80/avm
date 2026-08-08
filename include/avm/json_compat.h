#pragma once

#include "avm/bootstrap_runtime.h"
#include "nlohmann/json.hpp"

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace avm
{

class JsonProjectionError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

class JsonProgramImporter
{
public:
	using Json = nlohmann::json;

	JsonProgramImporter(LinkStore &store, const BootstrapVocabulary &vocabulary, LinkId sequence_relation)
	    : store_(store), vocabulary_(vocabulary), sequence_relation_(sequence_relation), builder_(store, vocabulary_)
	{
		if (!store_.contains(sequence_relation_))
			throw std::invalid_argument("JSON compatibility sequence relation is not present in LinkStore");
	}

	LinkId import_program(const Json &expression) { return import_expression(expression); }

	Json project_value(LinkId value) const
	{
		if (value == vocabulary_.nil)
			return nullptr;
		if (value == vocabulary_.true_value)
			return true;
		if (value == vocabulary_.false_value)
			return false;

		const auto literal = literals_by_id_.find(value);
		if (literal == literals_by_id_.end())
			throw JsonProjectionError("result LinkId has no JSON projection");
		return literal->second;
	}

private:
	struct FunctionSymbol
	{
		LinkId handle;
		bool syntactically_defined;
	};

	LinkId import_expression(const Json &expression)
	{
		switch (expression.type())
		{
		case Json::value_t::null:
			return builder_.literal(vocabulary_.nil);
		case Json::value_t::boolean:
			return builder_.literal(expression.get<bool>() ? vocabulary_.true_value : vocabulary_.false_value);
		case Json::value_t::string:
			return import_string(expression.get_ref<const std::string &>());
		case Json::value_t::number_integer:
		case Json::value_t::number_unsigned:
		case Json::value_t::number_float:
			return import_opaque_literal(expression);
		case Json::value_t::array:
			return import_sequence(expression);
		case Json::value_t::object:
			return import_operator(expression);
		default:
			throw JsonProjectionError("unsupported JSON value in AVM program projection");
		}
	}

	LinkId import_string(const std::string &value)
	{
		if (const auto formal = find_formal(value))
			return builder_.parameter(*formal);
		return import_opaque_literal(Json(value));
	}

	LinkId import_opaque_literal(const Json &value)
	{
		const std::string key = std::to_string(static_cast<int>(value.type())) + ":" + value.dump();
		const auto existing = literal_ids_.find(key);
		LinkId value_id;
		if (existing != literal_ids_.end())
		{
			value_id = existing->second;
		}
		else
		{
			value_id = store_.create_point();
			literal_ids_.emplace(key, value_id);
			literals_by_id_.emplace(value_id, value);
		}
		return builder_.literal(value_id);
	}

	LinkId import_sequence(const Json &sequence)
	{
		std::vector<LinkId> expressions;
		expressions.reserve(sequence.size());
		for (const Json &item : sequence)
			expressions.push_back(import_expression(item));

		const LinkId payload = encode_link_list(store_, vocabulary_.nil, expressions);
		return encode_relation_entity(store_, RelationEntity{sequence_relation_, vocabulary_.unit, payload});
	}

	LinkId import_operator(const Json &expression)
	{
		if (expression.size() != 1)
			throw JsonProjectionError("operator expression must contain exactly one member");

		const auto member = expression.begin();
		const std::string &name = member.key();
		const Json &arguments = member.value();
		if (!arguments.is_array() || arguments.empty())
			throw JsonProjectionError("operator arguments must be a non-empty JSON array");

		if (name == "Not")
		{
			require_arity(arguments, 1, "Not");
			return builder_.logical_not(import_expression(arguments[0]));
		}
		if (name == "And")
		{
			require_arity(arguments, 2, "And");
			return builder_.logical_and(import_expression(arguments[0]), import_expression(arguments[1]));
		}
		if (name == "Or")
		{
			require_arity(arguments, 2, "Or");
			return builder_.logical_or(import_expression(arguments[0]), import_expression(arguments[1]));
		}
		if (name == "If")
		{
			require_arity(arguments, 3, "If");
			return builder_.conditional(import_expression(arguments[0]), import_expression(arguments[1]),
			                            import_expression(arguments[2]));
		}
		if (name == "Def")
			return import_definition(arguments);
		if (name == "Call")
			return import_call(arguments);

		throw JsonProjectionError("unknown JSON AVM operator: " + name);
	}

	LinkId import_definition(const Json &arguments)
	{
		require_arity(arguments, 3, "Def");
		if (!arguments[0].is_string())
			throw JsonProjectionError("Def function name must be a string");
		if (!arguments[1].is_array())
			throw JsonProjectionError("Def formal parameters must be an array");

		const std::string function_name = arguments[0].get<std::string>();
		const std::optional<FunctionSymbol> previous = current_function_symbol(function_name);
		const FunctionSymbol symbol = begin_definition(function_name);

		std::vector<LinkId> parameters;
		std::map<std::string, LinkId> scope;
		parameters.reserve(arguments[1].size());
		for (const Json &parameter : arguments[1])
		{
			if (!parameter.is_string())
			{
				restore_function_symbol(function_name, previous);
				throw JsonProjectionError("Def formal parameter names must be strings");
			}
			const LinkId formal = store_.create_point();
			parameters.push_back(formal);
			scope[parameter.get<std::string>()] = formal;
		}

		parameter_scopes_.push_back(std::move(scope));
		try
		{
			const LinkId body = import_expression(arguments[2]);
			parameter_scopes_.pop_back();
			return builder_.deferred_function_definition(symbol.handle, parameters, body);
		}
		catch (...)
		{
			parameter_scopes_.pop_back();
			restore_function_symbol(function_name, previous);
			throw;
		}
	}

	LinkId import_call(const Json &arguments)
	{
		if (arguments.empty() || !arguments[0].is_string())
			throw JsonProjectionError("Call requires a string function name");

		const LinkId function = function_for_call(arguments[0].get_ref<const std::string &>());
		std::vector<LinkId> actuals;
		actuals.reserve(arguments.size() - 1);
		for (std::size_t i = 1; i < arguments.size(); ++i)
			actuals.push_back(import_expression(arguments[i]));
		return builder_.call(function, actuals);
	}

	std::optional<LinkId> find_formal(const std::string &name) const
	{
		for (auto scope = parameter_scopes_.rbegin(); scope != parameter_scopes_.rend(); ++scope)
		{
			const auto formal = scope->find(name);
			if (formal != scope->end())
				return formal->second;
		}
		return std::nullopt;
	}

	std::optional<FunctionSymbol> current_function_symbol(const std::string &name) const
	{
		const auto found = functions_.find(name);
		if (found == functions_.end())
			return std::nullopt;
		return found->second;
	}

	FunctionSymbol begin_definition(const std::string &name)
	{
		const auto found = functions_.find(name);
		if (found == functions_.end())
		{
			const FunctionSymbol symbol{builder_.create_function_handle(), true};
			functions_[name] = symbol;
			return symbol;
		}

		if (!found->second.syntactically_defined)
		{
			found->second.syntactically_defined = true;
			return found->second;
		}

		const FunctionSymbol replacement{builder_.create_function_handle(), true};
		found->second = replacement;
		return replacement;
	}

	LinkId function_for_call(const std::string &name)
	{
		const auto found = functions_.find(name);
		if (found != functions_.end())
			return found->second.handle;

		const FunctionSymbol provisional{builder_.create_function_handle(), false};
		functions_[name] = provisional;
		return provisional.handle;
	}

	void restore_function_symbol(const std::string &name, const std::optional<FunctionSymbol> &previous)
	{
		if (previous)
			functions_[name] = *previous;
		else
			functions_.erase(name);
	}

	static void require_arity(const Json &arguments, std::size_t expected, const char *operator_name)
	{
		if (arguments.size() != expected)
			throw JsonProjectionError(std::string(operator_name) + " has invalid arity");
	}

	LinkStore &store_;
	BootstrapVocabulary vocabulary_;
	LinkId sequence_relation_;
	ProgramBuilder builder_;
	std::map<std::string, FunctionSymbol> functions_;
	std::vector<std::map<std::string, LinkId>> parameter_scopes_;
	std::map<std::string, LinkId> literal_ids_;
	std::map<LinkId, Json> literals_by_id_;
};

class JsonCompatibilitySession
{
public:
	using Json = nlohmann::json;

	explicit JsonCompatibilitySession(std::size_t max_call_depth = 1000)
	    : store_(), runtime_(store_, max_call_depth), sequence_relation_(store_.create_point()),
	      importer_(store_, runtime_.vocabulary(), sequence_relation_)
	{
		register_sequence_handler();
	}

	LinkId import_program(const Json &expression) { return importer_.import_program(expression); }

	LinkId execute(LinkId root) { return runtime_.execute(root); }

	Json project_result(LinkId value) const { return importer_.project_value(value); }

	Json interpret(const Json &expression)
	{
		try
		{
			return project_result(execute(import_program(expression)));
		}
		catch (const std::exception &)
		{
			return nullptr;
		}
	}

	const LinkStore &store() const { return store_; }

	LinkStore &store() { return store_; }

	const BootstrapRuntime &runtime() const { return runtime_; }

	BootstrapRuntime &runtime() { return runtime_; }

	LinkId sequence_relation() const { return sequence_relation_; }

private:
	void register_sequence_handler()
	{
		runtime_.executor().register_native(
		    sequence_relation_,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    if (context.subject != runtime_.vocabulary().unit)
				    throw std::runtime_error("JSON compatibility sequence is not an executable expression");

			    const std::vector<LinkId> expressions =
			        decode_link_list(store_, runtime_.vocabulary().nil, context.object);
			    LinkId result = runtime_.vocabulary().nil;
			    for (const LinkId expression : expressions)
			    {
				    try
				    {
					    result = executor.execute(expression, context.entity, context.frame);
				    }
				    catch (const std::exception &)
				    {
					    result = runtime_.vocabulary().nil;
				    }
			    }
			    return result;
		    });
	}

	InMemoryLinkStore store_;
	BootstrapRuntime runtime_;
	LinkId sequence_relation_;
	JsonProgramImporter importer_;
};

} // namespace avm