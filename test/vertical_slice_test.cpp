#include "avm/bootstrap_runtime.h"
#include "avm/json_compat.h"
#include "avm/persistent_link_store.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

namespace
{

using Json = nlohmann::json;

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-vertical-slice-" + std::to_string(nonce) + ".bin");
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

struct ImportedProgram
{
	avm::BootstrapVocabulary vocabulary;
	avm::LinkId sequence_relation;
	avm::LinkId boolean_root;
	avm::LinkId lazy_if_root;
	avm::LinkId call_root;
	avm::LinkId recursive_call_root;
};

ImportedProgram import_and_execute(avm::LinkStore &store)
{
	avm::BootstrapRuntime runtime(store);
	const avm::BootstrapVocabulary vocabulary = runtime.vocabulary();
	const avm::LinkId sequence_relation = store.create_point();
	avm::JsonProgramImporter importer(store, vocabulary, sequence_relation);

	const Json boolean_program = {
	    {"And", Json::array({Json{{"Not", Json::array({false})}}, Json{{"Or", Json::array({false, true})}}})}};
	const avm::LinkId boolean_root = importer.import_program(boolean_program);
	assert(runtime.execute(boolean_root) == vocabulary.true_value);

	const Json lazy_if_program = {
	    {"If", Json::array({true, Json{{"Not", Json::array({false})}}, Json{{"Call", Json::array({"missing"})}}})}};
	const avm::LinkId lazy_if_root = importer.import_program(lazy_if_program);
	assert(runtime.execute(lazy_if_root) == vocabulary.true_value);

	const Json identity_definition = {{"Def", Json::array({"identity", Json::array({"x"}), "x"})}};
	const avm::LinkId identity_definition_root = importer.import_program(identity_definition);
	assert(runtime.execute(identity_definition_root) == vocabulary.nil);

	const Json identity_call = {{"Call", Json::array({"identity", true})}};
	const avm::LinkId call_root = importer.import_program(identity_call);
	assert(runtime.execute(call_root) == vocabulary.true_value);

	const Json recursive_body = {
	    {"If", Json::array({"x", true, Json{{"Call", Json::array({"eventually_true", true})}}})}};
	const Json recursive_definition_arguments = Json::array({"eventually_true", Json::array({"x"}), recursive_body});
	const Json recursive_definition = {{"Def", recursive_definition_arguments}};
	const avm::LinkId recursive_definition_root = importer.import_program(recursive_definition);
	assert(runtime.execute(recursive_definition_root) == vocabulary.nil);

	const Json recursive_call = {{"Call", Json::array({"eventually_true", false})}};
	const avm::LinkId recursive_call_root = importer.import_program(recursive_call);
	assert(runtime.execute(recursive_call_root) == vocabulary.true_value);

	const avm::RelationEntity decoded_boolean = avm::decode_relation_entity(store, boolean_root);
	assert(decoded_boolean.relation == vocabulary.and_relation);
	assert(decoded_boolean.subject == vocabulary.unit);

	return ImportedProgram{vocabulary, sequence_relation, boolean_root, lazy_if_root, call_root, recursive_call_root};
}

void assert_program_executes(avm::LinkStore &store, const ImportedProgram &program)
{
	const std::size_t before_restore = store.size();
	avm::BootstrapRuntime runtime(store, program.vocabulary);
	assert(store.size() == before_restore);

	assert(store.contains(program.boolean_root));
	assert(store.contains(program.lazy_if_root));
	assert(store.contains(program.call_root));
	assert(store.contains(program.recursive_call_root));

	assert(runtime.execute(program.boolean_root) == program.vocabulary.true_value);
	assert(runtime.execute(program.lazy_if_root) == program.vocabulary.true_value);
	assert(runtime.execute(program.call_root) == program.vocabulary.true_value);
	assert(runtime.execute(program.recursive_call_root) == program.vocabulary.true_value);
}

void test_in_memory_vertical_slice()
{
	avm::InMemoryLinkStore store;
	const ImportedProgram program = import_and_execute(store);
	assert_program_executes(store, program);
}

void test_persistent_reopen_vertical_slice()
{
	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	ImportedProgram program{};

	{
		avm::PersistentLinkStore store(path);
		program = import_and_execute(store);
		assert(store.contains(program.boolean_root));
	}

	{
		avm::PersistentLinkStore reopened(path);
		assert_program_executes(reopened, program);
	}

	{
		avm::PersistentLinkStore reopened_again(path);
		assert_program_executes(reopened_again, program);
	}
}

void test_legacy_11_vocabulary_upgrades_once()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime current(store);
	const avm::BootstrapVocabulary current_vocabulary = current.vocabulary();

	avm::BootstrapVocabulary legacy{
	    current_vocabulary.unit,
	    current_vocabulary.nil,
	    current_vocabulary.true_value,
	    current_vocabulary.false_value,
	    current_vocabulary.quote_relation,
	    current_vocabulary.parameter_relation,
	    current_vocabulary.sequence_relation,
	    current_vocabulary.not_relation,
	    current_vocabulary.and_relation,
	    current_vocabulary.or_relation,
	    current_vocabulary.if_relation,
	    current_vocabulary.function_relation,
	    current_vocabulary.call_relation,
	    current_vocabulary.binding_relation,
	    current_vocabulary.frame_relation,
	};
	assert(legacy.begin_relation == avm::invalid_link_id);
	assert(legacy.end_relation == avm::invalid_link_id);
	assert(legacy.same_relation == avm::invalid_link_id);
	assert(legacy.link_exists_relation == avm::invalid_link_id);

	const std::size_t before_upgrade = store.size();
	avm::BootstrapRuntime upgraded(store, legacy);
	assert(store.size() == before_upgrade + 4);

	const avm::BootstrapVocabulary upgraded_vocabulary = upgraded.vocabulary();
	assert(upgraded_vocabulary.unit == legacy.unit);
	assert(upgraded_vocabulary.nil == legacy.nil);
	assert(upgraded_vocabulary.true_value == legacy.true_value);
	assert(upgraded_vocabulary.false_value == legacy.false_value);
	assert(upgraded_vocabulary.quote_relation == legacy.quote_relation);
	assert(upgraded_vocabulary.parameter_relation == legacy.parameter_relation);
	assert(upgraded_vocabulary.sequence_relation == legacy.sequence_relation);
	assert(upgraded_vocabulary.not_relation == legacy.not_relation);
	assert(upgraded_vocabulary.and_relation == legacy.and_relation);
	assert(upgraded_vocabulary.or_relation == legacy.or_relation);
	assert(upgraded_vocabulary.if_relation == legacy.if_relation);
	assert(upgraded_vocabulary.function_relation == legacy.function_relation);
	assert(upgraded_vocabulary.call_relation == legacy.call_relation);
	assert(upgraded_vocabulary.binding_relation == legacy.binding_relation);
	assert(upgraded_vocabulary.frame_relation == legacy.frame_relation);
	assert(store.contains(upgraded_vocabulary.begin_relation));
	assert(store.contains(upgraded_vocabulary.end_relation));
	assert(store.contains(upgraded_vocabulary.same_relation));
	assert(store.contains(upgraded_vocabulary.link_exists_relation));

	const std::size_t after_upgrade = store.size();
	avm::BootstrapRuntime restored(store, upgraded_vocabulary);
	static_cast<void>(restored);
	assert(store.size() == after_upgrade);
}

void test_partially_missing_structural_vocabulary_rejected_without_mutation()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::BootstrapVocabulary partial = runtime.vocabulary();
	partial.begin_relation = avm::invalid_link_id;

	const std::size_t before = store.size();
	bool rejected = false;
	try
	{
		avm::BootstrapRuntime restored(store, partial);
		static_cast<void>(restored);
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == before);
}

void test_invalid_restored_vocabulary_rejected()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::BootstrapVocabulary invalid = runtime.vocabulary();
	invalid.frame_relation = invalid.binding_relation;

	bool rejected = false;
	try
	{
		avm::BootstrapRuntime restored(store, invalid);
		static_cast<void>(restored);
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
	test_in_memory_vertical_slice();
	test_persistent_reopen_vertical_slice();
	test_legacy_11_vocabulary_upgrades_once();
	test_partially_missing_structural_vocabulary_rejected_without_mutation();
	test_invalid_restored_vocabulary_rejected();
	return 0;
}
