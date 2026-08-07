#pragma once

#include "avm/relations_model.h"

#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace avm
{

struct BootstrapVocabulary
{
	LinkId unit;
	LinkId nil;
	LinkId true_value;
	LinkId false_value;
	LinkId quote_relation;
	LinkId parameter_relation;
	LinkId sequence_relation;
	LinkId not_relation;
	LinkId and_relation;
	LinkId or_relation;
	LinkId if_relation;
	LinkId function_relation;
	LinkId call_relation;
	LinkId binding_relation;
	LinkId frame_relation;

	static BootstrapVocabulary create(LinkStore &store)
	{
		return BootstrapVocabulary{
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(), store.create_point(), store.create_point(),
		};
	}
};

inline LinkId encode_link_list(LinkStore &store, LinkId nil, const std::vector<LinkId> &items)
{
	if (!store.contains(nil))
		throw std::invalid_argument("list nil identity is not present in LinkStore");

	LinkId tail = nil;
	for (auto it = items.rbegin(); it != items.rend(); ++it)
	{
		if (!store.contains(*it))
			throw std::invalid_argument("list item is not present in LinkStore");
		tail = store.intern(*it, tail);
	}
	return tail;
}

inline std::vector<LinkId> decode_link_list(const LinkStore &store, LinkId nil, LinkId head,
                                            std::size_t max_items = 100000)
{
	if (!store.contains(nil))
		throw std::invalid_argument("list nil identity is not present in LinkStore");
	if (!store.contains(head))
		throw std::invalid_argument("list head is not present in LinkStore");

	std::vector<LinkId> result;
	std::set<LinkId> visited;
	LinkId cursor = head;

	while (cursor != nil)
	{
		if (!visited.insert(cursor).second)
			throw std::runtime_error("cycle detected in link list");
		if (result.size() >= max_items)
			throw std::runtime_error("link list exceeds configured limit");

		const Link cell = store.get(cursor);
		result.push_back(cell.begin);
		cursor = cell.end;

		if (!store.contains(cursor))
			throw std::runtime_error("link list tail references an unknown LinkId");
	}

	return result;
}

struct FunctionDefinition
{
	LinkId entity;
	LinkId handle;
	std::vector<LinkId> parameters;
	LinkId body;
};

inline std::optional<FunctionDefinition> find_function_definition(const LinkStore &store,
                                                                  const BootstrapVocabulary &vocabulary, LinkId handle)
{
	if (!store.contains(handle))
		return std::nullopt;

	std::optional<FunctionDefinition> found;
	for (const LinkId candidate : store.outgoing(vocabulary.function_relation))
	{
		const RelationEntity decoded = decode_relation_entity(store, candidate);
		if (decoded.relation != vocabulary.function_relation || decoded.subject != handle)
			continue;

		const Link payload = store.get(decoded.object);
		FunctionDefinition definition{
		    candidate,
		    handle,
		    decode_link_list(store, vocabulary.nil, payload.begin),
		    payload.end,
		};

		if (found)
			throw std::logic_error("multiple function definitions for one handle");
		found = std::move(definition);
	}
	return found;
}

struct CallExpression
{
	LinkId function;
	std::vector<LinkId> arguments;
};

inline CallExpression decode_call_expression(const LinkStore &store, const BootstrapVocabulary &vocabulary,
                                             LinkId entity)
{
	const RelationEntity decoded = decode_relation_entity(store, entity);
	if (decoded.relation != vocabulary.call_relation || decoded.subject != vocabulary.unit)
		throw std::invalid_argument("entity is not an AVM call expression");

	const Link payload = store.get(decoded.object);
	return CallExpression{
	    payload.begin,
	    decode_link_list(store, vocabulary.nil, payload.end),
	};
}

class ProgramBuilder
{
public:
	ProgramBuilder(LinkStore &store, const BootstrapVocabulary &vocabulary) : store_(store), vocabulary_(vocabulary) {}

	LinkId literal(LinkId value)
	{
		require(value, "literal value");
		return expression(vocabulary_.quote_relation, value);
	}

	LinkId parameter(LinkId formal)
	{
		require(formal, "formal parameter");
		return expression(vocabulary_.parameter_relation, formal);
	}

	LinkId logical_not(LinkId argument)
	{
		require(argument, "NOT argument");
		return expression(vocabulary_.not_relation, encode_link_list(store_, vocabulary_.nil, {argument}));
	}

	LinkId logical_and(LinkId left, LinkId right) { return binary(vocabulary_.and_relation, left, right); }

	LinkId logical_or(LinkId left, LinkId right) { return binary(vocabulary_.or_relation, left, right); }

	LinkId conditional(LinkId condition, LinkId then_branch, LinkId else_branch)
	{
		require(condition, "If condition");
		require(then_branch, "If then branch");
		require(else_branch, "If else branch");
		return expression(vocabulary_.if_relation,
		                  encode_link_list(store_, vocabulary_.nil, {condition, then_branch, else_branch}));
	}

	LinkId sequence(const std::vector<LinkId> &expressions)
	{
		return expression(vocabulary_.sequence_relation, encode_link_list(store_, vocabulary_.nil, expressions));
	}

	LinkId create_function_handle() { return store_.create_point(); }

	LinkId define_function(LinkId handle, const std::vector<LinkId> &parameters, LinkId body)
	{
		require(handle, "function handle");
		require(body, "function body");
		for (const LinkId parameter_id : parameters)
			require(parameter_id, "formal parameter");

		const LinkId parameter_list = encode_link_list(store_, vocabulary_.nil, parameters);
		const LinkId payload = store_.intern(parameter_list, body);
		const RelationEntity source{vocabulary_.function_relation, handle, payload};

		if (const auto existing = find_function_definition(store_, vocabulary_, handle))
		{
			if (existing->parameters == parameters && existing->body == body)
				return existing->entity;
			throw std::logic_error("function handle already has a different definition");
		}

		return encode_relation_entity(store_, source);
	}

	LinkId call(LinkId function, const std::vector<LinkId> &arguments)
	{
		require(function, "function handle");
		const LinkId argument_list = encode_link_list(store_, vocabulary_.nil, arguments);
		const LinkId payload = store_.intern(function, argument_list);
		return expression(vocabulary_.call_relation, payload);
	}

private:
	LinkId expression(LinkId relation, LinkId object)
	{
		require(relation, "expression relation");
		require(object, "expression object");
		return encode_relation_entity(store_, RelationEntity{relation, vocabulary_.unit, object});
	}

	LinkId binary(LinkId relation, LinkId left, LinkId right)
	{
		require(left, "left argument");
		require(right, "right argument");
		return expression(relation, encode_link_list(store_, vocabulary_.nil, {left, right}));
	}

	void require(LinkId id, const char *what) const
	{
		if (!store_.contains(id))
			throw std::invalid_argument(std::string(what) + " is not present in LinkStore");
	}

	LinkStore &store_;
	const BootstrapVocabulary &vocabulary_;
};

} // namespace avm
