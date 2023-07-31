// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator_SourceControl.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidator_SourceControl)

#define LOCTEXT_NAMESPACE "EditorValidator"

UEditorValidator_SourceControl::UEditorValidator_SourceControl()
	: Super()
{
	
}

bool UEditorValidator_SourceControl::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return Super::CanValidateAsset_Implementation(InAsset) && InAsset != nullptr;
}

EDataValidationResult UEditorValidator_SourceControl::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	check(InAsset);

	FName PackageFName = InAsset->GetOutermost()->GetFName();
	if (FPackageName::DoesPackageExist(PackageFName.ToString()))
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		FSourceControlStatePtr AssetState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(PackageFName.ToString()), EStateCacheUsage::Use);
		if (AssetState.IsValid() && AssetState->IsSourceControlled())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			// Check for assets that are submitted to source control that reference assets that are not
			static const FString ScriptPackagePrefix = TEXT("/Script/");
			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(PackageFName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			for (FName Dependency : Dependencies)
			{
				const FString DependencyStr = Dependency.ToString();
				if (!DependencyStr.StartsWith(ScriptPackagePrefix))
				{
					FSourceControlStatePtr DependencyState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(DependencyStr), EStateCacheUsage::Use);
					if (DependencyState.IsValid() && !DependencyState->IsSourceControlled())
					{
						AssetFails(InAsset, FText::Format(LOCTEXT("SourceControl_NotMarkedForAdd", "References {0} which is not marked for add in perforce"), FText::FromString(DependencyStr)), ValidationErrors);
					}
				}
			}
		}
	}

	if (GetValidationResult() != EDataValidationResult::Invalid)
	{
		AssetPasses(InAsset);
	}

	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
