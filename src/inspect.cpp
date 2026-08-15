#include "inspection_runner.h"

#include "avm/link_store.h"
#include "avm/program_model.h"

#include <fstream>
#include <iostream>
#include <string>

namespace
{

int run(std::istream &input)
{
	avm::InMemoryLinkStore store;
	const avm::BootstrapVocabulary vocabulary = avm::BootstrapVocabulary::create(store);
	avm::tooling::InspectionSession session(store, vocabulary);
	return avm::tooling::run_inspection_script(session, input, std::cout, std::cerr);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc == 1)
		return run(std::cin);

	if (argc != 2)
	{
		std::cerr << "usage: avm-inspect [script-file]\n";
		return 2;
	}

	std::ifstream input(argv[1]);
	if (!input)
	{
		std::cerr << "cannot open inspection script: " << argv[1] << '\n';
		return 2;
	}
	return run(input);
}
