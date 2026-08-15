#include "inspection_runner.h"

#include "avm/link_store.h"
#include "avm/program_model.h"

#include <cassert>
#include <sstream>
#include <string>

namespace
{

struct Fixture
{
	avm::InMemoryLinkStore store;
	avm::BootstrapVocabulary vocabulary = avm::BootstrapVocabulary::create(store);
	avm::tooling::InspectionSession session{store, vocabulary};
};

void test_read_only_script_is_ordered_and_non_mutating()
{
	Fixture fixture;
	const std::size_t before = fixture.store.size();
	std::istringstream input("  # comment\n\nfind 999999 999998\noutgoing 999999\n");
	std::ostringstream output;
	std::ostringstream errors;

	assert(avm::tooling::run_inspection_script(fixture.session, input, output, errors) == 0);
	const std::string expected = "find begin=999999 end=999998 id=-\noutgoing endpoint=999999 ids=[]\n";
	assert(output.str() == expected);
	assert(errors.str().empty());
	assert(fixture.store.size() == before);
}

void test_parser_failure_reports_line_and_stops()
{
	Fixture fixture;
	std::istringstream input("find 999999 999998\nunknown-command\nfind 1 1\n");
	std::ostringstream output;
	std::ostringstream errors;

	assert(avm::tooling::run_inspection_script(fixture.session, input, output, errors) == 1);
	assert(output.str() == "find begin=999999 end=999998 id=-\n");
	assert(errors.str().find("line 2: unknown inspection command: unknown-command") == 0);
}

void test_runtime_failure_reports_line_and_stops()
{
	Fixture fixture;
	std::istringstream input("# first line\nlink 999999\nfind 1 1\n");
	std::ostringstream output;
	std::ostringstream errors;

	assert(avm::tooling::run_inspection_script(fixture.session, input, output, errors) == 1);
	assert(output.str().empty());
	assert(errors.str().find("line 2:") == 0);
}

} // namespace

int main()
{
	test_read_only_script_is_ordered_and_non_mutating();
	test_parser_failure_reports_line_and_stops();
	test_runtime_failure_reports_line_and_stops();
}
