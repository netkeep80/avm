#pragma once

#include <optional>
#include <string>

namespace avm::showcase
{

std::optional<std::string> framebuffer_capture_path_from_environment();
void write_framebuffer_ppm(const std::string &path, int width, int height);

} // namespace avm::showcase
