// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalObjectLocatorFragmentType.h"
#include "UniversalObjectLocatorFragment.h"
#include "UniversalObjectLocatorStringParams.h"
#include "UniversalObjectLocatorInitializeParams.h"
#include "UniversalObjectLocatorInitializeResult.h"

namespace UE::UniversalObjectLocator
{

FResolveResult FFragmentType::ResolvePayload(const void* Payload, const FResolveParams& Params) const
{
	return (*InstanceBindings.Resolve)(Payload, Params);
}
FInitializeResult FFragmentType::InitializePayload(void* Payload, const FInitializeParams& InParams) const
{
	return (*InstanceBindings.Initialize)(Payload, InParams);
}
void FFragmentType::ToString(const void* Payload, FStringBuilderBase& OutStringBuilder) const
{
	const int32 StartPos = OutStringBuilder.Len();

	(*InstanceBindings.ToString)(Payload, OutStringBuilder);

#if DO_CHECK
	FStringView StringRepresentation(OutStringBuilder.GetData() + StartPos, OutStringBuilder.Len());
	checkf(!FAsciiSet::HasAny(StringRepresentation, ~FUniversalObjectLocatorFragment::ValidFragmentPayloadCharacters), TEXT("F%s::ToString resulted in an invalid character usage"), *PayloadType->GetName());
#endif
}
FParseStringResult FFragmentType::TryParseString(void* Payload, FStringView InString, const FParseStringParams& Params) const
{
	return (*InstanceBindings.TryParseString)(Payload, InString, Params);
}

uint32 FFragmentType::ComputePriority(const UObject* Object, const UObject* Context) const
{
	return (*StaticBindings.Priority)(Object, Context);
}

} // namespace UE::UniversalObjectLocator