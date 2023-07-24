// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/Accessors/PCGPropertyAccessor.h"

#include "UObject/EnumProperty.h"

TArray<const void*> PCGPropertyAccessor::GetContainerKeys(int32 Index, int32 Range, const IPCGAttributeAccessorKeys& Keys)
{
	TArray<const void*> ContainerKeys;
	ContainerKeys.SetNum(Range);
	TArrayView<const void*> ContainerKeysView(ContainerKeys);
	if (!Keys.GetKeys(Index, ContainerKeysView))
	{
		return TArray<const void*>{};
	}

	return ContainerKeys;
}

TArray<void*> PCGPropertyAccessor::GetContainerKeys(int32 Index, int32 Range, IPCGAttributeAccessorKeys& Keys)
{
	TArray<void*> ContainerKeys;
	ContainerKeys.SetNum(Range);
	TArrayView<void*> ContainerKeysView(ContainerKeys);
	if (!Keys.GetKeys(Index, ContainerKeysView))
	{
		return TArray<void*>{};
	}

	return ContainerKeys;
}

bool FPCGEnumPropertyAccessor::GetRangeImpl(TArrayView<int64> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
{
	return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> Type
		{
			return Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(PropertyAddressData);
		});
}

bool FPCGEnumPropertyAccessor::SetRangeImpl(TArrayView<const int64> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
{
	return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const int64& Value) -> void
		{
			Property->GetUnderlyingProperty()->SetIntPropertyValue(PropertyAddressData, Value);
		});
}