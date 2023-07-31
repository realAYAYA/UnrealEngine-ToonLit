// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolTargets/SkeletalMeshToolTarget.h"

#include "ConversionUtils/DynamicMeshViaMeshDescriptionUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SkeletalMeshAttributes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshToolTarget)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "SkeletalMeshToolTarget"

namespace USkeletalMeshToolTargetLocals
{
	int32 LODIndex = 0;
}


//
// USkeletalMeshReadOnlyToolTarget
//


bool USkeletalMeshReadOnlyToolTarget::IsValid() const
{
	return IsValid(SkeletalMesh);
}

bool USkeletalMeshReadOnlyToolTarget::IsValid(const USkeletalMesh* SkeletalMeshIn)
{
	if (!SkeletalMeshIn || !IsValidChecked(SkeletalMeshIn) || SkeletalMeshIn->IsUnreachable() || !SkeletalMeshIn->IsValidLowLevel())
	{
		return false;
	}

	return true;
}

int32 USkeletalMeshReadOnlyToolTarget::GetNumMaterials() const
{
	return ensure(IsValid()) ? SkeletalMesh->GetMaterials().Num() : 0;
}

UMaterialInterface* USkeletalMeshReadOnlyToolTarget::GetMaterial(int32 MaterialIndex) const
{
	return ensure(IsValid() && MaterialIndex < SkeletalMesh->GetMaterials().Num()) ? 
		SkeletalMesh->GetMaterials()[MaterialIndex].MaterialInterface : nullptr;
}

void USkeletalMeshReadOnlyToolTarget::GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const
{
	if (!ensure(IsValid())) return;
	GetMaterialSet(SkeletalMesh, MaterialSetOut, bPreferAssetMaterials);
}

void USkeletalMeshReadOnlyToolTarget::GetMaterialSet(const USkeletalMesh* SkeletalMeshIn, FComponentMaterialSet& MaterialSetOut,
	bool bPreferAssetMaterials)
{
	const TArray<FSkeletalMaterial>& Materials = SkeletalMeshIn->GetMaterials(); 
	MaterialSetOut.Materials.SetNum(Materials.Num());
	for (int32 k = 0; k < Materials.Num(); ++k)
	{
		MaterialSetOut.Materials[k] = Materials[k].MaterialInterface;
	}
}

bool USkeletalMeshReadOnlyToolTarget::CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset)
{
	if (!ensure(IsValid())) return false;
	return CommitMaterialSetUpdate(SkeletalMesh, MaterialSet, bApplyToAsset);
}

bool USkeletalMeshReadOnlyToolTarget::CommitMaterialSetUpdate(USkeletalMesh* SkeletalMeshIn, 
	const FComponentMaterialSet& MaterialSet, bool bApplyToAsset)
{
	if (!bApplyToAsset)
	{
		return false;
	}

	if (SkeletalMeshIn->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		UE_LOG(LogTemp, Warning, TEXT("CANNOT MODIFY BUILT-IN ENGINE ASSET %s"), *SkeletalMeshIn->GetPathName());
		return false;
	}

	// flush any pending rendering commands, which might touch a component while we are rebuilding its mesh
	FlushRenderingCommands();

	// make sure transactional flag is on
	SkeletalMeshIn->SetFlags(RF_Transactional);

	SkeletalMeshIn->Modify();

	const int NewNumMaterials = MaterialSet.Materials.Num();
	TArray<FSkeletalMaterial> &SkeletalMaterials = SkeletalMeshIn->GetMaterials(); 
	if (NewNumMaterials != SkeletalMaterials.Num())
	{
		SkeletalMaterials.SetNum(NewNumMaterials);
	}
	for (int k = 0; k < NewNumMaterials; ++k)
	{
		if (SkeletalMaterials[k].MaterialInterface != MaterialSet.Materials[k])
		{
			SkeletalMaterials[k].MaterialInterface = MaterialSet.Materials[k];
			if (SkeletalMaterials[k].MaterialSlotName.IsNone())
			{
				SkeletalMaterials[k].MaterialSlotName = MaterialSet.Materials[k]->GetFName();
			}
		}
	}

	SkeletalMeshIn->PostEditChange();

	return true;
}

const FMeshDescription* USkeletalMeshReadOnlyToolTarget::GetMeshDescription(const FGetMeshParameters& GetMeshParams)
{
	if (!ensure(IsValid()))
	{
		return nullptr;
	}
	ensure(GetMeshParams.bHaveRequestLOD == false);	// not supported yet, just returning default LOD

	if (!CachedMeshDescription.IsValid())
	{
		CachedMeshDescription = MakeUnique<FMeshDescription>();
		GetMeshDescription(SkeletalMesh, *CachedMeshDescription);
	}

	return CachedMeshDescription.Get();
}


FMeshDescription USkeletalMeshReadOnlyToolTarget::GetEmptyMeshDescription()
{
	FMeshDescription EmptyMeshDescription;
	FSkeletalMeshAttributes MeshAttributes(EmptyMeshDescription);
	MeshAttributes.Register();

	return EmptyMeshDescription;
}

void USkeletalMeshReadOnlyToolTarget::GetMeshDescription(const USkeletalMesh* SkeletalMeshIn, FMeshDescription& MeshDescription)
{
	using namespace USkeletalMeshToolTargetLocals;

	// Check first if we have bulk data available and non-empty.
	if (SkeletalMeshIn->IsLODImportedDataBuildAvailable(LODIndex) && !SkeletalMeshIn->IsLODImportedDataEmpty(LODIndex))
	{
		FSkeletalMeshImportData SkeletalMeshImportData;
		SkeletalMeshIn->LoadLODImportedData(LODIndex, SkeletalMeshImportData);
		SkeletalMeshImportData.GetMeshDescription(MeshDescription);
	}
	else
	{
		// Fall back on the LOD model directly if no bulk data exists. When we commit
		// the mesh description, we override using the bulk data. This can happen for older
		// skeletal meshes, from UE 4.24 and earlier.
		const FSkeletalMeshModel* SkeletalMeshModel = SkeletalMeshIn->GetImportedModel();
		if (SkeletalMeshModel && SkeletalMeshModel->LODModels.IsValidIndex(LODIndex))
		{
			SkeletalMeshModel->LODModels[LODIndex].GetMeshDescription(MeshDescription, SkeletalMeshIn);
		}			
	}
}

FDynamicMesh3 USkeletalMeshReadOnlyToolTarget::GetDynamicMesh()
{
	return GetDynamicMeshViaMeshDescription(*this);
}

USkeletalMesh* USkeletalMeshReadOnlyToolTarget::GetSkeletalMesh() const
{
	return IsValid() ? SkeletalMesh : nullptr;
}

//
// USkeletalMeshToolTarget
//

void USkeletalMeshToolTarget::CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitMeshParams)
{
	if (ensure(IsValid()) == false) return;

	if (!CachedMeshDescription.IsValid())
	{
		CachedMeshDescription = MakeUnique<FMeshDescription>();
		GetMeshDescription(SkeletalMesh, *CachedMeshDescription);
	}
	CommitMeshDescription(SkeletalMesh, CachedMeshDescription.Get(), Committer);
}

void USkeletalMeshToolTarget::CommitMeshDescription(USkeletalMesh* SkeletalMeshIn,
	FMeshDescription* MeshDescription, const FCommitter& Committer)
{
	using namespace USkeletalMeshToolTargetLocals;

	if (SkeletalMeshIn->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		FText Error = FText::Format(LOCTEXT("CannotModifyBuiltInAssetError", "Cannot modify built-in engine asset: {0}"), FText::FromString(*SkeletalMeshIn->GetPathName()));
		FNotificationInfo Info(Error);
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString());
		return;
	}

	// flush any pending rendering commands, which might touch a component while we are rebuilding it's mesh
	FlushRenderingCommands();

	// make sure transactional flag is on for this asset
	SkeletalMeshIn->SetFlags(RF_Transactional);

	verify(SkeletalMeshIn->Modify());

	FCommitterParams CommitterParams;

	CommitterParams.MeshDescriptionOut = MeshDescription;

	Committer(CommitterParams);

	FSkeletalMeshImportData SkeletalMeshImportData = 
		FSkeletalMeshImportData::CreateFromMeshDescription(*CommitterParams.MeshDescriptionOut);
	SkeletalMeshIn->SaveLODImportedData(LODIndex, SkeletalMeshImportData);

	// Make sure the mesh builder knows it's the latest variety, so that the render data gets
	// properly rebuilt.
	SkeletalMeshIn->SetLODImportedDataVersions(LODIndex, ESkeletalMeshGeoImportVersions::LatestVersion, ESkeletalMeshSkinningImportVersions::LatestVersion);
	SkeletalMeshIn->SetUseLegacyMeshDerivedDataKey(false);

	SkeletalMeshIn->PostEditChange();
}

void USkeletalMeshToolTarget::CommitDynamicMesh(const FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo)
{
	FMeshDescription CurrentMeshDescription = *GetMeshDescription();
	CommitDynamicMeshViaMeshDescription(MoveTemp(CurrentMeshDescription), *this, Mesh, CommitInfo);
}


// Factory

bool USkeletalMeshReadOnlyToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	// We are using an exact cast here to prevent subclasses, which might not meet all
	// requirements for functionality such as the deprecated DestructibleMesh, from 
	// being caught up as valid targets.
	// If you want to make the tool target work with some subclass of USkeletalMesh,
	// just add another factory that allows that class specifically (but make sure that
	// GetMeshDescription and such work properly)

	return ExactCast<USkeletalMesh>(SourceObject) 
		&& Requirements.AreSatisfiedBy(USkeletalMeshReadOnlyToolTarget::StaticClass());
}

UToolTarget* USkeletalMeshReadOnlyToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	USkeletalMeshReadOnlyToolTarget* Target = NewObject<USkeletalMeshReadOnlyToolTarget>();
	Target->SkeletalMesh = Cast<USkeletalMesh>(SourceObject);
	check(Target->SkeletalMesh && Requirements.AreSatisfiedBy(Target));

	return Target;
}

bool USkeletalMeshToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	// We are using an exact cast here to prevent subclasses, which might not meet all
	// requirements for functionality such as the deprecated DestructibleMesh, from 
	// being caught up as valid targets.
	// If you want to make the tool target work with some subclass of USkeletalMesh,
	// just add another factory that allows that class specifically (but make sure that
	// GetMeshDescription and such work properly)

	return ExactCast<USkeletalMesh>(SourceObject) 
		&& Requirements.AreSatisfiedBy(USkeletalMeshToolTarget::StaticClass());
}

UToolTarget* USkeletalMeshToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	USkeletalMeshToolTarget* Target = NewObject<USkeletalMeshToolTarget>();
	Target->SkeletalMesh = Cast<USkeletalMesh>(SourceObject);
	check(Target->SkeletalMesh && Requirements.AreSatisfiedBy(Target));

	return Target;
}

#undef LOCTEXT_NAMESPACE

