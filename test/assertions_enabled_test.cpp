#include <cassert>

#ifdef NDEBUG
#error "AVM test targets must be compiled with assertions enabled"
#endif

int main()
{
	bool assertion_expression_evaluated = false;
	assert((assertion_expression_evaluated = true));
	return assertion_expression_evaluated ? 0 : 1;
}
