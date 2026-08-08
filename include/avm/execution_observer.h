#pragma once

#include "avm/link_store.h"
#include "avm/semantic_context.h"

#include <optional>

namespace avm
{

struct ExecutionContext
{
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
	ExecutionEventKind kind;
	ExecutionContext context;
	std::optional<LinkId> result;
	std::optional<ExecutionFailurePhase> failure_phase = std::nullopt;

	bool operator==(const ExecutionEvent &) const = default;
};

class ExecutionObserver
{
public:
	virtual ~ExecutionObserver() = default;
	virtual void observe(const ExecutionEvent &event) = 0;
};

} // namespace avm
