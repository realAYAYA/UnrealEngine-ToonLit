// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/VCamWidget.h"
#include "WidgetReference.generated.h"

/**
 * A reference to any child widget within an UMG Blueprint; only widgets in the same widget tree can be referenced.
 * Has a detail customization for choosing widget.
 */
USTRUCT(BlueprintType)
struct VCAMCORE_API FChildWidgetReference
{
	GENERATED_BODY()

	/**
	 * Pointer to template widget within the Blueprint's widget tree.
	 * 
	 * Important: in UMG there are templates and previews.
	 * Templates are like the CDO and previews are what you see in the editor
	 * @see FWidgetReference
	 */
	UPROPERTY(EditAnywhere, Category = "Virtual Camera")
	TSoftObjectPtr<UWidget> Template;
	
	UWidget* ResolveWidget(UUserWidget& OwnerWidget) const;
	bool HasNoWidgetSet() const { return Template.IsValid(); }
};

/** Version that restraints Widget to UVCamWidget instances. */
USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamChildWidgetReference : public FChildWidgetReference
{
	GENERATED_BODY()
	
	UVCamWidget* ResolveVCamWidget(UUserWidget& OwnerWidget) const { return Cast<UVCamWidget>(ResolveWidget(OwnerWidget)); }
};

UCLASS()
class UWidgetReferenceBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "Virtual Camera")
	static UWidget* ResolveWidget(const FChildWidgetReference& WidgetReference, UUserWidget* OwnerWidget)
	{
		return OwnerWidget ? WidgetReference.ResolveWidget(*OwnerWidget) : nullptr;
	}
	
	UFUNCTION(BlueprintPure, Category = "Virtual Camera")
	static UVCamWidget* ResolveVCamWidget(const FVCamChildWidgetReference& WidgetReference, UUserWidget* OwnerWidget)
	{
		return OwnerWidget ? WidgetReference.ResolveVCamWidget(*OwnerWidget) : nullptr;
	}
};