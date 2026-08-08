#include "avm/json_compat.h"

#include <cassert>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<avm::JsonCompatibilitySession>);
static_assert(!std::is_copy_assignable_v<avm::JsonCompatibilitySession>);
static_assert(!std::is_move_constructible_v<avm::JsonCompatibilitySession>);
static_assert(!std::is_move_assignable_v<avm::JsonCompatibilitySession>);

namespace
{

using Json = nlohmann::json;

void test_primitives()
{
	avm::JsonCompatibilitySession session;
	assert(session.interpret(Json(true)) == Json(true));
	assert(session.interpret(Json(false)) == Json(false));
	assert(session.interpret(Json(nullptr)) == Json(nullptr));
	assert(session.interpret(Json{{"Not", Json::array({false})}}) == Json(true));
}

void test_persistent_definition_session()
{
	avm::JsonCompatibilitySession session;
	const Json definition = {{"Def", Json::array({"id", Json::array({"x"}), "x"})}};
	assert(session.interpret(definition) == Json(nullptr));
	assert(session.interpret(Json{{"Call", Json::array({"id", 42})}}) == Json(42));
	assert(session.interpret(Json{{"Call", Json::array({"id", "hello"})}}) == Json("hello"));
}

void test_undefined_function_compatibility()
{
	avm::JsonCompatibilitySession session;
	assert(session.interpret(Json{{"Call", Json::array({"id", true})}}) == Json(nullptr));
}

void test_recursion()
{
	avm::JsonCompatibilitySession session;
	const Json recursive_call = {{"Call", Json::array({"recur", false})}};
	const Json recursive_if = {{"If", Json::array({"flag", recursive_call, true})}};
	const Json recursive_def = {{"Def", Json::array({"recur", Json::array({"flag"}), recursive_if})}};
	const Json program = Json::array({recursive_def, Json{{"Call", Json::array({"recur", true})}}});
	assert(session.interpret(program) == Json(true));
}

void test_sequence_continues_after_compatibility_error()
{
	avm::JsonCompatibilitySession session;
	const Json program = Json::array({
	    Json{{"Call", Json::array({"later", true})}},
	    Json{{"Def", Json::array({"later", Json::array({"x"}), "x"})}},
	    Json{{"Call", Json::array({"later", false})}},
	});
	assert(session.interpret(program) == Json(false));
}

void test_lazy_if()
{
	avm::JsonCompatibilitySession session;
	const Json expression = {{"If", Json::array({true, true, Json{{"Call", Json::array({"missing"})}}})}};
	assert(session.interpret(expression) == Json(true));
}

} // namespace

int main()
{
	test_primitives();
	test_persistent_definition_session();
	test_undefined_function_compatibility();
	test_recursion();
	test_sequence_continues_after_compatibility_error();
	test_lazy_if();
	return 0;
}
