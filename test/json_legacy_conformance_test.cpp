#include "avm.h"
#include "avm/json_compat.h"

#include <cassert>
#include <iostream>
#include <vector>

rel_t *interpret(const json &expr);
void export_json(const rel_t *ent, json &j);
void clear_func_env();

namespace
{

json legacy_result(const json &expression)
{
	clear_func_env();
	rel_t *result = interpret(expression);
	json output;
	export_json(result, output);
	return output;
}

json new_result(const json &expression)
{
	avm::JsonCompatibilitySession session;
	return session.interpret(expression);
}

void require_same(const json &expression)
{
	const json legacy = legacy_result(expression);
	const json current = new_result(expression);
	if (current != legacy)
	{
		std::cerr << "JSON conformance mismatch\n"
		          << "expression: " << expression.dump() << '\n'
		          << "legacy:     " << legacy.dump() << '\n'
		          << "new:        " << current.dump() << std::endl;
	}
	assert(current == legacy);
}

} // namespace

int main()
{
	const std::vector<json> cases{
	    true,
	    false,
	    nullptr,
	    json{{"Not", json::array({true})}},
	    json{{"Not", json::array({false})}},
	    json{{"And", json::array({false, false})}},
	    json{{"And", json::array({false, true})}},
	    json{{"And", json::array({true, false})}},
	    json{{"And", json::array({true, true})}},
	    json{{"Or", json::array({false, false})}},
	    json{{"Or", json::array({false, true})}},
	    json{{"Or", json::array({true, false})}},
	    json{{"Or", json::array({true, true})}},
	    json{{"Not", json::array({json{{"And", json::array({true, false})}}})}},
	    json{{"If", json::array({true, false, true})}},
	    json{{"If", json::array({false, false, true})}},
	    json{{"If", json::array({true, true, json{{"Call", json::array({"missing"})}}})}},
	    json::array({true, false, true}),
	    json::array({json{{"Def", json::array({"id", json::array({"x"}), "x"})}},
	                 json{{"Call", json::array({"id", true})}}}),
	    json::array({json{{"Def",
	                      json::array({"both", json::array({"a", "b"}),
	                                   json{{"And", json::array({"a", "b"})}}})}},
	                 json{{"Call", json::array({"both", true, false})}}}),
	    json::array({json{{"Def",
	                      json::array({"recur", json::array({"flag"}),
	                                   json{{"If", json::array({"flag",
	                                                             json{{"Call", json::array({"recur", false})}},
	                                                             true})}}})}},
	                 json{{"Call", json::array({"recur", true})}}}),
	    json::array({json{{"Def", json::array({"id", json::array({"x"}), "x"})}},
	                 json{{"Call", json::array({"id", 42})}}}),
	    json::array({json{{"Def", json::array({"id", json::array({"x"}), "x"})}},
	                 json{{"Call", json::array({"id", "hello"})}}}),
	    json::array({json{{"Call", json::array({"later", true})}},
	                 json{{"Def", json::array({"later", json::array({"x"}), "x"})}},
	                 json{{"Call", json::array({"later", true})}}}),
	    json::object(),
	    json{{"Xor", json::array({true, false})}},
	    json{{"Not", json::array()}},
	    json{{"Not", true}},
	    json{{"Not", json::array({true, false})}},
	    json{{"If", json::array({nullptr, true, false})}},
	    json{{"Def", json::array({42, json::array(), true})}},
	    json{{"Call", json::array({"missing", true})}},
	};

	for (const json &expression : cases)
		require_same(expression);

	return 0;
}
