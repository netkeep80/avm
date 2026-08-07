#include "avm/executor.h"

#include <cassert>
#include <optional>
#include <stdexcept>

int main()
{
    avm::InMemoryLinkStore store;

    const avm::LinkId identity_relation = store.create_point();
    const avm::LinkId subject_relation = store.create_point();
    const avm::LinkId unknown_relation = store.create_point();
    const avm::LinkId subject = store.create_point();
    const avm::LinkId object = store.create_point();

    avm::Executor executor(store);

    executor.register_native(
        identity_relation,
        [](const avm::ExecutionContext &context, avm::Executor &) {
            return context.object;
        });

    executor.register_native(
        subject_relation,
        [subject](const avm::ExecutionContext &context, avm::Executor &) {
            assert(context.subject == subject);
            return context.subject;
        });

    const avm::LinkId identity_entity = avm::encode_relation_entity(
        store, avm::RelationEntity{identity_relation, subject, object});

    assert(executor.execute(identity_entity) == object);

    std::optional<avm::ExecutionContext> captured_context;
    const avm::LinkId parent_relation = store.create_point();
    executor.register_native(
        parent_relation,
        [&captured_context](const avm::ExecutionContext &context, avm::Executor &) {
            captured_context = context;
            return context.object;
        });

    const avm::LinkId parent_entity = avm::encode_relation_entity(
        store, avm::RelationEntity{parent_relation, subject, object});

    assert(executor.execute(parent_entity, identity_entity) == object);
    assert(captured_context.has_value());
    assert(captured_context->entity == parent_entity);
    assert(captured_context->relation == parent_relation);
    assert(captured_context->subject == subject);
    assert(captured_context->object == object);
    assert(captured_context->parent == identity_entity);
    assert(!captured_context->frame.has_value());

    const avm::LinkId frame = store.create_point();
    assert(executor.execute(parent_entity, identity_entity, frame) == object);
    assert(captured_context.has_value());
    assert(captured_context->parent == identity_entity);
    assert(captured_context->frame == frame);

    const avm::LinkId subject_entity = avm::encode_relation_entity(
        store, avm::RelationEntity{subject_relation, subject, object});
    assert(executor.execute(subject_entity) == subject);

    const avm::LinkId unknown_entity = avm::encode_relation_entity(
        store, avm::RelationEntity{unknown_relation, subject, object});
    const std::size_t before_unknown_execute = store.size();

    bool unknown_rejected = false;
    try
    {
        static_cast<void>(executor.execute(unknown_entity));
    }
    catch (const std::runtime_error &)
    {
        unknown_rejected = true;
    }
    assert(unknown_rejected);
    assert(store.size() == before_unknown_execute);

    bool duplicate_handler_rejected = false;
    try
    {
        executor.register_native(
            identity_relation,
            [](const avm::ExecutionContext &context, avm::Executor &) {
                return context.object;
            });
    }
    catch (const std::logic_error &)
    {
        duplicate_handler_rejected = true;
    }
    assert(duplicate_handler_rejected);

    const avm::LinkId invalid_result_relation = store.create_point();
    executor.register_native(
        invalid_result_relation,
        [](const avm::ExecutionContext &, avm::Executor &) {
            return static_cast<avm::LinkId>(999999);
        });
    const avm::LinkId invalid_result_entity = avm::encode_relation_entity(
        store, avm::RelationEntity{invalid_result_relation, subject, object});

    bool invalid_result_rejected = false;
    try
    {
        static_cast<void>(executor.execute(invalid_result_entity));
    }
    catch (const std::runtime_error &)
    {
        invalid_result_rejected = true;
    }
    assert(invalid_result_rejected);

    return 0;
}
