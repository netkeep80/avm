#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <stdexcept>

int main()
{
    avm::InMemoryLinkStore store;
    avm::BootstrapRuntime runtime(store);
    avm::ProgramBuilder builder = runtime.builder();
    const avm::BootstrapVocabulary &v = runtime.vocabulary();

    const avm::LinkId t = builder.literal(v.true_value);
    const avm::LinkId f = builder.literal(v.false_value);
    assert(runtime.execute(t) == v.true_value);
    assert(runtime.execute(f) == v.false_value);

    assert(runtime.execute(builder.logical_not(t)) == v.false_value);
    assert(runtime.execute(builder.logical_not(f)) == v.true_value);

    assert(runtime.execute(builder.logical_and(f, f)) == v.false_value);
    assert(runtime.execute(builder.logical_and(f, t)) == v.false_value);
    assert(runtime.execute(builder.logical_and(t, f)) == v.false_value);
    assert(runtime.execute(builder.logical_and(t, t)) == v.true_value);

    assert(runtime.execute(builder.logical_or(f, f)) == v.false_value);
    assert(runtime.execute(builder.logical_or(f, t)) == v.true_value);
    assert(runtime.execute(builder.logical_or(t, f)) == v.true_value);
    assert(runtime.execute(builder.logical_or(t, t)) == v.true_value);

    const avm::LinkId nested = builder.logical_not(builder.logical_and(t, f));
    const std::size_t before_nested_execute = store.size();
    assert(runtime.execute(nested) == v.true_value);
    assert(store.size() == before_nested_execute);

    const avm::LinkId arbitrary_value = store.create_point();
    const avm::LinkId invalid_not = builder.logical_not(builder.literal(arbitrary_value));
    const std::size_t before_invalid_not = store.size();
    bool invalid_boolean_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(invalid_not));
    }
    catch (const std::runtime_error &)
    {
        invalid_boolean_rejected = true;
    }
    assert(invalid_boolean_rejected);
    assert(store.size() == before_invalid_not);

    const avm::LinkId unknown_relation = store.create_point();
    const avm::LinkId failing_branch = avm::encode_relation_entity(
        store, avm::RelationEntity{unknown_relation, v.unit, v.false_value});

    const avm::LinkId lazy_true = builder.conditional(t, f, failing_branch);
    assert(runtime.execute(lazy_true) == v.false_value);

    const avm::LinkId lazy_false = builder.conditional(f, failing_branch, t);
    assert(runtime.execute(lazy_false) == v.true_value);

    const avm::LinkId selected_failure = builder.conditional(t, failing_branch, f);
    bool selected_branch_executed = false;
    try
    {
        static_cast<void>(runtime.execute(selected_failure));
    }
    catch (const std::runtime_error &)
    {
        selected_branch_executed = true;
    }
    assert(selected_branch_executed);

    const avm::LinkId invalid_condition = builder.conditional(builder.literal(arbitrary_value), t, f);
    bool invalid_condition_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(invalid_condition));
    }
    catch (const std::runtime_error &)
    {
        invalid_condition_rejected = true;
    }
    assert(invalid_condition_rejected);

    const avm::LinkId sequence = builder.sequence({t, f, nested});
    assert(runtime.execute(sequence) == v.true_value);
    assert(runtime.execute(builder.sequence({})) == v.nil);

    const avm::LinkId malformed_not = avm::encode_relation_entity(
        store, avm::RelationEntity{v.not_relation, v.unit, v.nil});
    bool arity_rejected = false;
    try
    {
        static_cast<void>(runtime.execute(malformed_not));
    }
    catch (const std::runtime_error &)
    {
        arity_rejected = true;
    }
    assert(arity_rejected);

    return 0;
}
