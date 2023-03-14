// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SafeZoneSlot.h"

#include "Components/SafeZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SafeZoneSlot)

USafeZoneSlot::USafeZoneSlot()
{
	bIsTitleSafe = true;
	SafeAreaScale = FMargin(1, 1, 1, 1);
}

void USafeZoneSlot::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( IsValid( Parent ) )
	{
		CastChecked< USafeZone >( Parent )->UpdateWidgetProperties();
	}
}

