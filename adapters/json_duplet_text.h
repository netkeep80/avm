#pragma once

#include "avm/projection.h"

#include <string_view>

namespace avm::json_duplet
{

ProjectionDescription project_duplet_term_text(std::string_view text);
ProjectionDescription project_duplet_document_text(std::string_view text);

} // namespace avm::json_duplet
