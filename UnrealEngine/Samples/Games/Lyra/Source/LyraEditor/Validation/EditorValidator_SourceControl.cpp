// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator_SourceControl.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ISourceControlModule.h"
#include "Misc/PackageName.h"
#include "SourceControlHelpers.h"
#include "Validation/EditorValidator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidator_SourceControl)

#define LOCTEXT_NAMESPACE "EditorValidator"

UEditorValidator_SourceControl::UEditorValidator_SourceControl()
	: Super()
{
	
}

bool UEditorValidator_SourceControl::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return InAsset != nullptr;
}

EDataValidationResult UEditorValidator_SourceControl::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
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
					if (DependencyState.IsValid() && !DependencyState->IsSourceControlled() && !DependencyState->IsUnknown())
					{
						// The editor doesn't sync state for all assets, so we only want to warn on assets that are known about
						AssetFails(InAsset, FText::Format(LOCTEXT("SourceControl_NotMarkedForAdd", "References {0} which is not marked for add in source control"), FText::FromString(DependencyStr)));
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
