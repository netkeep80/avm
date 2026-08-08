#pragma once

#include "avm/execution_observer.h"

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace avm
{

class BoundedExecutionTrace final : public ExecutionObserver
{
public:
	explicit BoundedExecutionTrace(std::size_t max_events) : max_events_(max_events)
	{
		static_assert(std::is_nothrow_copy_constructible_v<ExecutionEvent>);
		events_.reserve(max_events_);
	}

	void observe(const ExecutionEvent &event) noexcept override
	{
		if (events_.size() >= max_events_)
		{
			truncated_ = true;
			return;
		}

		events_.push_back(event);
	}

	std::span<const ExecutionEvent> events() const noexcept { return events_; }

	std::size_t size() const noexcept { return events_.size(); }

	std::size_t max_events() const noexcept { return max_events_; }

	bool empty() const noexcept { return events_.empty(); }

	bool truncated() const noexcept { return truncated_; }

	bool complete() const noexcept { return !truncated_; }

	void reset() noexcept
	{
		events_.clear();
		truncated_ = false;
	}

private:
	std::size_t max_events_;
	std::vector<ExecutionEvent> events_;
	bool truncated_ = false;
};

} // namespace avm
