// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PanelSlot.h"
#include "Layout/Margin.h"

#include "SafeZoneSlot.generated.h"

UCLASS()
class UMG_API USafeZoneSlot : public UPanelSlot
{
	GENERATED_BODY()
public:

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = SafeZone )
	bool bIsTitleSafe;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = SafeZone )
	FMargin SafeAreaScale;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = SafeZone )
	TEnumAsByte< EHorizontalAlignment > HAlign;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = SafeZone )
	TEnumAsByte< EVerticalAlignment > VAlign;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = SafeZone )
	FMargin Padding;

	USafeZoneSlot();

	virtual void SynchronizeProperties() override;
};
