#pragma once

#include "avm/execution_observer.h"
#include "avm/relations_model.h"

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace avm
{

class Executor;
using NativeRelationHandler = std::function<LinkId(const ExecutionContext &, Executor &)>;

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

	LinkId execute(LinkId entity, std::optional<LinkId> parent = std::nullopt,
	               std::optional<LinkId> frame = std::nullopt)
	{
		return execute_impl(entity, parent, frame, SemanticContextView{});
	}

	LinkId execute_in_context(LinkId entity, const SemanticContextView &semantic)
	{
		return execute_impl(entity, std::nullopt, std::nullopt, semantic);
	}

	LinkId execute_in_context(LinkId entity, const SemanticContextView &semantic, std::optional<LinkId> parent,
	                          std::optional<LinkId> frame)
	{
		return execute_impl(entity, parent, frame, semantic);
	}

	LinkId execute_same_semantic_context(LinkId entity, const ExecutionContext &parent_context)
	{
		if (!parent_context.semantic)
			throw std::logic_error("parent execution context has no semantic context");
		return execute_in_context(entity, parent_context.semantic, parent_context.entity, parent_context.frame);
	}

	LinkId execute_child_semantic_context(LinkId entity, const ExecutionContext &parent_context,
	                                      SemanticContextFrame child_frame)
	{
		if (!parent_context.semantic)
			throw std::logic_error("parent execution context has no semantic context");
		const SemanticContextView child = parent_context.semantic.child(std::move(child_frame));
		return execute_in_context(entity, child, parent_context.entity, parent_context.frame);
	}

private:
	LinkId execute_impl(LinkId entity, std::optional<LinkId> parent, std::optional<LinkId> frame,
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

		LinkId result = invalid_link_id;
		try
		{
			result = handler->second(context, *this);
		}
		catch (...)
		{
			notify(ExecutionEvent{ExecutionEventKind::Fail, context, std::nullopt, ExecutionFailurePhase::Handler});
			throw;
		}

		if (!store_.contains(result))
		{
			notify(ExecutionEvent{ExecutionEventKind::Fail, context, std::nullopt,
			                      ExecutionFailurePhase::ResultValidation});
			throw std::runtime_error("native relation returned an unknown LinkId");
		}

		notify(ExecutionEvent{ExecutionEventKind::Return, context, result});
		return result;
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

} // namespace avm
