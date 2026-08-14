#pragma once

#include "avm/executor.h"
#include "avm/reference.h"

#include <set>
#include <stdexcept>

namespace avm
{

struct SemanticExecutionVocabulary
{
	LinkId commit_relation_state;
	LinkId resolve_reference_relation;
	LinkId apply_pure_relation;

	static SemanticExecutionVocabulary create(LinkStore &store)
	{
		return SemanticExecutionVocabulary{
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		};
	}
};

inline void validate_semantic_execution_vocabulary(const LinkStore &store,
                                                   const SemanticExecutionVocabulary &vocabulary)
{
	const LinkId ids[] = {
	    vocabulary.commit_relation_state,
	    vocabulary.resolve_reference_relation,
	    vocabulary.apply_pure_relation,
	};

	std::set<LinkId> unique;
	for (const LinkId id : ids)
	{
		if (!store.contains(id))
			throw std::invalid_argument("semantic execution vocabulary contains an unknown LinkId");
		if (!unique.insert(id).second)
			throw std::invalid_argument("semantic execution vocabulary identities must be distinct");
	}
}

inline LinkId materialize_relation_state_commit(LinkStore &store, const SemanticExecutionVocabulary &vocabulary,
                                                LinkId unit, LinkId value_expression)
{
	validate_semantic_execution_vocabulary(store, vocabulary);
	if (!store.contains(unit))
		throw std::invalid_argument("semantic commit unit identity is not present in LinkStore");
	if (!store.contains(value_expression))
		throw std::invalid_argument("semantic commit expression is not present in LinkStore");
	return encode_relation_entity(store, RelationEntity{vocabulary.commit_relation_state, unit, value_expression});
}

inline LinkId materialize_reference_resolution(LinkStore &store, const SemanticExecutionVocabulary &vocabulary,
                                               LinkId unit, LinkId reference)
{
	validate_semantic_execution_vocabulary(store, vocabulary);
	if (!store.contains(unit))
		throw std::invalid_argument("reference resolution unit identity is not present in LinkStore");
	if (!store.contains(reference))
		throw std::invalid_argument("reference expression is not present in LinkStore");
	return encode_relation_entity(store, RelationEntity{vocabulary.resolve_reference_relation, unit, reference});
}

inline LinkId materialize_pure_relation_application(LinkStore &store,
                                                    const SemanticExecutionVocabulary &vocabulary,
                                                    LinkId target_relation, LinkId subject_expression,
                                                    LinkId object_expression)
{
	validate_semantic_execution_vocabulary(store, vocabulary);
	if (!store.contains(target_relation))
		throw std::invalid_argument("pure application target relation is not present in LinkStore");
	if (!store.contains(subject_expression) || !store.contains(object_expression))
		throw std::invalid_argument("pure application operand expression is not present in LinkStore");

	const LinkId operands = store.intern(subject_expression, object_expression);
	return encode_relation_entity(store, RelationEntity{vocabulary.apply_pure_relation, target_relation, operands});
}

namespace semantic_primitives_detail
{

inline void require_semantic_context(const ExecutionContext &context, const char *operation)
{
	if (!context.semantic)
		throw std::runtime_error(std::string(operation) + " requires semantic context");
}

inline void require_unit_subject(const ExecutionContext &context, LinkId unit, const char *operation)
{
	if (context.subject != unit)
		throw std::runtime_error(std::string(operation) + " requires the configured unit subject");
}

inline ExecutionOutcome execute_state_neutral(Executor &executor, LinkId expression,
                                              const ExecutionContext &parent_context, const char *operation)
{
	const ExecutionOutcome outcome = executor.execute_outcome_in_context(
	    expression, parent_context.semantic, parent_context.entity, parent_context.frame);
	if (!(outcome.semantic == parent_context.semantic))
		throw std::runtime_error(std::string(operation) + " requires state-neutral nested execution");
	return outcome;
}

} // namespace semantic_primitives_detail

inline void register_semantic_execution_primitives(Executor &executor, const SemanticExecutionVocabulary &vocabulary,
                                                   const ReferenceVocabulary &references, LinkId unit)
{
	validate_semantic_execution_vocabulary(executor.store(), vocabulary);
	validate_reference_vocabulary(executor.store(), references);
	if (!executor.store().contains(unit))
		throw std::invalid_argument("semantic execution unit identity is not present in LinkStore");

	executor.register_native(
	    vocabulary.commit_relation_state,
	    [unit](const ExecutionContext &context, Executor &current_executor)
	    {
		    semantic_primitives_detail::require_semantic_context(context, "relation-state commit");
		    semantic_primitives_detail::require_unit_subject(context, unit, "relation-state commit");

		    const ExecutionOutcome value = semantic_primitives_detail::execute_state_neutral(
		        current_executor, context.object, context, "relation-state commit source");
		    return ExecutionOutcome{value.result, context.semantic.with_relation_state(value.result)};
	    });

	executor.register_native(
	    vocabulary.resolve_reference_relation,
	    [references, unit](const ExecutionContext &context, Executor &current_executor)
	    {
		    semantic_primitives_detail::require_semantic_context(context, "reference resolution");
		    semantic_primitives_detail::require_unit_subject(context, unit, "reference resolution");

		    const auto value = resolve_reference(current_executor.store(), references, context.object, context.semantic);
		    if (!value)
			    throw std::runtime_error("semantic reference did not resolve");
		    return ExecutionOutcome{*value};
	    });

	executor.register_native(
	    vocabulary.apply_pure_relation,
	    [](const ExecutionContext &context, Executor &current_executor)
	    {
		    semantic_primitives_detail::require_semantic_context(context, "pure relation application");
		    if (!current_executor.store().contains(context.subject))
			    throw std::runtime_error("pure relation target is not present in LinkStore");
		    if (!current_executor.has_native(context.subject))
			    throw std::runtime_error("pure relation target has no registered handler");

		    const Link operand_expressions = current_executor.store().get(context.object);
		    const ExecutionOutcome subject = semantic_primitives_detail::execute_state_neutral(
		        current_executor, operand_expressions.begin, context, "pure relation subject expression");
		    const ExecutionOutcome object = semantic_primitives_detail::execute_state_neutral(
		        current_executor, operand_expressions.end, context, "pure relation object expression");

		    const LinkId target_entity = encode_relation_entity(
		        current_executor.store(), RelationEntity{context.subject, subject.result, object.result});
		    const SemanticContextView target_semantic = context.semantic.child(SemanticContextFrame{
		        context.entity,
		        context.semantic.role(SemanticContextRole::RelationState),
		        subject.result,
		        object.result,
		    });

		    const ExecutionOutcome target = current_executor.execute_outcome_in_context(
		        target_entity, target_semantic, context.entity, context.frame);
		    if (!(target.semantic == target_semantic))
			    throw std::runtime_error("pure relation target changed semantic state");

		    return ExecutionOutcome{target.result, context.semantic};
	    });
}

} // namespace avm
