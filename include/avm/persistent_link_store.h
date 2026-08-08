#pragma once

#include "avm/link_store.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace avm
{

class PersistentLinkStore final : public LinkStore
{
public:
	explicit PersistentLinkStore(std::filesystem::path path) : path_(std::move(path))
	{
		if (std::filesystem::exists(path_))
			load();
	}

	LinkId create_point() override
	{
		require_healthy();
		const LinkId id = allocate_id();
		commit_new_link(id, Link{id, id});
		return id;
	}

	LinkId intern(LinkId begin, LinkId end) override
	{
		require_healthy();
		require_existing_endpoint(begin, "begin");
		require_existing_endpoint(end, "end");

		if (const auto existing = find(begin, end))
			return *existing;

		const LinkId id = allocate_id();
		commit_new_link(id, Link{begin, end});
		return id;
	}

	std::optional<LinkId> find(LinkId begin, LinkId end) const override
	{
		require_healthy();
		const auto it = exact_.find({begin, end});
		if (it == exact_.end())
			return std::nullopt;
		return it->second;
	}

	Link get(LinkId id) const override
	{
		require_healthy();
		const auto it = links_.find(id);
		if (it == links_.end())
			throw std::out_of_range("unknown LinkId");
		return it->second;
	}

	std::vector<LinkId> outgoing(LinkId begin) const override
	{
		require_healthy();
		const auto it = outgoing_.find(begin);
		if (it == outgoing_.end())
			return {};
		return it->second;
	}

	std::vector<LinkId> incoming(LinkId end) const override
	{
		require_healthy();
		const auto it = incoming_.find(end);
		if (it == incoming_.end())
			return {};
		return it->second;
	}

	bool contains(LinkId id) const override
	{
		require_healthy();
		return links_.contains(id);
	}

	std::size_t size() const override
	{
		require_healthy();
		return links_.size();
	}

	const std::filesystem::path &path() const noexcept { return path_; }

	bool faulted() const noexcept { return faulted_; }

private:
	using Pair = std::pair<LinkId, LinkId>;

	static constexpr std::array<char, 8> magic_{{'A', 'V', 'M', 'L', 'N', 'K', '1', '\0'}};
	static constexpr std::uint32_t format_version_ = 1;

	static void write_u32(std::ostream &output, std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			output.put(static_cast<char>((value >> shift) & 0xffU));
	}

	static void write_u64(std::ostream &output, std::uint64_t value)
	{
		for (unsigned shift = 0; shift < 64; shift += 8)
			output.put(static_cast<char>((value >> shift) & 0xffU));
	}

	static std::uint32_t read_u32(std::istream &input)
	{
		std::uint32_t value = 0;
		for (unsigned shift = 0; shift < 32; shift += 8)
		{
			const int byte = input.get();
			if (byte == std::char_traits<char>::eof())
				throw std::runtime_error("truncated persistent LinkStore");
			value |= static_cast<std::uint32_t>(static_cast<unsigned char>(byte)) << shift;
		}
		return value;
	}

	static std::uint64_t read_u64(std::istream &input)
	{
		std::uint64_t value = 0;
		for (unsigned shift = 0; shift < 64; shift += 8)
		{
			const int byte = input.get();
			if (byte == std::char_traits<char>::eof())
				throw std::runtime_error("truncated persistent LinkStore");
			value |= static_cast<std::uint64_t>(static_cast<unsigned char>(byte)) << shift;
		}
		return value;
	}

	void require_healthy() const
	{
		if (faulted_)
			throw std::runtime_error("persistent LinkStore is faulted after a failed mutation commit");
	}

	LinkId allocate_id()
	{
		if (next_id_ == invalid_link_id || next_id_ == std::numeric_limits<LinkId>::max())
			throw std::overflow_error("LinkId space exhausted");
		return next_id_++;
	}

	void require_existing_endpoint(LinkId id, const char *role) const
	{
		if (!contains(id))
			throw std::invalid_argument(std::string("unknown ") + role + " LinkId");
	}

	void insert_link(LinkId id, Link link)
	{
		const Pair pair{link.begin, link.end};
		if (id == invalid_link_id || links_.contains(id))
			throw std::runtime_error("duplicate or invalid persistent LinkId");
		if (exact_.contains(pair))
			throw std::runtime_error("duplicate canonical pair in persistent LinkStore");

		links_.emplace(id, link);
		exact_.emplace(pair, id);
		outgoing_[link.begin].push_back(id);
		incoming_[link.end].push_back(id);
	}

	void commit_new_link(LinkId id, Link link)
	{
		try
		{
			insert_link(id, link);
			persist();
		}
		catch (...)
		{
			faulted_ = true;
			throw;
		}
	}

	void persist() const
	{
		if (path_.has_parent_path())
			std::filesystem::create_directories(path_.parent_path());

		std::ofstream output(path_, std::ios::binary | std::ios::trunc);
		if (!output)
			throw std::runtime_error("cannot open persistent LinkStore for writing");

		output.write(magic_.data(), static_cast<std::streamsize>(magic_.size()));
		write_u32(output, format_version_);
		write_u32(output, 0);
		write_u64(output, static_cast<std::uint64_t>(links_.size()));
		for (const auto &[id, link] : links_)
		{
			write_u64(output, id);
			write_u64(output, link.begin);
			write_u64(output, link.end);
		}
		output.flush();
		if (!output)
			throw std::runtime_error("failed to persist LinkStore snapshot");
	}

	void load()
	{
		std::ifstream input(path_, std::ios::binary);
		if (!input)
			throw std::runtime_error("cannot open persistent LinkStore for reading");

		std::array<char, magic_.size()> magic{};
		input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
		if (input.gcount() != static_cast<std::streamsize>(magic.size()) || magic != magic_)
			throw std::runtime_error("invalid persistent LinkStore magic");

		const std::uint32_t version = read_u32(input);
		const std::uint32_t reserved = read_u32(input);
		if (version != format_version_ || reserved != 0)
			throw std::runtime_error("unsupported persistent LinkStore format");

		const std::uint64_t count = read_u64(input);
		if (count >= std::numeric_limits<LinkId>::max())
			throw std::runtime_error("persistent LinkStore record count is too large");

		for (std::uint64_t index = 0; index < count; ++index)
		{
			const LinkId id = read_u64(input);
			const LinkId begin = read_u64(input);
			const LinkId end = read_u64(input);
			if (id != index + 1)
				throw std::runtime_error("persistent LinkStore LinkIds are not contiguous");
			insert_link(id, Link{begin, end});
		}

		if (input.peek() != std::char_traits<char>::eof())
			throw std::runtime_error("persistent LinkStore contains trailing data");

		for (const auto &[id, link] : links_)
		{
			if (!contains(link.begin) || !contains(link.end))
				throw std::runtime_error("persistent LinkStore contains unknown endpoint");
			if (link.begin == id || link.end == id)
			{
				if (link.begin != id || link.end != id)
					throw std::runtime_error("persistent LinkStore contains invalid self reference");
			}
		}

		next_id_ = static_cast<LinkId>(count) + 1;
	}

	std::filesystem::path path_;
	LinkId next_id_{1};
	std::map<LinkId, Link> links_;
	std::map<Pair, LinkId> exact_;
	std::map<LinkId, std::vector<LinkId>> outgoing_;
	std::map<LinkId, std::vector<LinkId>> incoming_;
	bool faulted_ = false;
};

} // namespace avm
