#pragma once

#include "avm/link_store.h"

#include <optional>

namespace avm
{

struct RelationEntity
{
    LinkId relation;
    LinkId subject;
    LinkId object;

    bool operator==(const RelationEntity &) const = default;
};

// Canonical Relations Model projection:
//
//   (relation, subject, object)
//       == (relation, (subject, object))
//
// The inner subject-object dyad and the outer entity dyad are both interned,
// so repeated encoding reuses the same canonical LinkIds.
inline LinkId encode_relation_entity(LinkStore &store, const RelationEntity &entity)
{
    const LinkId subject_object = store.intern(entity.subject, entity.object);
    return store.intern(entity.relation, subject_object);
}

// Look up an already materialized Relations Model entity without creating the
// inner subject-object dyad or the outer entity dyad.
inline std::optional<LinkId> find_relation_entity(const LinkStore &store, const RelationEntity &entity)
{
    const auto subject_object = store.find(entity.subject, entity.object);
    if (!subject_object)
        return std::nullopt;

    return store.find(entity.relation, *subject_object);
}

inline RelationEntity decode_relation_entity(const LinkStore &store, LinkId entity)
{
    const Link outer = store.get(entity);
    const Link subject_object = store.get(outer.end);

    return RelationEntity{
        .relation = outer.begin,
        .subject = subject_object.begin,
        .object = subject_object.end,
    };
}

} // namespace avm
