// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GameplayTagAssetBase.h"
#include "Framework/Application/SlateApplication.h"

#include "GameplayTagContainer.h"
#include "SGameplayTagPicker.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_GameplayTagAssetBase::FAssetTypeActions_GameplayTagAssetBase(FName InTagPropertyName)
	: OwnedGameplayTagPropertyName(InTagPropertyName)
{}

void FAssetTypeActions_GameplayTagAssetBase::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<UObject*> Objects;
	TArray<FGameplayTagContainer> Containers;
	
	for (int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx)
	{
		if (UObject* CurObj = InObjects[ObjIdx])
		{
			const FStructProperty* Property = FindFProperty<FStructProperty>(CurObj->GetClass(), OwnedGameplayTagPropertyName);
			if(Property != nullptr)
			{
				const FGameplayTagContainer& Container = *Property->ContainerPtrToValuePtr<FGameplayTagContainer>(CurObj); 
				Objects.Add(CurObj);
				Containers.Add(Container);
			}
		}
	}

	if (Containers.Num() > 0)
	{
		Section.AddMenuEntry(
			"GameplayTags_Edit",
			LOCTEXT("GameplayTags_Edit", "Edit Gameplay Tags..."),
			LOCTEXT("GameplayTags_EditToolTip", "Opens the Gameplay Tag Editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_GameplayTagAssetBase::OpenGameplayTagEditor, Objects, Containers), FCanExecuteAction()));
	}
}

void FAssetTypeActions_GameplayTagAssetBase::OpenGameplayTagEditor(TArray<UObject*> Objects, TArray<FGameplayTagContainer> Containers) const
{
	if (!Objects.Num() || !Containers.Num())
	{
		return;
	}

	check(Objects.Num() == Containers.Num());

	for (UObject* Object : Objects)
	{
		check (IsValid(Object));
		Object->SetFlags(RF_Transactional);
	}

	FText Title;
	FText AssetName;

	const int32 NumAssets = Containers.Num();
	if (NumAssets > 1)
	{
		AssetName = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseMultipleAssets", "{0} Assets"), FText::AsNumber( NumAssets ) );
		Title = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseEditorTitle", "Tag Editor: Owned Gameplay Tags: {0}"), AssetName );
	}
	else
	{
		AssetName = FText::FromString(GetNameSafe(Objects[0]));
		Title = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseEditorTitle", "Tag Editor: Owned Gameplay Tags: {0}"), AssetName );
	}

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(500, 600))
		[
			SNew(SGameplayTagPicker)
			.TagContainers(Containers)
			.MaxHeight(0) // unbounded
			.OnRefreshTagContainers_Lambda([Objects, OwnedGameplayTagPropertyName = OwnedGameplayTagPropertyName](SGameplayTagPicker& TagPicker)
			{
				// Refresh tags from objects, this is called e.g. on post undo/redo. 
				TArray<FGameplayTagContainer> Containers;
				for (UObject* Object : Objects)
				{
					// Adding extra entry even if the object has gone invalid to keep the container count the same as object count.
					FGameplayTagContainer& NewContainer = Containers.AddDefaulted_GetRef();
					if (IsValid(Object))
					{
						const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), OwnedGameplayTagPropertyName);
						if (Property != nullptr)
						{
							NewContainer = *Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Object); 
						}
					}
				}
				TagPicker.SetTagContainers(Containers);
			})
			.OnTagChanged_Lambda([Objects, OwnedGameplayTagPropertyName = OwnedGameplayTagPropertyName](const TArray<FGameplayTagContainer>& TagContainers)
			{
				// Sanity check that our arrays are in sync.
				if (Objects.Num() != TagContainers.Num())
				{
					return;
				}
				
				for (int32 Index = 0; Index < Objects.Num(); Index++)
				{
					UObject* Object = Objects[Index];
					if (!IsValid(Object))
					{
						continue;
					}

					FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), OwnedGameplayTagPropertyName);
					if (!Property)
					{
						continue;
					}

					Object->Modify();
					FGameplayTagContainer& Container = *Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Object); 

					FEditPropertyChain PropertyChain;
					PropertyChain.AddHead(Property);
					Object->PreEditChange(PropertyChain);

					Container = TagContainers[Index];
					
					FPropertyChangedEvent PropertyEvent(Property);
					Object->PostEditChangeProperty(PropertyEvent);
				}
			})
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}

uint32 FAssetTypeActions_GameplayTagAssetBase::GetCategories()
{ 
	return EAssetTypeCategories::Misc; 
}

#undef LOCTEXT_NAMESPACE
