#pragma once

#include "avm/projection.h"

#include <optional>
#include <stdexcept>

namespace avm::adapters
{

enum class AnumProjectionKind
{
	ProtocolValue,
	BoundaryForm,
	QuotedRaw,
	Raw,
};

enum class AnumProtocolValue
{
	Zero,
	One,
};

struct AnumL3Projection
{
	AnumProjectionKind kind;
	std::optional<AnumProtocolValue> protocol_value;
};

struct AnumL4Anchors
{
	LinkId zero;
	LinkId one;
};

inline std::optional<ProjectionDescription> to_avm_projection(const AnumL3Projection &projection,
                                                              const AnumL4Anchors &anchors)
{
	if (projection.kind != AnumProjectionKind::ProtocolValue)
	{
		if (projection.protocol_value)
			throw std::invalid_argument("non-protocol Anum projection cannot carry a protocol value");
		return std::nullopt;
	}

	if (!projection.protocol_value)
		throw std::invalid_argument("protocol-value Anum projection requires a protocol value");
	if (anchors.zero == invalid_link_id || anchors.one == invalid_link_id)
		throw std::invalid_argument("Anum protocol-value anchors must be valid LinkIds");
	if (anchors.zero == anchors.one)
		throw std::invalid_argument("Anum protocol-value anchors must be distinct");

	const LinkId root = *projection.protocol_value == AnumProtocolValue::Zero ? anchors.zero : anchors.one;
	return ProjectionDescription{{}, ProjectionRef::anchor(root)};
}

} // namespace avm::adapters
