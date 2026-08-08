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
		if (!store_.contains(entity))
			throw std::invalid_argument("execution entity is not present in LinkStore");

		const RelationEntity decoded = decode_relation_entity(store_, entity);
		const ExecutionContext context{
		    entity, decoded.relation, decoded.subject, decoded.object, parent, frame,
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

private:
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
