// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/VCamConnectionStructs.h"
#include "Util/WidgetReference.h"
#include "WidgetConnectionConfig.generated.h"

class UVCamStateSwitcherWidget;
class UVCamWidget;

USTRUCT(BlueprintType)
struct VCAMCORE_API FWidgetConnectionConfig
{
	GENERATED_BODY()
	
	/**
	 * Defines the widget whose connections should be updated. Must be a VCamWidget.
	 * This is the name of a child widget and can be used as argument to UWidgetTree::FindWidget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	FVCamChildWidgetReference Widget;
	
	/**
	 * Key: A valid key of in UVCamWidget::Connections of the widget identified via WidgetProperty.
	 * Value: The settings to use for this connection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	TMap<FName, FVCamConnectionTargetSettings> ConnectionTargets;

	UVCamWidget* ResolveWidget(UVCamStateSwitcherWidget* OwnerWidget) const;
	bool HasNoWidgetSet() const;
};