#pragma once

#include "avm/link_store.h"

#include <cstddef>
#include <memory>
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
	SemanticContextView() noexcept = default;

	static SemanticContextView root(SemanticContextFrame frame)
	{
		return SemanticContextView(std::vector<SemanticContextFrame>{std::move(frame)});
	}

	bool has_value() const noexcept { return static_cast<bool>(frames_); }

	explicit operator bool() const noexcept { return has_value(); }

	const SemanticContextFrame &current() const { return frames().back(); }

	std::size_t depth() const { return frames().size() - 1; }

	SemanticContextView parent() const { return ancestor(1); }

	SemanticContextView ancestor(std::size_t levels) const
	{
		const Frames &current_frames = frames();
		const std::size_t removable = current_frames.size() - 1;
		const std::size_t removed = levels < removable ? levels : removable;
		if (removed == 0)
			return *this;

		const std::size_t retained = current_frames.size() - removed;
		return SemanticContextView(Frames(current_frames.begin(), current_frames.begin() + retained));
	}

	SemanticContextView child(SemanticContextFrame frame) const
	{
		Frames child_frames = frames();
		child_frames.push_back(std::move(frame));
		return SemanticContextView(std::move(child_frames));
	}

	SemanticContextView with_relation_state(LinkId relation_state) const
	{
		Frames updated_frames = frames();
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

	bool operator==(const SemanticContextView &other) const
	{
		if (frames_ == other.frames_)
			return true;
		if (!frames_ || !other.frames_)
			return false;
		return *frames_ == *other.frames_;
	}

private:
	using Frames = std::vector<SemanticContextFrame>;

	explicit SemanticContextView(Frames frames)
	{
		if (frames.empty())
			throw std::invalid_argument("semantic context lineage must contain a root frame");
		frames_ = std::make_shared<const Frames>(std::move(frames));
	}

	const Frames &frames() const
	{
		if (!frames_)
			throw std::logic_error("semantic context view is empty");
		return *frames_;
	}

	std::shared_ptr<const Frames> frames_;
};

} // namespace avm
