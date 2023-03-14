// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothingSimulation.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

//==============================================================================
// FClothingSimulationContextCommon
//==============================================================================
DEFINE_STAT(STAT_ClothComputeNormals);
DEFINE_STAT(STAT_ClothInternalSolve);
DEFINE_STAT(STAT_ClothUpdateCollisions);
DEFINE_STAT(STAT_ClothSkinPhysMesh);
DEFINE_STAT(STAT_ClothFillContext);

static TAutoConsoleVariable<float> GClothMaxDeltaTimeTeleportMultiplier(
	TEXT("p.Cloth.MaxDeltaTimeTeleportMultiplier"),
	1.5f,
	TEXT("A multiplier of the MaxPhysicsDelta time at which we will automatically just teleport cloth to its new location\n")
	TEXT(" default: 1.5"));

FClothingSimulationContextCommon::FClothingSimulationContextCommon()
	: ComponentToWorld(FTransform::Identity)
	, WorldGravity(FVector::ZeroVector)
	, WindVelocity(FVector::ZeroVector)
	, WindAdaption(0.f)
	, DeltaSeconds(0.f)
	, VelocityScale(1.f)
	, TeleportMode(EClothingTeleportMode::None)
	, MaxDistanceScale(1.f)
	, PredictedLod(INDEX_NONE)
{}

FClothingSimulationContextCommon::~FClothingSimulationContextCommon()
{}

void FClothingSimulationContextCommon::Fill(const USkeletalMeshComponent* InComponent, float InDeltaSeconds, float InMaxPhysicsDelta)
{
	// Deprecated version always fills RefToLocals with current animation results instead of using reference pose on initialization
	const bool bIsInitialization = false;
	Fill(InComponent, InDeltaSeconds, InMaxPhysicsDelta, bIsInitialization);
}

void FClothingSimulationContextCommon::Fill(const USkeletalMeshComponent* InComponent, float InDeltaSeconds, float InMaxPhysicsDelta, bool bIsInitialization)
{
	SCOPE_CYCLE_COUNTER(STAT_ClothFillContext);
	LLM_SCOPE(ELLMTag::SkeletalMesh);

	check(InComponent);
	FillBoneTransforms(InComponent);
	FillRefToLocals(InComponent, bIsInitialization);
	FillComponentToWorld(InComponent);
	FillWorldGravity(InComponent);
	FillWindVelocity(InComponent);
	FillDeltaSeconds(InDeltaSeconds, InMaxPhysicsDelta);
	FillTeleportMode(InComponent, InDeltaSeconds, InMaxPhysicsDelta);
	FillMaxDistanceScale(InComponent);

	PredictedLod = InComponent->GetPredictedLODLevel();
}

void FClothingSimulationContextCommon::FillBoneTransforms(const USkeletalMeshComponent* InComponent)
{
	const USkeletalMesh* const SkeletalMesh = InComponent->GetSkeletalMeshAsset();

	if (USkinnedMeshComponent* const LeaderComponent = InComponent->LeaderPoseComponent.Get())
	{
		const TArray<int32>& LeaderBoneMap = InComponent->GetLeaderBoneMap();
		int32 NumBones = LeaderBoneMap.Num();

		if (NumBones == 0)
		{
			if (SkeletalMesh)
			{
				// This case indicates an invalid leader pose component (e.g. no skeletal mesh)
				NumBones = SkeletalMesh->GetRefSkeleton().GetNum();

				BoneTransforms.Empty(NumBones);
				BoneTransforms.AddDefaulted(NumBones);
			}
		}
		else
		{
			BoneTransforms.Reset(NumBones);
			BoneTransforms.AddDefaulted(NumBones);

			const TArray<FTransform>& LeaderTransforms = LeaderComponent->GetComponentSpaceTransforms();
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				bool bFoundLeader = false;
				if (LeaderBoneMap.IsValidIndex(BoneIndex))
				{
					const int32 LeaderIndex = LeaderBoneMap[BoneIndex];
					if (LeaderIndex != INDEX_NONE && LeaderIndex < LeaderTransforms.Num())
					{
						BoneTransforms[BoneIndex] = LeaderTransforms[LeaderIndex];
						bFoundLeader = true;
					}
				}

				if (!bFoundLeader && SkeletalMesh)
				{
					const int32 ParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex);

					BoneTransforms[BoneIndex] =
						BoneTransforms.IsValidIndex(ParentIndex) && ParentIndex < BoneIndex ?
						BoneTransforms[ParentIndex] * SkeletalMesh->GetRefSkeleton().GetRefBonePose()[BoneIndex] :
						SkeletalMesh->GetRefSkeleton().GetRefBonePose()[BoneIndex];
				}
			}
		}
	}
	else
	{
		BoneTransforms = InComponent->GetComponentSpaceTransforms();
	}
}

void FClothingSimulationContextCommon::FillRefToLocals(const USkeletalMeshComponent* InComponent, bool bIsInitialization)
{
	RefToLocals.Reset();

	// Constraints are initialized using bone distances upon initialization, so fill out reference pose
	if (bIsInitialization)
	{
		const USkeletalMesh* const SkeletalMesh = InComponent->GetSkeletalMeshAsset();
		if (SkeletalMesh)
		{
			const int32 NumBones = SkeletalMesh->GetRefSkeleton().GetNum();
			RefToLocals.AddUninitialized(NumBones);
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				RefToLocals[BoneIndex] = FMatrix44f::Identity;
			}
		}
		return;
	}

	InComponent->GetCurrentRefToLocalMatrices(RefToLocals, InComponent->GetPredictedLODLevel());
}

void FClothingSimulationContextCommon::FillComponentToWorld(const USkeletalMeshComponent* InComponent)
{
	ComponentToWorld = InComponent->GetComponentTransform();
}

void FClothingSimulationContextCommon::FillWorldGravity(const USkeletalMeshComponent* InComponent)
{
	const UWorld* const ComponentWorld = InComponent->GetWorld();
	check(ComponentWorld);
	WorldGravity = FVector(0.f, 0.f, ComponentWorld->GetGravityZ());
}

void FClothingSimulationContextCommon::FillWindVelocity(const USkeletalMeshComponent* InComponent)
{
	InComponent->GetWindForCloth_GameThread(WindVelocity, WindAdaption);
}

void FClothingSimulationContextCommon::FillDeltaSeconds(float InDeltaSeconds, float InMaxPhysicsDelta)
{
	DeltaSeconds = FMath::Min(InDeltaSeconds, InMaxPhysicsDelta);
}

void FClothingSimulationContextCommon::FillTeleportMode(const USkeletalMeshComponent* InComponent, float InDeltaSeconds, float InMaxPhysicsDelta)
{
	TeleportMode = (InDeltaSeconds > InMaxPhysicsDelta * GClothMaxDeltaTimeTeleportMultiplier.GetValueOnGameThread()) ?
		EClothingTeleportMode::Teleport :
		InComponent->ClothTeleportMode;

	VelocityScale = (TeleportMode == EClothingTeleportMode::None) ?
		FMath::Min(InDeltaSeconds, InMaxPhysicsDelta) / InDeltaSeconds : 1.f;
}

void FClothingSimulationContextCommon::FillMaxDistanceScale(const USkeletalMeshComponent* InComponent)
{
	MaxDistanceScale = InComponent->GetClothMaxDistanceScale();
}

//==============================================================================
// FClothingSimulationCommon
//==============================================================================

FClothingSimulationCommon::FClothingSimulationCommon()
{
	MaxPhysicsDelta = UPhysicsSettings::Get()->MaxPhysicsDeltaTime;
}

FClothingSimulationCommon::~FClothingSimulationCommon()
{}

void FClothingSimulationCommon::FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext)
{
	// Deprecated version always fills RefToLocals with current animation results instead of using reference pose on initialization
	const bool bIsInitialization = false;
	FillContext(InComponent, InDeltaTime, InOutContext, bIsInitialization);
}

void FClothingSimulationCommon::FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext, bool bIsInitialization)
{
	check(InOutContext);
	FClothingSimulationContextCommon* const Context = static_cast<FClothingSimulationContextCommon*>(InOutContext);

	Context->Fill(InComponent, InDeltaTime, MaxPhysicsDelta, bIsInitialization);

	// Checking the component here to track rare issue leading to invalid contexts
	if (!IsValid(InComponent))
	{
		const AActor* const CompOwner = InComponent->GetOwner();
		UE_LOG(LogSkeletalMesh, Warning, 
			TEXT("Attempting to fill a clothing simulation context for a PendingKill skeletal mesh component (Comp: %s, Actor: %s). "
				"Pending kill skeletal mesh components should be unregistered before marked pending kill."), 
			*InComponent->GetName(), CompOwner ? *CompOwner->GetName() : TEXT("None"));

		// Make sure we clear this out to skip any attempted simulations
		Context->BoneTransforms.Reset();
	}

	if (Context->BoneTransforms.Num() == 0)
	{
		const AActor* const CompOwner = InComponent->GetOwner();
		const USkinnedMeshComponent* const Leader = InComponent->LeaderPoseComponent.Get();
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Attempting to fill a clothing simulation context for a skeletal mesh component that has zero bones (Comp: %s, Leader: %s, Actor: %s)."), *InComponent->GetName(), Leader ? *Leader->GetName() : TEXT("None"), CompOwner ? *CompOwner->GetName() : TEXT("None"));

		// Make sure we clear this out to skip any attempted simulations
		Context->BoneTransforms.Reset();
	}
}
