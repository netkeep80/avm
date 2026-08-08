#pragma once

#include "avm/link_store.h"
#include "avm/semantic_context.h"

#include <utility>

namespace avm
{

struct ExecutionOutcome
{
	ExecutionOutcome(LinkId result) noexcept : result(result) {}

	ExecutionOutcome(LinkId result, SemanticContextView semantic) noexcept
	    : result(result), semantic(std::move(semantic))
	{
	}

	LinkId result;
	SemanticContextView semantic;

	bool operator==(const ExecutionOutcome &) const = default;
};

} // namespace avm
