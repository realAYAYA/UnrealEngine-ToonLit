// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGElement.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMeshSelectorBase)

#define LOCTEXT_NAMESPACE "PCGMeshSelectorBase"

void FPCGMeshMaterialOverrideHelper::Initialize(
	FPCGContext& InContext,
	bool bInUseMaterialOverrideAttributes,
	const TArray<TSoftObjectPtr<UMaterialInterface>>& InStaticMaterialOverrides,
	const TArray<FName>& InMaterialOverrideAttributeNames,
	const UPCGMetadata* InMetadata
)
{
	check(!bIsInitialized);

	bUseMaterialOverrideAttributes = bInUseMaterialOverrideAttributes;
	StaticMaterialOverrides = InStaticMaterialOverrides;
	MaterialOverrideAttributeNames = InMaterialOverrideAttributeNames;
	Metadata = InMetadata;

	Initialize(InContext);

	bIsInitialized = true;
}

void FPCGMeshMaterialOverrideHelper::Initialize(
	FPCGContext& InContext,
	bool bInByAttributeOverride,
	const TArray<FName>& InMaterialOverrideAttributeNames,
	const UPCGMetadata* InMetadata
)
{
	check(!bIsInitialized);

	bUseMaterialOverrideAttributes = bInByAttributeOverride;
	MaterialOverrideAttributeNames = InMaterialOverrideAttributeNames;
	Metadata = InMetadata;

	Initialize(InContext);

	bIsInitialized = true;
}

void FPCGMeshMaterialOverrideHelper::Initialize(FPCGContext& InContext)
{
	// Perform data setup & validation up-front
	if (bUseMaterialOverrideAttributes)
	{
		if (!Metadata)
		{
			PCGE_LOG_C(Error, GraphAndLog, &InContext, LOCTEXT("MetadataMissing", "Data has no metadata"));
			return;
		}

		for (const FName& MaterialOverrideAttributeName : MaterialOverrideAttributeNames)
		{
			if (!Metadata->HasAttribute(MaterialOverrideAttributeName))
			{
				PCGE_LOG_C(Error, GraphAndLog, &InContext, FText::Format(LOCTEXT("AttributeMissing", "Attribute '{0}' for material overrides is not present in the metadata"), FText::FromName(MaterialOverrideAttributeName)));
				return;
			}

			const FPCGMetadataAttributeBase* MaterialAttributeBase = Metadata->GetConstAttribute(MaterialOverrideAttributeName);
			check(MaterialAttributeBase);

			if (MaterialAttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
			{
				PCGE_LOG_C(Error, GraphAndLog, &InContext, LOCTEXT("AttributeInvalidType", "Material override attribute is not of valid type"));
				return;
			}

			MaterialAttributes.Add(static_cast<const FPCGMetadataAttribute<FString>*>(MaterialAttributeBase));
		}

		ValueKeyToOverrideMaterials.SetNum(MaterialOverrideAttributeNames.Num());
		WorkingMaterialOverrides.Reserve(MaterialOverrideAttributeNames.Num());
	}

	bIsValid = true;
}

void FPCGMeshMaterialOverrideHelper::Reset()
{
	MaterialAttributes.Reset();
	ValueKeyToOverrideMaterials.Reset();
	WorkingMaterialOverrides.Reset();
	bIsInitialized = false;
	bIsValid = false;
	bUseMaterialOverrideAttributes = false;
	StaticMaterialOverrides.Reset();
	MaterialOverrideAttributeNames.Reset();
	Metadata = nullptr;
}

const TArray<TSoftObjectPtr<UMaterialInterface>>& FPCGMeshMaterialOverrideHelper::GetMaterialOverrides(PCGMetadataEntryKey EntryKey)
{
	check(bIsValid);
	if (bUseMaterialOverrideAttributes)
	{
		WorkingMaterialOverrides.Reset();

		for (int32 MaterialIndex = 0; MaterialIndex < MaterialAttributes.Num(); ++MaterialIndex)
		{
			const FPCGMetadataAttribute<FString>* MaterialAttribute = MaterialAttributes[MaterialIndex];
			PCGMetadataValueKey MaterialValueKey = MaterialAttribute->GetValueKey(EntryKey);
			TSoftObjectPtr<UMaterialInterface>* NewMaterial = ValueKeyToOverrideMaterials[MaterialIndex].Find(MaterialValueKey);
			TSoftObjectPtr<UMaterialInterface> Material = nullptr;

			if (!NewMaterial)
			{
				FSoftObjectPath MaterialPath(MaterialAttribute->GetValue(MaterialValueKey));
				Material = TSoftObjectPtr<UMaterialInterface>(MaterialPath);
				ValueKeyToOverrideMaterials[MaterialIndex].Add(MaterialValueKey, Material);
			}
			else
			{
				Material = *NewMaterial;
			}

			WorkingMaterialOverrides.Add(Material);
		}

		return WorkingMaterialOverrides;
	}
	else
	{
		return StaticMaterialOverrides;
	}
}

#undef LOCTEXT_NAMESPACE
