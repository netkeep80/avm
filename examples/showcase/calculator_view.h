#pragma once

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "calculator_model.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace avm::showcase
{

// Presentation-adapter хранит только canonical LinkId/context handles, нужные GUI event loop.
// Арифметика, переходы состояния и валидация остаются в calculator_model.h и обычном Executor.
class CalculatorViewport
{
public:
	CalculatorViewport(LinkStore &store, BootstrapRuntime &runtime);

	void draw(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
	          std::size_t &selected_trace);
	void run_basic_demo(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
	                    std::size_t &selected_trace);

private:
	LinkStore &store_;
	BootstrapRuntime &runtime_;
	IntegerVocabulary integers_;
	CalculatorVocabulary calculator_;
	LinkId current_state_ = invalid_link_id;
	SemanticContextView semantic_;

	bool execute_event(LinkId relation, LinkId input, LinkId &selected_entity, std::optional<LinkId> &last_result,
	                   std::string &last_error, std::size_t &selected_trace);
	bool press_digit(std::int64_t value, LinkId &selected_entity, std::optional<LinkId> &last_result,
	                 std::string &last_error, std::size_t &selected_trace);
	bool press_operation(LinkId relation, LinkId &selected_entity, std::optional<LinkId> &last_result,
	                     std::string &last_error, std::size_t &selected_trace);
};

} // namespace avm::showcase
