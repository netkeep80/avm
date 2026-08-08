#include "avm/bootstrap_runtime.h"
#include "avm/executor.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

class RecordingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override { events.push_back(event); }

	void clear() { events.clear(); }

	std::vector<avm::ExecutionEvent> events;
};

class ThrowingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &) override
	{
		++calls;
		throw std::runtime_error("observer failure");
	}

	std::size_t calls = 0;
};

void verify_success_events_and_detach()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});

	RecordingObserver observer;
	avm::Executor executor(store, &observer);
	executor.register_native(relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &) { return context.object; });

	const std::size_t size_before = store.size();
	assert(executor.execute(entity) == object);
	assert(store.size() == size_before);
	assert(observer.events.size() == 2);
	assert(observer.events[0].kind == avm::ExecutionEventKind::Enter);
	const avm::ExecutionContext expected_context{entity, relation, subject, object, std::nullopt, std::nullopt};
	assert(observer.events[0].context == expected_context);
	assert(!observer.events[0].result.has_value());
	assert(observer.events[1].kind == avm::ExecutionEventKind::Return);
	assert(observer.events[1].context == observer.events[0].context);
	assert(observer.events[1].result == object);

	executor.set_observer(nullptr);
	assert(executor.execute(entity) == object);
	assert(store.size() == size_before);
	assert(observer.events.size() == 2);
}

void verify_nested_event_order_is_deterministic()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();

	const avm::LinkId true_literal = builder.literal(vocabulary.true_value);
	const avm::LinkId false_literal = builder.literal(vocabulary.false_value);
	const avm::LinkId negated = builder.logical_not(false_literal);
	const avm::LinkId root = builder.logical_and(true_literal, negated);

	RecordingObserver observer;
	runtime.executor().set_observer(&observer);
	const std::size_t size_before = store.size();
	assert(runtime.execute(root) == vocabulary.true_value);
	assert(store.size() == size_before);

	const std::vector<avm::ExecutionEvent> first = observer.events;
	assert(first.size() == 8);
	assert(first[0].kind == avm::ExecutionEventKind::Enter && first[0].context.entity == root);
	assert(first[1].kind == avm::ExecutionEventKind::Enter && first[1].context.entity == true_literal);
	assert(first[2].kind == avm::ExecutionEventKind::Return && first[2].context.entity == true_literal);
	assert(first[3].kind == avm::ExecutionEventKind::Enter && first[3].context.entity == negated);
	assert(first[4].kind == avm::ExecutionEventKind::Enter && first[4].context.entity == false_literal);
	assert(first[5].kind == avm::ExecutionEventKind::Return && first[5].context.entity == false_literal);
	assert(first[6].kind == avm::ExecutionEventKind::Return && first[6].context.entity == negated);
	assert(first[7].kind == avm::ExecutionEventKind::Return && first[7].context.entity == root);
	assert(first[1].context.parent == root);
	assert(first[3].context.parent == root);
	assert(first[4].context.parent == negated);

	observer.clear();
	assert(runtime.execute(root) == vocabulary.true_value);
	assert(store.size() == size_before);
	assert(observer.events == first);
}

void verify_function_call_trace_converges_with_link_native_frame()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();

	const avm::LinkId formal = store.create_point();
	const avm::LinkId handle = builder.create_function_handle();
	builder.define_function(handle, {formal}, builder.parameter(formal));
	const avm::LinkId call = builder.call(handle, {builder.literal(vocabulary.true_value)});

	RecordingObserver observer;
	runtime.executor().set_observer(&observer);
	assert(runtime.execute(call) == vocabulary.true_value);
	const std::size_t after_first = store.size();
	const std::vector<avm::ExecutionEvent> first = observer.events;
	assert(!first.empty());

	bool saw_frame = false;
	for (const avm::ExecutionEvent &event : first)
	{
		if (event.context.frame.has_value())
			saw_frame = true;
	}
	assert(saw_frame);

	observer.clear();
	assert(runtime.execute(call) == vocabulary.true_value);
	assert(store.size() == after_first);
	assert(observer.events == first);
}

void verify_failure_event_preserves_original_exception()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});

	RecordingObserver observer;
	avm::Executor executor(store, &observer);
	executor.register_native(relation, [](const avm::ExecutionContext &, avm::Executor &) -> avm::LinkId
	                         { throw std::logic_error("program failure"); });

	bool rejected = false;
	try
	{
		static_cast<void>(executor.execute(entity));
	}
	catch (const std::logic_error &error)
	{
		rejected = true;
		assert(std::string(error.what()) == "program failure");
	}
	assert(rejected);
	assert(observer.events.size() == 2);
	assert(observer.events[0].kind == avm::ExecutionEventKind::Enter);
	assert(observer.events[1].kind == avm::ExecutionEventKind::Fail);
	assert(observer.events[1].context == observer.events[0].context);
	assert(!observer.events[1].result.has_value());
}

void verify_observer_failure_cannot_control_execution()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});

	ThrowingObserver observer;
	avm::Executor executor(store, &observer);
	executor.register_native(relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &) { return context.object; });

	assert(executor.execute(entity) == object);
	assert(observer.calls == 2);
}

void verify_pre_context_failure_emits_no_event()
{
	avm::InMemoryLinkStore store;
	RecordingObserver observer;
	avm::Executor executor(store, &observer);

	bool rejected = false;
	try
	{
		static_cast<void>(executor.execute(999999));
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(observer.events.empty());
}

} // namespace

int main()
{
	verify_success_events_and_detach();
	verify_nested_event_order_is_deterministic();
	verify_function_call_trace_converges_with_link_native_frame();
	verify_failure_event_preserves_original_exception();
	verify_observer_failure_cannot_control_execution();
	verify_pre_context_failure_emits_no_event();
	return 0;
}
