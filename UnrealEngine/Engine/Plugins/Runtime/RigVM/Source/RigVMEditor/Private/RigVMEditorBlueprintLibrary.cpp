// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMEditorBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ScopedSlowTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEditorBlueprintLibrary)

#define LOCTEXT_NAMESPACE "RigVMEditorBlueprintLibrary"

void URigVMEditorBlueprintLibrary::RecompileVM(URigVMBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return;
	}
	InBlueprint->RecompileVM();
}

void URigVMEditorBlueprintLibrary::RecompileVMIfRequired(URigVMBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return;
	}
	InBlueprint->RecompileVMIfRequired();
}

void URigVMEditorBlueprintLibrary::RequestAutoVMRecompilation(URigVMBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return;
	}
	InBlueprint->RequestAutoVMRecompilation();
}

URigVMGraph* URigVMEditorBlueprintLibrary::GetModel(URigVMBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return nullptr;
	}
	return InBlueprint->GetDefaultModel();
}

URigVMController* URigVMEditorBlueprintLibrary::GetController(URigVMBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return nullptr;
	}
	return InBlueprint->GetController();
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssets()
{
	return LoadAssetsByClass(URigVMBlueprint::StaticClass());
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsByClass(TSubclassOf<URigVMBlueprint> InClass)
{
	return LoadAssetsWithAssetDataAndBlueprintFilters(InClass, FRigVMAssetDataFilter(), FRigVMBlueprintFilter());
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithBlueprintFilter_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMBlueprintFilterDynamic InBlueprintFilter)
{
	return LoadAssetsWithAssetDataAndBlueprintFilters(InClass, FRigVMAssetDataFilter(), FRigVMBlueprintFilter::CreateLambda(
		[InBlueprintFilter](const URigVMBlueprint* Blueprint, const TArray<FRigVMBlueprintLoadLogEntry>& LogEntries) -> bool
		{
			return InBlueprintFilter.Execute(Blueprint, LogEntries);
		})
	);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithAssetDataFilter_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMAssetDataFilterDynamic InAssetDataFilter)
{
	return LoadAssetsWithAssetDataAndBlueprintFilters(InClass, FRigVMAssetDataFilter::CreateLambda(
		[InAssetDataFilter](const FAssetData& AssetData) -> bool
		{
			return InAssetDataFilter.Execute(AssetData);
		}),
		FRigVMBlueprintFilter()
	);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithNodeFilter_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMNodeFilterDynamic InNodeFilter)
{
	return LoadAssetsWithNodeFilter(InClass, FRigVMNodeFilter::CreateLambda(
		[InNodeFilter](const URigVMBlueprint* Blueprint, const URigVMNode* Node) -> bool
		{
			return InNodeFilter.Execute(Blueprint, Node);
		})
	);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithNodeFilter(
	TSubclassOf<URigVMBlueprint> InClass,
	FRigVMNodeFilter InNodeFilter)
{
	return LoadAssetsWithAssetDataAndNodeFilters(InClass, FRigVMAssetDataFilter(), InNodeFilter);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithAssetDataAndBlueprintFilters_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass,
	FRigVMAssetDataFilterDynamic InAssetDataFilter,
	FRigVMBlueprintFilterDynamic InBlueprintFilter)
{
	return LoadAssetsWithAssetDataAndBlueprintFilters(InClass,
		FRigVMAssetDataFilter::CreateLambda(
		[InAssetDataFilter](const FAssetData& AssetData) -> bool
			{
				return InAssetDataFilter.Execute(AssetData);
			}
		),
		FRigVMBlueprintFilter::CreateLambda(
		[InBlueprintFilter](const URigVMBlueprint* Blueprint, const TArray<FRigVMBlueprintLoadLogEntry>& LogEntries) -> bool
			{
				return InBlueprintFilter.Execute(Blueprint, LogEntries);
			}
		)
	);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithAssetDataAndBlueprintFilters(
	TSubclassOf<URigVMBlueprint> InClass, 
	FRigVMAssetDataFilter InAssetDataFilter, 
	FRigVMBlueprintFilter InBlueprintFilter
)
{
	TArray<FAssetData> AssetDataList = GetAssetsWithFilter(InClass, InAssetDataFilter);

	const int32 NumAssets = AssetDataList.Num();

	TArray<URigVMBlueprint*> LoadedAssets;
	{
		const FString Title = FString::Printf(TEXT("Load all %s assets..."), *InClass->GetName()); 
		FScopedSlowTask LoadAssetsTask(NumAssets, FText::FromString(Title));
		LoadAssetsTask.MakeDialog(true);

		for(int32 Index = 0; Index < NumAssets; Index++)
		{
			if (LoadAssetsTask.ShouldCancel())
			{
				break;
			}

			const FAssetData& AssetData = AssetDataList[Index];
			LoadAssetsTask.EnterProgressFrame(1, FText::FromName(AssetData.PackageName));		

			static constexpr TCHAR Format[] = TEXT("[%d/%d]: %s -> %s");
			UE_LOG(LogRigVM, Display, Format, Index, NumAssets, *AssetData.AssetName.ToString(), *AssetData.PackageName.ToString())

			// completely ignore exceptions during this scope
			FScopedScriptExceptionHandler ScopedScriptExceptionHandler([](ELogVerbosity::Type Verbosity, const TCHAR* ErrorMessage, const TCHAR* StackMessage)
			{
				FString Message;
				if(ErrorMessage)
				{
					Message += ErrorMessage;
				}
				if(StackMessage)
				{
					if(!Message.IsEmpty())
					{
						Message += TEXT("\n");
					}
					Message += StackMessage;
				}

				if(Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Fatal)
				{
					UE_LOG(LogRigVMDeveloper, Error, TEXT("%s"), *Message);
				}
				else if(Verbosity == ELogVerbosity::Warning)
				{
					UE_LOG(LogRigVMDeveloper, Warning, TEXT("%s"), *Message);
				}
				else
				{
					UE_LOG(LogRigVMDeveloper, Display, TEXT("%s"), *Message);
				}
			});

			// set up a lambda to record all error messages
			TArray<FRigVMBlueprintLoadLogEntry> LogEntries;

			if(InBlueprintFilter.IsBound())
			{
				URigVMBlueprint::QueueCompilerMessageDelegate(FOnRigVMReportCompilerMessage::FDelegate::CreateLambda(
					[&LogEntries](EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage)
					{
						ERigVMBlueprintLoadLogSeverity Severity = ERigVMBlueprintLoadLogSeverity::Display;
						if(InSeverity == EMessageSeverity::Warning)
						{
							Severity = ERigVMBlueprintLoadLogSeverity::Warning;
						}
						else if(InSeverity == EMessageSeverity::Error)
						{
							Severity = ERigVMBlueprintLoadLogSeverity::Error;
						}
						LogEntries.Emplace(Severity, InSubject, InMessage);
					})
				);
			}
			
			if(URigVMBlueprint* Blueprint = Cast<URigVMBlueprint>(AssetData.GetAsset()))
			{
				if(InBlueprintFilter.IsBound())
				{
					if(InBlueprintFilter.Execute(Blueprint, LogEntries))
					{
						LoadedAssets.Add(Blueprint);
					}
				}
				else
				{
					LoadedAssets.Add(Blueprint);
				}

				Blueprint->OnReportCompilerMessage().Clear();
			}

			if(InBlueprintFilter.IsBound())
			{
				URigVMBlueprint::ClearQueuedCompilerMessageDelegates();
			}
			LoadAssetsTask.ForceRefresh();
		}
	}

	return LoadedAssets;
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithAssetDataAndNodeFilters_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMAssetDataFilterDynamic InAssetDataFilter,
	FRigVMNodeFilterDynamic InNodeFilter)
{
	return LoadAssetsWithAssetDataAndNodeFilters(InClass,
		FRigVMAssetDataFilter::CreateLambda(
		[InAssetDataFilter](const FAssetData& AssetData) -> bool
		{
			return InAssetDataFilter.Execute(AssetData);
		}),
		FRigVMNodeFilter::CreateLambda(
		[InNodeFilter](const URigVMBlueprint* Blueprint, const URigVMNode* Node) -> bool
		{
			return InNodeFilter.Execute(Blueprint, Node);
		})
	);
}

TArray<URigVMBlueprint*> URigVMEditorBlueprintLibrary::LoadAssetsWithAssetDataAndNodeFilters(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMAssetDataFilter InAssetDataFilter, FRigVMNodeFilter InNodeFilter)
{
	return LoadAssetsWithAssetDataAndBlueprintFilters(InClass, InAssetDataFilter, FRigVMBlueprintFilter::CreateLambda(
		[InNodeFilter](const URigVMBlueprint* Blueprint, const TArray<FRigVMBlueprintLoadLogEntry>& LogEntries) -> bool
		{
			TArray<URigVMGraph*> Models = Blueprint->GetAllModels();
			int32 NumNodes = 0;
			for(const URigVMGraph* Model : Models)
			{
				NumNodes += Model->GetNodes().Num();
			}
			FScopedSlowTask FilterNodesTask(NumNodes, LOCTEXT("FilteringNodes", "Filtering Nodes"));
			for(const URigVMGraph* Model : Models)
			{
				for(const URigVMNode* Node : Model->GetNodes())
				{
					FilterNodesTask.EnterProgressFrame(1);		
					if(InNodeFilter.Execute(Blueprint, Node))
					{
						return true;
					}
				}
			}
			return false;
		})
	);
}

TArray<FAssetData> URigVMEditorBlueprintLibrary::GetAssetsWithFilter_ForBlueprint(
	TSubclassOf<URigVMBlueprint> InClass, FRigVMAssetDataFilterDynamic InAssetDataFilter)
{
	return GetAssetsWithFilter(InClass, FRigVMAssetDataFilter::CreateLambda(
		[InAssetDataFilter](const FAssetData& AssetData) -> bool
		{
			return InAssetDataFilter.Execute(AssetData);
		})
	);
}

TArray<FAssetData> URigVMEditorBlueprintLibrary::GetAssetsWithFilter(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilter InAssetDataFilter)
{
	if(InClass == nullptr)
	{
		InClass = URigVMBlueprint::StaticClass();
	}

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(InClass->GetClassPathName(), AssetDataList, true);

	if(InAssetDataFilter.IsBound())
	{
		TArray<FAssetData> FilteredAssetDataList;
		for(const FAssetData& AssetData : AssetDataList)
		{
			if(InAssetDataFilter.Execute(AssetData))
			{
				FilteredAssetDataList.Add(AssetData);
			}			
		}
		Swap(FilteredAssetDataList, AssetDataList);
	}

	return AssetDataList;
}

#undef LOCTEXT_NAMESPACE
