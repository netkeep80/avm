#pragma once

#include "avm/executor.h"
#include "avm/program_model.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace avm
{

struct IndependentProjectionVocabulary
{
	LinkId relation;

	static IndependentProjectionVocabulary create(LinkStore &store)
	{
		return IndependentProjectionVocabulary{store.create_point()};
	}
};

inline void validate_independent_projection_vocabulary(const LinkStore &store,
                                                       const IndependentProjectionVocabulary &vocabulary)
{
	if (!store.contains(vocabulary.relation))
		throw std::invalid_argument("independent projection relation is not present in LinkStore");
}

namespace independent_projection_detail
{

inline ExecutionOutcome execute(const ExecutionContext &context, Executor &executor)
{
	if (!context.semantic)
		throw std::logic_error("independent projection requires an explicit semantic context");

	const LinkId list_nil = context.object;
	const std::vector<LinkId> bodies =
	    decode_link_list(executor.store(), list_nil, context.subject, std::numeric_limits<std::size_t>::max());

	std::vector<LinkId> results;
	results.reserve(bodies.size());
	for (const LinkId body : bodies)
	{
		const ExecutionOutcome outcome =
		    executor.execute_outcome_in_context(body, context.semantic, context.entity, context.frame);
		results.push_back(outcome.result);
	}

	const LinkId result_list = encode_link_list(executor.store(), list_nil, results);
	return ExecutionOutcome{result_list, context.semantic};
}

struct Handler
{
	ExecutionOutcome operator()(const ExecutionContext &context, Executor &executor) const
	{
		return execute(context, executor);
	}
};

} // namespace independent_projection_detail

inline void register_independent_projection_runtime(Executor &executor,
                                                    const IndependentProjectionVocabulary &vocabulary)
{
	validate_independent_projection_vocabulary(executor.store(), vocabulary);
	executor.register_native(vocabulary.relation, independent_projection_detail::Handler{});
}

} // namespace avm
