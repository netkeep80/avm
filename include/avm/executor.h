#pragma once

#include "avm/execution_observer.h"
#include "avm/execution_outcome.h"
#include "avm/relations_model.h"
#include "avm/text_value.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avm
{

class Executor;
using NativeRelationHandler = std::function<ExecutionOutcome(const ExecutionContext &, Executor &)>;

class Executor
{
public:
	explicit Executor(LinkStore &store, ExecutionObserver *observer = nullptr) : store_(store), observer_(observer) {}

	Executor(const Executor &) = delete;
	Executor &operator=(const Executor &) = delete;
	Executor(Executor &&) = delete;
	Executor &operator=(Executor &&) = delete;

	LinkStore &store() { return store_; }

	const LinkStore &store() const { return store_; }

	void set_observer(ExecutionObserver *observer) noexcept { observer_ = observer; }

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

	bool has_native(LinkId relation) const { return native_handlers_.contains(relation); }

	ExecutionOutcome execute_outcome(LinkId entity, std::optional<LinkId> parent = std::nullopt,
	                                 std::optional<LinkId> frame = std::nullopt)
	{
		return execute_impl(entity, parent, frame, SemanticContextView{});
	}

	LinkId execute(LinkId entity, std::optional<LinkId> parent = std::nullopt,
	               std::optional<LinkId> frame = std::nullopt)
	{
		return execute_outcome(entity, parent, frame).result;
	}

	ExecutionOutcome execute_outcome_in_context(LinkId entity, const SemanticContextView &semantic)
	{
		return execute_impl(entity, std::nullopt, std::nullopt, semantic);
	}

	ExecutionOutcome execute_outcome_in_context(LinkId entity, const SemanticContextView &semantic,
	                                            std::optional<LinkId> parent, std::optional<LinkId> frame)
	{
		return execute_impl(entity, parent, frame, semantic);
	}

	LinkId execute_in_context(LinkId entity, const SemanticContextView &semantic)
	{
		return execute_outcome_in_context(entity, semantic).result;
	}

	LinkId execute_in_context(LinkId entity, const SemanticContextView &semantic, std::optional<LinkId> parent,
	                          std::optional<LinkId> frame)
	{
		return execute_outcome_in_context(entity, semantic, parent, frame).result;
	}

	ExecutionOutcome execute_same_semantic_context_outcome(LinkId entity, const ExecutionContext &parent_context)
	{
		if (!parent_context.semantic)
			throw std::logic_error("parent execution context has no semantic context");
		return execute_outcome_in_context(entity, parent_context.semantic, parent_context.entity, parent_context.frame);
	}

	LinkId execute_same_semantic_context(LinkId entity, const ExecutionContext &parent_context)
	{
		return execute_same_semantic_context_outcome(entity, parent_context).result;
	}

	ExecutionOutcome execute_child_semantic_context_outcome(LinkId entity, const ExecutionContext &parent_context,
	                                                        SemanticContextFrame child_frame)
	{
		if (!parent_context.semantic)
			throw std::logic_error("parent execution context has no semantic context");
		const SemanticContextView child = parent_context.semantic.child(std::move(child_frame));
		return execute_outcome_in_context(entity, child, parent_context.entity, parent_context.frame);
	}

	LinkId execute_child_semantic_context(LinkId entity, const ExecutionContext &parent_context,
	                                      SemanticContextFrame child_frame)
	{
		return execute_child_semantic_context_outcome(entity, parent_context, std::move(child_frame)).result;
	}

private:
	ExecutionOutcome execute_impl(LinkId entity, std::optional<LinkId> parent, std::optional<LinkId> frame,
	                              SemanticContextView semantic)
	{
		if (!store_.contains(entity))
			throw std::invalid_argument("execution entity is not present in LinkStore");

		const RelationEntity decoded = decode_relation_entity(store_, entity);
		const ExecutionContext context{
		    entity, decoded.relation, decoded.subject, decoded.object, parent, frame, std::move(semantic),
		};
		notify(ExecutionEvent{ExecutionEventKind::Enter, context, std::nullopt});

		const auto handler = native_handlers_.find(context.relation);
		if (handler == native_handlers_.end())
		{
			notify(ExecutionEvent{ExecutionEventKind::Fail, context, std::nullopt, ExecutionFailurePhase::Dispatch});
			throw std::runtime_error("unknown relation LinkId: " + std::to_string(context.relation));
		}

		ExecutionOutcome outcome{invalid_link_id};
		try
		{
			outcome = handler->second(context, *this);
		}
		catch (...)
		{
			notify(ExecutionEvent{ExecutionEventKind::Fail, context, std::nullopt, ExecutionFailurePhase::Handler});
			throw;
		}

		if (!store_.contains(outcome.result))
		{
			notify(ExecutionEvent{ExecutionEventKind::Fail, context, std::nullopt,
			                      ExecutionFailurePhase::ResultValidation});
			throw std::runtime_error("native relation returned an unknown LinkId");
		}

		if (!outcome.semantic)
			outcome.semantic = context.semantic;

		notify(ExecutionEvent{ExecutionEventKind::Return, context, outcome.result, std::nullopt, outcome.semantic});
		return outcome;
	}

	void notify(const ExecutionEvent &event) noexcept
	{
		if (observer_ == nullptr)
			return;

		try
		{
			observer_->observe(event);
		}
		catch (...)
		{
		}
	}

	LinkStore &store_;
	ExecutionObserver *observer_ = nullptr;
	std::map<LinkId, NativeRelationHandler> native_handlers_;
};

class EffectCapabilityDenied final : public std::runtime_error
{
public:
	EffectCapabilityDenied() : std::runtime_error("effect capability denied") {}
};

class EffectProviderUnavailable final : public std::runtime_error
{
public:
	EffectProviderUnavailable() : std::runtime_error("effect provider unavailable") {}
};

class EffectLookupMiss final : public std::runtime_error
{
public:
	EffectLookupMiss() : std::runtime_error("external entity lookup miss") {}
};

class EffectCapabilityPolicy
{
public:
	EffectCapabilityPolicy() = default;
	explicit EffectCapabilityPolicy(std::set<LinkId> allowed) : allowed_(std::move(allowed)) {}

	bool allows(LinkId capability) const noexcept { return allowed_.contains(capability); }

private:
	std::set<LinkId> allowed_;
};

class ExternalEntityProvider
{
public:
	virtual ~ExternalEntityProvider() = default;
	virtual std::optional<LinkId> lookup(std::string_view name) = 0;
};

struct ExternalEntityLookupEffect
{
	LinkId relation;
	LinkId capability;
	LinkId unit;
	TextVocabulary text;
};

inline void register_external_entity_lookup_effect(Executor &executor, const ExternalEntityLookupEffect &effect,
                                                   const EffectCapabilityPolicy &policy,
                                                   ExternalEntityProvider *provider)
{
	const LinkStore &store = executor.store();
	if (!store.contains(effect.relation))
		throw std::invalid_argument("effect relation is not present in LinkStore");
	if (!store.contains(effect.capability))
		throw std::invalid_argument("effect capability is not present in LinkStore");
	if (!store.contains(effect.unit))
		throw std::invalid_argument("effect unit is not present in LinkStore");
	validate_text_vocabulary(store, effect.text);

	// Authority фиксируется в момент binding: handler не должен зависеть от lifetime локального policy объекта.
	executor.register_native(
	    effect.relation,
	    [effect, policy, provider](const ExecutionContext &context, Executor &runtime) -> ExecutionOutcome
	    {
		    if (context.subject != effect.unit)
			    throw std::runtime_error("external entity lookup is not an executable effect expression");
		    if (!policy.allows(effect.capability))
			    throw EffectCapabilityDenied();
		    if (provider == nullptr)
			    throw EffectProviderUnavailable();

		    const std::vector<std::uint8_t> bytes = decode_text(runtime.store(), effect.text, context.object);
		    const std::string name(bytes.begin(), bytes.end());
		    const std::optional<LinkId> result = provider->lookup(name);
		    if (!result)
			    throw EffectLookupMiss();

		    // Внешний lookup наблюдает host data, но не получает права materialize-ить graph AVM.
		    if (!runtime.store().contains(*result))
			    throw std::runtime_error("external entity provider returned an unknown LinkId");

		    return ExecutionOutcome{*result};
	    });
}

} // namespace avm
