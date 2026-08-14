#include "calculator_view.h"

#include "imgui.h"

#include <limits>
#include <stdexcept>

namespace avm::showcase
{

CalculatorViewport::CalculatorViewport(LinkStore &store, BootstrapRuntime &runtime)
    : store_(store), runtime_(runtime), integers_(IntegerVocabulary::create(store)),
      calculator_(CalculatorVocabulary::create(store))
{
	register_integer_arithmetic(runtime_.executor(), integers_);
	register_calculator_runtime(runtime_.executor(), runtime_.vocabulary(), integers_, calculator_);
	current_state_ = realize_initial_calculator_state(store_, runtime_.vocabulary(), integers_, calculator_);
	semantic_ = SemanticContextView::root(SemanticContextFrame{
	    store_.create_point(),
	    current_state_,
	    store_.create_point(),
	    store_.create_point(),
	});
}

bool CalculatorViewport::execute_event(LinkId relation, LinkId input, LinkId &selected_entity,
                                       std::optional<LinkId> &last_result, std::string &last_error,
                                       std::size_t &selected_trace)
{
	selected_trace = std::numeric_limits<std::size_t>::max();
	last_error.clear();
	last_result.reset();
	try
	{
		// UI materialize-ит только явное пользовательское событие; переход выполняет принятая headless-семантика.
		const LinkId entity = realize_calculator_event(store_, calculator_, relation, current_state_, input);
		selected_entity = entity;
		const ExecutionOutcome outcome = runtime_.executor().execute_outcome_in_context(entity, semantic_);
		if (outcome.semantic.role(SemanticContextRole::RelationState) != outcome.result)
			throw std::logic_error("calculator event returned inconsistent semantic relation-state");

		current_state_ = outcome.result;
		semantic_ = outcome.semantic;
		last_result = outcome.result;
		return true;
	}
	catch (const std::exception &error)
	{
		// Ошибка остаётся только presentation-данными: предыдущее canonical state/context сохраняет authority.
		last_error = error.what();
		return false;
	}
}

bool CalculatorViewport::press_digit(std::int64_t value, LinkId &selected_entity, std::optional<LinkId> &last_result,
                                     std::string &last_error, std::size_t &selected_trace)
{
	return execute_event(calculator_.press_digit_relation, realize_integer(store_, integers_, value), selected_entity,
	                     last_result, last_error, selected_trace);
}

bool CalculatorViewport::press_operation(LinkId relation, LinkId &selected_entity, std::optional<LinkId> &last_result,
                                         std::string &last_error, std::size_t &selected_trace)
{
	return execute_event(relation, runtime_.vocabulary().unit, selected_entity, last_result, last_error,
	                     selected_trace);
}

void CalculatorViewport::run_basic_demo(LinkId &selected_entity, std::optional<LinkId> &last_result,
                                        std::string &last_error, std::size_t &selected_trace)
{
	const auto require_step = [&last_error](bool success, const char *step)
	{
		if (!success)
			throw std::runtime_error(std::string("calculator-basic failed at ") + step + ": " + last_error);
	};

	// Детерминированный walkthrough не имеет собственной арифметики: это те же event helpers, что вызывают кнопки.
	require_step(press_digit(7, selected_entity, last_result, last_error, selected_trace), "7");
	const bool add_succeeded =
	    press_operation(calculator_.press_add_relation, selected_entity, last_result, last_error, selected_trace);
	require_step(add_succeeded, "+");
	require_step(press_digit(3, selected_entity, last_result, last_error, selected_trace), "3");
	const bool equals_succeeded = press_operation(calculator_.press_equals_relation, selected_entity, last_result,
	                                              last_error, selected_trace);
	require_step(equals_succeeded, "=");

	const CalculatorState state =
	    decode_calculator_state(store_, runtime_.vocabulary(), integers_, calculator_, current_state_);
	if (decode_integer(store_, integers_, state.display) != 10)
		throw std::logic_error("calculator-basic did not finish with canonical Integer(10)");
}

void CalculatorViewport::draw(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
                              std::size_t &selected_trace)
{
	ImGui::SeparatorText("Link-native calculator");
	try
	{
		const CalculatorState state =
		    decode_calculator_state(store_, runtime_.vocabulary(), integers_, calculator_, current_state_);
		const std::int64_t display = decode_integer(store_, integers_, state.display);
		ImGui::Text("display: %lld", static_cast<long long>(display));
		ImGui::Text("state #%llu", static_cast<unsigned long long>(state.entity));
		ImGui::Text("semantic $rel #%llu",
		            static_cast<unsigned long long>(semantic_.role(SemanticContextRole::RelationState)));
	}
	catch (const std::exception &error)
	{
		ImGui::TextWrapped("calculator decode failed: %s", error.what());
		return;
	}

	const ImVec2 key_size(48.0F, 32.0F);
	if (ImGui::BeginTable("calculator_keys", 4, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Button("7", key_size))
			press_digit(7, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("8", key_size))
			press_digit(8, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("9", key_size))
			press_digit(9, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("C", key_size))
			press_operation(calculator_.clear_relation, selected_entity, last_result, last_error, selected_trace);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Button("4", key_size))
			press_digit(4, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("5", key_size))
			press_digit(5, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("6", key_size))
			press_digit(6, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("+", key_size))
			press_operation(calculator_.press_add_relation, selected_entity, last_result, last_error, selected_trace);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Button("1", key_size))
			press_digit(1, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("2", key_size))
			press_digit(2, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("3", key_size))
			press_digit(3, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("-", key_size))
			press_operation(calculator_.press_subtract_relation, selected_entity, last_result, last_error,
			                selected_trace);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Button("0", key_size))
			press_digit(0, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("*", key_size))
			press_operation(calculator_.press_multiply_relation, selected_entity, last_result, last_error,
			                selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("/", key_size))
			press_operation(calculator_.press_divide_relation, selected_entity, last_result, last_error, selected_trace);
		ImGui::TableNextColumn();
		if (ImGui::Button("=", key_size))
			press_operation(calculator_.press_equals_relation, selected_entity, last_result, last_error, selected_trace);
		ImGui::EndTable();
	}

	if (!last_error.empty())
		ImGui::TextWrapped("failure: %s", last_error.c_str());
}

} // namespace avm::showcase
