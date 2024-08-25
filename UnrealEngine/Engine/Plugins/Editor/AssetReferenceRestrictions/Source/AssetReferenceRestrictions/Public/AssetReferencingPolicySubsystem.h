// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "AssetReferencingPolicySubsystem.generated.h"

struct FAssetReferenceFilterContext;
class IAssetReferenceFilter;
struct FDomainDatabase;

/** Subsystem to register the domain-based asset referencing policy restrictions with the editor */
UCLASS()
class ASSETREFERENCERESTRICTIONS_API UAssetReferencingPolicySubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~UEditorSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of UEditorSubsystem interface

	TSharedPtr<FDomainDatabase> GetDomainDB() const;
private:
	void UpdateDBIfNecessary() const;

	TSharedPtr<IAssetReferenceFilter> HandleMakeAssetReferenceFilter(const FAssetReferenceFilterContext& Context);

	TSharedPtr<FDomainDatabase> DomainDB;
};
