#include "adapters/anum_projection_bridge.h"

#include <cassert>
#include <stdexcept>

namespace
{

using avm::adapters::AnumL3Projection;
using avm::adapters::AnumL4Anchors;
using avm::adapters::AnumProjectionKind;
using avm::adapters::AnumProtocolValue;

void test_protocol_values_map_to_explicit_anchors()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId zero = store.create_point();
	const avm::LinkId one = store.create_point();
	const AnumL4Anchors anchors{zero, one};
	const std::size_t before = store.size();

	const auto zero_description = avm::adapters::to_avm_projection(
	    AnumL3Projection{AnumProjectionKind::ProtocolValue, AnumProtocolValue::Zero}, anchors);
	const auto one_description = avm::adapters::to_avm_projection(
	    AnumL3Projection{AnumProjectionKind::ProtocolValue, AnumProtocolValue::One}, anchors);

	assert(zero_description);
	assert(one_description);
	assert(store.size() == before);

	const auto found_zero = avm::find_projection(store, *zero_description);
	const auto found_one = avm::find_projection(store, *one_description);
	assert(found_zero && found_zero->root == zero);
	assert(found_one && found_one->root == one);
	assert(store.size() == before);

	const avm::ProjectionResult realized_zero = avm::realize_projection(store, *zero_description);
	const avm::ProjectionResult realized_one = avm::realize_projection(store, *one_description);
	assert(realized_zero.root == zero);
	assert(realized_one.root == one);
	assert(realized_zero.nodes.empty());
	assert(realized_one.nodes.empty());
	assert(store.size() == before);
}

void test_unresolved_projection_kinds_remain_without_denotation()
{
	avm::InMemoryLinkStore store;
	const AnumL4Anchors anchors{store.create_point(), store.create_point()};
	const std::size_t before = store.size();

	for (const AnumProjectionKind kind : {
	         AnumProjectionKind::BoundaryForm,
	         AnumProjectionKind::QuotedRaw,
	         AnumProjectionKind::Raw,
	     })
	{
		const auto description = avm::adapters::to_avm_projection(AnumL3Projection{kind, std::nullopt}, anchors);
		assert(!description);
		assert(store.size() == before);
	}
}

void test_missing_anchor_stays_non_mutating()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId zero = store.create_point();
	const avm::LinkId absent_one = zero + 100;
	const AnumL4Anchors anchors{zero, absent_one};
	const auto description = avm::adapters::to_avm_projection(
	    AnumL3Projection{AnumProjectionKind::ProtocolValue, AnumProtocolValue::One}, anchors);
	assert(description);

	const std::size_t before_find = store.size();
	assert(!avm::find_projection(store, *description));
	assert(store.size() == before_find);

	bool rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, *description));
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == before_find);
}

void test_malformed_external_results_are_rejected()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId zero = store.create_point();
	const avm::LinkId one = store.create_point();
	const AnumL4Anchors anchors{zero, one};

	bool missing_value_rejected = false;
	try
	{
		static_cast<void>(avm::adapters::to_avm_projection(
		    AnumL3Projection{AnumProjectionKind::ProtocolValue, std::nullopt}, anchors));
	}
	catch (const std::invalid_argument &)
	{
		missing_value_rejected = true;
	}
	assert(missing_value_rejected);

	bool extra_value_rejected = false;
	try
	{
		static_cast<void>(avm::adapters::to_avm_projection(
		    AnumL3Projection{AnumProjectionKind::Raw, AnumProtocolValue::Zero}, anchors));
	}
	catch (const std::invalid_argument &)
	{
		extra_value_rejected = true;
	}
	assert(extra_value_rejected);

	bool invalid_anchor_rejected = false;
	try
	{
		static_cast<void>(avm::adapters::to_avm_projection(
		    AnumL3Projection{AnumProjectionKind::ProtocolValue, AnumProtocolValue::Zero},
		    AnumL4Anchors{avm::invalid_link_id, one}));
	}
	catch (const std::invalid_argument &)
	{
		invalid_anchor_rejected = true;
	}
	assert(invalid_anchor_rejected);

	bool duplicate_anchor_rejected = false;
	try
	{
		static_cast<void>(avm::adapters::to_avm_projection(
		    AnumL3Projection{AnumProjectionKind::ProtocolValue, AnumProtocolValue::One}, AnumL4Anchors{zero, zero}));
	}
	catch (const std::invalid_argument &)
	{
		duplicate_anchor_rejected = true;
	}
	assert(duplicate_anchor_rejected);
}

} // namespace

int main()
{
	test_protocol_values_map_to_explicit_anchors();
	test_unresolved_projection_kinds_remain_without_denotation();
	test_missing_anchor_stays_non_mutating();
	test_malformed_external_results_are_rejected();
	return 0;
}
