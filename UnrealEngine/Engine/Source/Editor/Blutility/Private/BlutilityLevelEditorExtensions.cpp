// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlutilityLevelEditorExtensions.h"

#include "ActorActionUtility.h"
#include "AssetRegistry/AssetData.h"
#include "BlutilityMenuExtensions.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "EditorUtilityBlueprint.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectIterator.h"

class FUICommandList;

#define LOCTEXT_NAMESPACE "BlutilityLevelEditorExtensions"

FDelegateHandle LevelViewportExtenderHandle;

class FBlutilityLevelEditorExtensions_Impl
{
public:
	static TSharedRef<FExtender> OnExtendLevelEditorActorContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
	{
		TSharedRef<FExtender> Extender(new FExtender());
			   
		TMap<IEditorUtilityExtension*, TSet<int32>> UtilityAndSelectionIndices;

		// Run thru the actors to determine if any meet our criteria
		TArray<AActor*> SupportedActors;
		if (SelectedActors.Num() > 0)
		{
			// Check blueprint utils (we need to load them to query their validity against these assets)
			TArray<FAssetData> UtilAssets;
			FBlutilityMenuExtensions::GetBlutilityClasses(UtilAssets, UActorActionUtility::StaticClass()->GetClassPathName());

			TMap<UActorActionUtility*, UClass*> ActorActionUtilities;

			// Process asset based utilities
			for (const FAssetData& UtilAsset : UtilAssets)
			{
				if (UEditorUtilityBlueprint* Blueprint = Cast<UEditorUtilityBlueprint>(UtilAsset.GetAsset()))
				{
					if (UClass* BPClass = Blueprint->GeneratedClass.Get())
					{
						if (UActorActionUtility* ActorActionUtility = Cast<UActorActionUtility>(BPClass->GetDefaultObject()))
						{
							UClass*& SupportedClass = ActorActionUtilities.FindOrAdd(ActorActionUtility);
							if (!SupportedClass)
							{
								SupportedClass = ActorActionUtility->GetSupportedClass();
							}
						}
					}
				}
			}

			// Process non-asset based utilities
			for (TObjectIterator<UClass> AssetActionClassIt; AssetActionClassIt; ++AssetActionClassIt)
			{
				if (AssetActionClassIt->IsChildOf(UActorActionUtility::StaticClass()) && UActorActionUtility::StaticClass()->GetFName() != AssetActionClassIt->GetFName() && AssetActionClassIt->ClassGeneratedBy == nullptr)
				{
					if (UActorActionUtility* ActorActionUtility = Cast<UActorActionUtility>(AssetActionClassIt->GetDefaultObject()))
					{
						UClass*& SupportedClass = ActorActionUtilities.FindOrAdd(ActorActionUtility);
						if (!SupportedClass)
						{
							SupportedClass = ActorActionUtility->GetSupportedClass();
						}
					}
				}
			}

			if (UtilAssets.Num() + ActorActionUtilities.Num() > 0)
			{
				for (AActor* Actor : SelectedActors)
				{
					if (Actor)
					{
						int32 SupportedActorIndex = INDEX_NONE;
						for (const TPair<UActorActionUtility*, UClass*>& ActorActionUtilityIt : ActorActionUtilities)
						{
							UClass* SupportedClass = ActorActionUtilityIt.Value;
							if (SupportedClass == nullptr || (SupportedClass && Actor->GetClass()->IsChildOf(SupportedClass)))
							{
								UActorActionUtility* Action = ActorActionUtilityIt.Key;
								if (SupportedActorIndex == INDEX_NONE)
								{
									SupportedActorIndex = SupportedActors.Add(Actor);
								}
								UtilityAndSelectionIndices.FindOrAdd(Action).Add(SupportedActorIndex);
							}
						}
					}
				}
			}
		}

		if (SupportedActors.Num() > 0)
		{
			// Add asset actions extender
			Extender->AddMenuExtension(
				"ActorOptions",
				EExtensionHook::After,
				CommandList,
				FMenuExtensionDelegate::CreateStatic(&FBlutilityMenuExtensions::CreateActorBlutilityActionsMenu, UtilityAndSelectionIndices, SupportedActors));
		}

		return Extender;
	}
};

void FBlutilityLevelEditorExtensions::InstallHooks()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&FBlutilityLevelEditorExtensions_Impl::OnExtendLevelEditorActorContextMenu));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FBlutilityLevelEditorExtensions::RemoveHooks()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

#undef LOCTEXT_NAMESPACE
