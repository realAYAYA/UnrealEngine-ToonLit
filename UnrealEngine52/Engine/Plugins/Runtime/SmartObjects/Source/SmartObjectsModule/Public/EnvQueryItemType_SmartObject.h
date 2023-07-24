// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "SmartObjectSubsystem.h"
#include "EnvQueryItemType_SmartObject.generated.h"


struct FSmartObjectSlotEQSItem
{
	FVector Location;
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectSlotHandle SlotHandle;

	FSmartObjectSlotEQSItem(const FVector InLocation, const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectSlotHandle InSlotHandle)
		: Location(InLocation), SmartObjectHandle(InSmartObjectHandle), SlotHandle(InSlotHandle)
	{}

	FORCEINLINE operator FVector() const { return Location; }

	bool operator==(const FSmartObjectSlotEQSItem& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle && SlotHandle == Other.SlotHandle;
	}
};


UCLASS()
class SMARTOBJECTSMODULE_API UEnvQueryItemType_SmartObject : public UEnvQueryItemType_VectorBase
{
	GENERATED_BODY()
public:
	typedef FSmartObjectSlotEQSItem FValueType;

	UEnvQueryItemType_SmartObject();

	static const FSmartObjectSlotEQSItem& GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FSmartObjectSlotEQSItem& Value);

	virtual FVector GetItemLocation(const uint8* RawData) const override;
};
