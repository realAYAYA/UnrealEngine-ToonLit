// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolTargets/SkeletalMeshComponentToolTarget.h"

#include "Components/SkeletalMeshComponent.h"
#include "ConversionUtils/DynamicMeshViaMeshDescriptionUtil.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ToolTargets/SkeletalMeshToolTarget.h"
#include "SkeletalMeshAttributes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshComponentToolTarget)

using namespace UE::Geometry;

namespace USkeletalMeshComponentToolTargetLocals
{
	int32 LODIndex = 0;
}


//
// USkeletalMeshComponentReadOnlyToolTarget
//

bool USkeletalMeshComponentReadOnlyToolTarget::IsValid() const
{
	if (!UPrimitiveComponentToolTarget::IsValid())
	{
		return false;
	}
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component);
	if (SkeletalMeshComponent == nullptr)
	{
		return false;
	}
	const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();

	return USkeletalMeshReadOnlyToolTarget::IsValid(SkeletalMesh);
}

int32 USkeletalMeshComponentReadOnlyToolTarget::GetNumMaterials() const
{
	return ensure(IsValid()) ? Component->GetNumMaterials() : 0;
}

UMaterialInterface* USkeletalMeshComponentReadOnlyToolTarget::GetMaterial(int32 MaterialIndex) const
{
	return ensure(IsValid()) ? Component->GetMaterial(MaterialIndex) : nullptr;
}

void USkeletalMeshComponentReadOnlyToolTarget::GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const
{
	if (!ensure(IsValid())) return;

	if (bPreferAssetMaterials)
	{
		const USkeletalMesh* SkeletalMesh = Cast<USkeletalMeshComponent>(Component)->GetSkeletalMeshAsset();
		USkeletalMeshToolTarget::GetMaterialSet(SkeletalMesh, MaterialSetOut, bPreferAssetMaterials);
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

bool USkeletalMeshComponentReadOnlyToolTarget::CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset)
{
	if (!ensure(IsValid())) return false;

	if (bApplyToAsset)
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMeshComponent>(Component)->GetSkeletalMeshAsset();

		// unregister the component while we update it's static mesh
		TUniquePtr<FComponentReregisterContext> ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component);

		return USkeletalMeshToolTarget::CommitMaterialSetUpdate(SkeletalMesh, MaterialSet, bApplyToAsset);
	}
	else
	{
		const int32 NumMaterialsNeeded = Component->GetNumMaterials();
		const int32 NumMaterialsGiven = MaterialSet.Materials.Num();

		// We wrote the below code to support a mismatch in the number of materials.
		// However, it is not yet clear whether this might be desirable, and we don't
		// want to inadvertently hide bugs in the meantime. So, we keep this ensure here
		// for now, and we can remove it if we decide that we want the ability.
		ensure(NumMaterialsNeeded == NumMaterialsGiven);

		check(NumMaterialsGiven > 0);

		for (int32 i = 0; i < NumMaterialsNeeded; ++i)
		{
			const int32 MaterialToUseIndex = FMath::Min(i, NumMaterialsGiven - 1);
			Component->SetMaterial(i, MaterialSet.Materials[MaterialToUseIndex]);
		}
	}

	return true;
}

const FMeshDescription* USkeletalMeshComponentReadOnlyToolTarget::GetMeshDescription(const FGetMeshParameters& GetMeshParams)
{
	if (!ensure(IsValid()))
	{
		return nullptr;
	}
	ensure(GetMeshParams.bHaveRequestLOD == false);	// not supported yet, just returning default LOD

	if (!CachedMeshDescription.IsValid())
	{
		CachedMeshDescription = MakeUnique<FMeshDescription>();
		const USkeletalMesh* SkeletalMesh = Cast<USkeletalMeshComponent>(Component)->GetSkeletalMeshAsset();
		USkeletalMeshToolTarget::GetMeshDescription(SkeletalMesh, *CachedMeshDescription);
	}
	
	return CachedMeshDescription.Get();
}

FMeshDescription USkeletalMeshComponentReadOnlyToolTarget::GetEmptyMeshDescription()
{
	FMeshDescription EmptyMeshDescription;
	FSkeletalMeshAttributes MeshAttributes(EmptyMeshDescription);
	MeshAttributes.Register();

	return EmptyMeshDescription;
}

FDynamicMesh3 USkeletalMeshComponentReadOnlyToolTarget::GetDynamicMesh()
{
	return GetDynamicMeshViaMeshDescription(*this);
}


USkeletalMesh* USkeletalMeshComponentReadOnlyToolTarget::GetSkeletalMesh() const
{
	return IsValid() ? Cast<USkeletalMeshComponent>(Component)->GetSkeletalMeshAsset() : nullptr;
}


//
// USkeletalMeshComponentToolTarget
//


void USkeletalMeshComponentToolTarget::CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitMeshParams)
{
	if (ensure(IsValid()) == false) return;

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMeshComponent>(Component)->GetSkeletalMeshAsset();
	if (!CachedMeshDescription.IsValid())
	{
		CachedMeshDescription = MakeUnique<FMeshDescription>();
		USkeletalMeshToolTarget::GetMeshDescription(SkeletalMesh, *CachedMeshDescription);
	}

	// unregister the component while we update its skeletal mesh
	FComponentReregisterContext ComponentReregisterContext(Component);

	USkeletalMeshToolTarget::CommitMeshDescription(SkeletalMesh, CachedMeshDescription.Get(), Committer);

	// this rebuilds physics, but it doesn't undo!
	Component->RecreatePhysicsState();

	CachedMeshDescription.Reset();
}


void USkeletalMeshComponentToolTarget::CommitDynamicMesh(const FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo)
{
	const FMeshDescription* CurrentMeshDescription = GetMeshDescription();
	if (ensureMsgf(CurrentMeshDescription, TEXT("Unable to commit mesh, perhaps the user deleted "
		"the asset while the tool was active?")))
	{
		CommitDynamicMeshViaMeshDescription(FMeshDescription(*CurrentMeshDescription), *this, Mesh, CommitInfo);
	}
}


// Factory

bool USkeletalMeshComponentReadOnlyToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	// We are using an exact cast here to prevent subclasses, which might not meet all
	// requirements for functionality such as the deprecated DestructibleMeshComponent, from 
	// being caught up as valid targets.
	// If you want to make the tool target work with some subclass of USkeletalMeshComponent,
	// just add another factory that allows that class specifically(but make sure that
	// GetMeshDescription and such work properly)

	return Cast<USkeletalMeshComponent>(SourceObject) && Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset() &&
		ExactCast<USkeletalMesh>(Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset()) &&
		Requirements.AreSatisfiedBy(USkeletalMeshComponentReadOnlyToolTarget::StaticClass());
}

UToolTarget* USkeletalMeshComponentReadOnlyToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	USkeletalMeshComponentReadOnlyToolTarget* Target = NewObject<USkeletalMeshComponentReadOnlyToolTarget>();
	Target->Component = Cast<USkeletalMeshComponent>(SourceObject);
	check(Target->Component && Requirements.AreSatisfiedBy(Target));

	return Target;
}

bool USkeletalMeshComponentToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	// We are using an exact cast here to prevent subclasses, which might not meet all
	// requirements for functionality such as the deprecated DestructibleMeshComponent, from 
	// being caught up as valid targets.
	// If you want to make the tool target work with some subclass of USkeletalMeshComponent,
	// just add another factory that allows that class specifically(but make sure that
	// GetMeshDescription and such work properly)

	return Cast<USkeletalMeshComponent>(SourceObject) && Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset() &&
		ExactCast<USkeletalMesh>(Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset()) &&
		Requirements.AreSatisfiedBy(USkeletalMeshComponentToolTarget::StaticClass());
}

UToolTarget* USkeletalMeshComponentToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	USkeletalMeshComponentToolTarget* Target = NewObject<USkeletalMeshComponentToolTarget>();
	Target->Component = Cast<USkeletalMeshComponent>(SourceObject);
	check(Target->Component && Requirements.AreSatisfiedBy(Target));

	return Target;
}

