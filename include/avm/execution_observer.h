#pragma once

#include "avm/link_store.h"
#include "avm/semantic_context.h"

#include <optional>
#include <utility>

namespace avm
{

struct ExecutionContext
{
	ExecutionContext(LinkId entity, LinkId relation, LinkId subject, LinkId object, std::optional<LinkId> parent,
	                 std::optional<LinkId> frame, SemanticContextView semantic = {})
	    : entity(entity), relation(relation), subject(subject), object(object), parent(parent), frame(frame),
	      semantic(std::move(semantic))
	{
	}

	LinkId entity;
	LinkId relation;
	LinkId subject;
	LinkId object;
	std::optional<LinkId> parent;
	std::optional<LinkId> frame;
	SemanticContextView semantic;

	bool operator==(const ExecutionContext &) const = default;
};

enum class ExecutionEventKind
{
	Enter,
	Return,
	Fail,
};

enum class ExecutionFailurePhase
{
	Dispatch,
	Handler,
	ResultValidation,
};

struct ExecutionEvent
{
	ExecutionEvent(ExecutionEventKind kind, ExecutionContext context, std::optional<LinkId> result,
	               std::optional<ExecutionFailurePhase> failure_phase = std::nullopt,
	               SemanticContextView semantic_result = {})
	    : kind(kind), context(std::move(context)), result(result), failure_phase(failure_phase),
	      semantic_result(std::move(semantic_result))
	{
	}

	ExecutionEventKind kind;
	ExecutionContext context;
	std::optional<LinkId> result;
	std::optional<ExecutionFailurePhase> failure_phase;
	SemanticContextView semantic_result;

	bool operator==(const ExecutionEvent &) const = default;
};

class ExecutionObserver
{
public:
	virtual ~ExecutionObserver() = default;
	virtual void observe(const ExecutionEvent &event) = 0;
};

} // namespace avm
