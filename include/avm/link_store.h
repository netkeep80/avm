#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace avm
{

using LinkId = std::uint64_t;
inline constexpr LinkId invalid_link_id = 0;

struct Link
{
    LinkId begin;
    LinkId end;

    bool operator==(const Link &) const = default;
};

class LinkStore
{
public:
    virtual ~LinkStore() = default;

    // A point is a self-link. It is the only bootstrap operation needed to
    // introduce a new independent identity into an otherwise link-only store.
    virtual LinkId create_point() = 0;

    // Return the canonical identity of (begin, end), creating it if necessary.
    virtual LinkId intern(LinkId begin, LinkId end) = 0;

    // Read operations never materialize missing links.
    virtual std::optional<LinkId> find(LinkId begin, LinkId end) const = 0;
    virtual Link get(LinkId id) const = 0;
    virtual std::vector<LinkId> outgoing(LinkId begin) const = 0;
    virtual std::vector<LinkId> incoming(LinkId end) const = 0;
    virtual bool contains(LinkId id) const = 0;
    virtual std::size_t size() const = 0;
};

class InMemoryLinkStore final : public LinkStore
{
public:
    LinkId create_point() override
    {
        const LinkId id = allocate_id();
        insert_link(id, Link{id, id});
        return id;
    }

    LinkId intern(LinkId begin, LinkId end) override
    {
        require_existing_endpoint(begin, "begin");
        require_existing_endpoint(end, "end");

        if (const auto existing = find(begin, end))
            return *existing;

        const LinkId id = allocate_id();
        insert_link(id, Link{begin, end});
        return id;
    }

    std::optional<LinkId> find(LinkId begin, LinkId end) const override
    {
        const auto it = exact_.find({begin, end});
        if (it == exact_.end())
            return std::nullopt;
        return it->second;
    }

    Link get(LinkId id) const override
    {
        const auto it = links_.find(id);
        if (it == links_.end())
            throw std::out_of_range("unknown LinkId");
        return it->second;
    }

    std::vector<LinkId> outgoing(LinkId begin) const override
    {
        const auto it = outgoing_.find(begin);
        if (it == outgoing_.end())
            return {};
        return it->second;
    }

    std::vector<LinkId> incoming(LinkId end) const override
    {
        const auto it = incoming_.find(end);
        if (it == incoming_.end())
            return {};
        return it->second;
    }

    bool contains(LinkId id) const override
    {
        return links_.contains(id);
    }

    std::size_t size() const override
    {
        return links_.size();
    }

private:
    using Pair = std::pair<LinkId, LinkId>;

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
        if (exact_.contains(pair))
            throw std::logic_error("attempt to insert duplicate canonical link");

        links_.emplace(id, link);
        exact_.emplace(pair, id);
        outgoing_[link.begin].push_back(id);
        incoming_[link.end].push_back(id);
    }

    LinkId next_id_{1};
    std::map<LinkId, Link> links_;
    std::map<Pair, LinkId> exact_;
    std::map<LinkId, std::vector<LinkId>> outgoing_;
    std::map<LinkId, std::vector<LinkId>> incoming_;
};

} // namespace avm
