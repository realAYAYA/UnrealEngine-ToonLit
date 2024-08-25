// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "CustomizableObjectDGGUI.generated.h"

class UAnimInstance;
class UCustomizableObjectInstanceUsage;
class UObject;
class UWorld;


UCLASS(Abstract, BlueprintType, Blueprintable)
class CUSTOMIZABLEOBJECT_API UDGGUI : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "DGGUI")
	class UCustomizableObjectInstanceUsage* GetCustomizableObjectInstanceUsage();

	UFUNCTION(BlueprintImplementableEvent, Category = "DGGUI")
	void SetCustomizableObjectInstanceUsage(class UCustomizableObjectInstanceUsage* CustomizableObjectInstanceUsage);

	static void OpenDGGUI(const int32 SlotID, UCustomizableObjectInstanceUsage* SelectedCustomizableObjectInstanceUsage, const UWorld* CurrentWorld, const int32 PlayerIndex = 0);
	static bool CloseExistingDGGUI(const UWorld* CurrentWorld);
};
