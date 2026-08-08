#include "avm/executor.h"
#include "avm/semantic_context.h"

#include <cassert>
#include <vector>

namespace
{

class RecordingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override { events.push_back(event); }

	std::vector<avm::ExecutionEvent> events;
};

} // namespace

int main()
{
	avm::InMemoryLinkStore store;

	const avm::LinkId root_entity = store.create_point();
	const avm::LinkId root_relation_state = store.create_point();
	const avm::LinkId root_subject = store.create_point();
	const avm::LinkId root_object = store.create_point();

	const avm::SemanticContextFrame root_frame{
	    root_entity,
	    root_relation_state,
	    root_subject,
	    root_object,
	};
	const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);

	assert(root.depth() == 0);
	assert(root.current() == root_frame);
	assert(root.role(avm::SemanticContextRole::Entity) == root_entity);
	assert(root.role(avm::SemanticContextRole::RelationState) == root_relation_state);
	assert(root.role(avm::SemanticContextRole::Subject) == root_subject);
	assert(root.role(avm::SemanticContextRole::Object) == root_object);
	assert(root.parent() == root);
	assert(root.ancestor(1000) == root);

	const avm::LinkId child_entity_identity = store.create_point();
	const avm::LinkId child_relation_state = store.create_point();
	const avm::LinkId child_subject = store.create_point();
	const avm::LinkId child_object = store.create_point();
	const avm::SemanticContextFrame child_frame{
	    child_entity_identity,
	    child_relation_state,
	    child_subject,
	    child_object,
	};
	const avm::SemanticContextView child = root.child(child_frame);

	assert(child.depth() == 1);
	assert(child.current() == child_frame);
	assert(child.parent() == root);
	assert(child.ancestor(1000) == root);

	const avm::LinkId grandchild_entity = store.create_point();
	const avm::LinkId grandchild_relation_state = store.create_point();
	const avm::LinkId grandchild_subject = store.create_point();
	const avm::LinkId grandchild_object = store.create_point();
	const avm::SemanticContextView grandchild = child.child(avm::SemanticContextFrame{
	    grandchild_entity,
	    grandchild_relation_state,
	    grandchild_subject,
	    grandchild_object,
	});

	assert(grandchild.depth() == 2);
	assert(grandchild.parent() == child);
	assert(grandchild.ancestor(2) == root);
	assert(grandchild.ancestor(3) == root);

	const avm::LinkId replacement_relation_state = store.create_point();
	const avm::SemanticContextView updated = grandchild.with_relation_state(replacement_relation_state);
	assert(updated.depth() == grandchild.depth());
	assert(updated.current().entity == grandchild.current().entity);
	assert(updated.current().subject == grandchild.current().subject);
	assert(updated.current().object == grandchild.current().object);
	assert(updated.current().relation_state == replacement_relation_state);
	assert(grandchild.current().relation_state == grandchild_relation_state);
	assert(updated.parent() == child);

	const std::size_t before_context_reads = store.size();
	static_cast<void>(updated.role(avm::SemanticContextRole::Entity));
	static_cast<void>(updated.role(avm::SemanticContextRole::RelationState));
	static_cast<void>(updated.parent());
	static_cast<void>(updated.ancestor(1000));
	assert(store.size() == before_context_reads);

	RecordingObserver observer;
	avm::Executor executor(store, &observer);

	const avm::LinkId context_relation = store.create_point();
	const avm::LinkId dispatch_subject = store.create_point();
	const avm::LinkId dispatch_object = store.create_point();
	executor.register_native(context_relation,
	                         [root_relation_state, context_relation](const avm::ExecutionContext &context,
	                                                                avm::Executor &)
	                         {
		                         assert(context.relation == context_relation);
		                         assert(context.semantic.has_value());
		                         assert(context.semantic->role(avm::SemanticContextRole::RelationState) ==
		                                root_relation_state);
		                         assert(context.relation != root_relation_state);
		                         return context.semantic->role(avm::SemanticContextRole::RelationState);
	                         });

	const avm::LinkId context_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{context_relation, dispatch_subject, dispatch_object});
	const std::size_t before_context_execute = store.size();
	assert(executor.execute_in_context(context_entity, root) == root_relation_state);
	assert(store.size() == before_context_execute);
	assert(observer.events.size() == 2);
	assert(observer.events[0].kind == avm::ExecutionEventKind::Enter);
	assert(observer.events[0].context.semantic == root);
	assert(observer.events[1].kind == avm::ExecutionEventKind::Return);
	assert(observer.events[1].context.semantic == root);
	assert(observer.events[1].result == root_relation_state);

	observer.events.clear();
	const avm::LinkId same_child_relation = store.create_point();
	executor.register_native(same_child_relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         assert(context.semantic.has_value());
		                         return context.semantic->role(avm::SemanticContextRole::Subject);
	                         });
	const avm::LinkId same_child_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{same_child_relation, dispatch_subject, dispatch_object});

	const avm::LinkId same_parent_relation = store.create_point();
	executor.register_native(same_parent_relation,
	                         [same_child_entity](const avm::ExecutionContext &context, avm::Executor &current_executor)
	                         {
		                         return current_executor.execute_same_semantic_context(same_child_entity, context);
	                         });
	const avm::LinkId same_parent_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{same_parent_relation, dispatch_subject, dispatch_object});

	assert(executor.execute_in_context(same_parent_entity, root) == root_subject);
	assert(observer.events.size() == 4);
	assert(observer.events[0].context.semantic == root);
	assert(observer.events[1].context.entity == same_child_entity);
	assert(observer.events[1].context.parent == same_parent_entity);
	assert(observer.events[1].context.semantic == root);
	assert(observer.events[2].context.semantic == root);
	assert(observer.events[3].context.semantic == root);

	observer.events.clear();
	const avm::LinkId explicit_child_relation = store.create_point();
	executor.register_native(explicit_child_relation,
	                         [child_frame](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         assert(context.semantic.has_value());
		                         assert(context.semantic->depth() == 1);
		                         assert(context.semantic->current() == child_frame);
		                         return context.semantic->role(avm::SemanticContextRole::Object);
	                         });
	const avm::LinkId explicit_child_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{explicit_child_relation, dispatch_subject, dispatch_object});

	const avm::LinkId explicit_parent_relation = store.create_point();
	executor.register_native(explicit_parent_relation,
	                         [explicit_child_entity, child_frame](const avm::ExecutionContext &context,
	                                                              avm::Executor &current_executor)
	                         {
		                         return current_executor.execute_child_semantic_context(explicit_child_entity, context,
		                                                                                        child_frame);
	                         });
	const avm::LinkId explicit_parent_entity = avm::encode_relation_entity(
	    store, avm::RelationEntity{explicit_parent_relation, dispatch_subject, dispatch_object});

	assert(executor.execute_in_context(explicit_parent_entity, root) == child_object);
	assert(observer.events.size() == 4);
	assert(observer.events[0].context.semantic == root);
	assert(observer.events[1].context.semantic == child);
	assert(observer.events[1].context.parent == explicit_parent_entity);
	assert(observer.events[2].context.semantic == child);
	assert(observer.events[3].context.semantic == root);

	observer.events.clear();
	const avm::LinkId legacy_relation = store.create_point();
	executor.register_native(legacy_relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         assert(!context.semantic.has_value());
		                         return context.object;
	                         });
	const avm::LinkId legacy_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{legacy_relation, dispatch_subject, dispatch_object});
	assert(executor.execute(legacy_entity) == dispatch_object);
	assert(observer.events.size() == 2);
	assert(!observer.events[0].context.semantic.has_value());
	assert(!observer.events[1].context.semantic.has_value());

	return 0;
}
