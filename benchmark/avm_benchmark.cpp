#include "avm/bootstrap_runtime.h"
#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"
#include "avm/relations_query.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

struct Measurement
{
	std::string name;
	std::size_t operations;
	std::uint64_t elapsed_ns;
};

struct FileCleanup
{
	std::filesystem::path path;
	~FileCleanup()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

Measurement measure(std::string name, std::size_t operations, auto &&function)
{
	const auto started = Clock::now();
	for (std::size_t i = 0; i < operations; ++i)
		function(i);
	const auto finished = Clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
	return Measurement{std::move(name), operations, static_cast<std::uint64_t>(elapsed)};
}

void print(const Measurement &measurement)
{
	double ns_per_op = 0.0;
	if (measurement.operations != 0)
		ns_per_op = static_cast<double>(measurement.elapsed_ns) / static_cast<double>(measurement.operations);

	std::cout << measurement.name << '\t' << measurement.operations << '\t' << measurement.elapsed_ns << '\t'
	          << ns_per_op << '\n';
}

std::filesystem::path temporary_store_path()
{
	const auto nonce = Clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-benchmark-" + std::to_string(nonce) + ".bin");
}

std::size_t scale_iterations(std::size_t fanout)
{
	switch (fanout)
	{
	case 1:
		return 10000;
	case 8:
		return 5000;
	case 64:
		return 2000;
	case 256:
		return 500;
	default:
		throw std::invalid_argument("unsupported Relations query benchmark fan-out");
	}
}

void assert_match_count(const avm::LinkStore &store, const avm::RelationQuery &query, std::size_t expected,
                        std::string_view label)
{
	const std::size_t actual = avm::query_relation_entities(store, query).size();
	if (actual != expected)
		throw std::runtime_error(std::string(label) + " Relations query match-count mismatch: expected " +
		                         std::to_string(expected) + ", got " + std::to_string(actual));
}

void benchmark_relation_query_scale(std::size_t fanout, std::size_t &sink)
{
	avm::InMemoryLinkStore query_store;
	const avm::LinkId relation = query_store.create_point();
	const avm::LinkId subject = query_store.create_point();
	const avm::LinkId object = query_store.create_point();

	for (std::size_t i = 0; i < fanout; ++i)
	{
		const avm::LinkId relation_subject = query_store.create_point();
		const avm::LinkId relation_object = query_store.create_point();
		static_cast<void>(avm::encode_relation_entity(query_store, {relation, relation_subject, relation_object}));

		const avm::LinkId subject_relation = query_store.create_point();
		const avm::LinkId subject_object = query_store.create_point();
		static_cast<void>(avm::encode_relation_entity(query_store, {subject_relation, subject, subject_object}));

		const avm::LinkId object_relation = query_store.create_point();
		const avm::LinkId object_subject = query_store.create_point();
		static_cast<void>(avm::encode_relation_entity(query_store, {object_relation, object_subject, object}));

		const avm::LinkId exact_relation = query_store.create_point();
		static_cast<void>(avm::encode_relation_entity(query_store, {exact_relation, subject, object}));
	}

	const avm::RelationQuery relation_query{.relation = relation};
	const avm::RelationQuery subject_query{.subject = subject};
	const avm::RelationQuery object_query{.object = object};
	const avm::RelationQuery subject_object_query{.subject = subject, .object = object};

	const std::size_t relation_matches = fanout + 1;
	const std::size_t subject_matches = 2 * fanout + 1;
	const std::size_t object_matches = 3 * fanout + 2;
	const std::size_t subject_object_matches = fanout;

	assert_match_count(query_store, relation_query, relation_matches, "relation");
	assert_match_count(query_store, subject_query, subject_matches, "subject");
	assert_match_count(query_store, object_query, object_matches, "object");
	assert_match_count(query_store, subject_object_query, subject_object_matches, "subject+object");

	const std::size_t iterations = scale_iterations(fanout);
	std::cout << "# query_scale\t" << fanout << '\t' << iterations << '\t' << relation_matches << '\t'
	          << subject_matches << '\t' << object_matches << '\t' << subject_object_matches << '\n';

	const std::string suffix = "_fanout_" + std::to_string(fanout);
	const auto relation_bench = [&](std::size_t) { sink ^= avm::query_relation_entities(query_store, relation_query).size(); };
	print(measure("relations_query_relation" + suffix, iterations, relation_bench));

	const auto subject_bench = [&](std::size_t) { sink ^= avm::query_relation_entities(query_store, subject_query).size(); };
	print(measure("relations_query_subject" + suffix, iterations, subject_bench));

	const auto object_bench = [&](std::size_t) { sink ^= avm::query_relation_entities(query_store, object_query).size(); };
	print(measure("relations_query_object" + suffix, iterations, object_bench));

	const auto pair_bench = [&](std::size_t) {
		sink ^= avm::query_relation_entities(query_store, subject_object_query).size();
	};
	print(measure("relations_query_subject_object" + suffix, iterations, pair_bench));
}

} // namespace

int main()
{
	constexpr std::size_t sample_size = 20000;
	constexpr std::size_t query_iterations = 100000;
	constexpr std::size_t persistent_sample_size = 256;
	constexpr std::array<std::size_t, 4> query_fanouts{1, 8, 64, 256};

	std::cout << "name\toperations\telapsed_ns\tns_per_op\n";

	avm::InMemoryLinkStore store;
	std::vector<avm::LinkId> points;
	points.reserve(sample_size + 2);
	for (std::size_t i = 0; i < sample_size + 2; ++i)
		points.push_back(store.create_point());

	std::vector<avm::LinkId> pairs;
	pairs.reserve(sample_size);
	const auto intern_new = [&](std::size_t i) { pairs.push_back(store.intern(points[i], points[i + 1])); };
	print(measure("intern_new_pair", sample_size, intern_new));

	std::size_t sink = 0;
	const auto intern_existing = [&](std::size_t i)
	{
		const avm::LinkId id = store.intern(points[i % sample_size], points[(i % sample_size) + 1]);
		sink ^= static_cast<std::size_t>(id);
	};
	print(measure("intern_existing_pair", query_iterations, intern_existing));

	const auto find_hit = [&](std::size_t i)
	{
		const auto found = store.find(points[i % sample_size], points[(i % sample_size) + 1]);
		sink ^= static_cast<std::size_t>(found.value_or(avm::invalid_link_id));
	};
	print(measure("find_hit", query_iterations, find_hit));

	const auto find_miss = [&](std::size_t i)
	{
		const auto found = store.find(points[i % sample_size], points[(i % sample_size) + 2]);
		sink ^= static_cast<std::size_t>(found.value_or(avm::invalid_link_id));
	};
	print(measure("find_miss", query_iterations, find_miss));

	const auto outgoing_query = [&](std::size_t i) { sink ^= store.outgoing(points[i % sample_size]).size(); };
	print(measure("outgoing_query", query_iterations, outgoing_query));

	const auto incoming_query = [&](std::size_t i) { sink ^= store.incoming(points[(i % sample_size) + 1]).size(); };
	print(measure("incoming_query", query_iterations, incoming_query));

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	avm::LinkId entity = avm::invalid_link_id;

	const auto encode_entity = [&](std::size_t)
	{
		entity = avm::encode_relation_entity(store, avm::RelationEntity{relation, subject, object});
		sink ^= static_cast<std::size_t>(entity);
	};
	print(measure("relations_model_encode", query_iterations, encode_entity));

	const auto decode_entity = [&](std::size_t)
	{
		const avm::RelationEntity decoded = avm::decode_relation_entity(store, entity);
		sink ^= static_cast<std::size_t>(decoded.object);
	};
	print(measure("relations_model_decode", query_iterations, decode_entity));

	for (const std::size_t fanout : query_fanouts)
		benchmark_relation_query_scale(fanout, sink);

	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	const auto execute_minimal = [&](std::size_t) { sink ^= static_cast<std::size_t>(runtime.execute(expression)); };
	print(measure("execute_minimal_relation", query_iterations, execute_minimal));

	const std::filesystem::path persistent_path = temporary_store_path();
	FileCleanup cleanup{persistent_path};
	{
		avm::PersistentLinkStore persistent(persistent_path);
		std::vector<avm::LinkId> persistent_points;
		persistent_points.reserve(persistent_sample_size + 1);
		for (std::size_t i = 0; i < persistent_sample_size + 1; ++i)
			persistent_points.push_back(persistent.create_point());
		for (std::size_t i = 0; i < persistent_sample_size; ++i)
			static_cast<void>(persistent.intern(persistent_points[i], persistent_points[i + 1]));
	}

	const auto reopen_persistent = [&](std::size_t)
	{
		avm::PersistentLinkStore reopened(persistent_path);
		sink ^= reopened.size();
	};
	print(measure("persistent_reopen_index_rebuild", 1, reopen_persistent));

	std::cout << "# sink\t" << sink << '\n';
	return 0;
}
