#pragma once

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/relations_model.h"

#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>

namespace avm::showcase
{

struct CalculatorVocabulary
{
	LinkId state_relation;
	LinkId press_digit_relation;
	LinkId press_add_relation;
	LinkId press_subtract_relation;
	LinkId press_multiply_relation;
	LinkId press_divide_relation;
	LinkId press_equals_relation;
	LinkId clear_relation;

	static CalculatorVocabulary create(LinkStore &store)
	{
		return CalculatorVocabulary{
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		};
	}
};

inline void validate_calculator_vocabulary(const LinkStore &store, const CalculatorVocabulary &vocabulary)
{
	const std::array<LinkId, 8> identities{
	    vocabulary.state_relation,
	    vocabulary.press_digit_relation,
	    vocabulary.press_add_relation,
	    vocabulary.press_subtract_relation,
	    vocabulary.press_multiply_relation,
	    vocabulary.press_divide_relation,
	    vocabulary.press_equals_relation,
	    vocabulary.clear_relation,
	};

	std::set<LinkId> unique;
	for (const LinkId identity : identities)
	{
		if (!store.contains(identity))
			throw std::invalid_argument("calculator vocabulary identity is not present in LinkStore");
		if (!unique.insert(identity).second)
			throw std::invalid_argument("calculator vocabulary identities must be distinct");
	}
}

struct CalculatorState
{
	LinkId entity;
	LinkId display;
	LinkId accumulator;
	LinkId pending_operation;
	LinkId entering_new_number;

	bool operator==(const CalculatorState &) const = default;
};

inline bool is_calculator_pending_operation(const BootstrapVocabulary &bootstrap, const IntegerVocabulary &integers,
                                            LinkId operation)
{
	return operation == bootstrap.unit || operation == integers.add_relation || operation == integers.subtract_relation ||
	       operation == integers.multiply_relation || operation == integers.divide_relation;
}

inline void validate_calculator_state_fields(const LinkStore &store, const BootstrapVocabulary &bootstrap,
                                             const IntegerVocabulary &integers, LinkId display, LinkId accumulator,
                                             LinkId pending_operation, LinkId entering_new_number)
{
	static_cast<void>(decode_integer(store, integers, display));
	static_cast<void>(decode_integer(store, integers, accumulator));

	if (!is_calculator_pending_operation(bootstrap, integers, pending_operation))
		throw std::invalid_argument("calculator pending operation is not canonical");
	if (entering_new_number != bootstrap.true_value && entering_new_number != bootstrap.false_value)
		throw std::invalid_argument("calculator entering-new-number flag is not canonical Boolean");
}

inline CalculatorState decode_calculator_state(const LinkStore &store, const BootstrapVocabulary &bootstrap,
                                                const IntegerVocabulary &integers,
                                                const CalculatorVocabulary &calculator, LinkId entity)
{
	validate_calculator_vocabulary(store, calculator);
	const RelationEntity state_entity = decode_relation_entity(store, entity);
	if (state_entity.relation != calculator.state_relation)
		throw std::invalid_argument("entity is not a CalculatorState");

	const Link payload = store.get(state_entity.object);
	const Link pending_and_flag = store.get(payload.end);
	validate_calculator_state_fields(store, bootstrap, integers, state_entity.subject, payload.begin,
	                                 pending_and_flag.begin, pending_and_flag.end);
	return CalculatorState{
	    entity,
	    state_entity.subject,
	    payload.begin,
	    pending_and_flag.begin,
	    pending_and_flag.end,
	};
}

inline LinkId realize_calculator_state(LinkStore &store, const BootstrapVocabulary &bootstrap,
                                       const IntegerVocabulary &integers, const CalculatorVocabulary &calculator,
                                       LinkId display, LinkId accumulator, LinkId pending_operation,
                                       LinkId entering_new_number)
{
	validate_calculator_vocabulary(store, calculator);
	validate_calculator_state_fields(store, bootstrap, integers, display, accumulator, pending_operation,
	                                 entering_new_number);

	// Сначала валидируем все поля и только после этого материализуем canonical state structure.
	const LinkId pending_and_flag = store.intern(pending_operation, entering_new_number);
	const LinkId payload = store.intern(accumulator, pending_and_flag);
	return encode_relation_entity(store, RelationEntity{calculator.state_relation, display, payload});
}

inline LinkId realize_initial_calculator_state(LinkStore &store, const BootstrapVocabulary &bootstrap,
                                               const IntegerVocabulary &integers,
                                               const CalculatorVocabulary &calculator)
{
	return realize_calculator_state(store, bootstrap, integers, calculator, integers.zero, integers.zero, bootstrap.unit,
	                                bootstrap.true_value);
}

inline bool is_calculator_event_relation(const CalculatorVocabulary &calculator, LinkId relation)
{
	return relation == calculator.press_digit_relation || relation == calculator.press_add_relation ||
	       relation == calculator.press_subtract_relation || relation == calculator.press_multiply_relation ||
	       relation == calculator.press_divide_relation || relation == calculator.press_equals_relation ||
	       relation == calculator.clear_relation;
}

inline LinkId realize_calculator_event(LinkStore &store, const CalculatorVocabulary &calculator, LinkId relation,
                                       LinkId current_state, LinkId input)
{
	validate_calculator_vocabulary(store, calculator);
	if (!is_calculator_event_relation(calculator, relation))
		throw std::invalid_argument("relation is not a calculator event relation");
	if (!store.contains(current_state) || !store.contains(input))
		throw std::invalid_argument("calculator event references an unknown LinkId");
	return encode_relation_entity(store, RelationEntity{relation, current_state, input});
}

inline LinkId calculator_integer_relation_for_event(const CalculatorVocabulary &calculator,
                                                    const IntegerVocabulary &integers, LinkId relation)
{
	if (relation == calculator.press_add_relation)
		return integers.add_relation;
	if (relation == calculator.press_subtract_relation)
		return integers.subtract_relation;
	if (relation == calculator.press_multiply_relation)
		return integers.multiply_relation;
	if (relation == calculator.press_divide_relation)
		return integers.divide_relation;
	throw std::invalid_argument("calculator event does not select an arithmetic relation");
}

inline bool calculator_entering_new_number(const CalculatorState &state, const BootstrapVocabulary &bootstrap)
{
	return state.entering_new_number == bootstrap.true_value;
}

inline void validate_calculator_event_context(const ExecutionContext &context, const CalculatorState &state)
{
	if (!context.semantic)
		throw std::invalid_argument("calculator event requires explicit semantic context");
	if (context.semantic.role(SemanticContextRole::RelationState) != state.entity)
		throw std::invalid_argument("calculator semantic relation-state must equal event subject state");
}

inline LinkId execute_calculator_integer_relation(const ExecutionContext &context, Executor &executor,
                                                  LinkId relation, LinkId left, LinkId right)
{
	const LinkId expression = encode_relation_entity(executor.store(), RelationEntity{relation, left, right});
	const ExecutionOutcome arithmetic = executor.execute_same_semantic_context_outcome(expression, context);
	if (arithmetic.semantic != context.semantic)
		throw std::logic_error("pure Integer relation unexpectedly changed calculator semantic context");
	return arithmetic.result;
}

inline LinkId apply_calculator_pending_operation(const ExecutionContext &context, Executor &executor,
                                                 const BootstrapVocabulary &bootstrap,
                                                 const IntegerVocabulary &integers, const CalculatorState &state)
{
	if (!is_calculator_pending_operation(bootstrap, integers, state.pending_operation) ||
	    state.pending_operation == bootstrap.unit)
		throw std::invalid_argument("calculator state has no canonical pending arithmetic operation");
	return execute_calculator_integer_relation(context, executor, state.pending_operation, state.accumulator,
	                                           state.display);
}

inline ExecutionOutcome calculator_state_transition(const ExecutionContext &context, LinkId next_state)
{
	if (!context.semantic)
		throw std::logic_error("calculator state transition requires semantic context");
	return ExecutionOutcome{next_state, context.semantic.with_relation_state(next_state)};
}

inline ExecutionOutcome execute_calculator_digit(const ExecutionContext &context, Executor &executor,
                                                  const BootstrapVocabulary &bootstrap,
                                                  const IntegerVocabulary &integers,
                                                  const CalculatorVocabulary &calculator)
{
	const CalculatorState state =
	    decode_calculator_state(executor.store(), bootstrap, integers, calculator, context.subject);
	validate_calculator_event_context(context, state);
	const std::int64_t digit = decode_integer(executor.store(), integers, context.object);
	if (digit < 0 || digit > 9)
		throw std::invalid_argument("calculator digit must be in range 0..9");

	LinkId display = context.object;
	if (!calculator_entering_new_number(state, bootstrap))
	{
		const LinkId ten = realize_integer(executor.store(), integers, 10);
		const LinkId shifted = execute_calculator_integer_relation(context, executor, integers.multiply_relation,
		                                                           state.display, ten);
		display = execute_calculator_integer_relation(context, executor, integers.add_relation, shifted, context.object);
	}

	const LinkId next_state = realize_calculator_state(executor.store(), bootstrap, integers, calculator, display,
	                                                   state.accumulator, state.pending_operation,
	                                                   bootstrap.false_value);
	return calculator_state_transition(context, next_state);
}

inline ExecutionOutcome execute_calculator_operation(const ExecutionContext &context, Executor &executor,
                                                      const BootstrapVocabulary &bootstrap,
                                                      const IntegerVocabulary &integers,
                                                      const CalculatorVocabulary &calculator)
{
	const CalculatorState state =
	    decode_calculator_state(executor.store(), bootstrap, integers, calculator, context.subject);
	validate_calculator_event_context(context, state);
	if (context.object != bootstrap.unit)
		throw std::invalid_argument("calculator operation event requires unit input");

	LinkId display = state.display;
	LinkId accumulator = state.accumulator;
	if (!calculator_entering_new_number(state, bootstrap))
	{
		const LinkId value = state.pending_operation == bootstrap.unit
		                         ? state.display
		                         : apply_calculator_pending_operation(context, executor, bootstrap, integers, state);
		display = value;
		accumulator = value;
	}
	else if (state.pending_operation == bootstrap.unit)
	{
		accumulator = state.display;
	}

	const LinkId pending = calculator_integer_relation_for_event(calculator, integers, context.relation);
	const LinkId next_state = realize_calculator_state(executor.store(), bootstrap, integers, calculator, display,
	                                                   accumulator, pending, bootstrap.true_value);
	return calculator_state_transition(context, next_state);
}

inline ExecutionOutcome execute_calculator_equals(const ExecutionContext &context, Executor &executor,
                                                   const BootstrapVocabulary &bootstrap,
                                                   const IntegerVocabulary &integers,
                                                   const CalculatorVocabulary &calculator)
{
	const CalculatorState state =
	    decode_calculator_state(executor.store(), bootstrap, integers, calculator, context.subject);
	validate_calculator_event_context(context, state);
	if (context.object != bootstrap.unit)
		throw std::invalid_argument("calculator equals event requires unit input");

	LinkId value = state.display;
	if (state.pending_operation != bootstrap.unit)
	{
		if (calculator_entering_new_number(state, bootstrap))
			throw std::invalid_argument("calculator equals requires a right operand");
		value = apply_calculator_pending_operation(context, executor, bootstrap, integers, state);
	}

	const LinkId next_state = realize_calculator_state(executor.store(), bootstrap, integers, calculator, value, value,
	                                                   bootstrap.unit, bootstrap.true_value);
	return calculator_state_transition(context, next_state);
}

inline ExecutionOutcome execute_calculator_clear(const ExecutionContext &context, Executor &executor,
                                                  const BootstrapVocabulary &bootstrap,
                                                  const IntegerVocabulary &integers,
                                                  const CalculatorVocabulary &calculator)
{
	const CalculatorState state =
	    decode_calculator_state(executor.store(), bootstrap, integers, calculator, context.subject);
	validate_calculator_event_context(context, state);
	if (context.object != bootstrap.unit)
		throw std::invalid_argument("calculator clear event requires unit input");

	const LinkId next_state = realize_initial_calculator_state(executor.store(), bootstrap, integers, calculator);
	return calculator_state_transition(context, next_state);
}

inline void register_calculator_runtime(Executor &executor, const BootstrapVocabulary &bootstrap,
                                        const IntegerVocabulary &integers, const CalculatorVocabulary &calculator)
{
	validate_calculator_vocabulary(executor.store(), calculator);
	validate_integer_vocabulary(executor.store(), integers);
	for (const LinkId relation :
	     {integers.add_relation, integers.subtract_relation, integers.multiply_relation, integers.divide_relation})
	{
		if (!executor.has_native(relation))
			throw std::invalid_argument("calculator requires registered canonical Integer arithmetic");
	}

	executor.register_native(calculator.press_digit_relation,
	                         [bootstrap, integers, calculator](const ExecutionContext &context, Executor &current)
	                         { return execute_calculator_digit(context, current, bootstrap, integers, calculator); });

	const auto operation_handler = [bootstrap, integers, calculator](const ExecutionContext &context, Executor &current)
	{ return execute_calculator_operation(context, current, bootstrap, integers, calculator); };
	executor.register_native(calculator.press_add_relation, operation_handler);
	executor.register_native(calculator.press_subtract_relation, operation_handler);
	executor.register_native(calculator.press_multiply_relation, operation_handler);
	executor.register_native(calculator.press_divide_relation, operation_handler);

	executor.register_native(calculator.press_equals_relation,
	                         [bootstrap, integers, calculator](const ExecutionContext &context, Executor &current)
	                         { return execute_calculator_equals(context, current, bootstrap, integers, calculator); });
	executor.register_native(calculator.clear_relation,
	                         [bootstrap, integers, calculator](const ExecutionContext &context, Executor &current)
	                         { return execute_calculator_clear(context, current, bootstrap, integers, calculator); });
}

} // namespace avm::showcase
