#include "json_duplet_converter.h"

#include <cassert>
#include <string>

namespace
{

using Json = avm::json_duplet::Json;

bool conversion_rejected(const Json &value, bool forward)
{
	try
	{
		if (forward)
			static_cast<void>(avm::json_duplet::convert_explicit_triplets_to_duplets(value));
		else
			static_cast<void>(avm::json_duplet::convert_relation_duplets_to_explicit_triplets(value));
		return false;
	}
	catch (const avm::json_duplet::ConversionError &)
	{
		return true;
	}
}

Json relation(Json relation_value, Json subject_value, Json object_value)
{
	Json result = Json::object();
	result["$rel"] = std::move(relation_value);
	result["$sub"] = std::move(subject_value);
	result["$obj"] = std::move(object_value);
	return result;
}

Json duplet(Json begin, Json end)
{
	Json result = Json::object();
	result["<<"] = std::move(begin);
	result[">>"] = std::move(end);
	return result;
}

} // namespace

int main()
{
	const Json explicit_relation = relation("R", "S", "O");
	const Json expected_relation = duplet("R", duplet("S", "O"));
	const Json converted = avm::json_duplet::convert_explicit_triplets_to_duplets(explicit_relation);
	assert(converted == expected_relation);
	assert(converted.at(">>").at("<<") == "S");
	assert(converted.at(">>").at(">>") == "O");

	const Json nested = relation(relation("RR", "RS", "RO"), relation("SR", "SS", "SO"),
	                             relation("OR", "OS", "OO"));
	const Json nested_converted = avm::json_duplet::convert_explicit_triplets_to_duplets(nested);
	assert(nested_converted.at("<<") == duplet("RR", duplet("RS", "RO")));
	assert(nested_converted.at(">>").at("<<") == duplet("SR", duplet("SS", "SO")));
	assert(nested_converted.at(">>").at(">>") == duplet("OR", duplet("OS", "OO")));

	Json container = Json::object();
	container["name"] = "demo";
	container["items"] = Json::array({explicit_relation, 7, relation("X", "Y", "Z")});
	const Json container_converted = avm::json_duplet::convert_explicit_triplets_to_duplets(container);
	assert(container_converted.at("name") == "demo");
	assert(container_converted.at("items")[0] == expected_relation);
	assert(container_converted.at("items")[1] == 7);
	assert(container_converted.at("items")[2] == duplet("X", duplet("Y", "Z")));

	Json missing_subject = Json::object();
	missing_subject["$rel"] = "+";
	missing_subject["$obj"] = 1;
	assert(conversion_rejected(missing_subject, true));

	Json missing_object = Json::object();
	missing_object["$rel"] = "+";
	missing_object["$sub"] = 1;
	assert(conversion_rejected(missing_object, true));

	Json mixed_relation = explicit_relation;
	mixed_relation["extra"] = true;
	assert(conversion_rejected(mixed_relation, true));

	const Json restored = avm::json_duplet::convert_relation_duplets_to_explicit_triplets(converted);
	assert(restored == explicit_relation);

	const Json nested_restored = avm::json_duplet::convert_relation_duplets_to_explicit_triplets(nested_converted);
	assert(nested_restored == nested);

	Json malformed_begin = Json::object();
	malformed_begin["<<"] = "A";
	assert(conversion_rejected(malformed_begin, false));

	Json malformed_end = Json::object();
	malformed_end[">>"] = "B";
	assert(conversion_rejected(malformed_end, false));

	Json mixed_duplet = duplet("A", duplet("B", "C"));
	mixed_duplet["extra"] = 1;
	assert(conversion_rejected(mixed_duplet, false));

	const Json standalone_duplet = duplet("A", "B");
	assert(conversion_rejected(standalone_duplet, false));

	const Json scalar = 42;
	assert(avm::json_duplet::convert_explicit_triplets_to_duplets(scalar) == scalar);
	assert(avm::json_duplet::convert_relation_duplets_to_explicit_triplets(scalar) == scalar);

	return 0;
}
