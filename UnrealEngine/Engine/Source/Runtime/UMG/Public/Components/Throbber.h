// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Widgets/Images/SThrobber.h"
#include "Throbber.generated.h"

class USlateBrushAsset;

/**
 * A Throbber widget that shows several zooming circles in a row.
 */
UCLASS()
class UMG_API UThrobber : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	
	/** How many pieces there are */
	UE_DEPRECATED(5.1, "Direct access to NumberOfPieces is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetNumberOfPieces", Category=Appearance, meta=( ClampMin="1", ClampMax="25", UIMin="1", UIMax="25" ))
	int32 NumberOfPieces;

	/** Should the pieces animate horizontally? */
	UE_DEPRECATED(5.1, "Direct access to bAnimateHorizontally is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="IsAnimateHorizontally", Setter="SetAnimateHorizontally", BlueprintSetter = "SetAnimateHorizontally", Category = Appearance)
	bool bAnimateHorizontally;

	/** Should the pieces animate vertically? */
	UE_DEPRECATED(5.1, "Direct access to bAnimateVertically is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="IsAnimateVertically", Setter="SetAnimateVertically", BlueprintSetter="SetAnimateVertically", Category = Appearance)
	bool bAnimateVertically;

	/** Should the pieces animate their opacity? */
	UE_DEPRECATED(5.1, "Direct access to bAnimateOpacity is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="IsAnimateOpacity", Setter="SetAnimateOpacity", BlueprintSetter="SetAnimateOpacity", Category = Appearance)
	bool bAnimateOpacity;

	/** The animated pieces. */
	UE_DEPRECATED(5.1, "Direct access to Image is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Appearance)
	FSlateBrush Image;

public:

	/** Sets how many pieces there are */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetNumberOfPieces(int32 InNumberOfPieces);
	int32 GetNumberOfPieces() const;

	/** Sets whether the pieces animate horizontally. */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimateHorizontally(bool bInAnimateHorizontally);
	bool IsAnimateHorizontally() const;

	/** Sets whether the pieces animate vertically. */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimateVertically(bool bInAnimateVertically);
	bool IsAnimateVertically() const;

	/** Sets whether the pieces animate their opacity. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetAnimateOpacity(bool bInAnimateOpacity);
	bool IsAnimateOpacity() const;

	void SetImage(const FSlateBrush& Brush);
	const FSlateBrush& GetImage() const;

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

private:
	/** Gets the combined value of the animation properties as a single SThrobber::EAnimation value. */
	SThrobber::EAnimation GetAnimation() const;

private:
	/** The Throbber widget managed by this object. */
	TSharedPtr<SThrobber> MyThrobber;
};
