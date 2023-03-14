// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorUtils.h"

#include "AssetTypeCategories.h"
#include "UObject/NoExportTypes.h"
#include "Elements/PCGExecuteBlueprint.h"
#include "Engine/Blueprint.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"

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

	FString DummyPackageName;
	AssetTools.CreateUniqueAssetName(OutPackagePath, NewAssetTentativeName, DummyPackageName, OutUniqueName);
}

