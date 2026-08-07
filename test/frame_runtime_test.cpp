#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <stdexcept>

namespace
{

avm::LinkId find_frame_for_function(
    const avm::LinkStore &store,
    const avm::BootstrapVocabulary &vocabulary,
    avm::LinkId function,
    bool require_parent)
{
    for (const avm::LinkId candidate : store.outgoing(vocabulary.frame_relation))
    {
        if (candidate == vocabulary.frame_relation)
            continue;

        const avm::RelationEntity row = avm::decode_relation_entity(store, candidate);
        if (row.relation != vocabulary.frame_relation)
            continue;

        const avm::DecodedCallFrame frame = avm::decode_call_frame(store, vocabulary, candidate);
        if (frame.function != function)
            continue;
        if (require_parent && frame.parent == vocabulary.nil)
            continue;
        return candidate;
    }
    return avm::invalid_link_id;
}

} // namespace

int main()
{
    avm::InMemoryLinkStore store;
    avm::BootstrapRuntime runtime(store, 16);
    avm::ProgramBuilder builder = runtime.builder();
    const avm::BootstrapVocabulary &v = runtime.vocabulary();
    const avm::LinkId t = builder.literal(v.true_value);

    const avm::LinkId formal = store.create_point();
    const avm::LinkId identity = builder.create_function_handle();
    const avm::LinkId parameter = builder.parameter(formal);
    builder.define_function(identity, {formal}, parameter);
    assert(runtime.execute(builder.call(identity, {t})) == v.true_value);

    const avm::LinkId identity_frame_id = find_frame_for_function(store, v, identity, false);
    assert(identity_frame_id != avm::invalid_link_id);
    const avm::DecodedCallFrame identity_frame = avm::decode_call_frame(store, v, identity_frame_id);
    assert(identity_frame.parent == v.nil);
    assert(identity_frame.function == identity);
    assert(identity_frame.bindings.size() == 1);

    const avm::RelationEntity binding = avm::decode_relation_entity(store, identity_frame.bindings[0]);
    assert(binding.relation == v.binding_relation);
    assert(binding.subject == formal);
    assert(binding.object == v.true_value);
    assert(runtime.executor().execute(parameter, std::nullopt, identity_frame_id) == v.true_value);

    const avm::LinkId outer_formal = store.create_point();
    const avm::LinkId inner_formal = store.create_point();
    const avm::LinkId inner = builder.create_function_handle();
    builder.define_function(inner, {inner_formal}, builder.parameter(inner_formal));

    const avm::LinkId outer = builder.create_function_handle();
    builder.define_function(
        outer,
        {outer_formal},
        builder.call(inner, {builder.parameter(outer_formal)}));
    assert(runtime.execute(builder.call(outer, {t})) == v.true_value);

    const avm::LinkId nested_frame_id = find_frame_for_function(store, v, inner, true);
    assert(nested_frame_id != avm::invalid_link_id);
    const avm::DecodedCallFrame nested_frame = avm::decode_call_frame(store, v, nested_frame_id);
    assert(nested_frame.parent != v.nil);
    const avm::DecodedCallFrame parent_frame = avm::decode_call_frame(store, v, nested_frame.parent);
    assert(parent_frame.function == outer);

    bool vocabulary_point_rejected = false;
    try
    {
        static_cast<void>(avm::decode_call_frame(store, v, v.frame_relation));
    }
    catch (const std::runtime_error &)
    {
        vocabulary_point_rejected = true;
    }
    assert(vocabulary_point_rejected);

    const avm::LinkId fake_binding = store.create_point();
    const avm::LinkId bad_binding_list = avm::encode_link_list(store, v.nil, {fake_binding});
    const avm::LinkId bad_binding_payload = store.intern(identity, bad_binding_list);
    const avm::LinkId bad_binding_frame = avm::encode_relation_entity(
        store, avm::RelationEntity{v.frame_relation, v.nil, bad_binding_payload});
    bool non_binding_rejected = false;
    try
    {
        static_cast<void>(runtime.executor().execute(parameter, std::nullopt, bad_binding_frame));
    }
    catch (const std::runtime_error &)
    {
        non_binding_rejected = true;
    }
    assert(non_binding_rejected);

    const avm::LinkId invalid_parent = store.create_point();
    const avm::LinkId empty_payload = store.intern(identity, v.nil);
    const avm::LinkId invalid_parent_frame = avm::encode_relation_entity(
        store, avm::RelationEntity{v.frame_relation, invalid_parent, empty_payload});
    bool invalid_parent_rejected = false;
    try
    {
        static_cast<void>(runtime.executor().execute(parameter, std::nullopt, invalid_parent_frame));
    }
    catch (const std::runtime_error &)
    {
        invalid_parent_rejected = true;
    }
    assert(invalid_parent_rejected);

    bool zero_depth_rejected = false;
    try
    {
        avm::InMemoryLinkStore another_store;
        avm::BootstrapRuntime invalid_runtime(another_store, 0);
        static_cast<void>(invalid_runtime);
    }
    catch (const std::invalid_argument &)
    {
        zero_depth_rejected = true;
    }
    assert(zero_depth_rejected);

    return 0;
}
