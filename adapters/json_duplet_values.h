#pragma once

#include "json_duplet_projection.h"

#include "avm/integer_value.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace avm::json_duplet
{

using SymbolAnchors = std::map<std::string, LinkId>;

inline ProjectionRef append_integer_projection(ProjectionDescription &description, const IntegerVocabulary &vocabulary,
                                               std::int64_t value)
{
	if (value == 0)
		return ProjectionRef::anchor(vocabulary.zero);

	const std::vector<bool> bits = integer_magnitude_bits(integer_unsigned_magnitude(value));
	ProjectionRef suffix = ProjectionRef::anchor(vocabulary.magnitude_end);
	for (auto bit = bits.rbegin(); bit != bits.rend(); ++bit)
	{
		const LinkId marker = *bit ? vocabulary.bit_one : vocabulary.bit_zero;
		const ProjectionNodeId node_id = description.nodes.size();
		description.nodes.push_back(ProjectionNode{ProjectionRef::anchor(marker), suffix});
		suffix = ProjectionRef::node(node_id);
	}

	const LinkId sign = value < 0 ? vocabulary.negative : vocabulary.positive;
	const ProjectionNodeId wrapper_id = description.nodes.size();
	description.nodes.push_back(ProjectionNode{ProjectionRef::anchor(sign), suffix});
	return ProjectionRef::node(wrapper_id);
}

class NativeLeafResolver
{
public:
	NativeLeafResolver(IntegerVocabulary integers, const SymbolAnchors &symbols) : integers_(integers), symbols_(symbols) {}

	template <typename Json>
	ProjectionRef operator()(const Json &value, ProjectionDescription &description, const std::string &path) const
	{
		if (!value.is_object() || value.size() != 1)
			throw ProjectionError(path + ": unsupported native AVM leaf; expected exactly one leaf marker");

		if (value.contains("$link"))
			return ProjectionRef::anchor(detail::decode_link_anchor(value, path));
		if (value.contains("$symbol"))
			return resolve_symbol(value, path);
		if (value.contains("$integer"))
			return resolve_integer(value, description, path);

		throw ProjectionError(path + ": unsupported native AVM leaf marker");
	}

private:
	template <typename Json> ProjectionRef resolve_symbol(const Json &value, const std::string &path) const
	{
		const Json &encoded_symbol = value.at("$symbol");
		if (!encoded_symbol.is_string())
			throw ProjectionError(path + ".$symbol: symbol name must be a string");

		const std::string name = encoded_symbol.template get<std::string>();
		const auto symbol = symbols_.find(name);
		if (symbol == symbols_.end())
			throw ProjectionError(path + ".$symbol: unknown symbol: " + name);
		if (symbol->second == invalid_link_id)
			throw ProjectionError(path + ".$symbol: symbol resolves to invalid LinkId 0");
		return ProjectionRef::anchor(symbol->second);
	}

	template <typename Json>
	ProjectionRef resolve_integer(const Json &value, ProjectionDescription &description, const std::string &path) const
	{
		const Json &encoded_integer = value.at("$integer");
		std::int64_t integer = 0;
		if (encoded_integer.is_number_unsigned())
		{
			const std::uint64_t unsigned_value = encoded_integer.template get<std::uint64_t>();
			const std::uint64_t max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
			if (unsigned_value > max)
				throw ProjectionError(path + ".$integer: value exceeds int64 host projection domain");
			integer = static_cast<std::int64_t>(unsigned_value);
		}
		else if (encoded_integer.is_number_integer())
		{
			integer = encoded_integer.template get<std::int64_t>();
		}
		else
		{
			throw ProjectionError(path + ".$integer: value must be an integer JSON number");
		}

		return append_integer_projection(description, integers_, integer);
	}

	IntegerVocabulary integers_;
	const SymbolAnchors &symbols_;
};

} // namespace avm::json_duplet
