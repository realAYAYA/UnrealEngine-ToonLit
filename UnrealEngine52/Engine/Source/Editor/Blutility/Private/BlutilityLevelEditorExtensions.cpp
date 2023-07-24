// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlutilityLevelEditorExtensions.h"

#include "ActorActionUtility.h"
#include "AssetRegistry/AssetData.h"
#include "BlutilityMenuExtensions.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "CoreTypes.h"
#include "EditorUtilityAssetPrototype.h"
#include "Delegates/Delegate.h"
#include "EditorUtilityBlueprint.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "LevelEditor.h"
#include "Algo/AnyOf.h"
#include "Logging/MessageLog.h"
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
			   
		TMap<TSharedRef<FAssetActionUtilityPrototype>, TSet<int32>> UtilityAndSelectionIndices;

		// Run thru the actors to determine if any meet our criteria
		TArray<AActor*> SupportedActors;
		if (SelectedActors.Num() > 0)
		{
			FMessageLog EditorErrors("EditorErrors");

			auto ProcessActorAction = [&UtilityAndSelectionIndices, &SelectedActors, &SupportedActors, &EditorErrors](const TSharedRef<FAssetActionUtilityPrototype>& ActionUtilityPrototype)
			{
				if (ActionUtilityPrototype->IsLatestVersion())
				{
					TArray<TSoftClassPtr<UObject>> SupportedClassPtrs = ActionUtilityPrototype->GetSupportedClasses();
					if (SupportedClassPtrs.Num() > 0)
					{
						for (AActor* Actor : SelectedActors)
						{
							if (Actor)
							{
								UClass* ActorClass = Actor->GetClass();
								const bool bPassesClassFilter = 
									Algo::AnyOf(SupportedClassPtrs, [ActorClass](TSoftClassPtr<UObject> ClassPtr){ return ActorClass->IsChildOf(ClassPtr.Get()); });

								if (bPassesClassFilter)
								{
									const int32 Index = SupportedActors.AddUnique(Actor);
									UtilityAndSelectionIndices.FindOrAdd(ActionUtilityPrototype).Add(Index);
								}
							}
						}
					}
				}
				else
                {
					EditorErrors.NewPage(LOCTEXT("ScriptedActions", "Scripted Actions"));
					TSharedRef<FTokenizedMessage> ErrorMessage = EditorErrors.Error();
                    ErrorMessage->AddToken(FAssetNameToken::Create(ActionUtilityPrototype->GetUtilityBlueprintAsset().GetObjectPathString(),FText::FromString(ActionUtilityPrototype->GetUtilityBlueprintAsset().GetObjectPathString())));
                    ErrorMessage->AddToken(FTextToken::Create(LOCTEXT("NeedsToBeUpdated", "needs to be re-saved and possibly upgraded.")));
                }
			};

			// Check blueprint utils (we need to load them to query their validity against these assets)
			TArray<FAssetData> UtilAssets;
			FBlutilityMenuExtensions::GetBlutilityClasses(UtilAssets, UActorActionUtility::StaticClass()->GetClassPathName());

			// Process asset based utilities
            for (const FAssetData& UtilAsset : UtilAssets)
            {
            	if (const UClass* ParentClass = UBlueprint::GetBlueprintParentClassFromAssetTags(UtilAsset))
            	{
            		// We only care about UEditorUtilityBlueprint's that are compiling subclasses of UActorActionUtility
            		if (ParentClass->IsChildOf(UActorActionUtility::StaticClass()))
            		{
            			ProcessActorAction(MakeShared<FAssetActionUtilityPrototype>(UtilAsset));
					}
				}
			}
			
			EditorErrors.Notify(LOCTEXT("SomeProblemsWithActorActionUtility", "There were some problems with some ActorActionUtility Blueprints."));
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
