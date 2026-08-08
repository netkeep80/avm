#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

std::filesystem::path temporary_path(const std::string &suffix)
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() /
	       ("avm-persistent-link-store-" + std::to_string(nonce) + "-" + suffix + ".bin");
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

std::vector<char> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void expect_open_failure(const std::filesystem::path &path)
{
	bool rejected = false;
	try
	{
		avm::PersistentLinkStore store(path);
		static_cast<void>(store);
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
}

void test_empty_reopen()
{
	const std::filesystem::path path = temporary_path("empty");
	FileCleanup cleanup{path};
	{
		avm::PersistentLinkStore store(path);
		assert(store.size() == 0);
		const avm::LinkId point = store.create_point();
		assert(point == 1);
	}
	{
		avm::PersistentLinkStore reopened(path);
		assert(reopened.size() == 1);
		assert(reopened.get(1) == (avm::Link{1, 1}));
	}
}

void test_reopen_identity_and_indexes()
{
	const std::filesystem::path path = temporary_path("identity");
	FileCleanup cleanup{path};
	avm::LinkId first = avm::invalid_link_id;
	avm::LinkId second = avm::invalid_link_id;
	avm::LinkId pair = avm::invalid_link_id;
	avm::LinkId entity = avm::invalid_link_id;
	std::vector<avm::LinkId> outgoing_before;
	std::vector<avm::LinkId> incoming_before;

	{
		avm::PersistentLinkStore store(path);
		first = store.create_point();
		second = store.create_point();
		pair = store.intern(first, second);
		assert(store.intern(first, second) == pair);
		entity = avm::encode_relation_entity(store, avm::RelationEntity{first, second, first});
		outgoing_before = store.outgoing(first);
		incoming_before = store.incoming(second);
	}

	const std::vector<char> bytes_before_find = read_file(path);
	{
		avm::PersistentLinkStore reopened(path);
		assert(reopened.size() == 5);
		assert(reopened.find(first, second) == pair);
		assert(reopened.intern(first, second) == pair);
		assert(reopened.outgoing(first) == outgoing_before);
		assert(reopened.incoming(second) == incoming_before);
		assert(avm::decode_relation_entity(reopened, entity) == (avm::RelationEntity{first, second, first}));

		const std::size_t size_before_miss = reopened.size();
		assert(!reopened.find(second, second + 1000).has_value());
		assert(reopened.size() == size_before_miss);
	}
	assert(read_file(path) == bytes_before_find);

	{
		avm::PersistentLinkStore reopened_again(path);
		assert(reopened_again.find(first, second) == pair);
		const avm::RelationEntity reopened_entity = avm::decode_relation_entity(reopened_again, entity);
		assert(reopened_entity == (avm::RelationEntity{first, second, first}));
	}
}

void test_corruption_rejection()
{
	const std::filesystem::path bad_magic_path = temporary_path("bad-magic");
	FileCleanup bad_magic_cleanup{bad_magic_path};
	{
		std::ofstream output(bad_magic_path, std::ios::binary | std::ios::trunc);
		output << "not-an-avm-store";
	}
	expect_open_failure(bad_magic_path);

	const std::filesystem::path truncated_path = temporary_path("truncated");
	FileCleanup truncated_cleanup{truncated_path};
	{
		avm::PersistentLinkStore store(truncated_path);
		store.create_point();
	}
	std::filesystem::resize_file(truncated_path, 20);
	expect_open_failure(truncated_path);

	const std::filesystem::path trailing_path = temporary_path("trailing");
	FileCleanup trailing_cleanup{trailing_path};
	{
		avm::PersistentLinkStore store(trailing_path);
		store.create_point();
	}
	{
		std::ofstream output(trailing_path, std::ios::binary | std::ios::app);
		output.put('x');
	}
	expect_open_failure(trailing_path);
}

void test_endpoint_validation_survives_reopen()
{
	const std::filesystem::path path = temporary_path("endpoint");
	FileCleanup cleanup{path};
	{
		avm::PersistentLinkStore store(path);
		store.create_point();
		bool rejected = false;
		try
		{
			static_cast<void>(store.intern(1, 9999));
		}
		catch (const std::invalid_argument &)
		{
			rejected = true;
		}
		assert(rejected);
		assert(store.size() == 1);
	}
	{
		avm::PersistentLinkStore reopened(path);
		assert(reopened.size() == 1);
		assert(reopened.get(1) == (avm::Link{1, 1}));
	}
}

} // namespace

int main()
{
	test_empty_reopen();
	test_reopen_identity_and_indexes();
	test_corruption_rejection();
	test_endpoint_validation_survives_reopen();
	return 0;
}
