#include <avm/avm.h>

int main()
{
	static_assert(avm::version_major == 1);
	static_assert(avm::version_minor == 0);
	static_assert(avm::version_patch == 0);
	static_assert(avm::version_string == "1.0.0");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	const avm::LinkId result = runtime.execute(expression);

	return result == runtime.vocabulary().true_value ? 0 : 1;
}
