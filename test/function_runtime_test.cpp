#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <stdexcept>

int main()
{
    avm::InMemoryLinkStore store;
    avm::BootstrapRuntime runtime(store, 8);
    avm::ProgramBuilder builder = runtime.builder();
    const avm::BootstrapVocabulary &v = runtime.vocabulary();
    const avm::LinkId t = builder.literal(v.true_value);
    const avm::LinkId f = builder.literal(v.false_value);

    const avm::LinkId x = store.create_point();
    const avm::LinkId identity = builder.create_function_handle();
    const avm::LinkId identity_body = builder.parameter(x);
    const avm::LinkId identity_definition = builder.define_function(identity, {x}, identity_body);
    assert(runtime.execute(identity_definition) == identity);
    assert(runtime.execute(builder.call(identity, {t})) == v.true_value);
    assert(runtime.execute(builder.call(identity, {f})) == v.false_value);

    const avm::LinkId a = store.create_point();
    const avm::LinkId b = store.create_point();
    const avm::LinkId conjunction = builder.create_function_handle();
    builder.define_function(
        conjunction,
        {a, b},
        builder.logical_and(builder.parameter(a), builder.parameter(b)));
    assert(runtime.execute(builder.call(conjunction, {t, t})) == v.true_value);
    assert(runtime.execute(builder.call(conjunction, {t, f})) == v.false_value);

    const avm::LinkId inner_x = store.create_point();
    const avm::LinkId inner = builder.create_function_handle();
    builder.define_function(inner, {inner_x}, builder.parameter(inner_x));

    const avm::LinkId outer_x = store.create_point();
    const avm::LinkId outer = builder.create_function_handle();
    builder.define_function(outer, {outer_x}, builder.call(inner, {t}));
    assert(runtime.execute(builder.call(outer, {f})) == v.true_value);

    const avm::LinkId recurse_flag = store.create_point();
    const avm::LinkId finite_recursive = builder.create_function_handle();
    const avm::LinkId finite_body = builder.conditional(
        builder.parameter(recurse_flag),
        builder.call(finite_recursive, {f}),
        t);
    builder.define_function(finite_recursive, {recurse_flag}, finite_body);
    assert(runtime.execute(builder.call(finite_recursive, {t})) == v.true_value);

    const avm::LinkId infinite_param = store.create_point();
    const avm::LinkId infinite = builder.create_function_handle();
    const avm::LinkId infinite_body = builder.call(infinite, {t});
    builder.define_function(infinite, {infinite_param}, infinite_body);
    bool depth_guard_triggered = false;
    try
    {
        static_cast<void>(runtime.execute(builder.call(infinite, {t})));
    }
    catch (const std::runtime_error &)
    {
        depth_guard_triggered = true;
    }
    assert(depth_guard_triggered);

    bool arity_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(builder.call(identity, {})));
    }
    catch (const std::runtime_error &)
    {
        arity_rejected = true;
    }
    assert(arity_rejected);

    const avm::LinkId undefined_function = builder.create_function_handle();
    bool undefined_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(builder.call(undefined_function, {t})));
    }
    catch (const std::runtime_error &)
    {
        undefined_rejected = true;
    }
    assert(undefined_rejected);

    const avm::LinkId unbound_formal = store.create_point();
    const avm::LinkId unbound_parameter = builder.parameter(unbound_formal);
    bool unbound_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(unbound_parameter));
    }
    catch (const std::runtime_error &)
    {
        unbound_rejected = true;
    }
    assert(unbound_rejected);

    const avm::LinkId malformed_frame = store.create_point();
    bool malformed_frame_rejected = false;
    try
    {
        static_cast<void>(runtime.executor().execute(unbound_parameter, std::nullopt, malformed_frame));
    }
    catch (const std::runtime_error &)
    {
        malformed_frame_rejected = true;
    }
    assert(malformed_frame_rejected);

    const avm::LinkId malformed_handle = builder.create_function_handle();
    const avm::LinkId malformed_payload = store.create_point();
    avm::encode_relation_entity(
        store, avm::RelationEntity{v.function_relation, malformed_handle, malformed_payload});
    bool malformed_definition_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(builder.call(malformed_handle, {})));
    }
    catch (const std::runtime_error &)
    {
        malformed_definition_rejected = true;
    }
    assert(malformed_definition_rejected);

    const auto binding_candidates = store.outgoing(v.binding_relation);
    const auto frame_candidates = store.outgoing(v.frame_relation);
    assert(binding_candidates.size() > 1);
    assert(frame_candidates.size() > 1);

    bool saw_identity_binding = false;
    for (const avm::LinkId candidate : binding_candidates)
    {
        const avm::RelationEntity row = avm::decode_relation_entity(store, candidate);
        if (row.relation == v.binding_relation && row.subject == x)
            saw_identity_binding = true;
    }
    assert(saw_identity_binding);

    bool saw_identity_frame = false;
    for (const avm::LinkId candidate : frame_candidates)
    {
        if (candidate == v.frame_relation)
            continue;
        const avm::RelationEntity row = avm::decode_relation_entity(store, candidate);
        if (row.relation != v.frame_relation)
            continue;
        const avm::DecodedCallFrame frame = avm::decode_call_frame(store, v, candidate);
        if (frame.function == identity)
            saw_identity_frame = true;
    }
    assert(saw_identity_frame);

    return 0;
}
