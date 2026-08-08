#include <avm/avm.h>

#include <array>
#include <cstddef>

namespace
{

class FixedObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override
	{
		if (count < events.size())
			events[count++] = event;
	}

	std::array<avm::ExecutionEvent, 4> events{};
	std::size_t count = 0;
};

} // namespace

int main()
{
	static_assert(avm::version_major == 1);
	static_assert(avm::version_minor == 3);
	static_assert(avm::version_patch == 0);
	static_assert(avm::version_string == "1.3.0");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	FixedObserver observer;
	runtime.executor().set_observer(&observer);
	const avm::LinkId result = runtime.execute(expression);
	if (result != runtime.vocabulary().true_value)
		return 1;
	if (observer.count != 4 || observer.events[0].kind != avm::ExecutionEventKind::Enter ||
	    observer.events[0].context.entity != expression || observer.events[3].kind != avm::ExecutionEventKind::Return ||
	    observer.events[3].context.entity != expression || observer.events[3].result != result)
		return 2;
	runtime.executor().set_observer(nullptr);

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});

	const auto matches =
	    avm::query_relation_entities(store, {.relation = relation, .subject = subject, .object = object});
	if (matches.size() != 1 || matches.front().entity_id != entity ||
	    matches.front().entity != avm::RelationEntity{relation, subject, object})
		return 3;

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	if (store.find(begin, end).has_value())
		return 4;

	const avm::LinkId begin_literal = builder.literal(begin);
	const avm::LinkId end_literal = builder.literal(end);
	const avm::LinkId materialize = builder.pair_intern(begin_literal, end_literal);
	const avm::LinkId begin_expression = builder.link_begin(materialize);
	const std::size_t size_before = store.size();

	const avm::LinkId pair = runtime.execute(materialize);
	if (store.size() != size_before + 1 || store.find(begin, end) != pair)
		return 5;

	const std::size_t size_after = store.size();
	if (runtime.execute(materialize) != pair || runtime.execute(begin_expression) != begin ||
	    store.size() != size_after)
		return 6;

	const avm::LinkId formal = store.create_point();
	const avm::LinkId is_self_link = builder.create_function_handle();
	const avm::LinkId parameter = builder.parameter(formal);
	builder.define_function(is_self_link, {formal},
	                        builder.identity_equal(builder.link_begin(parameter), builder.link_end(parameter)));
	if (runtime.executor().has_native(is_self_link))
		return 7;

	const avm::LinkId point = store.create_point();
	if (runtime.execute(builder.call(is_self_link, {builder.literal(point)})) != runtime.vocabulary().true_value)
		return 8;
	if (runtime.execute(builder.call(is_self_link, {builder.literal(pair)})) != runtime.vocabulary().false_value)
		return 9;

	return 0;
}
