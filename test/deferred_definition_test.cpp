#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <stdexcept>

int main()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store, 16);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &v = runtime.vocabulary();

	const avm::LinkId t = builder.literal(v.true_value);
	const avm::LinkId f = builder.literal(v.false_value);
	const avm::LinkId formal = store.create_point();
	const avm::LinkId handle = builder.create_function_handle();
	const avm::LinkId body = builder.parameter(formal);
	const avm::LinkId deferred = builder.deferred_function_definition(handle, {formal}, body);

	assert(!avm::find_function_definition(store, v, handle).has_value());
	const avm::DeferredFunctionDefinition decoded = avm::decode_deferred_function_definition(store, v, deferred);
	assert(decoded.handle == handle);
	assert(decoded.parameters == (std::vector<avm::LinkId>{formal}));
	assert(decoded.body == body);

	const avm::LinkId call = builder.call(handle, {t});
	bool call_before_definition_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(call));
	}
	catch (const std::runtime_error &)
	{
		call_before_definition_rejected = true;
	}
	assert(call_before_definition_rejected);

	assert(runtime.execute(deferred) == v.nil);
	const auto materialized = avm::find_function_definition(store, v, handle);
	assert(materialized.has_value());
	assert(materialized->handle == handle);
	assert(materialized->parameters == (std::vector<avm::LinkId>{formal}));
	assert(materialized->body == body);
	assert(runtime.execute(call) == v.true_value);

	const std::size_t before_repeat = store.size();
	assert(runtime.execute(deferred) == v.nil);
	assert(store.size() == before_repeat);

	const avm::LinkId conflicting = builder.deferred_function_definition(handle, {formal}, f);
	bool conflict_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(conflicting));
	}
	catch (const std::logic_error &)
	{
		conflict_rejected = true;
	}
	assert(conflict_rejected);

	avm::InMemoryLinkStore ordered_store;
	avm::BootstrapRuntime ordered_runtime(ordered_store, 16);
	avm::ProgramBuilder ordered_builder = ordered_runtime.builder();
	const avm::BootstrapVocabulary &ordered_v = ordered_runtime.vocabulary();
	const avm::LinkId ordered_t = ordered_builder.literal(ordered_v.true_value);
	const avm::LinkId ordered_formal = ordered_store.create_point();
	const avm::LinkId ordered_handle = ordered_builder.create_function_handle();
	const avm::LinkId ordered_body = ordered_builder.parameter(ordered_formal);
	const avm::LinkId ordered_def =
	    ordered_builder.deferred_function_definition(ordered_handle, {ordered_formal}, ordered_body);
	const avm::LinkId ordered_call = ordered_builder.call(ordered_handle, {ordered_t});
	assert(ordered_runtime.execute(ordered_builder.sequence({ordered_def, ordered_call})) == ordered_v.true_value);

	avm::InMemoryLinkStore reversed_store;
	avm::BootstrapRuntime reversed_runtime(reversed_store, 16);
	avm::ProgramBuilder reversed_builder = reversed_runtime.builder();
	const avm::BootstrapVocabulary &reversed_v = reversed_runtime.vocabulary();
	const avm::LinkId reversed_t = reversed_builder.literal(reversed_v.true_value);
	const avm::LinkId reversed_formal = reversed_store.create_point();
	const avm::LinkId reversed_handle = reversed_builder.create_function_handle();
	const avm::LinkId reversed_body = reversed_builder.parameter(reversed_formal);
	const avm::LinkId reversed_def =
	    reversed_builder.deferred_function_definition(reversed_handle, {reversed_formal}, reversed_body);
	const avm::LinkId reversed_call = reversed_builder.call(reversed_handle, {reversed_t});
	bool reversed_sequence_rejected = false;
	try
	{
		static_cast<void>(reversed_runtime.execute(reversed_builder.sequence({reversed_call, reversed_def})));
	}
	catch (const std::runtime_error &)
	{
		reversed_sequence_rejected = true;
	}
	assert(reversed_sequence_rejected);
	assert(!avm::find_function_definition(reversed_store, reversed_v, reversed_handle).has_value());

	avm::InMemoryLinkStore recursive_store;
	avm::BootstrapRuntime recursive_runtime(recursive_store, 16);
	avm::ProgramBuilder recursive_builder = recursive_runtime.builder();
	const avm::BootstrapVocabulary &recursive_v = recursive_runtime.vocabulary();
	const avm::LinkId recursive_t = recursive_builder.literal(recursive_v.true_value);
	const avm::LinkId recursive_f = recursive_builder.literal(recursive_v.false_value);
	const avm::LinkId flag = recursive_store.create_point();
	const avm::LinkId recursive_handle = recursive_builder.create_function_handle();
	const avm::LinkId recursive_body = recursive_builder.conditional(
	    recursive_builder.parameter(flag), recursive_builder.call(recursive_handle, {recursive_f}), recursive_t);
	const avm::LinkId recursive_def =
	    recursive_builder.deferred_function_definition(recursive_handle, {flag}, recursive_body);
	const avm::LinkId recursive_call = recursive_builder.call(recursive_handle, {recursive_t});
	assert(recursive_runtime.execute(recursive_builder.sequence({recursive_def, recursive_call})) ==
	       recursive_v.true_value);

	const avm::LinkId malformed_payload = store.create_point();
	const avm::LinkId malformed = avm::encode_relation_entity(
	    store, avm::RelationEntity{v.function_relation, v.unit, malformed_payload});
	bool malformed_rejected = false;
	try
	{
		static_cast<void>(avm::decode_deferred_function_definition(store, v, malformed));
	}
	catch (const std::runtime_error &)
	{
		malformed_rejected = true;
	}
	assert(malformed_rejected);

	return 0;
}
