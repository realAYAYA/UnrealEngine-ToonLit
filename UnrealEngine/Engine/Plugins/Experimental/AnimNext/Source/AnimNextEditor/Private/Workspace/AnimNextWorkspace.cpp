// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspace.h"

#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextParameterBlock.h"
#include "Scheduler/AnimNextSchedule.h"
#include "UObject/AssetRegistryTagsContext.h"

const FName UAnimNextWorkspace::ExportsAssetRegistryTag = TEXT("Exports");

bool UAnimNextWorkspace::AddAsset(const FAssetData& InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsAssetSupported(InAsset))
	{
		ReportError(TEXT("UAnimNextWorkspace::AddAsset: Unsupported asset supplied."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	const int32 NewIndex = Assets.AddUnique(TSoftObjectPtr<UObject>(InAsset.GetSoftObjectPath()));
	if(NewIndex != INDEX_NONE)
	{
		BroadcastModified();
	}

	return NewIndex != INDEX_NONE;
}

bool UAnimNextWorkspace::AddAsset(UObject* InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAsset == nullptr)
	{
		ReportError(TEXT("UAnimNextWorkspace::AddAsset: Invalid asset supplied."));
		return false;
	}

	return AddAsset(FAssetData(InAsset), bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextWorkspace::AddAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UAnimNextWorkspace::AddAssets: No assets supplied."));
		return false;
	}

	bool bAdded = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(const FAssetData& Asset : InAssets)
		{
			bAdded |= AddAsset(Asset, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bAdded)
	{
		BroadcastModified();
	}

	return bAdded;
}

bool UAnimNextWorkspace::AddAssets(const TArray<UObject*>& InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UAnimNextWorkspace::AddAssets: No assets supplied."));
		return false;
	}

	bool bAdded = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(UObject* Asset : InAssets)
		{
			bAdded |= AddAsset(FAssetData(Asset), bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bAdded)
	{
		BroadcastModified();
	}

	return bAdded;
}

bool UAnimNextWorkspace::RemoveAsset(const FAssetData& InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsAssetSupported(InAsset))
	{
		ReportError(TEXT("UAnimNextWorkspace::RemoveAsset: Unsupported asset supplied."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	const int32 NumRemoved = Assets.Remove(TSoftObjectPtr<UObject>(InAsset.GetSoftObjectPath()));
	if(NumRemoved > 0)
	{
		BroadcastModified();
	}

	return NumRemoved > 0;
}

bool UAnimNextWorkspace::RemoveAsset(UObject* InAsset, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAsset == nullptr)
	{
		ReportError(TEXT("UAnimNextWorkspace::RemoveAsset: Invalid asset supplied."));
		return false;
	}

	return RemoveAsset(FAssetData(InAsset), bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextWorkspace::RemoveAssets(TArray<UObject*> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UAnimNextWorkspace::RemoveAssets: No assets supplied."));
		return false;
	}

	bool bRemoved = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(UObject* Asset : InAssets)
		{
			bRemoved |= RemoveAsset(FAssetData(Asset), bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bRemoved)
	{
		BroadcastModified();
	}

	return bRemoved;
}

bool UAnimNextWorkspace::RemoveAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InAssets.Num() == 0)
	{
		ReportError(TEXT("UAnimNextWorkspace::RemoveAssets: No assets supplied."));
		return false;
	}

	bool bRemoved = false;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(const FAssetData& Asset : InAssets)
		{
			bRemoved |= RemoveAsset(Asset, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	if(bRemoved)
	{
		BroadcastModified();
	}

	return bRemoved;
}

const TArray<FTopLevelAssetPath>& UAnimNextWorkspace::GetSupportedAssetClassPaths()
{
	static TArray<FTopLevelAssetPath> SupportedAssets = {
		UAnimNextParameterBlock::StaticClass()->GetClassPathName(),
		UAnimNextGraph::StaticClass()->GetClassPathName(),
		UAnimNextSchedule::StaticClass()->GetClassPathName(),
	};

	return SupportedAssets;
}

bool UAnimNextWorkspace::IsAssetSupported(const FAssetData& InAsset)
{
	const TArray<FTopLevelAssetPath>& SupportedAssets = GetSupportedAssetClassPaths();
	return SupportedAssets.Contains(InAsset.AssetClassPath);
}

void UAnimNextWorkspace::BroadcastModified()
{
	if(!bSuspendNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UAnimNextWorkspace::ReportError(const TCHAR* InMessage) const
{
#if WITH_EDITOR
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
#endif
}

void UAnimNextWorkspace::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	BroadcastModified();
}

void UAnimNextWorkspace::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UAnimNextWorkspace::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FAnimNextWorkspaceAssetRegistryExports Exports;
	Exports.Assets.Reserve(Assets.Num());

	for(const TSoftObjectPtr<UObject>& Asset : Assets)
	{
		Exports.Assets.Emplace(Asset.GetUniqueID());
	}

	FString TagValue;
	FAnimNextWorkspaceAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);

	Context.AddTag(FAssetRegistryTag(ExportsAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}
