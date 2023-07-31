// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "HAL/Platform.h"
#include "Templates/SubclassOf.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectDGGUI.generated.h"

class UAnimInstance;
class UCustomizableSkeletalComponent;
class UObject;
class UWorld;


UCLASS(Abstract, BlueprintType, Blueprintable)
class CUSTOMIZABLEOBJECT_API UDGGUI : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "DGGUI")
	class UCustomizableSkeletalComponent* GetCustomizableSkeletalComponent();

	UFUNCTION(BlueprintImplementableEvent, Category = "DGGUI")
	void SetCustomizableSkeletalComponent(class UCustomizableSkeletalComponent* CustomizableSkeletalComponent);

	static void OpenDGGUI(const int32 SlotID, UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent, const UWorld* CurrentWorld, const int32 PlayerIndex = 0);
	static bool CloseExistingDGGUI(const UWorld* CurrentWorld);
private:
	void CustomizableSkeletalMeshPreUpdate(class UCustomizableSkeletalComponent* Component, class USkeletalMesh* NextMesh);
	void CustomizableSkeletalMeshUpdated();
	TSubclassOf<UAnimInstance> LastAnimationClass;
};
