#pragma once

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/text_value.h"
#include "json_duplet_values.h"

#include <cstddef>
#include <optional>
#include <string>

namespace avm::showcase
{

// Source pane остаётся presentation-only: parser/projector/find/realize/Executor берутся из уже принятого Native JSON path.
class NativeJsonViewport
{
public:
	NativeJsonViewport(LinkStore &store, BootstrapRuntime &runtime, IntegerVocabulary integers, TextVocabulary text);

	void draw(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
	          std::size_t &selected_trace);

private:
	LinkStore &store_;
	BootstrapRuntime &runtime_;
	IntegerVocabulary integers_;
	TextVocabulary text_;
	json_duplet::NativeLeafResolver resolver_;
	std::string status_;

	ProjectionDescription project_source() const;
	void find_only(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
	               std::size_t &selected_trace);
	void realize_and_execute(LinkId &selected_entity, std::optional<LinkId> &last_result, std::string &last_error,
	                         std::size_t &selected_trace);
};

} // namespace avm::showcase
