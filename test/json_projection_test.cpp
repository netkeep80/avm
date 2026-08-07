#include "avm/json_compat.h"

#include <cassert>

namespace
{

using Json = nlohmann::json;

Json run(const Json &expression, std::size_t max_depth = 1000)
{
	avm::JsonCompatibilitySession session(max_depth);
	return session.interpret(expression);
}

} // namespace

int main()
{
	assert(run(true) == Json(true));
	assert(run(false) == Json(false));
	assert(run(nullptr).is_null());

	assert(run(Json{{"Not", Json::array({true})}}) == Json(false));
	assert(run(Json{{"Not", Json::array({false})}}) == Json(true));

	assert(run(Json{{"And", Json::array({false, false})}}) == Json(false));
	assert(run(Json{{"And", Json::array({false, true})}}) == Json(false));
	assert(run(Json{{"And", Json::array({true, false})}}) == Json(false));
	assert(run(Json{{"And", Json::array({true, true})}}) == Json(true));

	assert(run(Json{{"Or", Json::array({false, false})}}) == Json(false));
	assert(run(Json{{"Or", Json::array({false, true})}}) == Json(true));
	assert(run(Json{{"Or", Json::array({true, false})}}) == Json(true));
	assert(run(Json{{"Or", Json::array({true, true})}}) == Json(true));

	const Json nested = {{"Not", Json::array({Json{{"And", Json::array({true, false})}}})}};
	assert(run(nested) == Json(true));

	const Json deeply_nested = {{"And",
	                             Json::array({Json{{"Or", Json::array({true, false})}},
	                                          Json{{"Not", Json::array({false})}}})}};
	assert(run(deeply_nested) == Json(true));

	assert(run(Json{{"If", Json::array({true, false, true})}}) == Json(false));
	assert(run(Json{{"If", Json::array({false, false, true})}}) == Json(true));

	const Json lazy_true = {
	    {"If", Json::array({true, true, Json{{"Call", Json::array({"missing"})}}})}};
	assert(run(lazy_true) == Json(true));

	const Json lazy_false = {
	    {"If", Json::array({false, Json{{"Call", Json::array({"missing"})}}, false})}};
	assert(run(lazy_false) == Json(false));

	const Json selected_failure = {
	    {"If", Json::array({true, Json{{"Call", Json::array({"missing"})}}, false})}};
	assert(run(selected_failure).is_null());

	assert(run(Json::array()).is_null());
	assert(run(Json::array({true, false, true})) == Json(true));

	const Json identity_program = Json::array({
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	    Json{{"Call", Json::array({"id", true})}},
	});
	assert(run(identity_program) == Json(true));

	const Json two_argument_program = Json::array({
	    Json{{"Def",
	          Json::array({"both", Json::array({"a", "b"}), Json{{"And", Json::array({"a", "b"})}}})}},
	    Json{{"Call", Json::array({"both", true, false})}},
	});
	assert(run(two_argument_program) == Json(false));

	const Json nested_calls = Json::array({
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	    Json{{"Def",
	          Json::array({"negate", Json::array({"x"}),
	                       Json{{"Not", Json::array({Json{{"Call", Json::array({"id", "x"})}}})}}})}},
	    Json{{"Call", Json::array({"negate", false})}},
	});
	assert(run(nested_calls) == Json(true));

	const Json shadowing = Json::array({
	    Json{{"Def", Json::array({"inner", Json::array({"x"}), "x"})}},
	    Json{{"Def",
	          Json::array({"outer", Json::array({"x"}), Json{{"Call", Json::array({"inner", false})}}})}},
	    Json{{"Call", Json::array({"outer", true})}},
	});
	assert(run(shadowing) == Json(false));

	const Json finite_recursive = Json::array({
	    Json{{"Def",
	          Json::array({"recur", Json::array({"flag"}),
	                       Json{{"If",
	                             Json::array({"flag", Json{{"Call", Json::array({"recur", false})}}, true})}}})}},
	    Json{{"Call", Json::array({"recur", true})}},
	});
	assert(run(finite_recursive, 16) == Json(true));

	const Json infinite_recursive = Json::array({
	    Json{{"Def",
	          Json::array({"loop", Json::array({"x"}), Json{{"Call", Json::array({"loop", "x"})}}})}},
	    Json{{"Call", Json::array({"loop", true})}},
	});
	assert(run(infinite_recursive, 4).is_null());

	const Json call_before_def = Json::array({
	    Json{{"Call", Json::array({"id", true})}},
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	});
	assert(run(call_before_def).is_null());

	const Json forward_reference = Json::array({
	    Json{{"Def",
	          Json::array({"first", Json::array({"x"}), Json{{"Call", Json::array({"second", "x"})}}})}},
	    Json{{"Def", Json::array({"second", Json::array({"y"}), "y"})}},
	    Json{{"Call", Json::array({"first", true})}},
	});
	assert(run(forward_reference) == Json(true));

	const Json redefinition = Json::array({
	    Json{{"Def", Json::array({"value", Json::array(), true})}},
	    Json{{"Call", Json::array({"value"})}},
	    Json{{"Def", Json::array({"value", Json::array(), false})}},
	    Json{{"Call", Json::array({"value"})}},
	});
	assert(run(redefinition) == Json(false));

	const Json number_identity = Json::array({
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	    Json{{"Call", Json::array({"id", 42})}},
	});
	assert(run(number_identity) == Json(42));

	const Json float_identity = Json::array({
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	    Json{{"Call", Json::array({"id", 3.25})}},
	});
	assert(run(float_identity) == Json(3.25));

	const Json string_identity = Json::array({
	    Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}},
	    Json{{"Call", Json::array({"id", "hello"})}},
	});
	assert(run(string_identity) == Json("hello"));

	avm::JsonCompatibilitySession detached_session;
	Json source = Json{{"Not", Json::array({false})}};
	const avm::LinkId detached_root = detached_session.import_program(source);
	source = nullptr;
	const avm::RelationEntity detached_entity =
	    avm::decode_relation_entity(detached_session.store(), detached_root);
	assert(detached_entity.relation == detached_session.runtime().vocabulary().not_relation);
	const avm::LinkId detached_result = detached_session.execute(detached_root);
	assert(detached_session.project_result(detached_result) == Json(true));

	avm::JsonCompatibilitySession persistent_session;
	assert(persistent_session.interpret(
	           Json{{"Def", Json::array({"id", Json::array({"x"}), "x"})}})
	           .is_null());
	assert(persistent_session.interpret(Json{{"Call", Json::array({"id", false})}}) == Json(false));

	assert(run(Json::object()).is_null());
	assert(run(Json{{"Not", Json::array({true})}, {"And", Json::array({true, true})}}).is_null());
	assert(run(Json{{"Xor", Json::array({true, false})}}).is_null());
	assert(run(Json{{"Not", Json::array()}}).is_null());
	assert(run(Json{{"Not", true}}).is_null());
	assert(run(Json{{"Not", Json::array({true, false})}}).is_null());
	assert(run(Json{{"And", Json::array({true})}}).is_null());
	assert(run(Json{{"Or", Json::array({true, false, true})}}).is_null());
	assert(run(Json{{"If", Json::array({true, false})}}).is_null());
	assert(run(Json{{"If", Json::array({nullptr, true, false})}}).is_null());
	assert(run(Json{{"Def", Json::array({42, Json::array(), true})}}).is_null());
	assert(run(Json{{"Def", Json::array({"f", true, true})}}).is_null());
	assert(run(Json{{"Def", Json::array({"f", Json::array({42}), true})}}).is_null());
	assert(run(Json{{"Call", Json::array()}}).is_null());
	assert(run(Json{{"Call", Json::array({42})}}).is_null());
	assert(run(Json{{"Call", Json::array({"missing", true})}}).is_null());

	return 0;
}
