#pragma once

#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/relations_query.h"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace avm::tooling
{

class InspectionSession
{
public:
	InspectionSession(LinkStore &store, BootstrapVocabulary vocabulary, std::size_t trace_capacity = 256,
	                  std::size_t max_call_depth = 1000)
	    : runtime_(store, std::move(vocabulary), max_call_depth), trace_(trace_capacity)
	{
	}

	const BootstrapVocabulary &vocabulary() const noexcept { return runtime_.vocabulary(); }

	Link inspect_link(LinkId id) const { return store().get(id); }

	std::optional<LinkId> find_pair(LinkId begin, LinkId end) const { return store().find(begin, end); }

	std::vector<LinkId> outgoing(LinkId begin) const { return store().outgoing(begin); }

	std::vector<LinkId> incoming(LinkId end) const { return store().incoming(end); }

	std::vector<RelationMatch> query_relations(const RelationQuery &query) const
	{
		return query_relation_entities(store(), query);
	}

	RelationEntity decode_relation(LinkId entity) const { return decode_relation_entity(store(), entity); }

	std::optional<FunctionDefinition> function_definition(LinkId handle) const
	{
		return find_function_definition(store(), runtime_.vocabulary(), handle);
	}

	DecodedCallFrame call_frame(LinkId frame) const { return decode_call_frame(store(), runtime_.vocabulary(), frame); }

	LinkId execute(LinkId root) { return runtime_.execute(root); }

	LinkId trace_execute(LinkId root)
	{
		trace_.reset();
		runtime_.executor().set_observer(&trace_);
		try
		{
			const LinkId result = runtime_.execute(root);
			runtime_.executor().set_observer(nullptr);
			return result;
		}
		catch (...)
		{
			runtime_.executor().set_observer(nullptr);
			throw;
		}
	}

	std::span<const ExecutionEvent> trace_events() const noexcept { return trace_.events(); }

	bool trace_truncated() const noexcept { return trace_.truncated(); }

	std::size_t trace_capacity() const noexcept { return trace_.max_events(); }

	void reset_trace() noexcept { trace_.reset(); }

private:
	const LinkStore &store() const
	{
		const BootstrapRuntime &runtime = runtime_;
		return runtime.executor().store();
	}

	BootstrapRuntime runtime_;
	BoundedExecutionTrace trace_;
};

} // namespace avm::tooling
