#include "avm/bootstrap_runtime.h"
#include "avm/persistent_link_store.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

namespace
{

struct StructuralLibrary
{
	avm::LinkId is_self_link;
	avm::LinkId pair_matches;
	avm::LinkId is_self_via_pair_matches;
};

StructuralLibrary define_structural_library(avm::LinkStore &store, avm::BootstrapRuntime &runtime)
{
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId self_x = store.create_point();
	const avm::LinkId is_self_link = builder.create_function_handle();
	const avm::LinkId self_parameter = builder.parameter(self_x);
	const avm::LinkId self_body =
	    builder.identity_equal(builder.link_begin(self_parameter), builder.link_end(self_parameter));
	builder.define_function(is_self_link, {self_x}, self_body);

	const avm::LinkId match_x = store.create_point();
	const avm::LinkId match_begin = store.create_point();
	const avm::LinkId match_end = store.create_point();
	const avm::LinkId pair_matches = builder.create_function_handle();
	const avm::LinkId begin_matches =
	    builder.identity_equal(builder.link_begin(builder.parameter(match_x)), builder.parameter(match_begin));
	const avm::LinkId end_matches =
	    builder.identity_equal(builder.link_end(builder.parameter(match_x)), builder.parameter(match_end));
	builder.define_function(pair_matches, {match_x, match_begin, match_end},
	                        builder.logical_and(begin_matches, end_matches));

	const avm::LinkId nested_x = store.create_point();
	const avm::LinkId is_self_via_pair_matches = builder.create_function_handle();
	const avm::LinkId nested_parameter = builder.parameter(nested_x);
	const avm::LinkId nested_begin = builder.link_begin(nested_parameter);
	const avm::LinkId nested_body = builder.call(pair_matches, {nested_parameter, nested_begin, nested_begin});
	builder.define_function(is_self_via_pair_matches, {nested_x}, nested_body);

	return StructuralLibrary{is_self_link, pair_matches, is_self_via_pair_matches};
}

void assert_composed_definitions_are_not_native(avm::BootstrapRuntime &runtime, const StructuralLibrary &library)
{
	assert(!runtime.executor().has_native(library.is_self_link));
	assert(!runtime.executor().has_native(library.pair_matches));
	assert(!runtime.executor().has_native(library.is_self_via_pair_matches));
}

void assert_definition_roots_use_existing_relations(const avm::LinkStore &store,
                                                    const avm::BootstrapVocabulary &vocabulary,
                                                    const StructuralLibrary &library)
{
	const auto self_definition = avm::find_function_definition(store, vocabulary, library.is_self_link);
	const auto match_definition = avm::find_function_definition(store, vocabulary, library.pair_matches);
	const auto nested_definition =
	    avm::find_function_definition(store, vocabulary, library.is_self_via_pair_matches);
	assert(self_definition.has_value());
	assert(match_definition.has_value());
	assert(nested_definition.has_value());

	assert(avm::decode_relation_entity(store, self_definition->body).relation == vocabulary.same_relation);
	assert(avm::decode_relation_entity(store, match_definition->body).relation == vocabulary.and_relation);
	assert(avm::decode_relation_entity(store, nested_definition->body).relation == vocabulary.call_relation);
}

void verify_composed_structural_library_behavior()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();
	const StructuralLibrary library = define_structural_library(store, runtime);

	assert_composed_definitions_are_not_native(runtime, library);
	assert_definition_roots_use_existing_relations(store, vocabulary, library);

	const avm::LinkId point = store.create_point();
	const avm::LinkId left = store.create_point();
	const avm::LinkId right = store.create_point();
	const avm::LinkId nonpoint = store.intern(left, right);
	const avm::LinkId self_pair = store.intern(nonpoint, nonpoint);

	const avm::LinkId point_call = builder.call(library.is_self_link, {builder.literal(point)});
	const avm::LinkId nonpoint_call = builder.call(library.is_self_link, {builder.literal(nonpoint)});
	const avm::LinkId self_pair_call = builder.call(library.is_self_link, {builder.literal(self_pair)});
	assert(runtime.execute(point_call) == vocabulary.true_value);
	assert(runtime.execute(nonpoint_call) == vocabulary.false_value);
	assert(runtime.execute(self_pair_call) == vocabulary.true_value);

	const avm::LinkId match_call = builder.call(
	    library.pair_matches, {builder.literal(nonpoint), builder.literal(left), builder.literal(right)});
	const avm::LinkId wrong_end_call = builder.call(
	    library.pair_matches, {builder.literal(nonpoint), builder.literal(left), builder.literal(point)});
	assert(runtime.execute(match_call) == vocabulary.true_value);
	assert(runtime.execute(wrong_end_call) == vocabulary.false_value);

	const avm::LinkId nested_point =
	    builder.call(library.is_self_via_pair_matches, {builder.literal(point)});
	const avm::LinkId nested_nonpoint =
	    builder.call(library.is_self_via_pair_matches, {builder.literal(nonpoint)});
	assert(runtime.execute(nested_point) == vocabulary.true_value);
	assert(runtime.execute(nested_nonpoint) == vocabulary.false_value);
}

void verify_call_frame_effect_accounting_converges()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();
	const StructuralLibrary library = define_structural_library(store, runtime);

	const avm::LinkId left = store.create_point();
	const avm::LinkId right = store.create_point();
	const avm::LinkId fresh = store.intern(left, right);
	const avm::LinkId call = builder.call(library.is_self_link, {builder.literal(fresh)});

	const std::size_t before_size = store.size();
	const std::size_t before_bindings = store.outgoing(vocabulary.binding_relation).size();
	const std::size_t before_frames = store.outgoing(vocabulary.frame_relation).size();
	assert(runtime.execute(call) == vocabulary.false_value);
	const std::size_t after_first_size = store.size();
	const std::size_t after_first_bindings = store.outgoing(vocabulary.binding_relation).size();
	const std::size_t after_first_frames = store.outgoing(vocabulary.frame_relation).size();

	assert(after_first_size > before_size);
	assert(after_first_bindings > before_bindings);
	assert(after_first_frames > before_frames);

	assert(runtime.execute(call) == vocabulary.false_value);
	assert(store.size() == after_first_size);
	assert(store.outgoing(vocabulary.binding_relation).size() == after_first_bindings);
	assert(store.outgoing(vocabulary.frame_relation).size() == after_first_frames);
}

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-structural-library-" + std::to_string(nonce) + ".bin");
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

void verify_composed_definitions_survive_reopen()
{
	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	avm::BootstrapVocabulary vocabulary{};
	StructuralLibrary library{};
	avm::LinkId call = avm::invalid_link_id;

	{
		avm::PersistentLinkStore store(path);
		avm::BootstrapRuntime runtime(store);
		vocabulary = runtime.vocabulary();
		library = define_structural_library(store, runtime);
		avm::ProgramBuilder builder = runtime.builder();

		const avm::LinkId left = store.create_point();
		const avm::LinkId right = store.create_point();
		const avm::LinkId nonpoint = store.intern(left, right);
		call = builder.call(library.is_self_link, {builder.literal(nonpoint)});
		assert_composed_definitions_are_not_native(runtime, library);
		assert_definition_roots_use_existing_relations(store, vocabulary, library);
	}

	{
		avm::PersistentLinkStore reopened(path);
		const std::size_t before_runtime = reopened.size();
		avm::BootstrapRuntime runtime(reopened, vocabulary);
		assert(reopened.size() == before_runtime);
		assert_composed_definitions_are_not_native(runtime, library);
		assert_definition_roots_use_existing_relations(reopened, vocabulary, library);

		assert(runtime.execute(call) == vocabulary.false_value);
		const std::size_t after_first_call = reopened.size();
		assert(runtime.execute(call) == vocabulary.false_value);
		assert(reopened.size() == after_first_call);
	}
}

} // namespace

int main()
{
	verify_composed_structural_library_behavior();
	verify_call_frame_effect_accounting_converges();
	verify_composed_definitions_survive_reopen();
	return 0;
}
