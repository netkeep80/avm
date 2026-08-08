#include <avm/avm.h>

int main()
{
	static_assert(avm::version_major == 1);
	static_assert(avm::version_minor == 2);
	static_assert(avm::version_patch == 0);
	static_assert(avm::version_string == "1.2.0");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	const avm::LinkId result = runtime.execute(expression);
	if (result != runtime.vocabulary().true_value)
		return 1;

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});

	const auto matches =
	    avm::query_relation_entities(store, {.relation = relation, .subject = subject, .object = object});
	if (matches.size() != 1 || matches.front().entity_id != entity ||
	    matches.front().entity != avm::RelationEntity{relation, subject, object})
		return 2;

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId pair = store.intern(begin, end);
	const avm::LinkId begin_expression = builder.link_begin(builder.literal(pair));
	const std::size_t size_before = store.size();
	if (runtime.execute(begin_expression) != begin || store.size() != size_before)
		return 3;

	return 0;
}
