#pragma once

#include "avm/executor.h"
#include "avm/program_model.h"

#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

struct ForeachVocabulary
{
	LinkId object_relation;
	LinkId subject_relation;

	static ForeachVocabulary create(LinkStore &store)
	{
		return ForeachVocabulary{
		    store.create_point(),
		    store.create_point(),
		};
	}
};

inline void validate_foreach_vocabulary(const LinkStore &store, const ForeachVocabulary &vocabulary, LinkId list_nil)
{
	const LinkId ids[] = {
	    vocabulary.object_relation,
	    vocabulary.subject_relation,
	    list_nil,
	};

	std::set<LinkId> unique;
	for (const LinkId id : ids)
	{
		if (!store.contains(id))
			throw std::invalid_argument("foreach vocabulary contains an unknown LinkId");
		if (!unique.insert(id).second)
			throw std::invalid_argument("foreach relation identities and list nil must be distinct");
	}
}

namespace foreach_detail
{

enum class Orientation
{
	Object,
	Subject,
};

inline SemanticContextFrame child_frame(const SemanticContextView &parent, Orientation orientation, LinkId item)
{
	const SemanticContextFrame current = parent.current();
	if (orientation == Orientation::Object)
	{
		return SemanticContextFrame{
		    current.entity,
		    current.relation_state,
		    current.subject,
		    item,
		};
	}

	return SemanticContextFrame{
	    current.entity,
	    current.relation_state,
	    item,
	    current.object,
	};
}

inline ExecutionOutcome execute_foreach(const ExecutionContext &context, Executor &executor, LinkId list_nil,
                                        Orientation orientation)
{
	if (!context.semantic)
		throw std::logic_error("foreach execution requires an explicit semantic context");

	const std::vector<LinkId> items =
	    decode_link_list(executor.store(), list_nil, context.object, std::numeric_limits<std::size_t>::max());
	std::vector<LinkId> results;
	results.reserve(items.size());

	for (const LinkId item : items)
	{
		const SemanticContextFrame child = child_frame(context.semantic, orientation, item);
		const ExecutionOutcome outcome =
		    executor.execute_child_semantic_context_outcome(context.subject, context, child);
		results.push_back(outcome.result);
	}

	const LinkId result_list = encode_link_list(executor.store(), list_nil, results);
	return ExecutionOutcome{result_list, context.semantic};
}

inline NativeRelationHandler handler(LinkId list_nil, Orientation orientation)
{
	return [list_nil, orientation](const ExecutionContext &context, Executor &executor)
	{
		return execute_foreach(context, executor, list_nil, orientation);
	};
}

} // namespace foreach_detail

inline void register_foreach_runtime(Executor &executor, const ForeachVocabulary &vocabulary, LinkId list_nil)
{
	validate_foreach_vocabulary(executor.store(), vocabulary, list_nil);
	const NativeRelationHandler object_handler =
	    foreach_detail::handler(list_nil, foreach_detail::Orientation::Object);
	const NativeRelationHandler subject_handler =
	    foreach_detail::handler(list_nil, foreach_detail::Orientation::Subject);
	executor.register_native(vocabulary.object_relation, object_handler);
	executor.register_native(vocabulary.subject_relation, subject_handler);
}

} // namespace avm
