#pragma once

#include "avm/executor.h"

#include <optional>
#include <span>

namespace avm
{

inline ExecutionOutcome execute_same_context_sequence(Executor &executor, std::span<const LinkId> entities,
                                                      LinkId empty_result, SemanticContextView semantic,
                                                      std::optional<LinkId> parent = std::nullopt,
                                                      std::optional<LinkId> frame = std::nullopt)
{
	ExecutionOutcome outcome{empty_result, semantic};
	for (const LinkId entity : entities)
	{
		if (outcome.semantic)
			outcome = executor.execute_outcome_in_context(entity, outcome.semantic, parent, frame);
		else
			outcome = executor.execute_outcome(entity, parent, frame);
	}
	return outcome;
}

inline ExecutionOutcome execute_same_context_sequence(Executor &executor, std::span<const LinkId> entities,
                                                      LinkId empty_result, const ExecutionContext &parent_context)
{
	return execute_same_context_sequence(executor, entities, empty_result, parent_context.semantic,
	                                     parent_context.entity, parent_context.frame);
}

} // namespace avm
