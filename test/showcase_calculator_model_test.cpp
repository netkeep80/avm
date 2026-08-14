#include "calculator_model.h"

#include "avm/persistent_link_store.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

namespace
{

struct CalculatorSession
{
	avm::LinkStore &store;
	avm::BootstrapRuntime runtime;
	avm::IntegerVocabulary integers;
	avm::showcase::CalculatorVocabulary calculator;
	avm::LinkId initial_state = avm::invalid_link_id;
	avm::LinkId current_state = avm::invalid_link_id;
	avm::SemanticContextView semantic;

	explicit CalculatorSession(avm::LinkStore &target)
	    : store(target), runtime(target), integers(avm::IntegerVocabulary::create(target)),
	      calculator(avm::showcase::CalculatorVocabulary::create(target))
	{
		avm::register_integer_arithmetic(runtime.executor(), integers);
		avm::showcase::register_calculator_runtime(runtime.executor(), runtime.vocabulary(), integers, calculator);
		initial_state =
		    avm::showcase::realize_initial_calculator_state(store, runtime.vocabulary(), integers, calculator);
		current_state = initial_state;
		semantic = avm::SemanticContextView::root(avm::SemanticContextFrame{
		    store.create_point(),
		    current_state,
		    store.create_point(),
		    store.create_point(),
		});
	}

	avm::LinkId realize_event(avm::LinkId relation, avm::LinkId input)
	{
		return avm::showcase::realize_calculator_event(store, calculator, relation, current_state, input);
	}

	avm::ExecutionOutcome event(avm::LinkId relation, avm::LinkId input)
	{
		const avm::LinkId entity = realize_event(relation, input);
		const avm::ExecutionOutcome outcome = runtime.executor().execute_outcome_in_context(entity, semantic);
		assert(outcome.semantic.role(avm::SemanticContextRole::RelationState) == outcome.result);
		current_state = outcome.result;
		semantic = outcome.semantic;
		return outcome;
	}

	avm::ExecutionOutcome digit(std::int64_t value)
	{
		return event(calculator.press_digit_relation, avm::realize_integer(store, integers, value));
	}

	avm::ExecutionOutcome operation(avm::LinkId relation) { return event(relation, runtime.vocabulary().unit); }

	avm::ExecutionOutcome equals() { return operation(calculator.press_equals_relation); }

	avm::ExecutionOutcome clear() { return operation(calculator.clear_relation); }
};

void expect_state(const CalculatorSession &session, std::int64_t display, std::int64_t accumulator,
                  avm::LinkId pending_operation, bool entering_new_number)
{
	const avm::showcase::CalculatorState state = avm::showcase::decode_calculator_state(
	    session.store, session.runtime.vocabulary(), session.integers, session.calculator, session.current_state);
	assert(avm::decode_integer(session.store, session.integers, state.display) == display);
	assert(avm::decode_integer(session.store, session.integers, state.accumulator) == accumulator);
	assert(state.pending_operation == pending_operation);
	const avm::LinkId expected_flag = entering_new_number ? session.runtime.vocabulary().true_value
	                                                      : session.runtime.vocabulary().false_value;
	assert(state.entering_new_number == expected_flag);
	assert(session.semantic.role(avm::SemanticContextRole::RelationState) == state.entity);
}

void expect_initial(const CalculatorSession &session)
{
	expect_state(session, 0, 0, session.runtime.vocabulary().unit, true);
}

void run_arithmetic_scenarios(CalculatorSession &session)
{
	expect_initial(session);

	session.digit(7);
	expect_state(session, 7, 0, session.runtime.vocabulary().unit, false);
	session.operation(session.calculator.press_add_relation);
	expect_state(session, 7, 7, session.integers.add_relation, true);
	session.digit(3);
	expect_state(session, 3, 7, session.integers.add_relation, false);
	session.equals();
	expect_state(session, 10, 10, session.runtime.vocabulary().unit, true);

	session.clear();
	assert(session.current_state == session.initial_state);
	session.digit(9);
	session.operation(session.calculator.press_subtract_relation);
	session.digit(4);
	session.equals();
	expect_state(session, 5, 5, session.runtime.vocabulary().unit, true);

	session.clear();
	session.digit(6);
	session.operation(session.calculator.press_multiply_relation);
	session.digit(7);
	session.equals();
	expect_state(session, 42, 42, session.runtime.vocabulary().unit, true);

	session.clear();
	session.digit(8);
	session.operation(session.calculator.press_divide_relation);
	session.digit(2);
	session.equals();
	expect_state(session, 4, 4, session.runtime.vocabulary().unit, true);

	session.clear();
	session.digit(1);
	session.operation(session.calculator.press_add_relation);
	session.digit(2);
	session.operation(session.calculator.press_add_relation);
	expect_state(session, 3, 3, session.integers.add_relation, true);
	session.digit(3);
	session.equals();
	expect_state(session, 6, 6, session.runtime.vocabulary().unit, true);

	session.clear();
	assert(session.current_state == session.initial_state);
	expect_initial(session);
}

void verify_invalid_digit_does_not_publish_state(CalculatorSession &session)
{
	session.clear();
	const avm::LinkId before_state = session.current_state;
	const avm::SemanticContextView before_semantic = session.semantic;
	const avm::LinkId invalid_digit = avm::realize_integer(session.store, session.integers, 12);
	const avm::LinkId event = session.realize_event(session.calculator.press_digit_relation, invalid_digit);

	bool rejected = false;
	try
	{
		static_cast<void>(session.runtime.executor().execute_outcome_in_context(event, before_semantic));
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(session.current_state == before_state);
	assert(session.semantic == before_semantic);
	expect_initial(session);
}

void verify_division_by_zero_does_not_publish_state(CalculatorSession &session)
{
	session.clear();
	session.digit(8);
	session.operation(session.calculator.press_divide_relation);
	session.digit(0);
	expect_state(session, 0, 8, session.integers.divide_relation, false);

	const avm::LinkId before_state = session.current_state;
	const avm::SemanticContextView before_semantic = session.semantic;
	const avm::LinkId event =
	    session.realize_event(session.calculator.press_equals_relation, session.runtime.vocabulary().unit);

	bool rejected = false;
	try
	{
		static_cast<void>(session.runtime.executor().execute_outcome_in_context(event, before_semantic));
	}
	catch (const std::domain_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(session.current_state == before_state);
	assert(session.semantic == before_semantic);
	expect_state(session, 0, 8, session.integers.divide_relation, false);
}

void verify_malformed_state_reads_are_non_mutating(CalculatorSession &session)
{
	const avm::LinkId foreign_display = session.store.create_point();
	const avm::LinkId pending_and_flag =
	    session.store.intern(session.runtime.vocabulary().unit, session.runtime.vocabulary().true_value);
	const avm::LinkId payload = session.store.intern(session.integers.zero, pending_and_flag);
	const avm::RelationEntity bad_integer_entity{session.calculator.state_relation, foreign_display, payload};
	const avm::LinkId malformed_integer_state = avm::encode_relation_entity(session.store, bad_integer_entity);
	const std::size_t before_bad_integer_read = session.store.size();

	bool bad_integer_rejected = false;
	try
	{
		static_cast<void>(avm::showcase::decode_calculator_state(session.store, session.runtime.vocabulary(),
		                                                         session.integers, session.calculator,
		                                                         malformed_integer_state));
	}
	catch (const std::runtime_error &)
	{
		bad_integer_rejected = true;
	}
	assert(bad_integer_rejected);
	assert(session.store.size() == before_bad_integer_read);

	const avm::LinkId invalid_pending = session.store.create_point();
	const avm::LinkId invalid_pending_and_flag =
	    session.store.intern(invalid_pending, session.runtime.vocabulary().true_value);
	const avm::LinkId invalid_pending_payload = session.store.intern(session.integers.zero, invalid_pending_and_flag);
	const avm::RelationEntity bad_pending_entity{
	    session.calculator.state_relation,
	    session.integers.zero,
	    invalid_pending_payload,
	};
	const avm::LinkId malformed_pending_state = avm::encode_relation_entity(session.store, bad_pending_entity);
	const std::size_t before_bad_pending_read = session.store.size();

	bool bad_pending_rejected = false;
	try
	{
		static_cast<void>(avm::showcase::decode_calculator_state(session.store, session.runtime.vocabulary(),
		                                                         session.integers, session.calculator,
		                                                         malformed_pending_state));
	}
	catch (const std::invalid_argument &)
	{
		bad_pending_rejected = true;
	}
	assert(bad_pending_rejected);
	assert(session.store.size() == before_bad_pending_read);
}

void verify_mismatched_semantic_state_is_rejected(CalculatorSession &session)
{
	session.clear();
	const avm::LinkId digit = avm::realize_integer(session.store, session.integers, 5);
	const avm::LinkId event = session.realize_event(session.calculator.press_digit_relation, digit);
	const avm::SemanticContextView wrong = session.semantic.with_relation_state(session.store.create_point());

	bool rejected = false;
	try
	{
		static_cast<void>(session.runtime.executor().execute_outcome_in_context(event, wrong));
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
	expect_initial(session);
}

void run_failure_scenarios(CalculatorSession &session)
{
	verify_invalid_digit_does_not_publish_state(session);
	verify_division_by_zero_does_not_publish_state(session);
	verify_malformed_state_reads_are_non_mutating(session);
	verify_mismatched_semantic_state_is_rejected(session);
}

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-showcase-calculator-" + std::to_string(nonce) + ".bin");
}

struct FileCleanup
{
	std::filesystem::path path;

	~FileCleanup()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

struct PersistedCalculatorState
{
	avm::BootstrapVocabulary bootstrap;
	avm::IntegerVocabulary integers;
	avm::showcase::CalculatorVocabulary calculator;
	avm::LinkId state = avm::invalid_link_id;
	avm::SemanticContextFrame semantic_frame{};
};

void verify_persistent_reopen()
{
	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	PersistedCalculatorState saved{};

	{
		avm::PersistentLinkStore store(path);
		CalculatorSession session(store);
		session.digit(7);
		session.operation(session.calculator.press_add_relation);
		session.digit(3);
		session.equals();
		expect_state(session, 10, 10, session.runtime.vocabulary().unit, true);

		saved.bootstrap = session.runtime.vocabulary();
		saved.integers = session.integers;
		saved.calculator = session.calculator;
		saved.state = session.current_state;
		saved.semantic_frame = session.semantic.current();
	}

	{
		avm::PersistentLinkStore reopened(path);
		const std::size_t before_restore = reopened.size();
		avm::BootstrapRuntime runtime(reopened, saved.bootstrap);
		avm::validate_integer_vocabulary(reopened, saved.integers);
		avm::showcase::validate_calculator_vocabulary(reopened, saved.calculator);
		avm::Executor &executor = runtime.executor();
		avm::register_integer_arithmetic(executor, saved.integers);
		avm::showcase::register_calculator_runtime(executor, saved.bootstrap, saved.integers, saved.calculator);
		assert(reopened.size() == before_restore);

		const avm::showcase::CalculatorState restored = avm::showcase::decode_calculator_state(
		    reopened, saved.bootstrap, saved.integers, saved.calculator, saved.state);
		assert(avm::decode_integer(reopened, saved.integers, restored.display) == 10);
		assert(avm::decode_integer(reopened, saved.integers, restored.accumulator) == 10);
		assert(restored.pending_operation == saved.bootstrap.unit);
		assert(restored.entering_new_number == saved.bootstrap.true_value);
		assert(reopened.size() == before_restore);

		avm::SemanticContextView semantic = avm::SemanticContextView::root(saved.semantic_frame);
		const avm::LinkId six = avm::realize_integer(reopened, saved.integers, 6);
		const avm::LinkId digit_event = avm::showcase::realize_calculator_event(
		    reopened, saved.calculator, saved.calculator.press_digit_relation, saved.state, six);
		const avm::ExecutionOutcome digit = runtime.executor().execute_outcome_in_context(digit_event, semantic);
		assert(digit.semantic.role(avm::SemanticContextRole::RelationState) == digit.result);

		const avm::showcase::CalculatorState after_digit = avm::showcase::decode_calculator_state(
		    reopened, saved.bootstrap, saved.integers, saved.calculator, digit.result);
		assert(avm::decode_integer(reopened, saved.integers, after_digit.display) == 6);
		assert(after_digit.pending_operation == saved.bootstrap.unit);
		assert(after_digit.entering_new_number == saved.bootstrap.false_value);
	}
}

void verify_registration_requires_integer_runtime()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::showcase::CalculatorVocabulary calculator = avm::showcase::CalculatorVocabulary::create(store);

	bool rejected = false;
	try
	{
		avm::showcase::register_calculator_runtime(runtime.executor(), runtime.vocabulary(), integers, calculator);
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
}

} // namespace

int main()
{
	verify_registration_requires_integer_runtime();

	{
		avm::InMemoryLinkStore store;
		CalculatorSession session(store);
		run_arithmetic_scenarios(session);
		run_failure_scenarios(session);
	}

	{
		const std::filesystem::path path = temporary_path();
		FileCleanup cleanup{path};
		avm::PersistentLinkStore store(path);
		CalculatorSession session(store);
		run_arithmetic_scenarios(session);
		run_failure_scenarios(session);
	}

	verify_persistent_reopen();
	return 0;
}
