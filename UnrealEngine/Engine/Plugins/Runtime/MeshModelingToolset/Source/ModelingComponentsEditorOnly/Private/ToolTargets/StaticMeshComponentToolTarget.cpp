// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolTargets/StaticMeshComponentToolTarget.h"

#include "ComponentReregisterContext.h"
#include "Components/StaticMeshComponent.h"
#include "ConversionUtils/DynamicMeshViaMeshDescriptionUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "ToolTargets/StaticMeshToolTarget.h"
#include "AssetUtils/MeshDescriptionUtil.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StaticMeshComponentToolTarget)

using namespace UE::Geometry;

void UStaticMeshComponentToolTarget::SetEditingLOD(EMeshLODIdentifier RequestedEditingLOD)
{
	EMeshLODIdentifier ValidEditingLOD = EMeshLODIdentifier::LOD0;

	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	if (ensure(StaticMeshComponent != nullptr))
	{
		UStaticMesh* StaticMeshAsset = StaticMeshComponent->GetStaticMesh();
		ValidEditingLOD = UStaticMeshToolTarget::GetValidEditingLOD(StaticMeshAsset, RequestedEditingLOD);
	}

	EditingLOD = ValidEditingLOD;
}


bool UStaticMeshComponentToolTarget::IsValid() const
{
	if (!UPrimitiveComponentToolTarget::IsValid())
	{
		return false;
	}
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	if (StaticMeshComponent == nullptr)
	{
		return false;
	}
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	
	return UStaticMeshToolTarget::HasNonGeneratedLOD(StaticMesh, EditingLOD);
}



int32 UStaticMeshComponentToolTarget::GetNumMaterials() const
{
	return ensure(IsValid()) ? Component->GetNumMaterials() : 0;
}

UMaterialInterface* UStaticMeshComponentToolTarget::GetMaterial(int32 MaterialIndex) const
{
	return ensure(IsValid()) ? Component->GetMaterial(MaterialIndex) : nullptr;
}

void UStaticMeshComponentToolTarget::GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const
{
	if (!ensure(IsValid())) return;

	if (bPreferAssetMaterials)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh();
		UStaticMeshToolTarget::GetMaterialSet(StaticMesh, MaterialSetOut, bPreferAssetMaterials);
	}
	else
	{
		int32 NumMaterials = Component->GetNumMaterials();
		MaterialSetOut.Materials.SetNum(NumMaterials);
		for (int32 k = 0; k < NumMaterials; ++k)
		{
			MaterialSetOut.Materials[k] = Component->GetMaterial(k);
		}
	}
}

bool UStaticMeshComponentToolTarget::CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset)
{
	if (!ensure(IsValid())) return false;

	if (bApplyToAsset)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh();

		// unregister the component while we update it's static mesh
		TUniquePtr<FComponentReregisterContext> ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component.Get());

		return UStaticMeshToolTarget::CommitMaterialSetUpdate(StaticMesh, MaterialSet, bApplyToAsset);
	}
	else
	{
		// filter out any Engine materials that we don't want to be permanently assigning
		TArray<UMaterialInterface*> FilteredMaterials = MaterialSet.Materials;
		for (int32 k = 0; k < FilteredMaterials.Num(); ++k)
		{
			FString AssetPath = FilteredMaterials[k]->GetPathName();
			if (AssetPath.StartsWith(TEXT("/MeshModelingToolsetExp/")))
			{
				FilteredMaterials[k] = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}

		int32 NumMaterialsNeeded = Component->GetNumMaterials();
		int32 NumMaterialsGiven = FilteredMaterials.Num();

		// We wrote the below code to support a mismatch in the number of materials.
		// However, it is not yet clear whether this might be desirable, and we don't
		// want to inadvertantly hide bugs in the meantime. So, we keep this ensure here
		// for now, and we can remove it if we decide that we want the ability.
		ensure(NumMaterialsNeeded == NumMaterialsGiven);

		check(NumMaterialsGiven > 0);

		for (int32 i = 0; i < NumMaterialsNeeded; ++i)
		{
			int32 MaterialToUseIndex = FMath::Min(i, NumMaterialsGiven - 1);
			Component->SetMaterial(i, FilteredMaterials[MaterialToUseIndex]);
		}
	}

	return true;
}


const FMeshDescription* UStaticMeshComponentToolTarget::GetMeshDescription(const FGetMeshParameters& GetMeshParams)
{
	if (ensure(IsValid()))
	{
		ensure(GetMeshParams.bWantMeshTangents == false);		// cannot support in this path because we have to modify the FoundMeshDescription pointer!

		UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh();
		EMeshLODIdentifier UseLOD = EditingLOD;
		if (StaticMesh && GetMeshParams.bHaveRequestLOD)
		{
			UseLOD = UStaticMeshToolTarget::GetValidEditingLOD(StaticMesh, GetMeshParams.RequestLOD);
			ensure(UseLOD == GetMeshParams.RequestLOD);		// probably a bug somewhere if this is not true
		}

		return UStaticMeshToolTarget::GetMeshDescriptionWithScaleApplied(StaticMesh, (int32)UseLOD, CachedMeshDescriptions);
	}
	return nullptr;
}

FMeshDescription UStaticMeshComponentToolTarget::GetEmptyMeshDescription()
{
	FMeshDescription EmptyMeshDescription;
	FStaticMeshAttributes Attributes(EmptyMeshDescription);
	Attributes.Register();
	return EmptyMeshDescription;
}

FMeshDescription UStaticMeshComponentToolTarget::GetMeshDescriptionCopy(const FGetMeshParameters& GetMeshParams)
{
	auto ApplyBuildScaleIfNeeded = [](FMeshDescription& MeshDescription, FVector BuildScale)
	{
		if (!BuildScale.Equals(FVector::OneVector))
		{
			FTransform ScaleTransform = FTransform::Identity;
			ScaleTransform.SetScale3D(BuildScale);
			FStaticMeshOperations::ApplyTransform(MeshDescription, ScaleTransform, true);
		}
	};
	if (ensure(IsValid()))
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh())
		{
			EMeshLODIdentifier UseLOD = EditingLOD;
			if (GetMeshParams.bHaveRequestLOD)
			{
				UseLOD = UStaticMeshToolTarget::GetValidEditingLOD(StaticMesh, GetMeshParams.RequestLOD);
				ensure(UseLOD == GetMeshParams.RequestLOD);		// probably a bug somewhere if this is not true
			}
			if (UseLOD == EMeshLODIdentifier::HiResSource )
			{
				if (StaticMesh->IsHiResMeshDescriptionValid())
				{
					FMeshDescription MeshDescriptionCopy = *StaticMesh->GetHiResMeshDescription();
					const FStaticMeshSourceModel& SourceModel = StaticMesh->GetHiResSourceModel();
					ApplyBuildScaleIfNeeded(MeshDescriptionCopy, SourceModel.BuildSettings.BuildScale3D);
					UE::MeshDescription::InitializeAutoGeneratedAttributes(MeshDescriptionCopy, &SourceModel.BuildSettings);
					return MeshDescriptionCopy;
				}
			}
			else
			{
				if (StaticMesh->IsMeshDescriptionValid(static_cast<int32>(UseLOD)))
				{
					if (FMeshDescription* SourceMesh = StaticMesh->GetMeshDescription(static_cast<int32>(UseLOD)))
					{
						FMeshDescription MeshDescriptionCopy = *SourceMesh;
						ApplyBuildScaleIfNeeded(MeshDescriptionCopy, StaticMesh->GetSourceModel((int32)UseLOD).BuildSettings.BuildScale3D);
						UE::MeshDescription::InitializeAutoGeneratedAttributes(MeshDescriptionCopy, StaticMesh, static_cast<int32>(UseLOD));
						return MeshDescriptionCopy;
					}
				}
			}
		}
	}
	

	FMeshDescription EmptyMeshDescription;
	FStaticMeshAttributes Attributes(EmptyMeshDescription);
	Attributes.Register();
	return EmptyMeshDescription;
}


TArray<EMeshLODIdentifier> UStaticMeshComponentToolTarget::GetAvailableLODs(bool bSkipAutoGenerated) const
{
	UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh();
	return UStaticMeshToolTarget::GetAvailableLODs(StaticMesh, bSkipAutoGenerated);
}


void UStaticMeshComponentToolTarget::CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitParams)
{
	if (ensure(IsValid()) == false) return;

	UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Component)->GetStaticMesh();

	EMeshLODIdentifier WriteToLOD = (CommitParams.bHaveTargetLOD && CommitParams.TargetLOD != EMeshLODIdentifier::Default) ? CommitParams.TargetLOD : EditingLOD;

	// unregister the component while we update its static mesh
	FComponentReregisterContext ComponentReregisterContext(Component.Get());

	UStaticMeshToolTarget::CommitMeshDescription(StaticMesh, Committer, WriteToLOD);

	// this rebuilds physics, but it doesn't undo!
	Component->RecreatePhysicsState();
}

FDynamicMesh3 UStaticMeshComponentToolTarget::GetDynamicMesh()
{
	return GetDynamicMeshViaMeshDescription(*this);
}

FDynamicMesh3 UStaticMeshComponentToolTarget::GetDynamicMesh(bool bRequestTangents)
{
	return GetDynamicMeshViaMeshDescription(*this, bRequestTangents);
}

void UStaticMeshComponentToolTarget::CommitDynamicMesh(const FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo)
{
	const FMeshDescription* CurrentMeshDescription = GetMeshDescription();
	if (ensureMsgf(CurrentMeshDescription, TEXT("Unable to commit mesh, perhaps the user deleted "
		"the asset while the tool was active?")))
	{
		CommitDynamicMeshViaMeshDescription(FMeshDescription(*CurrentMeshDescription), *this, Mesh, CommitInfo);
	}
}

UStaticMesh* UStaticMeshComponentToolTarget::GetStaticMesh() const
{
	return IsValid() ? Cast<UStaticMeshComponent>(Component)->GetStaticMesh() : nullptr;
}


UBodySetup* UStaticMeshComponentToolTarget::GetBodySetup() const
{
	UStaticMesh* StaticMesh = GetStaticMesh();
	if (StaticMesh)
	{
		return StaticMesh->GetBodySetup();
	}
	return nullptr;
}


IInterface_CollisionDataProvider* UStaticMeshComponentToolTarget::GetComplexCollisionProvider() const
{
	UStaticMesh* StaticMesh = GetStaticMesh();
	if (StaticMesh)
	{
		return Cast<IInterface_CollisionDataProvider>(StaticMesh);
	}
	return nullptr;
}


// Factory

bool UStaticMeshComponentToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	const UStaticMeshComponent* Component = GetValid(Cast<UStaticMeshComponent>(SourceObject));
	const UStaticMesh* StaticMesh = (Component && !Component->IsUnreachable() && Component->IsValidLowLevel()) ? Component->GetStaticMesh() : nullptr;
	return StaticMesh
		&& !StaticMesh->GetOutermost()->bIsCookedForEditor
		&& UStaticMeshToolTarget::HasNonGeneratedLOD(StaticMesh, EditingLOD)
		&& Requirements.AreSatisfiedBy(UStaticMeshComponentToolTarget::StaticClass());
}

UToolTarget* UStaticMeshComponentToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	UStaticMeshComponentToolTarget* Target = NewObject<UStaticMeshComponentToolTarget>();// TODO: Should we set an outer here?
	Target->InitializeComponent(Cast<UStaticMeshComponent>(SourceObject));
	Target->SetEditingLOD(EditingLOD);
	checkSlow(Target->Component.Get() && Requirements.AreSatisfiedBy(Target));

	return Target;
}


void UStaticMeshComponentToolTargetFactory::SetActiveEditingLOD(EMeshLODIdentifier NewEditingLOD)
{
	EditingLOD = NewEditingLOD;
}
