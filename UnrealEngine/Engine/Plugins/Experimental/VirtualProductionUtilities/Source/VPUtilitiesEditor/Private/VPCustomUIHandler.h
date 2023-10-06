// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Templates/SubclassOf.h"
#include "VPCustomUIHandler.generated.h"


class UCameraUIBase;
class FMenuBuilder;

UCLASS(Blueprintable)
class UVPCustomUIHandler : public UObject
{
	GENERATED_BODY()

public:
	void Init();
	void Uninit();
	
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> VirtualProductionWidget;

	/** Menu extension for the VR Editor's 'Windows' menu */
	TSharedPtr<const class FExtensionBase> VRRadialMenuWindowsExtension;

protected:
	void FillVRRadialMenuWindows(FMenuBuilder& MenuBuilder);
	void UpdateUMGUIForVR(TSubclassOf<class UUserWidget> InWidget, FName Name, FVector2D InSize = FVector2D::ZeroVector);
	void UpdateSlateUIForVR(TSharedRef<SWidget> InWidget, FName Name, FVector2D InSize = FVector2D::ZeroVector);
};