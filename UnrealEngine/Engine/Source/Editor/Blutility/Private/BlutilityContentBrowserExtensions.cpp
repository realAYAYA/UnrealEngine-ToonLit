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
#include "Delegates/Delegate.h"
#include "EditorUtilityBlueprint.h"
#include "Engine/Blueprint.h"
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
		TMap<IEditorUtilityExtension*, TSet<int32>> UtilityAndSelectionIndices;
		TArray<IEditorUtilityExtension*> SupportedUtils;
		TArray<FAssetData> SupportedAssets;
		if (SelectedAssets.Num() > 0)
		{
			// Check blueprint utils (we need to load them to query their validity against these assets)
			TArray<FAssetData> UtilAssets;
			FBlutilityMenuExtensions::GetBlutilityClasses(UtilAssets, UAssetActionUtility::StaticClass()->GetClassPathName());

			// Collect all UAssetActionUtility derived classes
			TSet<UAssetActionUtility*> AssetClasses;
			for (TObjectIterator<UClass> AssetActionClassIt; AssetActionClassIt; ++AssetActionClassIt)
			{
				if (AssetActionClassIt->IsChildOf(UAssetActionUtility::StaticClass()) && UAssetActionUtility::StaticClass()->GetFName() != AssetActionClassIt->GetFName() && AssetActionClassIt->ClassGeneratedBy == nullptr)
				{
					AssetClasses.Add(Cast<UAssetActionUtility>(AssetActionClassIt->GetDefaultObject()));
				}
			}

			auto ProcessAssetAction = [&SupportedAssets, &UtilityAndSelectionIndices, &SelectedAssets](UAssetActionUtility* InAction)
			{
				if (UClass* SupportedClass = InAction->GetSupportedClass())
				{
					const bool bIsActionForBlueprints = InAction->IsActionForBlueprints();

					for (const FAssetData& Asset : SelectedAssets)
					{
						bool bPassesClassFilter = false;
						if (bIsActionForBlueprints)
						{
							if (TSubclassOf<UBlueprint> AssetClass = Asset.GetClass())
							{
								if (UBlueprint* AssetAsBlueprint = Cast<UBlueprint>(Asset.GetAsset()))
								{
									// It's a blueprint, but is it the right kind?
									bPassesClassFilter = AssetAsBlueprint->ParentClass && AssetAsBlueprint->ParentClass->IsChildOf(SupportedClass);
								}
								else
								{
									// Not a blueprint
									bPassesClassFilter = false;
								}
							}
							else
							{
								// Not a blueprint
								bPassesClassFilter = false;
							}
						}
						else
						{
							// Is the asset the right kind?
							bPassesClassFilter = Asset.IsInstanceOf(SupportedClass);
						}

						if (bPassesClassFilter)
						{
							const int32 Index = SupportedAssets.AddUnique(Asset);
							UtilityAndSelectionIndices.FindOrAdd(InAction).Add(Index);
						}
					}
				}
				else
				{
					TSet<int32>& ActionIndices = UtilityAndSelectionIndices.FindOrAdd(InAction);
					for (const FAssetData& Asset : SelectedAssets)
					{ 
						const int32 Index = SupportedAssets.AddUnique(Asset);
						ActionIndices.Add(Index);
					}
				}
			};

			// Process asset based utilities
			for (const FAssetData& UtilAsset : UtilAssets)
			{
				FString ParentClassName;
				if (UtilAsset.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
				{
					UObject* Outer = nullptr;
					ResolveName(Outer, ParentClassName, false, false);
					const UClass* ParentClass = FindObject<UClass>(Outer, *ParentClassName);

					// We only care about UEditorUtilityBlueprint's that are compiling subclasses of UAssetActionUtility
					if (ParentClass && ParentClass->IsChildOf(UAssetActionUtility::StaticClass()))
					{
						if (const UEditorUtilityBlueprint* Blueprint = Cast<UEditorUtilityBlueprint>(UtilAsset.GetAsset()))
						{
							if (const UClass* BPClass = Blueprint->GeneratedClass.Get())
							{
								if (UAssetActionUtility* DefaultObject = Cast<UAssetActionUtility>(BPClass->GetDefaultObject()))
								{
									ProcessAssetAction(DefaultObject);
								}
							}
						}
					}
				}
			}

			// Process non-asset based utilities
			for (UAssetActionUtility* Action : AssetClasses)
			{
				ProcessAssetAction(Action);
			}
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
