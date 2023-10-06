// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerMappableKeySlot.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

FPlayerMappableKeySlot FPlayerMappableKeySlot::FirstKeySlot = FPlayerMappableKeySlot(0);
FPlayerMappableKeySlot FPlayerMappableKeySlot::SecondKeySlot = FPlayerMappableKeySlot(1);
FPlayerMappableKeySlot FPlayerMappableKeySlot::ThirdKeySlot = FPlayerMappableKeySlot(2);
FPlayerMappableKeySlot FPlayerMappableKeySlot::FourthKeySlot = FPlayerMappableKeySlot(3);

FPlayerMappableKeySlot::FPlayerMappableKeySlot() { }
FPlayerMappableKeySlot::FPlayerMappableKeySlot(const int32 InSlotNumber) : SlotNumber(InSlotNumber) { }

int32 FPlayerMappableKeySlot::GetSlotNumber() const
{
	return SlotNumber;
}

uint32 GetTypeHash(const FPlayerMappableKeySlot& Ref)
{
	return GetTypeHash(Ref.GetSlotNumber());
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS