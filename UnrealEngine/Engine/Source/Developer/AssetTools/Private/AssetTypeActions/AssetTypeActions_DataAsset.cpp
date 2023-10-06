// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_DataAsset.h"
#include "Engine/Engine.h"
#include "ToolMenuSection.h"
#include "ClassViewerModule.h"
#include "Kismet2/SClassPickerDialog.h"
#include "ClassViewerFilter.h"
#include "ObjectTools.h"
#include "SDetailsDiff.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

static const FName NAME_NativeClass(TEXT("NativeClass"));

class FNewNodeClassFilter : public IClassViewerFilter
{
public:
	FNewNodeClassFilter(UClass* InBaseClass)
		: BaseClass(InBaseClass)
	{
	}

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (InClass != nullptr)
		{
			return InClass->IsChildOf(BaseClass);
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(BaseClass);
	}

private:
	UClass* BaseClass;
};

FText FAssetTypeActions_DataAsset::GetDisplayNameFromAssetData(const FAssetData& AssetData) const
{
	if (AssetData.IsValid())
	{
		const FAssetDataTagMapSharedView::FFindTagResult NativeClassTag = AssetData.TagsAndValues.FindTag(NAME_NativeClass);
		if (NativeClassTag.IsSet())
		{
			if (UClass* FoundClass = FindObjectSafe<UClass>(nullptr, *NativeClassTag.GetValue()))
			{
				return FText::Format(LOCTEXT("DataAssetWithType", "Data Asset ({0})"), FoundClass->GetDisplayNameText());
			}
		}
	}

	return FText::GetEmpty();
}

void FAssetTypeActions_DataAsset::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UDataAsset>> DataAssets = GetTypedWeakObjectPtrs<UDataAsset>(InObjects);

	Section.AddMenuEntry(
		"DataAsset_ChangeClass",
		LOCTEXT("DataAsset_ChangeClass", "Convert to Different DataAsset Type"),
		LOCTEXT("DataAsset_ChangeClassTip", "Change the class these Data Assets are subclassed from."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_DataAsset::ExecuteChangeDataAssetClass, DataAssets)
		)
	);
}

void FAssetTypeActions_DataAsset::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	const TSharedRef<SDetailsDiff> DetailsDiff = SDetailsDiff::CreateDiffWindow(OldAsset, NewAsset, OldRevision, NewRevision, GetSupportedClass());
	// allow users to edit NewAsset if it's a local asset
	if (!FPackageName::IsTempPackage(NewAsset->GetPackage()->GetName()))
	{
		DetailsDiff->SetOutputObject(NewAsset);
	}
}

void FAssetTypeActions_DataAsset::ExecuteChangeDataAssetClass(TArray<TWeakObjectPtr<UDataAsset>> InDataAssets)
{
	const FText TitleText = LOCTEXT("DataAsset_PickNewDataAssetClass", "Pick New DataAsset Class");
	FClassViewerInitializationOptions Options;
	Options.ClassFilters.Add(MakeShared<FNewNodeClassFilter>(UDataAsset::StaticClass()));
	UClass* OutNewDataAssetClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, OutNewDataAssetClass, UDataAsset::StaticClass());

	if (bPressedOk && OutNewDataAssetClass != nullptr)
	{
		for (TWeakObjectPtr<UDataAsset> DataAssetPtr : InDataAssets)
		{
			if (UDataAsset* OldDataAsset = DataAssetPtr.Get())
			{
				if (OldDataAsset && OldDataAsset->IsValidLowLevel())
				{
					FName ObjectName = OldDataAsset->GetFName();
					UObject* Outer = OldDataAsset->GetOuter();
					OldDataAsset->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);

					UObject* NewDataAsset = NewObject<UObject>(Outer, OutNewDataAssetClass, ObjectName, OldDataAsset->GetFlags());

					// Migrate Data
					{
						UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyOptions;
						CopyOptions.bNotifyObjectReplacement = true;
						UEngine::CopyPropertiesForUnrelatedObjects(OldDataAsset, NewDataAsset, CopyOptions);
					}

					NewDataAsset->MarkPackageDirty();

					// Consolidate or "Replace" the old object with the new object for any living references.
					bool bShowDeleteConfirmation = false;
					TArray<UObject*> OldDataAssetArray({ (UObject*)OldDataAsset });
					ObjectTools::ConsolidateObjects(NewDataAsset, OldDataAssetArray, bShowDeleteConfirmation);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
