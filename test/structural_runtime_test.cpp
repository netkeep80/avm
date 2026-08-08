#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <stdexcept>
#include <string_view>

namespace
{

void assert_execution_does_not_mutate(avm::BootstrapRuntime &runtime, avm::InMemoryLinkStore &store,
                                      avm::LinkId expression, avm::LinkId expected)
{
	const std::size_t size_before = store.size();
	assert(runtime.execute(expression) == expected);
	assert(store.size() == size_before);
}

void verify_begin_and_end()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId pair = store.intern(begin, end);

	const avm::LinkId begin_expression = builder.link_begin(builder.literal(pair));
	const avm::LinkId end_expression = builder.link_end(builder.literal(pair));

	assert_execution_does_not_mutate(runtime, store, begin_expression, begin);
	assert_execution_does_not_mutate(runtime, store, end_expression, end);
}

void verify_point_projections_are_self()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId point = store.create_point();
	const avm::LinkId begin_expression = builder.link_begin(builder.literal(point));
	const avm::LinkId end_expression = builder.link_end(builder.literal(point));

	assert_execution_does_not_mutate(runtime, store, begin_expression, point);
	assert_execution_does_not_mutate(runtime, store, end_expression, point);
}

void verify_nested_structural_composition()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId a = store.create_point();
	const avm::LinkId b = store.create_point();
	const avm::LinkId c = store.create_point();
	const avm::LinkId inner = store.intern(a, b);
	const avm::LinkId outer = store.intern(c, inner);

	const avm::LinkId expression = builder.link_begin(builder.link_end(builder.literal(outer)));
	assert_execution_does_not_mutate(runtime, store, expression, a);
}

void verify_identity_comparison()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId left = store.create_point();
	const avm::LinkId right = store.create_point();

	const avm::LinkId same = builder.identity_equal(builder.literal(left), builder.literal(left));
	const avm::LinkId different = builder.identity_equal(builder.literal(left), builder.literal(right));

	assert_execution_does_not_mutate(runtime, store, same, runtime.vocabulary().true_value);
	assert_execution_does_not_mutate(runtime, store, different, runtime.vocabulary().false_value);
}

void verify_link_existence_is_observational()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId missing_end = store.create_point();
	static_cast<void>(store.intern(begin, end));
	assert(!store.find(begin, missing_end).has_value());

	const avm::LinkId existing = builder.link_exists(builder.literal(begin), builder.literal(end));
	const avm::LinkId missing = builder.link_exists(builder.literal(begin), builder.literal(missing_end));

	assert_execution_does_not_mutate(runtime, store, existing, runtime.vocabulary().true_value);
	const std::size_t size_before_missing = store.size();
	assert(runtime.execute(missing) == runtime.vocabulary().false_value);
	assert(store.size() == size_before_missing);
	assert(!store.find(begin, missing_end).has_value());
}

void verify_pair_intern_is_explicit_and_idempotent()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	assert(!store.find(begin, end).has_value());

	const avm::LinkId begin_literal = builder.literal(begin);
	const avm::LinkId end_literal = builder.literal(end);
	const avm::LinkId exists = builder.link_exists(begin_literal, end_literal);
	const avm::LinkId materialize = builder.pair_intern(begin_literal, end_literal);
	const avm::LinkId projected_begin = builder.link_begin(materialize);
	const avm::LinkId projected_end = builder.link_end(materialize);

	assert_execution_does_not_mutate(runtime, store, exists, runtime.vocabulary().false_value);
	assert(!store.find(begin, end).has_value());

	const std::size_t before_materialize = store.size();
	const avm::LinkId pair = runtime.execute(materialize);
	assert(store.size() == before_materialize + 1);
	assert(store.find(begin, end) == pair);

	const std::size_t after_materialize = store.size();
	assert(runtime.execute(materialize) == pair);
	assert(store.size() == after_materialize);
	assert_execution_does_not_mutate(runtime, store, exists, runtime.vocabulary().true_value);
	assert_execution_does_not_mutate(runtime, store, projected_begin, begin);
	assert_execution_does_not_mutate(runtime, store, projected_end, end);
}

void verify_pair_intern_returns_preexisting_canonical_identity()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId existing = store.intern(begin, end);
	const avm::LinkId expression = builder.pair_intern(builder.literal(begin), builder.literal(end));

	const std::size_t before = store.size();
	assert(runtime.execute(expression) == existing);
	assert(store.size() == before);
}

void verify_pair_intern_of_nonpoint_self_pair_is_new_identity()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId left = store.create_point();
	const avm::LinkId right = store.create_point();
	const avm::LinkId nonpoint = store.intern(left, right);
	const avm::Link nonpoint_as_self{nonpoint, nonpoint};
	assert(store.get(nonpoint) != nonpoint_as_self);

	const avm::LinkId expression = builder.pair_intern(builder.literal(nonpoint), builder.literal(nonpoint));
	const avm::LinkId self_pair = runtime.execute(expression);
	const avm::Link expected{nonpoint, nonpoint};
	assert(self_pair != nonpoint);
	assert(store.get(self_pair) == expected);
	assert(store.find(nonpoint, nonpoint) == self_pair);
}

void verify_nil_nil_exists_without_missing_sentinel_ambiguity()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId nil = runtime.vocabulary().nil;
	assert(store.find(nil, nil) == nil);

	const avm::LinkId expression = builder.link_exists(builder.literal(nil), builder.literal(nil));
	assert_execution_does_not_mutate(runtime, store, expression, runtime.vocabulary().true_value);
}

void verify_arity_validation()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();

	const avm::LinkId value = store.create_point();
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId first = builder.literal(value);
	const avm::LinkId second = builder.literal(value);
	const avm::LinkId arguments = avm::encode_link_list(store, vocabulary.nil, {first, second});
	const avm::LinkId malformed =
	    avm::encode_relation_entity(store, avm::RelationEntity{vocabulary.begin_relation, vocabulary.unit, arguments});

	bool thrown = false;
	try
	{
		static_cast<void>(runtime.execute(malformed));
	}
	catch (const std::runtime_error &error)
	{
		thrown = true;
		assert(std::string_view(error.what()).find("argument count") != std::string_view::npos);
	}
	assert(thrown);
}

} // namespace

int main()
{
	verify_begin_and_end();
	verify_point_projections_are_self();
	verify_nested_structural_composition();
	verify_identity_comparison();
	verify_link_existence_is_observational();
	verify_pair_intern_is_explicit_and_idempotent();
	verify_pair_intern_returns_preexisting_canonical_identity();
	verify_pair_intern_of_nonpoint_self_pair_is_new_identity();
	verify_nil_nil_exists_without_missing_sentinel_ambiguity();
	verify_arity_validation();
	return 0;
}
