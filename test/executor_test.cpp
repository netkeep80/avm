#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/executor.h"

#include <cassert>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible_v<avm::Executor>);
static_assert(!std::is_copy_assignable_v<avm::Executor>);
static_assert(!std::is_move_constructible_v<avm::Executor>);
static_assert(!std::is_move_assignable_v<avm::Executor>);

static_assert(!std::is_copy_constructible_v<avm::BootstrapRuntime>);
static_assert(!std::is_copy_assignable_v<avm::BootstrapRuntime>);
static_assert(!std::is_move_constructible_v<avm::BootstrapRuntime>);
static_assert(!std::is_move_assignable_v<avm::BootstrapRuntime>);

namespace
{

class FakeExternalEntityProvider final : public avm::ExternalEntityProvider
{
public:
	std::optional<avm::LinkId> lookup(std::string_view name) override
	{
		++calls;
		const auto found = values.find(std::string(name));
		if (found == values.end())
			return std::nullopt;
		return found->second;
	}

	std::map<std::string, avm::LinkId> values;
	std::size_t calls = 0;
};

struct EffectFixture
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime{store};
	avm::TextVocabulary text = avm::TextVocabulary::create(store);
	avm::LinkId lookup_relation = store.create_point();
	avm::LinkId lookup_capability = store.create_point();
	FakeExternalEntityProvider provider;

	avm::LinkId request(std::string_view name)
	{
		const std::vector<std::uint8_t> bytes(name.begin(), name.end());
		const avm::LinkId text_value = avm::realize_text(store, text, bytes);
		const avm::RelationEntity entity{lookup_relation, runtime.vocabulary().unit, text_value};
		return avm::encode_relation_entity(store, entity);
	}

	avm::ExternalEntityLookupEffect effect() const
	{
		return avm::ExternalEntityLookupEffect{
		    lookup_relation,
		    lookup_capability,
		    runtime.vocabulary().unit,
		    text,
		};
	}
};

void test_effect_capability_boundary()
{
	EffectFixture fixture;
	const avm::LinkId existing = fixture.store.create_point();
	fixture.provider.values.emplace("known", existing);
	const avm::LinkId entity = fixture.request("known");
	const std::size_t before = fixture.store.size();
	const avm::EffectCapabilityPolicy policy({fixture.lookup_capability});
	avm::Executor &executor = fixture.runtime.executor();
	const avm::ExternalEntityLookupEffect effect = fixture.effect();
	avm::register_external_entity_lookup_effect(executor, effect, policy, &fixture.provider);
	avm::BoundedExecutionTrace trace(8);
	fixture.runtime.executor().set_observer(&trace);

	assert(fixture.runtime.execute(entity) == existing);
	assert(fixture.store.size() == before);
	assert(fixture.provider.calls == 1);
	assert(trace.size() == 2);
	assert(trace.events()[0].kind == avm::ExecutionEventKind::Enter);
	assert(trace.events()[0].context.relation == fixture.lookup_relation);
	assert(trace.events()[1].kind == avm::ExecutionEventKind::Return);
	assert(trace.events()[1].result == existing);
}

void test_denied_effect_never_calls_provider()
{
	EffectFixture fixture;
	const avm::LinkId entity = fixture.request("known");
	const std::size_t before = fixture.store.size();
	const avm::EffectCapabilityPolicy policy;
	avm::Executor &executor = fixture.runtime.executor();
	const avm::ExternalEntityLookupEffect effect = fixture.effect();
	avm::register_external_entity_lookup_effect(executor, effect, policy, &fixture.provider);
	avm::BoundedExecutionTrace trace(8);
	fixture.runtime.executor().set_observer(&trace);

	bool denied = false;
	try
	{
		fixture.runtime.execute(entity);
	}
	catch (const avm::EffectCapabilityDenied &)
	{
		denied = true;
	}

	assert(denied);
	assert(fixture.provider.calls == 0);
	assert(fixture.store.size() == before);
	assert(trace.size() == 2);
	assert(trace.events()[1].kind == avm::ExecutionEventKind::Fail);
	assert(trace.events()[1].failure_phase == avm::ExecutionFailurePhase::Handler);
}

void test_external_lookup_never_realizes_provider_data()
{
	EffectFixture fixture;
	const avm::LinkId miss_entity = fixture.request("missing");
	const std::size_t before_miss = fixture.store.size();
	const avm::EffectCapabilityPolicy policy({fixture.lookup_capability});
	avm::Executor &executor = fixture.runtime.executor();
	const avm::ExternalEntityLookupEffect effect = fixture.effect();
	avm::register_external_entity_lookup_effect(executor, effect, policy, &fixture.provider);

	bool missed = false;
	try
	{
		fixture.runtime.execute(miss_entity);
	}
	catch (const avm::EffectLookupMiss &)
	{
		missed = true;
	}
	assert(missed);
	assert(fixture.store.size() == before_miss);

	EffectFixture unknown_fixture;
	unknown_fixture.provider.values.emplace("foreign", 999999);
	const avm::LinkId unknown_entity = unknown_fixture.request("foreign");
	const std::size_t before_unknown = unknown_fixture.store.size();
	const avm::EffectCapabilityPolicy unknown_policy({unknown_fixture.lookup_capability});
	avm::Executor &unknown_executor = unknown_fixture.runtime.executor();
	const avm::ExternalEntityLookupEffect unknown_effect = unknown_fixture.effect();
	avm::ExternalEntityProvider *unknown_provider = &unknown_fixture.provider;
	avm::register_external_entity_lookup_effect(unknown_executor, unknown_effect, unknown_policy, unknown_provider);

	bool rejected = false;
	try
	{
		unknown_fixture.runtime.execute(unknown_entity);
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(unknown_fixture.store.size() == before_unknown);
}

void test_pure_program_needs_no_effect_provider()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::LinkId value = store.create_point();
	const avm::LinkId literal = runtime.builder().literal(value);
	assert(runtime.execute(literal) == value);
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;

	const avm::LinkId identity_relation = store.create_point();
	const avm::LinkId subject_relation = store.create_point();
	const avm::LinkId unknown_relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();

	avm::Executor executor(store);

	executor.register_native(identity_relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &) { return context.object; });

	executor.register_native(subject_relation,
	                         [subject](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         assert(context.subject == subject);
		                         return context.subject;
	                         });

	const avm::LinkId identity_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{identity_relation, subject, object});

	assert(executor.execute(identity_entity) == object);

	std::optional<avm::ExecutionContext> captured_context;
	const avm::LinkId parent_relation = store.create_point();
	executor.register_native(parent_relation,
	                         [&captured_context](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         captured_context = context;
		                         return context.object;
	                         });

	const avm::LinkId parent_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{parent_relation, subject, object});

	assert(executor.execute(parent_entity, identity_entity) == object);
	assert(captured_context.has_value());
	assert(captured_context->entity == parent_entity);
	assert(captured_context->relation == parent_relation);
	assert(captured_context->subject == subject);
	assert(captured_context->object == object);
	assert(captured_context->parent == identity_entity);
	assert(!captured_context->frame.has_value());

	const avm::LinkId frame = store.create_point();
	assert(executor.execute(parent_entity, identity_entity, frame) == object);
	assert(captured_context.has_value());
	assert(captured_context->parent == identity_entity);
	assert(captured_context->frame == frame);

	const avm::LinkId subject_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{subject_relation, subject, object});
	assert(executor.execute(subject_entity) == subject);

	const avm::LinkId unknown_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{unknown_relation, subject, object});
	const std::size_t before_unknown_execute = store.size();

	bool unknown_rejected = false;
	try
	{
		static_cast<void>(executor.execute(unknown_entity));
	}
	catch (const std::runtime_error &)
	{
		unknown_rejected = true;
	}
	assert(unknown_rejected);
	assert(store.size() == before_unknown_execute);

	bool duplicate_handler_rejected = false;
	try
	{
		executor.register_native(identity_relation,
		                         [](const avm::ExecutionContext &context, avm::Executor &) { return context.object; });
	}
	catch (const std::logic_error &)
	{
		duplicate_handler_rejected = true;
	}
	assert(duplicate_handler_rejected);

	const avm::LinkId invalid_result_relation = store.create_point();
	executor.register_native(invalid_result_relation, [](const avm::ExecutionContext &, avm::Executor &)
	                         { return static_cast<avm::LinkId>(999999); });
	const avm::LinkId invalid_result_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{invalid_result_relation, subject, object});

	bool invalid_result_rejected = false;
	try
	{
		static_cast<void>(executor.execute(invalid_result_entity));
	}
	catch (const std::runtime_error &)
	{
		invalid_result_rejected = true;
	}
	assert(invalid_result_rejected);

	test_effect_capability_boundary();
	test_denied_effect_never_calls_provider();
	test_external_lookup_never_realizes_provider_data();
	test_pure_program_needs_no_effect_provider();
	return 0;
}
