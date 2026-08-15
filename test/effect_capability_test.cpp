#include "avm/bootstrap_runtime.h"
#include "avm/effect_capability.h"
#include "avm/execution_trace.h"

#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

struct Fixture
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
		return avm::encode_relation_entity(
		    store, avm::RelationEntity{lookup_relation, runtime.vocabulary().unit, text_value});
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

void test_allowed_lookup_returns_existing_identity_without_materialization()
{
	Fixture fixture;
	const avm::LinkId existing = fixture.store.create_point();
	fixture.provider.values.emplace("known", existing);
	const avm::LinkId entity = fixture.request("known");
	const std::size_t before = fixture.store.size();

	const avm::EffectCapabilityPolicy policy({fixture.lookup_capability});
	avm::register_external_entity_lookup_effect(fixture.runtime.executor(), fixture.effect(), policy, &fixture.provider);
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

void test_denied_lookup_never_calls_provider()
{
	Fixture fixture;
	const avm::LinkId entity = fixture.request("known");
	const std::size_t before = fixture.store.size();
	const avm::EffectCapabilityPolicy policy;
	avm::register_external_entity_lookup_effect(fixture.runtime.executor(), fixture.effect(), policy, &fixture.provider);
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
	assert(trace.events()[0].kind == avm::ExecutionEventKind::Enter);
	assert(trace.events()[1].kind == avm::ExecutionEventKind::Fail);
	assert(trace.events()[1].failure_phase == avm::ExecutionFailurePhase::Handler);
}

void test_provider_miss_and_unknown_identity_do_not_realize_links()
{
	Fixture fixture;
	const avm::LinkId miss_entity = fixture.request("missing");
	const std::size_t before_miss = fixture.store.size();
	const avm::EffectCapabilityPolicy policy({fixture.lookup_capability});
	avm::register_external_entity_lookup_effect(fixture.runtime.executor(), fixture.effect(), policy, &fixture.provider);

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

	Fixture unknown_fixture;
	unknown_fixture.provider.values.emplace("foreign", 999999);
	const avm::LinkId unknown_entity = unknown_fixture.request("foreign");
	const std::size_t before_unknown = unknown_fixture.store.size();
	const avm::EffectCapabilityPolicy unknown_policy({unknown_fixture.lookup_capability});
	avm::register_external_entity_lookup_effect(
	    unknown_fixture.runtime.executor(), unknown_fixture.effect(), unknown_policy, &unknown_fixture.provider);

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
	test_allowed_lookup_returns_existing_identity_without_materialization();
	test_denied_lookup_never_calls_provider();
	test_provider_miss_and_unknown_identity_do_not_realize_links();
	test_pure_program_needs_no_effect_provider();
}
