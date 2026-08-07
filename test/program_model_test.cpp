#include "avm/program_model.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    avm::InMemoryLinkStore store;
    const avm::BootstrapVocabulary vocabulary = avm::BootstrapVocabulary::create(store);
    avm::ProgramBuilder builder(store, vocabulary);

    const std::vector<avm::LinkId> vocabulary_ids{
        vocabulary.unit,
        vocabulary.nil,
        vocabulary.true_value,
        vocabulary.false_value,
        vocabulary.quote_relation,
        vocabulary.parameter_relation,
        vocabulary.sequence_relation,
        vocabulary.not_relation,
        vocabulary.and_relation,
        vocabulary.or_relation,
        vocabulary.if_relation,
        vocabulary.function_relation,
        vocabulary.call_relation,
        vocabulary.binding_relation,
        vocabulary.frame_relation,
    };
    for (const avm::LinkId id : vocabulary_ids)
    {
        assert(store.contains(id));
        assert(store.get(id) == (avm::Link{id, id}));
    }

    assert(avm::encode_link_list(store, vocabulary.nil, {}) == vocabulary.nil);
    assert(avm::decode_link_list(store, vocabulary.nil, vocabulary.nil).empty());

    const avm::LinkId a = store.create_point();
    const avm::LinkId b = store.create_point();
    const avm::LinkId c = store.create_point();
    const std::vector<avm::LinkId> abc{a, b, c};
    const avm::LinkId abc_list = avm::encode_link_list(store, vocabulary.nil, abc);
    assert(avm::decode_link_list(store, vocabulary.nil, abc_list) == abc);
    const std::size_t before_list_reencode = store.size();
    assert(avm::encode_link_list(store, vocabulary.nil, abc) == abc_list);
    assert(store.size() == before_list_reencode);

    bool cycle_rejected = false;
    try
    {
        static_cast<void>(avm::decode_link_list(store, vocabulary.nil, a));
    }
    catch (const std::runtime_error &)
    {
        cycle_rejected = true;
    }
    assert(cycle_rejected);

    const avm::LinkId true_literal = builder.literal(vocabulary.true_value);
    const std::size_t before_literal_rebuild = store.size();
    assert(builder.literal(vocabulary.true_value) == true_literal);
    assert(store.size() == before_literal_rebuild);
    assert(avm::decode_relation_entity(store, true_literal) ==
           (avm::RelationEntity{vocabulary.quote_relation, vocabulary.unit, vocabulary.true_value}));

    const avm::LinkId false_literal = builder.literal(vocabulary.false_value);
    const avm::LinkId nested = builder.logical_not(builder.logical_and(true_literal, false_literal));
    const avm::RelationEntity nested_decoded = avm::decode_relation_entity(store, nested);
    assert(nested_decoded.relation == vocabulary.not_relation);
    assert(nested_decoded.subject == vocabulary.unit);
    const auto nested_args = avm::decode_link_list(store, vocabulary.nil, nested_decoded.object);
    assert(nested_args.size() == 1);

    const avm::LinkId if_expression = builder.conditional(true_literal, nested, false_literal);
    const auto if_decoded = avm::decode_relation_entity(store, if_expression);
    assert(if_decoded.relation == vocabulary.if_relation);
    assert(avm::decode_link_list(store, vocabulary.nil, if_decoded.object) ==
           (std::vector<avm::LinkId>{true_literal, nested, false_literal}));

    const avm::LinkId formal = store.create_point();
    const avm::LinkId parameter_expression = builder.parameter(formal);
    assert(avm::decode_relation_entity(store, parameter_expression) ==
           (avm::RelationEntity{vocabulary.parameter_relation, vocabulary.unit, formal}));

    const avm::LinkId function = builder.create_function_handle();
    const avm::LinkId definition = builder.define_function(function, {formal}, parameter_expression);
    const auto found = avm::find_function_definition(store, vocabulary, function);
    assert(found.has_value());
    assert(found->entity == definition);
    assert(found->handle == function);
    assert(found->parameters == (std::vector<avm::LinkId>{formal}));
    assert(found->body == parameter_expression);

    const std::size_t before_definition_rebuild = store.size();
    assert(builder.define_function(function, {formal}, parameter_expression) == definition);
    assert(store.size() == before_definition_rebuild);

    bool conflicting_definition_rejected = false;
    try
    {
        static_cast<void>(builder.define_function(function, {}, false_literal));
    }
    catch (const std::logic_error &)
    {
        conflicting_definition_rejected = true;
    }
    assert(conflicting_definition_rejected);

    const avm::LinkId call = builder.call(function, {true_literal});
    const avm::CallExpression decoded_call = avm::decode_call_expression(store, vocabulary, call);
    assert(decoded_call.function == function);
    assert(decoded_call.arguments == (std::vector<avm::LinkId>{true_literal}));

    const avm::LinkId sequence = builder.sequence({definition, call});
    const avm::RelationEntity sequence_decoded = avm::decode_relation_entity(store, sequence);
    assert(sequence_decoded.relation == vocabulary.sequence_relation);
    assert(avm::decode_link_list(store, vocabulary.nil, sequence_decoded.object) ==
           (std::vector<avm::LinkId>{definition, call}));

    bool unknown_value_rejected = false;
    try
    {
        static_cast<void>(builder.literal(999999));
    }
    catch (const std::invalid_argument &)
    {
        unknown_value_rejected = true;
    }
    assert(unknown_value_rejected);

    return 0;
}
