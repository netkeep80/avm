#include "avm/persistent_link_store.h"
#include "avm/reference.h"

#include <cassert>
#include <exception>
#include <filesystem>
#include <functional>

namespace
{

bool rejected(const std::function<void()> &operation)
{
	try
	{
		operation();
		return false;
	}
	catch (const std::exception &)
	{
		return true;
	}
}

avm::SemanticContextFrame frame(avm::LinkStore &store)
{
	return avm::SemanticContextFrame{
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	};
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::ReferenceVocabulary vocabulary = avm::ReferenceVocabulary::create(store);
	avm::validate_reference_vocabulary(store, vocabulary);

	const avm::SemanticContextFrame root_frame = frame(store);
	const avm::SemanticContextFrame child1_frame = frame(store);
	const avm::SemanticContextFrame child2_frame = frame(store);
	const avm::SemanticContextFrame child3_frame = frame(store);
	const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);
	const avm::SemanticContextView child1 = root.child(child1_frame);
	const avm::SemanticContextView child2 = child1.child(child2_frame);
	const avm::SemanticContextView child3 = child2.child(child3_frame);

	const std::size_t before_find = store.size();
	assert(!avm::find_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 3).has_value());
	assert(store.size() == before_find);

	const avm::LinkId current_entity =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Entity);
	const avm::LinkId current_relation =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::RelationState);
	const avm::LinkId current_subject =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject);
	const avm::LinkId current_object =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Object);

	assert(avm::resolve_reference(store, vocabulary, current_entity, child3) == child3_frame.entity);
	assert(avm::resolve_reference(store, vocabulary, current_relation, child3) == child3_frame.relation_state);
	assert(avm::resolve_reference(store, vocabulary, current_subject, child3) == child3_frame.subject);
	assert(avm::resolve_reference(store, vocabulary, current_object, child3) == child3_frame.object);

	const avm::LinkId parent_subject =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 1);
	const avm::LinkId grandparent_subject =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 2);
	const avm::LinkId root_subject =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 3);
	const avm::LinkId saturated_subject =
	    avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 100);

	assert(avm::resolve_reference(store, vocabulary, parent_subject, child3) == child2_frame.subject);
	assert(avm::resolve_reference(store, vocabulary, grandparent_subject, child3) == child1_frame.subject);
	assert(avm::resolve_reference(store, vocabulary, root_subject, child3) == root_frame.subject);
	assert(avm::resolve_reference(store, vocabulary, saturated_subject, child3) == root_frame.subject);

	const std::size_t before_repeat = store.size();
	assert(avm::realize_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 3) == root_subject);
	assert(store.size() == before_repeat);
	assert(avm::find_context_role_reference(store, vocabulary, avm::ReferenceRole::Subject, 3) == root_subject);

	const avm::LinkId named_target = store.create_point();
	const std::size_t before_named_find = store.size();
	assert(!avm::find_named_reference(store, vocabulary, named_target).has_value());
	assert(store.size() == before_named_find);
	const avm::LinkId named = avm::realize_named_reference(store, vocabulary, named_target);
	assert(avm::resolve_reference(store, vocabulary, named, child3) == named_target);
	const std::size_t before_named_repeat = store.size();
	assert(avm::realize_named_reference(store, vocabulary, named_target) == named);
	assert(store.size() == before_named_repeat);

	const avm::LinkId begin_value = store.create_point();
	const avm::LinkId end_value = store.create_point();
	const avm::LinkId pair = store.intern(begin_value, end_value);
	const avm::LinkId pair_reference = avm::realize_named_reference(store, vocabulary, pair);
	const avm::LinkId begin_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::Begin, pair_reference);
	const avm::LinkId end_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::End, pair_reference);
	assert(avm::resolve_reference(store, vocabulary, begin_reference, child3) == begin_value);
	assert(avm::resolve_reference(store, vocabulary, end_reference, child3) == end_value);

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, avm::RelationEntity{relation, subject, object});
	const avm::LinkId entity_reference = avm::realize_named_reference(store, vocabulary, entity);
	const avm::LinkId relation_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::RelationPart, entity_reference);
	const avm::LinkId subject_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::SubjectPart, entity_reference);
	const avm::LinkId object_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::ObjectPart, entity_reference);
	assert(avm::resolve_reference(store, vocabulary, relation_reference, child3) == relation);
	assert(avm::resolve_reference(store, vocabulary, subject_reference, child3) == subject);
	assert(avm::resolve_reference(store, vocabulary, object_reference, child3) == object);

	const avm::LinkId nested_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::End, entity_reference);
	const avm::LinkId nested_subject_reference =
	    avm::realize_reference_projection(store, vocabulary, avm::ReferenceProjection::Begin, nested_reference);
	assert(avm::resolve_reference(store, vocabulary, nested_subject_reference, child3) == subject);

	const avm::LinkId controller_identity = store.create_point();
	assert(controller_identity != child3_frame.relation_state);
	assert(avm::resolve_reference(store, vocabulary, current_relation, child3) == child3_frame.relation_state);
	assert(avm::resolve_reference(store, vocabulary, current_relation, child3) != controller_identity);

	const avm::LinkId missing_reference = store.size() + 1000;
	const std::size_t before_missing = store.size();
	assert(!avm::resolve_reference(store, vocabulary, missing_reference, child3).has_value());
	assert(store.size() == before_missing);

	const avm::SemanticContextFrame missing_role_frame{
	    child3_frame.entity,
	    child3_frame.relation_state,
	    store.size() + 2000,
	    child3_frame.object,
	};
	const avm::SemanticContextView missing_role_context = child2.child(missing_role_frame);
	const std::size_t before_missing_role = store.size();
	assert(!avm::resolve_reference(store, vocabulary, current_subject, missing_role_context).has_value());
	assert(store.size() == before_missing_role);

	const avm::LinkId unknown_marker = store.create_point();
	const avm::LinkId malformed_reference = store.intern(unknown_marker, named_target);
	const bool malformed_rejected =
	    rejected([&] { static_cast<void>(avm::resolve_reference(store, vocabulary, malformed_reference, child3)); });
	assert(malformed_rejected);

	const avm::LinkId bad_selector = store.intern(vocabulary.named_reference, vocabulary.current_context);
	const avm::LinkId bad_role_reference = store.intern(vocabulary.subject_role, bad_selector);
	const bool bad_selector_rejected =
	    rejected([&] { static_cast<void>(avm::resolve_reference(store, vocabulary, bad_role_reference, child3)); });
	assert(bad_selector_rejected);

	const bool depth_rejected =
	    rejected([&] { static_cast<void>(avm::resolve_reference(store, vocabulary, saturated_subject, child3, 2)); });
	assert(depth_rejected);

	avm::ReferenceVocabulary duplicate_vocabulary = vocabulary;
	duplicate_vocabulary.object_role = duplicate_vocabulary.subject_role;
	assert(rejected([&] { avm::validate_reference_vocabulary(store, duplicate_vocabulary); }));

	const std::filesystem::path persistent_path = std::filesystem::temp_directory_path() / "avm_reference_test.links";
	std::filesystem::remove(persistent_path);

	avm::ReferenceVocabulary persistent_vocabulary{};
	avm::SemanticContextFrame persistent_root_frame{};
	avm::SemanticContextFrame persistent_child_frame{};
	avm::LinkId persistent_reference = avm::invalid_link_id;
	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		persistent_vocabulary = avm::ReferenceVocabulary::create(persistent_store);
		persistent_root_frame = frame(persistent_store);
		persistent_child_frame = frame(persistent_store);
		const auto object_role = avm::ReferenceRole::Object;
		persistent_reference =
		    avm::realize_context_role_reference(persistent_store, persistent_vocabulary, object_role, 1);
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		const avm::SemanticContextView persistent_context =
		    avm::SemanticContextView::root(persistent_root_frame).child(persistent_child_frame);
		const std::size_t before_resolve = reopened.size();
		assert(avm::resolve_reference(reopened, persistent_vocabulary, persistent_reference, persistent_context) ==
		       persistent_root_frame.object);
		assert(reopened.size() == before_resolve);

		const std::size_t before_realize = reopened.size();
		const auto object_role = avm::ReferenceRole::Object;
		const avm::LinkId repeated =
		    avm::realize_context_role_reference(reopened, persistent_vocabulary, object_role, 1);
		assert(repeated == persistent_reference);
		assert(reopened.size() == before_realize);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
