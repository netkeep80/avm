#pragma once

#include "avm/relations_model.h"

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace avm
{

struct ExecutionContext
{
    LinkId entity;
    LinkId relation;
    LinkId subject;
    LinkId object;
    std::optional<LinkId> parent;
    std::optional<LinkId> frame;
};

class Executor;
using NativeRelationHandler = std::function<LinkId(const ExecutionContext &, Executor &)>;

class Executor
{
public:
    explicit Executor(LinkStore &store)
        : store_(store)
    {
    }

    LinkStore &store()
    {
        return store_;
    }

    const LinkStore &store() const
    {
        return store_;
    }

    void register_native(LinkId relation, NativeRelationHandler handler)
    {
        if (!store_.contains(relation))
            throw std::invalid_argument("native relation is not present in LinkStore");
        if (!handler)
            throw std::invalid_argument("native relation handler is empty");

        const bool inserted = native_handlers_.emplace(relation, std::move(handler)).second;
        if (!inserted)
            throw std::logic_error("native relation handler is already registered");
    }

    bool has_native(LinkId relation) const
    {
        return native_handlers_.contains(relation);
    }

    LinkId execute(
        LinkId entity,
        std::optional<LinkId> parent = std::nullopt,
        std::optional<LinkId> frame = std::nullopt)
    {
        if (!store_.contains(entity))
            throw std::invalid_argument("execution entity is not present in LinkStore");

        const RelationEntity decoded = decode_relation_entity(store_, entity);
        const ExecutionContext context{
            entity,
            decoded.relation,
            decoded.subject,
            decoded.object,
            parent,
            frame,
        };

        const auto handler = native_handlers_.find(context.relation);
        if (handler == native_handlers_.end())
            throw std::runtime_error("unknown relation LinkId: " + std::to_string(context.relation));

        const LinkId result = handler->second(context, *this);
        if (!store_.contains(result))
            throw std::runtime_error("native relation returned an unknown LinkId");

        return result;
    }

private:
    LinkStore &store_;
    std::map<LinkId, NativeRelationHandler> native_handlers_;
};

} // namespace avm
