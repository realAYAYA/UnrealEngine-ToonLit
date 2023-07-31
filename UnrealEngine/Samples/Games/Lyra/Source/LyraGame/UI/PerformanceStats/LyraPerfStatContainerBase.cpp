// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerfStatContainerBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Widget.h"
#include "Delegates/Delegate.h"
#include "LyraPerfStatWidgetBase.h"
#include "Misc/AssertionMacros.h"
#include "Settings/LyraSettingsLocal.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerfStatContainerBase)

//////////////////////////////////////////////////////////////////////
// ULyraPerfStatsContainerBase

ULyraPerfStatContainerBase::ULyraPerfStatContainerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULyraPerfStatContainerBase::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateVisibilityOfChildren();

	ULyraSettingsLocal::Get()->OnPerfStatDisplayStateChanged().AddUObject(this, &ThisClass::UpdateVisibilityOfChildren);
}

void ULyraPerfStatContainerBase::NativeDestruct()
{
	ULyraSettingsLocal::Get()->OnPerfStatDisplayStateChanged().RemoveAll(this);

	Super::NativeDestruct();
}

void ULyraPerfStatContainerBase::UpdateVisibilityOfChildren()
{
	ULyraSettingsLocal* UserSettings = ULyraSettingsLocal::Get();

	const bool bShowTextWidgets = (StatDisplayModeFilter == ELyraStatDisplayMode::TextOnly) || (StatDisplayModeFilter == ELyraStatDisplayMode::TextAndGraph);
	const bool bShowGraphWidgets = (StatDisplayModeFilter == ELyraStatDisplayMode::GraphOnly) || (StatDisplayModeFilter == ELyraStatDisplayMode::TextAndGraph);
	
	check(WidgetTree);
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (ULyraPerfStatWidgetBase* TypedWidget = Cast<ULyraPerfStatWidgetBase>(Widget))
		{
			const ELyraStatDisplayMode SettingMode = UserSettings->GetPerfStatDisplayState(TypedWidget->GetStatToDisplay());

			bool bShowWidget = false;
			switch (SettingMode)
			{
			case ELyraStatDisplayMode::Hidden:
				bShowWidget = false;
				break;
			case ELyraStatDisplayMode::TextOnly:
				bShowWidget = bShowTextWidgets;
				break;
			case ELyraStatDisplayMode::GraphOnly:
				bShowWidget = bShowGraphWidgets;
				break;
			case ELyraStatDisplayMode::TextAndGraph:
				bShowWidget = bShowTextWidgets || bShowGraphWidgets;
				break;
			}

			TypedWidget->SetVisibility(bShowWidget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	});
}

