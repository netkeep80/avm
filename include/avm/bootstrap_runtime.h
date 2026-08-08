#pragma once

#include "avm/executor.h"
#include "avm/program_model.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

inline std::optional<LinkId> lookup_relation_value(const LinkStore &store, LinkId relation, LinkId subject)
{
	std::optional<LinkId> result;
	for (const LinkId candidate : store.outgoing(relation))
	{
		const RelationEntity decoded = decode_relation_entity(store, candidate);
		if (decoded.relation != relation || decoded.subject != subject)
			continue;

		if (result && *result != decoded.object)
			throw std::logic_error("relation lookup is not functional for the requested subject");
		result = decoded.object;
	}
	return result;
}

inline std::optional<LinkId> lookup_binary_relation_value(const LinkStore &store, LinkId relation, LinkId left,
                                                          LinkId right)
{
	const auto pair = store.find(left, right);
	if (!pair)
		return std::nullopt;
	return lookup_relation_value(store, relation, *pair);
}

struct DecodedCallFrame
{
	LinkId entity;
	LinkId parent;
	LinkId function;
	std::vector<LinkId> bindings;
};

inline DecodedCallFrame decode_call_frame(const LinkStore &store, const BootstrapVocabulary &vocabulary, LinkId frame)
{
	if (frame == vocabulary.frame_relation)
		throw std::runtime_error("frame vocabulary identity is not a call frame");

	const RelationEntity decoded = decode_relation_entity(store, frame);
	if (decoded.relation != vocabulary.frame_relation)
		throw std::runtime_error("LinkId is not an AVM call frame");

	const Link payload = store.get(decoded.object);
	return DecodedCallFrame{
	    frame,
	    decoded.subject,
	    payload.begin,
	    decode_link_list(store, vocabulary.nil, payload.end),
	};
}

class BootstrapRuntime
{
public:
	explicit BootstrapRuntime(LinkStore &store, std::size_t max_call_depth = 1000)
	    : BootstrapRuntime(store, BootstrapVocabulary::create(store), max_call_depth)
	{
	}

	BootstrapRuntime(LinkStore &store, BootstrapVocabulary vocabulary, std::size_t max_call_depth = 1000)
	    : store_(store), vocabulary_(std::move(vocabulary)), executor_(store), max_call_depth_(max_call_depth)
	{
		if (max_call_depth_ == 0)
			throw std::invalid_argument("maximum call depth must be greater than zero");
		upgrade_structural_vocabulary_if_needed();
		validate_vocabulary();
		materialize_truth_tables();
		register_handlers();
	}

	const BootstrapVocabulary &vocabulary() const { return vocabulary_; }

	ProgramBuilder builder() { return ProgramBuilder(store_, vocabulary_); }

	Executor &executor() { return executor_; }

	const Executor &executor() const { return executor_; }

	LinkId execute(LinkId root) { return executor_.execute(root); }

private:
	void upgrade_structural_vocabulary_if_needed()
	{
		const bool begin_missing = vocabulary_.begin_relation == invalid_link_id;
		const bool end_missing = vocabulary_.end_relation == invalid_link_id;
		const bool same_missing = vocabulary_.same_relation == invalid_link_id;
		const bool exists_missing = vocabulary_.link_exists_relation == invalid_link_id;
		const unsigned missing = static_cast<unsigned>(begin_missing) + static_cast<unsigned>(end_missing) +
		                         static_cast<unsigned>(same_missing) + static_cast<unsigned>(exists_missing);

		if (missing == 0)
			return;
		if (missing != 4)
			throw std::invalid_argument("bootstrap structural vocabulary must be fully present or fully absent");

		vocabulary_.begin_relation = store_.create_point();
		vocabulary_.end_relation = store_.create_point();
		vocabulary_.same_relation = store_.create_point();
		vocabulary_.link_exists_relation = store_.create_point();
	}

	void validate_vocabulary() const
	{
		const std::vector<LinkId> ids{
		    vocabulary_.unit,
		    vocabulary_.nil,
		    vocabulary_.true_value,
		    vocabulary_.false_value,
		    vocabulary_.quote_relation,
		    vocabulary_.parameter_relation,
		    vocabulary_.sequence_relation,
		    vocabulary_.not_relation,
		    vocabulary_.and_relation,
		    vocabulary_.or_relation,
		    vocabulary_.if_relation,
		    vocabulary_.function_relation,
		    vocabulary_.call_relation,
		    vocabulary_.binding_relation,
		    vocabulary_.frame_relation,
		    vocabulary_.begin_relation,
		    vocabulary_.end_relation,
		    vocabulary_.same_relation,
		    vocabulary_.link_exists_relation,
		};

		std::set<LinkId> unique;
		for (const LinkId id : ids)
		{
			if (!store_.contains(id))
				throw std::invalid_argument("bootstrap vocabulary contains an unknown LinkId");
			if (!unique.insert(id).second)
				throw std::invalid_argument("bootstrap vocabulary identities must be distinct");
		}
	}

	static void require_expression_subject(const ExecutionContext &context, LinkId unit)
	{
		if (context.subject != unit)
			throw std::runtime_error("runtime relation entity is not an executable expression");
	}

	std::vector<LinkId> expression_arguments(const ExecutionContext &context, std::size_t expected) const
	{
		require_expression_subject(context, vocabulary_.unit);
		std::vector<LinkId> arguments = decode_link_list(store_, vocabulary_.nil, context.object);
		if (arguments.size() != expected)
			throw std::runtime_error("expression has unexpected argument count");
		return arguments;
	}

	LinkId require_lookup(const std::optional<LinkId> &value, const char *message) const
	{
		if (!value)
			throw std::runtime_error(message);
		return *value;
	}

	std::size_t frame_depth(std::optional<LinkId> frame) const
	{
		std::size_t depth = 0;
		std::set<LinkId> visited;
		LinkId cursor = frame.value_or(vocabulary_.nil);
		while (cursor != vocabulary_.nil)
		{
			if (!visited.insert(cursor).second)
				throw std::runtime_error("cycle detected in call-frame chain");

			const DecodedCallFrame decoded = decode_call_frame(store_, vocabulary_, cursor);
			cursor = decoded.parent;
			++depth;
			if (depth > max_call_depth_)
				throw std::runtime_error("call-frame chain exceeds maximum depth");
		}
		return depth;
	}

	std::optional<LinkId> resolve_parameter(std::optional<LinkId> frame, LinkId formal) const
	{
		std::set<LinkId> visited;
		LinkId cursor = frame.value_or(vocabulary_.nil);
		while (cursor != vocabulary_.nil)
		{
			if (!visited.insert(cursor).second)
				throw std::runtime_error("cycle detected in call-frame chain");

			const DecodedCallFrame decoded = decode_call_frame(store_, vocabulary_, cursor);
			for (const LinkId binding : decoded.bindings)
			{
				const RelationEntity row = decode_relation_entity(store_, binding);
				if (row.relation != vocabulary_.binding_relation)
					throw std::runtime_error("call frame contains a non-binding entity");
				if (row.subject == formal)
					return row.object;
			}
			cursor = decoded.parent;
		}
		return std::nullopt;
	}

	LinkId materialize_frame(std::optional<LinkId> parent, LinkId function, const std::vector<LinkId> &formals,
	                         const std::vector<LinkId> &actuals)
	{
		if (formals.size() != actuals.size())
			throw std::logic_error("cannot materialize frame with mismatched binding counts");

		std::vector<LinkId> bindings;
		bindings.reserve(formals.size());
		for (std::size_t i = 0; i < formals.size(); ++i)
		{
			bindings.push_back(
			    encode_relation_entity(store_, RelationEntity{vocabulary_.binding_relation, formals[i], actuals[i]}));
		}

		const LinkId binding_list = encode_link_list(store_, vocabulary_.nil, bindings);
		const LinkId payload = store_.intern(function, binding_list);
		return encode_relation_entity(store_, RelationEntity{
		                                          vocabulary_.frame_relation,
		                                          parent.value_or(vocabulary_.nil),
		                                          payload,
		                                      });
	}

	void materialize_truth_tables()
	{
		encode_relation_entity(
		    store_, RelationEntity{vocabulary_.not_relation, vocabulary_.true_value, vocabulary_.false_value});
		encode_relation_entity(
		    store_, RelationEntity{vocabulary_.not_relation, vocabulary_.false_value, vocabulary_.true_value});

		const auto add_binary_row = [this](LinkId relation, LinkId left, LinkId right, LinkId result)
		{
			const LinkId key = store_.intern(left, right);
			encode_relation_entity(store_, RelationEntity{relation, key, result});
		};

		add_binary_row(vocabulary_.and_relation, vocabulary_.false_value, vocabulary_.false_value,
		               vocabulary_.false_value);
		add_binary_row(vocabulary_.and_relation, vocabulary_.false_value, vocabulary_.true_value,
		               vocabulary_.false_value);
		add_binary_row(vocabulary_.and_relation, vocabulary_.true_value, vocabulary_.false_value,
		               vocabulary_.false_value);
		add_binary_row(vocabulary_.and_relation, vocabulary_.true_value, vocabulary_.true_value,
		               vocabulary_.true_value);

		add_binary_row(vocabulary_.or_relation, vocabulary_.false_value, vocabulary_.false_value,
		               vocabulary_.false_value);
		add_binary_row(vocabulary_.or_relation, vocabulary_.false_value, vocabulary_.true_value,
		               vocabulary_.true_value);
		add_binary_row(vocabulary_.or_relation, vocabulary_.true_value, vocabulary_.false_value,
		               vocabulary_.true_value);
		add_binary_row(vocabulary_.or_relation, vocabulary_.true_value, vocabulary_.true_value, vocabulary_.true_value);

		encode_relation_entity(store_,
		                       RelationEntity{vocabulary_.if_relation, vocabulary_.true_value, vocabulary_.true_value});
		encode_relation_entity(
		    store_, RelationEntity{vocabulary_.if_relation, vocabulary_.false_value, vocabulary_.false_value});
	}

	void register_handlers()
	{
		executor_.register_native(vocabulary_.quote_relation,
		                          [this](const ExecutionContext &context, Executor &)
		                          {
			                          require_expression_subject(context, vocabulary_.unit);
			                          return context.object;
		                          });

		executor_.register_native(vocabulary_.sequence_relation,
		                          [this](const ExecutionContext &context, Executor &executor)
		                          {
			                          require_expression_subject(context, vocabulary_.unit);
			                          const std::vector<LinkId> expressions =
			                              decode_link_list(store_, vocabulary_.nil, context.object);

			                          LinkId result = vocabulary_.nil;
			                          for (const LinkId expression : expressions)
				                          result = executor.execute(expression, context.entity, context.frame);
			                          return result;
		                          });

		executor_.register_native(
		    vocabulary_.begin_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 1);
			    const LinkId value = executor.execute(arguments[0], context.entity, context.frame);
			    return store_.get(value).begin;
		    });

		executor_.register_native(
		    vocabulary_.end_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 1);
			    const LinkId value = executor.execute(arguments[0], context.entity, context.frame);
			    return store_.get(value).end;
		    });

		executor_.register_native(
		    vocabulary_.same_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 2);
			    const LinkId left = executor.execute(arguments[0], context.entity, context.frame);
			    const LinkId right = executor.execute(arguments[1], context.entity, context.frame);
			    return left == right ? vocabulary_.true_value : vocabulary_.false_value;
		    });

		executor_.register_native(
		    vocabulary_.link_exists_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 2);
			    const LinkId begin = executor.execute(arguments[0], context.entity, context.frame);
			    const LinkId end = executor.execute(arguments[1], context.entity, context.frame);
			    return store_.find(begin, end) ? vocabulary_.true_value : vocabulary_.false_value;
		    });

		executor_.register_native(
		    vocabulary_.not_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 1);
			    const LinkId value = executor.execute(arguments[0], context.entity, context.frame);
			    return require_lookup(lookup_relation_value(store_, vocabulary_.not_relation, value),
			                          "NOT operand is not a Boolean value");
		    });

		executor_.register_native(
		    vocabulary_.and_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 2);
			    const LinkId left = executor.execute(arguments[0], context.entity, context.frame);
			    const LinkId right = executor.execute(arguments[1], context.entity, context.frame);
			    return require_lookup(lookup_binary_relation_value(store_, vocabulary_.and_relation, left, right),
			                          "AND operands are not Boolean values");
		    });

		executor_.register_native(
		    vocabulary_.or_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    const std::vector<LinkId> arguments = expression_arguments(context, 2);
			    const LinkId left = executor.execute(arguments[0], context.entity, context.frame);
			    const LinkId right = executor.execute(arguments[1], context.entity, context.frame);
			    return require_lookup(lookup_binary_relation_value(store_, vocabulary_.or_relation, left, right),
			                          "OR operands are not Boolean values");
		    });

		executor_.register_native(vocabulary_.if_relation,
		                          [this](const ExecutionContext &context, Executor &executor)
		                          {
			                          const std::vector<LinkId> arguments = expression_arguments(context, 3);
			                          const LinkId condition =
			                              executor.execute(arguments[0], context.entity, context.frame);
			                          const LinkId selected = require_lookup(
			                              lookup_relation_value(store_, vocabulary_.if_relation, condition),
			                              "If condition is not a Boolean value");

			                          if (selected == vocabulary_.true_value)
				                          return executor.execute(arguments[1], context.entity, context.frame);
			                          if (selected == vocabulary_.false_value)
				                          return executor.execute(arguments[2], context.entity, context.frame);
			                          throw std::logic_error("If truth table returned a non-Boolean selector");
		                          });

		executor_.register_native(
		    vocabulary_.function_relation,
		    [this](const ExecutionContext &context, Executor &)
		    {
			    if (context.subject == vocabulary_.function_relation)
				    throw std::runtime_error("function vocabulary identity is not executable");

			    if (context.subject == vocabulary_.unit)
			    {
				    const DeferredFunctionDefinition definition =
				        decode_deferred_function_definition(store_, vocabulary_, context.entity);
				    static_cast<void>(materialize_function_definition(store_, vocabulary_, definition.handle,
				                                                      definition.parameters, definition.body));
				    return vocabulary_.nil;
			    }

			    const auto definition = find_function_definition(store_, vocabulary_, context.subject);
			    if (!definition || definition->entity != context.entity)
				    throw std::runtime_error("function definition entity is malformed or ambiguous");
			    return vocabulary_.nil;
		    });

		executor_.register_native(vocabulary_.parameter_relation,
		                          [this](const ExecutionContext &context, Executor &)
		                          {
			                          require_expression_subject(context, vocabulary_.unit);
			                          return require_lookup(resolve_parameter(context.frame, context.object),
			                                                "parameter is not bound in the current call-frame chain");
		                          });

		executor_.register_native(
		    vocabulary_.call_relation,
		    [this](const ExecutionContext &context, Executor &executor)
		    {
			    require_expression_subject(context, vocabulary_.unit);
			    const CallExpression call = decode_call_expression(store_, vocabulary_, context.entity);
			    const auto definition = find_function_definition(store_, vocabulary_, call.function);
			    if (!definition)
				    throw std::runtime_error("call references an undefined function handle");
			    if (definition->parameters.size() != call.arguments.size())
				    throw std::runtime_error("function call arity mismatch");
			    if (frame_depth(context.frame) >= max_call_depth_)
				    throw std::runtime_error("maximum function call depth exceeded");

			    std::vector<LinkId> actuals;
			    actuals.reserve(call.arguments.size());
			    for (const LinkId argument : call.arguments)
				    actuals.push_back(executor.execute(argument, context.entity, context.frame));

			    const LinkId frame = materialize_frame(context.frame, call.function, definition->parameters, actuals);
			    return executor.execute(definition->body, context.entity, frame);
		    });
	}

	LinkStore &store_;
	BootstrapVocabulary vocabulary_;
	Executor executor_;
	std::size_t max_call_depth_;
};

} // namespace avm
