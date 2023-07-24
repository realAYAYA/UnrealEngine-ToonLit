// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectDGGUI.h"

#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectDGGUI)

void UDGGUI::OpenDGGUI(const int32 SlotID, UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent, const UWorld* CurrentWorld, const int32 PlayerIndex)
{
#if WITH_EDITOR
	if (APlayerController* Player = UGameplayStatics::GetPlayerController(CurrentWorld, PlayerIndex))
	{
		FSoftClassPath DGUIPath(TEXT("/Mutable/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI.DynamicallyGeneratedGUI_DGGUI_C"));
		if (UClass* DGUI = DGUIPath.TryLoadClass<UDGGUI>())
		{
			UDGGUI* WDGUI = CreateWidget<UDGGUI>(Player, DGUI);
			if (WDGUI)
			{
				WDGUI->SetCustomizableSkeletalComponent(SelectedCustomizableSkeletalComponent);
				WDGUI->AddToViewport();
				Player->SetShowMouseCursor(true);
			}
		}
	}
#endif // WITH_EDITOR
}

bool UDGGUI::CloseExistingDGGUI(const UWorld* CurrentWorld)
{
#if WITH_EDITOR
	bool bClosing = false;
	for (TObjectIterator<UDGGUI> PreviousGUI; PreviousGUI; ++PreviousGUI)
	{
		if (PreviousGUI->IsValidLowLevel())
		{
			if (UCustomizableSkeletalComponent* CustomizableComponent = PreviousGUI->GetCustomizableSkeletalComponent())
			{
				PreviousGUI->SetCustomizableSkeletalComponent(nullptr);
				bClosing = true;
			}
			PreviousGUI->RemoveFromParent();
		}
	}
	if (bClosing)
	{
		if (APlayerController* Player = UGameplayStatics::GetPlayerController(CurrentWorld, 0))
		{
			Player->SetShowMouseCursor(false);
		}
		return true;
	}
#endif // WITH_EDITOR
	return false;
}
