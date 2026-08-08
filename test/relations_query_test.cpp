#include "avm/persistent_link_store.h"
#include "avm/relations_query.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Fixture
{
	avm::LinkId r1;
	avm::LinkId r2;
	avm::LinkId s1;
	avm::LinkId s2;
	avm::LinkId o1;
	avm::LinkId o2;
	avm::LinkId e1;
	avm::LinkId e2;
	avm::LinkId e3;
	avm::LinkId e4;
	avm::LinkId e5;
	avm::LinkId e6;
	avm::LinkId noise;
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

std::filesystem::path temporary_store_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-relations-query-" + std::to_string(nonce) + ".bin");
}

Fixture populate(avm::LinkStore &store)
{
	Fixture fixture{};
	fixture.r1 = store.create_point();
	fixture.r2 = store.create_point();
	fixture.s1 = store.create_point();
	fixture.s2 = store.create_point();
	fixture.o1 = store.create_point();
	fixture.o2 = store.create_point();

	fixture.e1 = avm::encode_relation_entity(store, {fixture.r1, fixture.s1, fixture.o1});
	fixture.e2 = avm::encode_relation_entity(store, {fixture.r1, fixture.s1, fixture.o2});
	fixture.e3 = avm::encode_relation_entity(store, {fixture.r1, fixture.s2, fixture.o1});
	fixture.e4 = avm::encode_relation_entity(store, {fixture.r2, fixture.s1, fixture.o1});
	fixture.e5 = avm::encode_relation_entity(store, {fixture.r2, fixture.s2, fixture.o2});
	fixture.e6 = avm::encode_relation_entity(store, {fixture.r2, fixture.s1, fixture.s1});

	const avm::LinkId noise_begin = store.create_point();
	const avm::LinkId noise_end = store.create_point();
	fixture.noise = store.intern(noise_begin, noise_end);
	return fixture;
}

std::vector<avm::LinkId> sorted(std::initializer_list<avm::LinkId> ids)
{
	std::vector<avm::LinkId> result(ids);
	std::sort(result.begin(), result.end());
	return result;
}

void assert_matches(const avm::LinkStore &store, const avm::RelationQuery &query,
                    const std::vector<avm::LinkId> &expected_ids)
{
	const std::size_t size_before = store.size();
	const std::vector<avm::RelationMatch> matches = avm::query_relation_entities(store, query);
	assert(store.size() == size_before);
	assert(std::is_sorted(matches.begin(), matches.end(),
	                      [](const avm::RelationMatch &left, const avm::RelationMatch &right)
	                      { return left.entity_id < right.entity_id; }));

	std::vector<avm::LinkId> actual_ids;
	actual_ids.reserve(matches.size());
	for (const avm::RelationMatch &match : matches)
	{
		actual_ids.push_back(match.entity_id);
		assert(match.entity == avm::decode_relation_entity(store, match.entity_id));
	}
	assert(actual_ids == expected_ids);
}

void verify_all_constraint_combinations(const avm::LinkStore &store, const Fixture &fixture)
{
	const auto pair_s1_o1 = store.find(fixture.s1, fixture.o1);
	const auto pair_s2_o1 = store.find(fixture.s2, fixture.o1);
	assert(pair_s1_o1.has_value());
	assert(pair_s2_o1.has_value());

	assert_matches(store, {.relation = fixture.r1}, sorted({fixture.r1, fixture.e1, fixture.e2, fixture.e3}));
	assert_matches(store, {.subject = fixture.s1},
	               sorted({fixture.s1, fixture.e1, fixture.e2, fixture.e4, fixture.e6}));
	assert_matches(store, {.object = fixture.o1},
	               sorted({fixture.o1, *pair_s1_o1, *pair_s2_o1, fixture.e1, fixture.e3, fixture.e4}));
	assert_matches(store, {.relation = fixture.r1, .subject = fixture.s1}, sorted({fixture.e1, fixture.e2}));
	assert_matches(store, {.relation = fixture.r1, .object = fixture.o1}, sorted({fixture.e1, fixture.e3}));
	assert_matches(store, {.subject = fixture.s1, .object = fixture.o1}, sorted({fixture.e1, fixture.e4}));
	assert_matches(store, {.relation = fixture.r1, .subject = fixture.s1, .object = fixture.o1}, sorted({fixture.e1}));
}

void verify_point_endpoint_entity(const avm::LinkStore &store, const Fixture &fixture)
{
	assert_matches(store, {.relation = fixture.r2, .subject = fixture.s1, .object = fixture.s1}, sorted({fixture.e6}));
}

void verify_missing_constraints_are_empty_and_non_materializing(const avm::LinkStore &store, const Fixture &fixture)
{
	const std::size_t size_before = store.size();
	assert(avm::query_relation_entities(store, {.subject = fixture.s2, .object = fixture.s1}).empty());
	assert(store.size() == size_before);

	const avm::LinkId unknown = static_cast<avm::LinkId>(store.size()) + 1000;
	assert(!store.contains(unknown));
	assert(avm::query_relation_entities(store, {.relation = unknown}).empty());
	assert(avm::query_relation_entities(store, {.subject = unknown}).empty());
	assert(avm::query_relation_entities(store, {.object = unknown}).empty());
	assert(store.size() == size_before);
}

void verify_unconstrained_query_is_rejected(const avm::LinkStore &store)
{
	bool thrown = false;
	try
	{
		static_cast<void>(avm::query_relation_entities(store, {}));
	}
	catch (const std::invalid_argument &error)
	{
		thrown = true;
		assert(std::string_view(error.what()).find("constraint") != std::string_view::npos);
	}
	assert(thrown);
}

void verify_noise_does_not_match_unrelated_constraints(const avm::LinkStore &store, const Fixture &fixture)
{
	const std::vector<avm::RelationMatch> matches = avm::query_relation_entities(store, {.relation = fixture.r1});
	assert(std::none_of(matches.begin(), matches.end(),
	                    [&](const avm::RelationMatch &match) { return match.entity_id == fixture.noise; }));
}

void verify_backend(avm::LinkStore &store, const Fixture &fixture)
{
	verify_all_constraint_combinations(store, fixture);
	verify_point_endpoint_entity(store, fixture);
	verify_missing_constraints_are_empty_and_non_materializing(store, fixture);
	verify_unconstrained_query_is_rejected(store);
	verify_noise_does_not_match_unrelated_constraints(store, fixture);
}

} // namespace

int main()
{
	avm::InMemoryLinkStore memory;
	const Fixture memory_fixture = populate(memory);
	verify_backend(memory, memory_fixture);

	const std::filesystem::path path = temporary_store_path();
	FileCleanup cleanup{path};
	Fixture persistent_fixture{};
	{
		avm::PersistentLinkStore persistent(path);
		persistent_fixture = populate(persistent);
		verify_backend(persistent, persistent_fixture);
	}
	{
		avm::PersistentLinkStore reopened(path);
		verify_backend(reopened, persistent_fixture);
	}

	return 0;
}
