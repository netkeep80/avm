#include "legacy_reference_compiler.h"

#include "avm/projection.h"
#include "avm/reference.h"
#include "avm/relations_model.h"
#include "avm/semantic_context.h"

#include <array>
#include <cassert>
#include <functional>
#include <string_view>

namespace
{

bool compile_rejected(std::string_view source, const avm::ReferenceVocabulary &vocabulary,
                      const avm::legacy_reference::NamedAnchors &names = {})
{
	try
	{
		static_cast<void>(avm::legacy_reference::compile(source, vocabulary, names));
		return false;
	}
	catch (const avm::legacy_reference::CompileError &)
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

avm::LinkId compile_realize_resolve(avm::LinkStore &store, const avm::ReferenceVocabulary &vocabulary,
                                    const avm::SemanticContextView &context, std::string_view source,
                                    const avm::legacy_reference::NamedAnchors &names = {})
{
	const avm::ProjectionDescription description = avm::legacy_reference::compile(source, vocabulary, names);
	const avm::LinkId reference = avm::realize_projection(store, description).root;
	const auto result = avm::resolve_reference(store, vocabulary, reference, context);
	assert(result.has_value());
	return *result;
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::ReferenceVocabulary vocabulary = avm::ReferenceVocabulary::create(store);

	const avm::SemanticContextFrame root_frame = frame(store);
	const avm::SemanticContextFrame child1_frame = frame(store);
	const avm::SemanticContextFrame child2_frame = frame(store);
	const avm::SemanticContextFrame child3_frame = frame(store);
	const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);
	const avm::SemanticContextView child1 = root.child(child1_frame);
	const avm::SemanticContextView child2 = child1.child(child2_frame);
	const avm::SemanticContextView child3 = child2.child(child3_frame);

	struct RelativeCase
	{
		std::string_view source;
		avm::LinkId expected;
	};

	const std::array<RelativeCase, 16> relative_cases{{
	    {"$ent", child3_frame.entity},
	    {"$rel", child3_frame.relation_state},
	    {"$sub", child3_frame.subject},
	    {"$obj", child3_frame.object},
	    {"$$ent", child2_frame.entity},
	    {"$$rel", child2_frame.relation_state},
	    {"$$sub", child2_frame.subject},
	    {"$$obj", child2_frame.object},
	    {"$$$ent", child1_frame.entity},
	    {"$$$rel", child1_frame.relation_state},
	    {"$$$sub", child1_frame.subject},
	    {"$$$obj", child1_frame.object},
	    {"$$$$ent", root_frame.entity},
	    {"$$$$rel", root_frame.relation_state},
	    {"$$$$sub", root_frame.subject},
	    {"$$$$obj", root_frame.object},
	}};

	for (const RelativeCase &test_case : relative_cases)
	{
		const std::size_t before_compile = store.size();
		const avm::ProjectionDescription description =
		    avm::legacy_reference::compile(test_case.source, vocabulary);
		assert(store.size() == before_compile);

		const std::size_t before_find = store.size();
		assert(!avm::find_projection(store, description).has_value());
		assert(store.size() == before_find);

		const avm::LinkId reference = avm::realize_projection(store, description).root;
		assert(avm::resolve_reference(store, vocabulary, reference, child3) == test_case.expected);

		const std::size_t before_repeat = store.size();
		const avm::ProjectionDescription repeated_description =
		    avm::legacy_reference::compile(test_case.source, vocabulary);
		assert(avm::realize_projection(store, repeated_description).root == reference);
		assert(store.size() == before_repeat);
	}

	assert(compile_realize_resolve(store, vocabulary, child3, "$$$$$$$$sub") == root_frame.subject);

	const avm::LinkId controller_identity = store.create_point();
	assert(controller_identity != child3_frame.relation_state);
	assert(compile_realize_resolve(store, vocabulary, child3, "$rel") == child3_frame.relation_state);
	assert(compile_realize_resolve(store, vocabulary, child3, "$rel") != controller_identity);

	const avm::LinkId ent1 = store.create_point();
	const avm::LinkId ent2 = store.create_point();
	avm::legacy_reference::NamedAnchors names;
	names.emplace("ent1", ent1);
	names.emplace("ent2", ent2);
	assert(compile_realize_resolve(store, vocabulary, child3, "ent1", names) == ent1);
	assert(compile_realize_resolve(store, vocabulary, child3, "ent2", names) == ent2);

	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId pair = store.intern(begin, end);
	names.emplace("pair", pair);
	assert(compile_realize_resolve(store, vocabulary, child3, "pair/begin", names) == begin);
	assert(compile_realize_resolve(store, vocabulary, child3, "pair/end", names) == end);

	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId relation_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{relation, subject, object});
	names.emplace("relation_entity", relation_entity);
	assert(compile_realize_resolve(store, vocabulary, child3, "relation_entity/relation", names) == relation);
	assert(compile_realize_resolve(store, vocabulary, child3, "relation_entity/subject", names) == subject);
	assert(compile_realize_resolve(store, vocabulary, child3, "relation_entity/object", names) == object);
	assert(compile_realize_resolve(store, vocabulary, child3, "relation_entity/end/begin", names) == subject);

	const avm::LinkId child_entity = child3_frame.entity;
	const avm::Link child_entity_link = store.get(child_entity);
	assert(compile_realize_resolve(store, vocabulary, child3, "$ent/begin") == child_entity_link.begin);
	assert(compile_realize_resolve(store, vocabulary, child3, "$ent/end") == child_entity_link.end);

	assert(compile_rejected("", vocabulary, names));
	assert(compile_rejected("$", vocabulary, names));
	assert(compile_rejected("$$", vocabulary, names));
	assert(compile_rejected("$foo", vocabulary, names));
	assert(compile_rejected("ent", vocabulary, names));
	assert(compile_rejected("missing", vocabulary, names));
	assert(compile_rejected("/ent1", vocabulary, names));
	assert(compile_rejected("ent1/", vocabulary, names));
	assert(compile_rejected("ent1//begin", vocabulary, names));

	// Frozen jsonRVM evidence contains /id and /val as JSON-container access.
	// They are deliberately not promoted into canonical AVM reference semantics.
	assert(compile_rejected("$ent/id", vocabulary, names));
	assert(compile_rejected("$$$$sub/id", vocabulary, names));
	assert(compile_rejected("ent1/id", vocabulary, names));
	assert(compile_rejected("ent1/val", vocabulary, names));
	assert(compile_rejected("ent1/0", vocabulary, names));
	assert(compile_rejected("ent1/val/id", vocabulary, names));

	avm::legacy_reference::NamedAnchors invalid_names;
	invalid_names.emplace("invalid", avm::invalid_link_id);
	assert(compile_rejected("invalid", vocabulary, invalid_names));

	avm::legacy_reference::NamedAnchors unknown_target_names;
	const avm::LinkId unknown_target = store.size() + 1000;
	unknown_target_names.emplace("future", unknown_target);
	const avm::ProjectionDescription unknown_target_description =
	    avm::legacy_reference::compile("future", vocabulary, unknown_target_names);
	const std::size_t before_unknown_find = store.size();
	assert(!avm::find_projection(store, unknown_target_description).has_value());
	assert(store.size() == before_unknown_find);

	bool unknown_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, unknown_target_description));
	}
	catch (const std::invalid_argument &)
	{
		unknown_realize_rejected = true;
	}
	assert(unknown_realize_rejected);
	assert(store.size() == before_unknown_find);

	return 0;
}
