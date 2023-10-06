// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerMappableKeySlot.generated.h"

/**
* Explicitly identifies the slot for a player mappable key
* Experimental: Do not count on long term support for this structure.
*/

struct UE_DEPRECATED(5.3, "FPlayerMappableKeySlot has been deprecated. Please use EPlayerMappableKeySlot instead.") FPlayerMappableKeySlot;

USTRUCT(BlueprintType, DisplayName="Player Mappable Key Slot (Experimental)", meta = (ScriptName = "PlayerMappableKeySlotData"))
struct ENHANCEDINPUT_API FPlayerMappableKeySlot
{
	GENERATED_BODY()

public:

	FPlayerMappableKeySlot();
	FPlayerMappableKeySlot(int32 InSlotNumber);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPlayerMappableKeySlot(FPlayerMappableKeySlot&&) = default;
	FPlayerMappableKeySlot(const FPlayerMappableKeySlot& Other) = default;
	FPlayerMappableKeySlot& operator=(const FPlayerMappableKeySlot& Other) = default;
	FPlayerMappableKeySlot& operator=(FPlayerMappableKeySlot&&) = default;
	~FPlayerMappableKeySlot() = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	int32 GetSlotNumber() const;
	
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS

ENHANCEDINPUT_API uint32 GetTypeHash(const FPlayerMappableKeySlot& Ref);

PRAGMA_ENABLE_DEPRECATION_WARNINGS