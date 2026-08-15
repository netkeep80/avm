#pragma once

#include "inspection_commands.h"

#include <cstddef>
#include <exception>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace avm::tooling
{

namespace runner_detail
{

inline std::string_view trim_left(std::string_view line) noexcept
{
	while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r'))
		line.remove_prefix(1);
	return line;
}

} // namespace runner_detail

inline int run_inspection_script(InspectionSession &session, std::istream &input, std::ostream &output,
                                 std::ostream &errors)
{
	// Runner отвечает только за построчное framing и остановку при ошибке; parsing/execution/rendering остаются
	// в существующем typed tooling API и не образуют второй runtime path.
	std::string line;
	std::size_t line_number = 0;
	while (std::getline(input, line))
	{
		++line_number;
		const std::string_view command_line = runner_detail::trim_left(line);
		if (command_line.empty() || command_line.front() == '#')
			continue;

		try
		{
			const InspectionCommand command = parse_inspection_command(command_line);
			const InspectionResult result = execute_inspection_command(session, command);
			output << render_inspection_result(result) << '\n';
		}
		catch (const std::exception &error)
		{
			errors << "line " << line_number << ": " << error.what() << '\n';
			return 1;
		}
	}

	if (input.bad())
	{
		errors << "inspection script input failure\n";
		return 1;
	}
	return 0;
}

} // namespace avm::tooling
