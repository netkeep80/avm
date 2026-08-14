#include "showcase_demo.h"

#include <GLFW/glfw3.h>

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace avm::showcase
{

void write_framebuffer_ppm(const std::string &path, int width, int height)
{
	if (width <= 0 || height <= 0)
		throw std::invalid_argument("showcase framebuffer has invalid dimensions");

	const std::size_t row_size = static_cast<std::size_t>(width) * 3U;
	std::vector<unsigned char> pixels(row_size * static_cast<std::size_t>(height));

	// Снимок читается из того же back-buffer, куда только что отрисован ImGui. Это evidence реального OpenGL frame,
	// а не X11/mockup-картинка; чтение не влияет на AVM semantic state.
	glFinish();
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	std::ofstream output(path, std::ios::binary);
	if (!output)
		throw std::runtime_error("cannot open showcase screenshot output");
	output << "P6\n" << width << ' ' << height << "\n255\n";
	for (int y = height - 1; y >= 0; --y)
	{
		const auto *row = reinterpret_cast<const char *>(pixels.data() + static_cast<std::size_t>(y) * row_size);
		output.write(row, static_cast<std::streamsize>(row_size));
	}
	if (!output)
		throw std::runtime_error("failed to write showcase framebuffer screenshot");
}

} // namespace avm::showcase
