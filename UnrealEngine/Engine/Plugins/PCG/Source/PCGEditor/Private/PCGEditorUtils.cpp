// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorUtils.h"

#include "PCGDataAsset.h"
#include "PCGGraph.h"
#include "Elements/PCGExecuteBlueprint.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"

bool PCGEditorUtils::IsAssetPCGBlueprint(const FAssetData& InAssetData)
{
	FString InNativeParentClassName = InAssetData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
	FString TargetNativeParentClassName = UPCGBlueprintElement::GetParentClassName();

	return InAssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName() && InNativeParentClassName == TargetNativeParentClassName;
}

void PCGEditorUtils::GetParentPackagePathAndUniqueName(const UObject* OriginalObject, const FString& NewAssetTentativeName, FString& OutPackagePath, FString& OutUniqueName)
{
	if (OriginalObject == nullptr)
	{
		return;
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString PackageRoot, PackagePath, PackageName;
	FPackageName::SplitLongPackageName(OriginalObject->GetPackage()->GetPathName(), PackageRoot, PackagePath, PackageName);

	OutPackagePath = PackageRoot / PackagePath;
	
	if (!FPackageName::IsValidObjectPath(OutPackagePath))
	{
		OutPackagePath = FPaths::ProjectContentDir();
	}
	
	FString DummyPackageName;
	AssetTools.CreateUniqueAssetName(OutPackagePath, NewAssetTentativeName, DummyPackageName, OutUniqueName);
}

void PCGEditorUtils::ForEachAssetData(const FARFilter& InFilter, TFunctionRef<bool(const FAssetData&)> InFunc)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssets(InFilter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (!InFunc(AssetData))
		{
			break;
		}
	}
}

void PCGEditorUtils::ForEachPCGBlueprintAssetData(TFunctionRef<bool(const FAssetData&)> InFunc)
{
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.TagsAndValues.Add(FBlueprintTags::NativeParentClassPath, UPCGBlueprintElement::GetParentClassName());

	ForEachAssetData(Filter, InFunc);
}

void PCGEditorUtils::ForEachPCGSettingsAssetData(TFunctionRef<bool(const FAssetData&)> InFunc)
{
	FARFilter Filter;
	Filter.ClassPaths.Add(UPCGSettings::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	ForEachAssetData(Filter, InFunc);
}

void PCGEditorUtils::ForEachPCGGraphAssetData(TFunctionRef<bool(const FAssetData&)> InFunc)
{
	FARFilter Filter;
	Filter.ClassPaths.Add(UPCGGraph::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	ForEachAssetData(Filter, InFunc);
}

void PCGEditorUtils::ForEachPCGAssetData(TFunctionRef<bool(const FAssetData&)> InFunc)
{
	FARFilter Filter;
	Filter.ClassPaths.Add(UPCGDataAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	ForEachAssetData(Filter, InFunc);
}

void PCGEditorUtils::ForcePCGBlueprintVariableVisibility()
{
	ForEachPCGBlueprintAssetData([](const FAssetData& AssetData)
	{
		const FString GeneratedClass = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		FSoftClassPath BlueprintClassPath = FSoftClassPath(GeneratedClass);
		TSubclassOf<UPCGBlueprintElement> BlueprintClass = BlueprintClassPath.TryLoadClass<UPCGBlueprintElement>();
		if (BlueprintClass)
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->NewVariables.IsEmpty())
				{
					return true;
				}

				const bool bHasEditOnInstanceVariables = (Blueprint->NewVariables.FindByPredicate([](const FBPVariableDescription& VarDesc) { return !(VarDesc.PropertyFlags & CPF_DisableEditOnInstance); }) != nullptr);
				if (!bHasEditOnInstanceVariables)
				{
					Blueprint->Modify();

					for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
					{
						VarDesc.PropertyFlags &= ~CPF_DisableEditOnInstance;
					}

					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
				}
			}
		}

		return true;
	});
}
