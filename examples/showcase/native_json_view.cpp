#include "native_json_view.h"

#include "json_duplet_text.h"

#include "imgui.h"
#include "nlohmann/json.hpp"

#include <limits>
#include <stdexcept>

namespace avm::showcase
{
namespace
{

using Json = nlohmann::ordered_json;

constexpr const char *integer_add_source = R"json({
  "$avm": "duplet-json/1",
  "$root": {
    "<<": {"$symbol": "integer_add"},
    ">>": {
      "<<": {"$integer": 7},
      ">>": {"$integer": 3}
    }
  }
})json";

json_duplet::SymbolAnchors native_symbols(const BootstrapVocabulary &bootstrap, const IntegerVocabulary &integers)
{
	json_duplet::SymbolAnchors symbols;
	symbols.emplace_back("false", bootstrap.false_value);
	symbols.emplace_back("integer_add", integers.add_relation);
	symbols.emplace_back("nil", bootstrap.nil);
	symbols.emplace_back("true", bootstrap.true_value);
	symbols.emplace_back("unit", bootstrap.unit);
	return symbols;
}

} // namespace

NativeJsonViewport::NativeJsonViewport(LinkStore &store, BootstrapRuntime &runtime, IntegerVocabulary integers,
                                       TextVocabulary text)
    : store_(store), runtime_(runtime), integers_(integers),
      resolver_(integers, text, native_symbols(runtime.vocabulary(), integers))
{
}

ProjectionDescription NativeJsonViewport::project_source() const
{
	const std::size_t before = store_.size();
	const ProjectionDescription description =
	    json_duplet::project_duplet_document_text<Json>(integer_add_source, resolver_);
	if (store_.size() != before)
		throw std::logic_error("Native JSON projection unexpectedly materialized links");
	return description;
}

void NativeJsonViewport::find_only(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
                                   std::size_t &selected_trace)
{
	selected_trace = std::numeric_limits<std::size_t>::max();
	last_error.clear();
	last_result.reset();
	try
	{
		const ProjectionDescription description = project_source();
		const std::size_t before = store_.size();
		const auto found = find_projection(store_, description);
		if (store_.size() != before)
			throw std::logic_error("Native JSON find unexpectedly materialized links");
		if (found)
		{
			selected_entity = found->root;
			status_ = "find: existing root #" + std::to_string(static_cast<unsigned long long>(found->root));
		}
		else
		{
			status_ = "find: missing (store unchanged)";
		}
	}
	catch (const std::exception &error)
	{
		last_error = error.what();
		status_ = "find failed";
	}
}

void NativeJsonViewport::realize_and_execute(LinkId &selected_entity, std::optional<LinkId> &last_result,
                                             std::string &last_error, std::size_t &selected_trace)
{
	selected_trace = std::numeric_limits<std::size_t>::max();
	last_error.clear();
	last_result.reset();
	try
	{
		const ProjectionDescription description = project_source();
		const ProjectionResult realized = realize_projection(store_, description);
		selected_entity = realized.root;
		last_result = runtime_.executor().execute(realized.root);
		const std::int64_t value = decode_integer(store_, integers_, *last_result);
		if (value != 10)
			throw std::logic_error("Native JSON integer-add example did not return Integer(10)");
		status_ = "realize + execute: Integer(10)";
	}
	catch (const std::exception &error)
	{
		last_error = error.what();
		status_ = "realize/execute failed";
	}
}

void NativeJsonViewport::draw(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
                              std::size_t &selected_trace)
{
	ImGui::SeparatorText("Native Duplet JSON");
	ImGui::TextWrapped("Source -> ProjectionDescription -> find | realize -> canonical graph -> ordinary Executor");
	ImGui::BeginChild("native_json_source", ImVec2(0.0F, 155.0F), true);
	ImGui::TextUnformatted(integer_add_source);
	ImGui::EndChild();

	if (ImGui::Button("Find only", ImVec2(-1.0F, 0.0F)))
		find_only(selected_entity, last_result, last_error, selected_trace);
	if (ImGui::Button("Realize + execute", ImVec2(-1.0F, 0.0F)))
		realize_and_execute(selected_entity, last_result, last_error, selected_trace);

	if (!status_.empty())
		ImGui::TextWrapped("%s", status_.c_str());
	if (!last_error.empty())
		ImGui::TextWrapped("failure: %s", last_error.c_str());
}

} // namespace avm::showcase
