#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

using RawDocumentId = std::uint64_t;
inline constexpr RawDocumentId invalid_raw_document_id = 0;
using RawBytes = std::vector<std::uint8_t>;

class RawCarrier
{
public:
	virtual ~RawCarrier() = default;

	virtual RawDocumentId put(RawBytes payload) = 0;
	virtual std::optional<RawBytes> get(RawDocumentId id) const = 0;
	virtual bool contains(RawDocumentId id) const = 0;
	virtual bool erase(RawDocumentId id) = 0;
	virtual std::size_t size() const = 0;
};

class InMemoryRawCarrier final : public RawCarrier
{
public:
	RawDocumentId put(RawBytes payload) override
	{
		const RawDocumentId id = allocate_id();
		documents_.emplace(id, std::move(payload));
		return id;
	}

	std::optional<RawBytes> get(RawDocumentId id) const override
	{
		const auto found = documents_.find(id);
		if (found == documents_.end())
			return std::nullopt;
		return found->second;
	}

	bool contains(RawDocumentId id) const override { return documents_.contains(id); }

	bool erase(RawDocumentId id) override { return documents_.erase(id) != 0; }

	std::size_t size() const override { return documents_.size(); }

private:
	RawDocumentId allocate_id()
	{
		if (next_id_ == invalid_raw_document_id)
			throw std::overflow_error("RawDocumentId space exhausted");

		const RawDocumentId id = next_id_;
		if (next_id_ == std::numeric_limits<RawDocumentId>::max())
			next_id_ = invalid_raw_document_id;
		else
			++next_id_;
		return id;
	}

	std::map<RawDocumentId, RawBytes> documents_;
	RawDocumentId next_id_ = 1;
};

} // namespace avm
