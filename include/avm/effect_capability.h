#pragma once

#include "avm/executor.h"
#include "avm/text_value.h"

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avm
{

class EffectCapabilityDenied final : public std::runtime_error
{
public:
	EffectCapabilityDenied() : std::runtime_error("effect capability denied") {}
};

class EffectProviderUnavailable final : public std::runtime_error
{
public:
	EffectProviderUnavailable() : std::runtime_error("effect provider unavailable") {}
};

class EffectLookupMiss final : public std::runtime_error
{
public:
	EffectLookupMiss() : std::runtime_error("external entity lookup miss") {}
};

class EffectCapabilityPolicy
{
public:
	EffectCapabilityPolicy() = default;
	explicit EffectCapabilityPolicy(std::set<LinkId> allowed) : allowed_(std::move(allowed)) {}

	bool allows(LinkId capability) const noexcept { return allowed_.contains(capability); }

private:
	std::set<LinkId> allowed_;
};

class ExternalEntityProvider
{
public:
	virtual ~ExternalEntityProvider() = default;
	virtual std::optional<LinkId> lookup(std::string_view name) = 0;
};

struct ExternalEntityLookupEffect
{
	LinkId relation;
	LinkId capability;
	LinkId unit;
	TextVocabulary text;
};

inline void register_external_entity_lookup_effect(Executor &executor, const ExternalEntityLookupEffect &effect,
                                                   const EffectCapabilityPolicy &policy,
                                                   ExternalEntityProvider *provider)
{
	const LinkStore &store = executor.store();
	if (!store.contains(effect.relation))
		throw std::invalid_argument("effect relation is not present in LinkStore");
	if (!store.contains(effect.capability))
		throw std::invalid_argument("effect capability is not present in LinkStore");
	if (!store.contains(effect.unit))
		throw std::invalid_argument("effect unit is not present in LinkStore");
	validate_text_vocabulary(store, effect.text);

	// Authority фиксируется в момент binding: handler не должен зависеть от lifetime локального policy объекта.
	executor.register_native(
	    effect.relation,
	    [effect, policy, provider](const ExecutionContext &context, Executor &runtime) -> ExecutionOutcome
	    {
		    if (context.subject != effect.unit)
			    throw std::runtime_error("external entity lookup is not an executable effect expression");
		    if (!policy.allows(effect.capability))
			    throw EffectCapabilityDenied();
		    if (provider == nullptr)
			    throw EffectProviderUnavailable();

		    const std::vector<std::uint8_t> bytes = decode_text(runtime.store(), effect.text, context.object);
		    const std::string name(bytes.begin(), bytes.end());
		    const std::optional<LinkId> result = provider->lookup(name);
		    if (!result)
			    throw EffectLookupMiss();

		    // Внешний lookup наблюдает host data, но не получает права materialize-ить graph AVM.
		    if (!runtime.store().contains(*result))
			    throw std::runtime_error("external entity provider returned an unknown LinkId");

		    return ExecutionOutcome{*result};
	    });
}

} // namespace avm
