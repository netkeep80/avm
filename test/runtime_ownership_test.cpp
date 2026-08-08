#include "avm/bootstrap_runtime.h"

#include <cassert>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<avm::Executor>);
static_assert(!std::is_copy_assignable_v<avm::Executor>);
static_assert(!std::is_move_constructible_v<avm::Executor>);
static_assert(!std::is_move_assignable_v<avm::Executor>);

static_assert(!std::is_copy_constructible_v<avm::BootstrapRuntime>);
static_assert(!std::is_copy_assignable_v<avm::BootstrapRuntime>);
static_assert(!std::is_move_constructible_v<avm::BootstrapRuntime>);
static_assert(!std::is_move_assignable_v<avm::BootstrapRuntime>);

int main()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId expression = builder.logical_not(builder.literal(runtime.vocabulary().false_value));
	assert(runtime.execute(expression) == runtime.vocabulary().true_value);
	return 0;
}
