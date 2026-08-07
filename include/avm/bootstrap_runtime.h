#pragma once

#include "avm/executor.h"
#include "avm/program_model.h"

#include <optional>
#include <stdexcept>
#include <vector>

namespace avm
{

inline std::optional<LinkId> lookup_relation_value(
    const LinkStore &store, LinkId relation, LinkId subject)
{
    std::optional<LinkId> result;
    for (const LinkId candidate : store.outgoing(relation))
    {
        const RelationEntity decoded = decode_relation_entity(store, candidate);
        if (decoded.relation != relation || decoded.subject != subject)
            continue;

        if (result && *result != decoded.object)
            throw std::logic_error("relation lookup is not functional for the requested subject");
        result = decoded.object;
    }
    return result;
}

inline std::optional<LinkId> lookup_binary_relation_value(
    const LinkStore &store, LinkId relation, LinkId left, LinkId right)
{
    const auto pair = store.find(left, right);
    if (!pair)
        return std::nullopt;
    return lookup_relation_value(store, relation, *pair);
}

class BootstrapRuntime
{
public:
    explicit BootstrapRuntime(LinkStore &store)
        : store_(store)
        , vocabulary_(BootstrapVocabulary::create(store))
        , executor_(store)
    {
        materialize_truth_tables();
        register_handlers();
    }

    const BootstrapVocabulary &vocabulary() const
    {
        return vocabulary_;
    }

    ProgramBuilder builder()
    {
        return ProgramBuilder(store_, vocabulary_);
    }

    Executor &executor()
    {
        return executor_;
    }

    const Executor &executor() const
    {
        return executor_;
    }

    LinkId execute(LinkId root)
    {
        return executor_.execute(root);
    }

private:
    static void require_expression_subject(const ExecutionContext &context, LinkId unit)
    {
        if (context.subject != unit)
            throw std::runtime_error("runtime relation entity is not an executable expression");
    }

    std::vector<LinkId> expression_arguments(const ExecutionContext &context, std::size_t expected) const
    {
        require_expression_subject(context, vocabulary_.unit);
        std::vector<LinkId> arguments = decode_link_list(store_, vocabulary_.nil, context.object);
        if (arguments.size() != expected)
            throw std::runtime_error("expression has unexpected argument count");
        return arguments;
    }

    LinkId require_lookup(const std::optional<LinkId> &value, const char *message) const
    {
        if (!value)
            throw std::runtime_error(message);
        return *value;
    }

    void materialize_truth_tables()
    {
        encode_relation_entity(
            store_, RelationEntity{vocabulary_.not_relation, vocabulary_.true_value, vocabulary_.false_value});
        encode_relation_entity(
            store_, RelationEntity{vocabulary_.not_relation, vocabulary_.false_value, vocabulary_.true_value});

        const auto add_binary_row = [this](LinkId relation, LinkId left, LinkId right, LinkId result) {
            const LinkId key = store_.intern(left, right);
            encode_relation_entity(store_, RelationEntity{relation, key, result});
        };

        add_binary_row(
            vocabulary_.and_relation,
            vocabulary_.false_value,
            vocabulary_.false_value,
            vocabulary_.false_value);
        add_binary_row(
            vocabulary_.and_relation,
            vocabulary_.false_value,
            vocabulary_.true_value,
            vocabulary_.false_value);
        add_binary_row(
            vocabulary_.and_relation,
            vocabulary_.true_value,
            vocabulary_.false_value,
            vocabulary_.false_value);
        add_binary_row(
            vocabulary_.and_relation,
            vocabulary_.true_value,
            vocabulary_.true_value,
            vocabulary_.true_value);

        add_binary_row(
            vocabulary_.or_relation,
            vocabulary_.false_value,
            vocabulary_.false_value,
            vocabulary_.false_value);
        add_binary_row(
            vocabulary_.or_relation,
            vocabulary_.false_value,
            vocabulary_.true_value,
            vocabulary_.true_value);
        add_binary_row(
            vocabulary_.or_relation,
            vocabulary_.true_value,
            vocabulary_.false_value,
            vocabulary_.true_value);
        add_binary_row(
            vocabulary_.or_relation,
            vocabulary_.true_value,
            vocabulary_.true_value,
            vocabulary_.true_value);

        encode_relation_entity(
            store_, RelationEntity{vocabulary_.if_relation, vocabulary_.true_value, vocabulary_.true_value});
        encode_relation_entity(
            store_, RelationEntity{vocabulary_.if_relation, vocabulary_.false_value, vocabulary_.false_value});
    }

    void register_handlers()
    {
        executor_.register_native(
            vocabulary_.quote_relation,
            [this](const ExecutionContext &context, Executor &) {
                require_expression_subject(context, vocabulary_.unit);
                return context.object;
            });

        executor_.register_native(
            vocabulary_.sequence_relation,
            [this](const ExecutionContext &context, Executor &executor) {
                require_expression_subject(context, vocabulary_.unit);
                const std::vector<LinkId> expressions =
                    decode_link_list(store_, vocabulary_.nil, context.object);

                LinkId result = vocabulary_.nil;
                for (const LinkId expression : expressions)
                    result = executor.execute(expression, context.entity);
                return result;
            });

        executor_.register_native(
            vocabulary_.not_relation,
            [this](const ExecutionContext &context, Executor &executor) {
                const std::vector<LinkId> arguments = expression_arguments(context, 1);
                const LinkId value = executor.execute(arguments[0], context.entity);
                return require_lookup(
                    lookup_relation_value(store_, vocabulary_.not_relation, value),
                    "NOT operand is not a Boolean value");
            });

        executor_.register_native(
            vocabulary_.and_relation,
            [this](const ExecutionContext &context, Executor &executor) {
                const std::vector<LinkId> arguments = expression_arguments(context, 2);
                const LinkId left = executor.execute(arguments[0], context.entity);
                const LinkId right = executor.execute(arguments[1], context.entity);
                return require_lookup(
                    lookup_binary_relation_value(store_, vocabulary_.and_relation, left, right),
                    "AND operands are not Boolean values");
            });

        executor_.register_native(
            vocabulary_.or_relation,
            [this](const ExecutionContext &context, Executor &executor) {
                const std::vector<LinkId> arguments = expression_arguments(context, 2);
                const LinkId left = executor.execute(arguments[0], context.entity);
                const LinkId right = executor.execute(arguments[1], context.entity);
                return require_lookup(
                    lookup_binary_relation_value(store_, vocabulary_.or_relation, left, right),
                    "OR operands are not Boolean values");
            });

        executor_.register_native(
            vocabulary_.if_relation,
            [this](const ExecutionContext &context, Executor &executor) {
                const std::vector<LinkId> arguments = expression_arguments(context, 3);
                const LinkId condition = executor.execute(arguments[0], context.entity);
                const LinkId selected = require_lookup(
                    lookup_relation_value(store_, vocabulary_.if_relation, condition),
                    "If condition is not a Boolean value");

                if (selected == vocabulary_.true_value)
                    return executor.execute(arguments[1], context.entity);
                if (selected == vocabulary_.false_value)
                    return executor.execute(arguments[2], context.entity);
                throw std::logic_error("If truth table returned a non-Boolean selector");
            });
    }

    LinkStore &store_;
    BootstrapVocabulary vocabulary_;
    Executor executor_;
};

} // namespace avm
