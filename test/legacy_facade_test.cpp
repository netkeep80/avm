#include "avm/legacy_json_compat.h"

#include <cassert>

namespace
{

json interpret_and_export(const json &expression)
{
	rel_t *result = interpret(expression);
	json projected;
	export_json(result, projected);
	return projected;
}

} // namespace

int main()
{
	clear_func_env();
	assert(interpret(json(true)) == rel_t::True);
	assert(interpret(json(false)) == rel_t::False);
	assert(interpret(json(nullptr)) == rel_t::E);
	assert(interpret_and_export(json{{"Not", json::array({false})}}) == json(true));

	clear_func_env();
	const json definition = {{"Def", json::array({"id", json::array({"x"}), "x"})}};
	assert(interpret(definition) == rel_t::E);
	assert(interpret_and_export(json{{"Call", json::array({"id", 42})}}) == json(42));
	assert(interpret_and_export(json{{"Call", json::array({"id", "hello"})}}) == json("hello"));

	clear_func_env();
	assert(interpret(json{{"Call", json::array({"id", true})}}) == rel_t::E);

	clear_func_env();
	const json recursive_call = {{"Call", json::array({"recur", false})}};
	const json recursive_if = {{"If", json::array({"flag", recursive_call, true})}};
	const json recursive_def = {{"Def", json::array({"recur", json::array({"flag"}), recursive_if})}};
	const json recursive_program = json::array({recursive_def, json{{"Call", json::array({"recur", true})}}});
	assert(interpret(recursive_program) == rel_t::True);

	clear_func_env();
	const json continue_after_error = json::array({
	    json{{"Call", json::array({"later", true})}},
	    json{{"Def", json::array({"later", json::array({"x"}), "x"})}},
	    json{{"Call", json::array({"later", false})}},
	});
	assert(interpret(continue_after_error) == rel_t::False);

	clear_func_env();
	const json lazy = {{"If", json::array({true, true, json{{"Call", json::array({"missing"})}}})}};
	assert(interpret(lazy) == rel_t::True);

	return 0;
}
