#pragma once

#include <optional>
#include <string>

namespace avm::showcase
{

struct ShowcaseOptions
{
	bool calculator_basic_demo = false;
	std::optional<std::string> screenshot_path;
};

ShowcaseOptions parse_showcase_options(int argc, char **argv);
void write_framebuffer_ppm(const std::string &path, int width, int height);

} // namespace avm::showcase
