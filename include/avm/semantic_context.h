#pragma once

#include "avm/link_store.h"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

struct SemanticContextFrame
{
	LinkId entity;
	LinkId relation_state;
	LinkId subject;
	LinkId object;

	bool operator==(const SemanticContextFrame &) const = default;
};

enum class SemanticContextRole
{
	Entity,
	RelationState,
	Subject,
	Object,
};

class SemanticContextView
{
public:
	static SemanticContextView root(SemanticContextFrame frame)
	{
		return SemanticContextView(std::vector<SemanticContextFrame>{std::move(frame)});
	}

	const SemanticContextFrame &current() const { return frames_.back(); }

	std::size_t depth() const noexcept { return frames_.size() - 1; }

	SemanticContextView parent() const { return ancestor(1); }

	SemanticContextView ancestor(std::size_t levels) const
	{
		const std::size_t removable = frames_.size() - 1;
		const std::size_t removed = levels < removable ? levels : removable;
		const std::size_t retained = frames_.size() - removed;
		return SemanticContextView(std::vector<SemanticContextFrame>(frames_.begin(), frames_.begin() + retained));
	}

	SemanticContextView child(SemanticContextFrame frame) const
	{
		std::vector<SemanticContextFrame> child_frames = frames_;
		child_frames.push_back(std::move(frame));
		return SemanticContextView(std::move(child_frames));
	}

	SemanticContextView with_relation_state(LinkId relation_state) const
	{
		std::vector<SemanticContextFrame> updated_frames = frames_;
		updated_frames.back().relation_state = relation_state;
		return SemanticContextView(std::move(updated_frames));
	}

	LinkId role(SemanticContextRole role) const
	{
		switch (role)
		{
		case SemanticContextRole::Entity:
			return current().entity;
		case SemanticContextRole::RelationState:
			return current().relation_state;
		case SemanticContextRole::Subject:
			return current().subject;
		case SemanticContextRole::Object:
			return current().object;
		}

		throw std::logic_error("unknown semantic context role");
	}

	bool operator==(const SemanticContextView &) const = default;

private:
	explicit SemanticContextView(std::vector<SemanticContextFrame> frames) : frames_(std::move(frames))
	{
		if (frames_.empty())
			throw std::invalid_argument("semantic context lineage must contain a root frame");
	}

	std::vector<SemanticContextFrame> frames_;
};

} // namespace avm
