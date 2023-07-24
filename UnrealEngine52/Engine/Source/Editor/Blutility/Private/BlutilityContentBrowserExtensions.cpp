// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlutilityContentBrowserExtensions.h"

#include "AssetActionUtility.h"
#include "AssetRegistry/AssetData.h"
#include "BlutilityMenuExtensions.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "EditorUtilityAssetPrototype.h"
#include "Delegates/Delegate.h"
#include "EditorUtilityBlueprint.h"
#include "Algo/AnyOf.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Modules/ModuleManager.h"
#include "Templates/Casts.h"
#include "Templates/ChooseClass.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectIterator.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "BlutilityContentBrowserExtensions"

static FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
static FDelegateHandle ContentBrowserExtenderDelegateHandle;

class FBlutilityContentBrowserExtensions_Impl
{
public:
	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run thru the assets to determine if any meet our criteria
		TMap<TSharedRef<FAssetActionUtilityPrototype>, TSet<int32>> UtilityAndSelectionIndices;
		TArray<FAssetData> SupportedAssets;
		
		if (SelectedAssets.Num() > 0)
		{
			FMessageLog EditorErrors("EditorErrors");
			
			auto ProcessAssetAction = [&EditorErrors, &SupportedAssets, &UtilityAndSelectionIndices, &SelectedAssets](const TSharedRef<FAssetActionUtilityPrototype>& ActionUtilityPrototype)
			{
				if (ActionUtilityPrototype->IsLatestVersion())
				{
					TArray<TSoftClassPtr<UObject>> SupportedClassPtrs = ActionUtilityPrototype->GetSupportedClasses();
					if (SupportedClassPtrs.Num() > 0)
					{
						const bool bIsActionForBlueprints = ActionUtilityPrototype->AreSupportedClassesForBlueprints();

						for (const FAssetData& Asset : SelectedAssets)
						{
							bool bPassesClassFilter = false;
							if (bIsActionForBlueprints)
							{
								if (TSubclassOf<UBlueprint> AssetClass = Asset.GetClass())
								{
									if (const UClass* Blueprint_ParentClass = UBlueprint::GetBlueprintParentClassFromAssetTags(Asset))
	                                {
										bPassesClassFilter = 
											Algo::AnyOf(SupportedClassPtrs, [Blueprint_ParentClass](TSoftClassPtr<UObject> ClassPtr){ return Blueprint_ParentClass->IsChildOf(ClassPtr.Get()); });
	                                }
								}
							}
							else
							{
								// Is the asset the right kind?
								bPassesClassFilter = 
									Algo::AnyOf(SupportedClassPtrs, [&Asset](TSoftClassPtr<UObject> ClassPtr){ return Asset.IsInstanceOf(ClassPtr.Get(), EResolveClass::Yes); });
							}

							if (bPassesClassFilter)
							{
								const int32 Index = SupportedAssets.AddUnique(Asset);
								UtilityAndSelectionIndices.FindOrAdd(ActionUtilityPrototype).Add(Index);
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
			FBlutilityMenuExtensions::GetBlutilityClasses(UtilAssets, UAssetActionUtility::StaticClass()->GetClassPathName());

			// Process asset based utilities
			for (const FAssetData& UtilAsset : UtilAssets)
			{
				if (const UClass* ParentClass = UBlueprint::GetBlueprintParentClassFromAssetTags(UtilAsset))
				{
					// We only care about UEditorUtilityBlueprint's that are compiling subclasses of UAssetActionUtility
					if (ParentClass->IsChildOf(UAssetActionUtility::StaticClass()))
					{
						ProcessAssetAction(MakeShared<FAssetActionUtilityPrototype>(UtilAsset));
					}
				}
			}

			EditorErrors.Notify(LOCTEXT("SomeProblemsWithAssetActionUtility", "There were some problems with some AssetActionUtility Blueprints."));
		}

		if (UtilityAndSelectionIndices.Num() > 0)
		{
			// Add asset actions extender
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FBlutilityMenuExtensions::CreateAssetBlutilityActionsMenu, MoveTemp(UtilityAndSelectionIndices), MoveTemp(SupportedAssets)));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	}
};

void FBlutilityContentBrowserExtensions::InstallHooks()
{
	ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBlutilityContentBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FBlutilityContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FBlutilityContentBrowserExtensions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FBlutilityContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate){ return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

#undef LOCTEXT_NAMESPACE
