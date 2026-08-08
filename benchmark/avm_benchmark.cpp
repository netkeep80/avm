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

template <class Function>
Measurement measure(std::string_view name, std::size_t operations, Function &&function)
{
	const auto started = Clock::now();
	for (std::size_t i = 0; i < operations; ++i)
		function(i);
	const auto finished = Clock::now();
	return Measurement{
	    name,
	    operations,
	    static_cast<std::uint64_t>(
	        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count()),
	};
}

void print(const Measurement &measurement)
{
	const double ns_per_op = measurement.operations == 0
	                             ? 0.0
	                             : static_cast<double>(measurement.elapsed_ns) /
	                                   static_cast<double>(measurement.operations);
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

	std::cout << "name\toperations\telapsed_ns\tns_per_op\n";

	avm::InMemoryLinkStore store;
	std::vector<avm::LinkId> points;
	points.reserve(sample_size + 2);
	for (std::size_t i = 0; i < sample_size + 2; ++i)
		points.push_back(store.create_point());

	std::vector<avm::LinkId> pairs;
	pairs.reserve(sample_size);
	print(measure("intern_new_pair", sample_size,
	              [&](std::size_t i) { pairs.push_back(store.intern(points[i], points[i + 1])); }));

	std::size_t sink = 0;
	print(measure("intern_existing_pair", query_iterations,
	              [&](std::size_t i)
	              {
		              sink ^= static_cast<std::size_t>(
		                  store.intern(points[i % sample_size], points[(i % sample_size) + 1]));
	              }));

	print(measure("find_hit", query_iterations,
	              [&](std::size_t i)
	              {
		              const auto found = store.find(points[i % sample_size], points[(i % sample_size) + 1]);
		              sink ^= static_cast<std::size_t>(found.value_or(avm::invalid_link_id));
	              }));

	print(measure("find_miss", query_iterations,
	              [&](std::size_t i)
	              {
		              const auto found = store.find(points[i % sample_size], points[(i % sample_size) + 2]);
		              sink ^= static_cast<std::size_t>(found.value_or(avm::invalid_link_id));
	              }));

	print(measure("outgoing_query", query_iterations,
	              [&](std::size_t i) { sink ^= store.outgoing(points[i % sample_size]).size(); }));
	print(measure("incoming_query", query_iterations,
	              [&](std::size_t i) { sink ^= store.incoming(points[(i % sample_size) + 1]).size(); }));

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	avm::LinkId entity = avm::invalid_link_id;
	print(measure("relations_model_encode", query_iterations,
	              [&](std::size_t)
	              {
		              entity = avm::encode_relation_entity(store, avm::RelationEntity{relation, subject, object});
		              sink ^= static_cast<std::size_t>(entity);
	              }));
	print(measure("relations_model_decode", query_iterations,
	              [&](std::size_t)
	              {
		              const avm::RelationEntity decoded = avm::decode_relation_entity(store, entity);
		              sink ^= static_cast<std::size_t>(decoded.object);
	              }));

	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	print(measure("execute_minimal_relation", query_iterations,
	              [&](std::size_t) { sink ^= static_cast<std::size_t>(runtime.execute(expression)); }));

	const std::filesystem::path persistent_path = temporary_store_path();
	FileCleanup cleanup{persistent_path};
	{
		avm::PersistentLinkStore persistent(persistent_path);
		std::vector<avm::LinkId> persistent_points;
		persistent_points.reserve(sample_size + 1);
		for (std::size_t i = 0; i < sample_size + 1; ++i)
			persistent_points.push_back(persistent.create_point());
		for (std::size_t i = 0; i < sample_size; ++i)
			static_cast<void>(persistent.intern(persistent_points[i], persistent_points[i + 1]));
	}

	print(measure("persistent_reopen_index_rebuild", 1,
	              [&](std::size_t)
	              {
		              avm::PersistentLinkStore reopened(persistent_path);
		              sink ^= reopened.size();
	              }));

	std::cout << "# sink\t" << sink << '\n';
	return 0;
}
