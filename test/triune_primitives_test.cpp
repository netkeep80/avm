#include "avm/bootstrap_runtime.h"
#include "avm/triune_primitives.h"

#include <cassert>
#include <stdexcept>
#include <vector>

namespace
{

class RecordingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override { events.push_back(event); }

	std::vector<avm::ExecutionEvent> events;
};

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::DirectTriuneVocabulary direct = avm::DirectTriuneVocabulary::create(store);
	avm::register_direct_triune_primitives(runtime.executor(), direct);

	const avm::LinkId subject_a = store.create_point();
	const avm::LinkId subject_b = store.create_point();
	const avm::LinkId object = store.create_point();

	const avm::LinkId subject_entity_a =
	    avm::encode_relation_entity(store, avm::RelationEntity{direct.subject_value_relation, subject_a, object});
	const avm::LinkId subject_entity_b =
	    avm::encode_relation_entity(store, avm::RelationEntity{direct.subject_value_relation, subject_b, object});

	const std::size_t before_subject_a = store.size();
	assert(runtime.execute(subject_entity_a) == subject_a);
	assert(store.size() == before_subject_a);

	const std::size_t before_subject_b = store.size();
	assert(runtime.execute(subject_entity_b) == subject_b);
	assert(store.size() == before_subject_b);
	assert(subject_a != subject_b);

	// The bootstrap unit identity is still a legitimate direct-triune subject.
	const avm::LinkId unit_subject_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{direct.subject_value_relation, runtime.vocabulary().unit, object});
	const std::size_t before_unit_subject = store.size();
	assert(runtime.execute(unit_subject_entity) == runtime.vocabulary().unit);
	assert(store.size() == before_unit_subject);

	const avm::LinkId target_begin = store.create_point();
	const avm::LinkId target_end = store.create_point();
	const avm::LinkId receiver = store.create_point();
	assert(!store.find(target_begin, target_end).has_value());

	const avm::LinkId target_descriptor = avm::materialize_pair_target(store, direct, target_begin, target_end);
	assert(!store.find(target_begin, target_end).has_value());
	assert(avm::decode_pair_target(store, direct, target_descriptor) ==
	       (avm::PairTarget{target_descriptor, target_begin, target_end}));

	const avm::LinkId find_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{direct.pair_find_relation, receiver, target_descriptor});
	assert(!store.find(target_begin, target_end).has_value());

	const std::size_t before_find_miss = store.size();
	bool find_miss_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(find_entity));
	}
	catch (const std::runtime_error &)
	{
		find_miss_rejected = true;
	}
	assert(find_miss_rejected);
	assert(store.size() == before_find_miss);
	assert(!store.find(target_begin, target_end).has_value());

	const avm::LinkId realize_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{direct.pair_realize_relation, receiver, target_descriptor});
	assert(!store.find(target_begin, target_end).has_value());

	const std::size_t before_realize = store.size();
	const avm::LinkId realized = runtime.execute(realize_entity);
	assert(store.size() == before_realize + 1);
	assert(store.find(target_begin, target_end) == realized);
	assert(store.get(realized) == (avm::Link{target_begin, target_end}));

	const std::size_t before_repeat_realize = store.size();
	assert(runtime.execute(realize_entity) == realized);
	assert(store.size() == before_repeat_realize);

	const std::size_t before_find_hit = store.size();
	assert(runtime.execute(find_entity) == realized);
	assert(store.size() == before_find_hit);

	RecordingObserver observer;
	runtime.executor().set_observer(&observer);
	const avm::LinkId observed_result = runtime.execute(subject_entity_a);
	assert(observed_result == subject_a);
	assert(observer.events.size() == 2);
	assert(observer.events[0].kind == avm::ExecutionEventKind::Enter);
	assert(observer.events[0].context.entity == subject_entity_a);
	assert(observer.events[0].context.relation == direct.subject_value_relation);
	assert(observer.events[0].context.subject == subject_a);
	assert(observer.events[0].context.object == object);
	assert(!observer.events[0].result.has_value());
	assert(observer.events[1].kind == avm::ExecutionEventKind::Return);
	assert(observer.events[1].context == observer.events[0].context);
	assert(observer.events[1].result == subject_a);

	// Direct primitives coexist with the existing bootstrap expression path on
	// the same Executor. No sentinel-based relation overloading is introduced.
	runtime.executor().set_observer(nullptr);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId true_expression = builder.literal(runtime.vocabulary().true_value);
	const avm::LinkId not_true = builder.logical_not(true_expression);
	assert(runtime.execute(not_true) == runtime.vocabulary().false_value);

	return 0;
}
