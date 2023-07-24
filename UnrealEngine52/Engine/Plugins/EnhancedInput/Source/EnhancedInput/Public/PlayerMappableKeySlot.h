// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerMappableKeySlot.generated.h"

/**
* Explicitly identifies the slot for a player mappable key
* Experimental: Do not count on long term support for this structure.
*/
USTRUCT(BlueprintType, DisplayName="Player Mappable Key Slot (Experimental)")
struct ENHANCEDINPUT_API FPlayerMappableKeySlot
{
	GENERATED_BODY()

public:

	FPlayerMappableKeySlot();
	FPlayerMappableKeySlot(int32 InSlotNumber);
	virtual ~FPlayerMappableKeySlot();

	virtual int32 GetSlotNumber() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	int32 SlotNumber = 0;

	static FPlayerMappableKeySlot FirstKeySlot;
	static FPlayerMappableKeySlot SecondKeySlot;
	static FPlayerMappableKeySlot ThirdKeySlot;
	static FPlayerMappableKeySlot FourthKeySlot;

	bool operator==(const FPlayerMappableKeySlot& OtherKeySlot) const
	{
		return GetSlotNumber() == OtherKeySlot.GetSlotNumber();
	}

};

ENHANCEDINPUT_API uint32 GetTypeHash(const FPlayerMappableKeySlot& Ref);
