// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetReferencingPolicySubsystem.h"
#include "AssetReferencingPolicySettings.h"
#include "DomainAssetReferenceFilter.h"
#include "Editor.h"
#include "AssetReferencingDomains.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetReferencingPolicySubsystem)

#define LOCTEXT_NAMESPACE "AssetReferencingPolicy"

bool UAssetReferencingPolicySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UAssetReferencingPolicySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	check(GEditor);
	GEditor->OnMakeAssetReferenceFilter().BindUObject(this, &ThisClass::HandleMakeAssetReferenceFilter);

	DomainDB = MakeShared<FDomainDatabase>();
	DomainDB->Init();
}

void UAssetReferencingPolicySubsystem::Deinitialize()
{
	check(GEditor);
	GEditor->OnMakeAssetReferenceFilter().Unbind();
	DomainDB.Reset();
}

TSharedPtr<IAssetReferenceFilter> UAssetReferencingPolicySubsystem::HandleMakeAssetReferenceFilter(const FAssetReferenceFilterContext& Context)
{
	return MakeShareable(new FDomainAssetReferenceFilter(Context, GetDomainDB()));
}

TSharedPtr<FDomainDatabase> UAssetReferencingPolicySubsystem::GetDomainDB() const
{
	DomainDB->UpdateIfNecessary();
	return DomainDB;
}

FAutoConsoleCommandWithWorldAndArgs GListDomainDatabaseCmd(
	TEXT("Editor.AssetReferenceRestrictions.ListDomainDatabase"),
	TEXT("Lists all of the asset reference domains the AssetReferenceRestrictions plugin knows about"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Params, UWorld* World)
		{
			if (UAssetReferencingPolicySubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>())
			{
				Subsystem->GetDomainDB()->DebugPrintAllDomains();
			}
		}));

#undef LOCTEXT_NAMESPACE
