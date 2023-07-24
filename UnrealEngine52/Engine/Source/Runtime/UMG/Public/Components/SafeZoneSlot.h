// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PanelSlot.h"
#include "Layout/Margin.h"

#include "SafeZoneSlot.generated.h"

class SSafeZone;

UCLASS()
class UMG_API USafeZoneSlot : public UPanelSlot
{
	GENERATED_BODY()
public:

	UE_DEPRECATED(5.2, "Direct access to bIsTitleSafe is deprecated. Please use the getter or setter.")
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Getter = "IsTitleSafe", Setter = "SetIsTitleSafe", Category = SafeZone)
	bool bIsTitleSafe;

	UE_DEPRECATED(5.2, "Direct access to SafeAreaScale is deprecated. Please use the getter or setter.")
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = SafeZone )
	FMargin SafeAreaScale;

	UE_DEPRECATED(5.2, "Direct access to HAlign is deprecated. Please use the getter or setter.")
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Getter = "GetHorizontalAlignment", Setter = "SetHorizontalAlignment", Category = SafeZone)
	TEnumAsByte< EHorizontalAlignment > HAlign;

	UE_DEPRECATED(5.2, "Direct access to VAlign is deprecated. Please use the getter or setter.")
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Getter = "GetVerticalAlignment", Setter = "SetVerticalAlignment", Category = SafeZone)
	TEnumAsByte< EVerticalAlignment > VAlign;

	UE_DEPRECATED(5.2, "Direct access to Padding is deprecated. Please use the getter or setter.")
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = SafeZone )
	FMargin Padding;

	USafeZoneSlot();

	void SetIsTitleSafe(bool InIsTitleSafe);
	bool IsTitleSafe() const;

	void SetSafeAreaScale(const FMargin& InSafeAreaScale);
	const FMargin& GetSafeAreaScale() const;

	void SetHorizontalAlignment(EHorizontalAlignment InHAlign);
	const EHorizontalAlignment GetHorizontalAlignment() const;

	void SetVerticalAlignment(EVerticalAlignment InVAlign);
	const EVerticalAlignment GetVerticalAlignment() const;

	void SetPadding(const FMargin& InPadding);
	const FMargin& GetPadding() const;

	//~ UPanelSlot interface
	virtual void SynchronizeProperties() override;
	//~ End of UPanelSlot interface

	/** Builds the underlying slot for the slate button. */
	void BuildSlot(TSharedRef<SSafeZone> InSafeZone);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:

	/** A pointer to the button to allow us to adjust the alignment, padding...etc at runtime. */
	TWeakPtr<SSafeZone> SafeZone;
};
