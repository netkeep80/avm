#include "avm/bootstrap_runtime.h"
#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

struct Measurement
{
	std::string_view name;
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

Measurement measure(std::string_view name, std::size_t operations, auto &&function)
{
	const auto started = Clock::now();
	for (std::size_t i = 0; i < operations; ++i)
		function(i);
	const auto finished = Clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
	return Measurement{name, operations, static_cast<std::uint64_t>(elapsed)};
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

} // namespace

int main()
{
	constexpr std::size_t sample_size = 20000;
	constexpr std::size_t query_iterations = 100000;
	constexpr std::size_t persistent_sample_size = 256;

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
