// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodySetupEnums.h"
#include "Misc/ConfigCacheIni.h"
#include "ClothingSimulationInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "ClothCollisionSource.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "SkeletalMeshSceneProxy.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "AnimationRuntime.h"
#include "ClothCollisionData.h"
#include "ClothingSimulationInteractor.h"
#include "Rendering/RenderCommandPipes.h"

#include "Logging/MessageLog.h"
#include "CollisionDebugDrawingPublic.h"


#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/TaperedCapsuleElem.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "RenderingThread.h"
#include "SceneInterface.h"

#if WITH_EDITOR
#include "ClothingSystemEditorInterfaceModule.h"
#include "PhysicsEngine/SphereElem.h"
#include "SimulationEditorExtender.h"
#endif

#include "Chaos/Capsule.h"
#include "Chaos/Convex.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/PhysicsObjectInterface.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshComponentPhysics"

DECLARE_CYCLE_STAT(TEXT("CreateClothing"), STAT_CreateClothing, STATGROUP_Physics);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

TAutoConsoleVariable<int32> CVarEnableClothPhysics(TEXT("p.ClothPhysics"), 1, TEXT("If 1, physics cloth will be used for simulation."), ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarEnableClothPhysicsUseTaskThread(TEXT("p.ClothPhysics.UseTaskThread"), 1, TEXT("If 1, run cloth on the task thread. If 0, run on game thread."));
static TAutoConsoleVariable<int32> CVarClothPhysicsTickWaitForParallelClothTask(TEXT("p.ClothPhysics.WaitForParallelClothTask"), 0, TEXT("If 1, always wait for cloth task completion in the Cloth Tick function. If 0, wait at end-of-frame updates instead if allowed by component settings"));

static TAutoConsoleVariable<bool> CVarClothTeleportOverride(TEXT("p.Cloth.TeleportOverride"), false, TEXT("Force console variable teleport override values over skeletal mesh properties.\n Default: false."));
static TAutoConsoleVariable<bool> CVarClothResetAfterTeleport(TEXT("p.Cloth.ResetAfterTeleport"), true, TEXT("Require p.Cloth.TeleportOverride. Reset the clothing after moving the clothing position (called teleport).\n Default: true."));
static TAutoConsoleVariable<float> CVarClothTeleportDistanceThreshold(TEXT("p.Cloth.TeleportDistanceThreshold"), 300.f, TEXT("Require p.Cloth.TeleportOverride. Conduct teleportation if the character's movement is greater than this threshold in 1 frame.\n Zero or negative values will skip the check.\n Default: 300."));
static TAutoConsoleVariable<float> CVarClothTeleportRotationThreshold(TEXT("p.Cloth.TeleportRotationThreshold"), 0.f, TEXT("Require p.Cloth.TeleportOverride. Rotation threshold in degrees, ranging from 0 to 180.\n Conduct teleportation if the character's rotation is greater than this threshold in 1 frame.\n Zero or negative values will skip the check.\n Default 0."));

static TAutoConsoleVariable<int32> CVarEnableKinematicDeferralPrePhysicsCondition(TEXT("p.EnableKinematicDeferralPrePhysicsCondition"), 1, TEXT("If is 1, and deferral would've been disallowed due to EUpdateTransformFlags, allow if in PrePhysics tick. If 0, condition is unchanged."));

//This is the total cloth time split up among multiple computation (updating gpu, updating sim, etc...)
DECLARE_CYCLE_STAT(TEXT("Cloth Total"), STAT_ClothTotalTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Cloth Writeback"), STAT_ClothWriteback, STATGROUP_Physics);

// Used as a default return value for invalid cloth data access
static const TMap<int32, FClothSimulData> SEmptyClothSimulationData;


void FSkeletalMeshComponentClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FSkeletalMeshComponentClothTickFunction_ExecuteTick);
#if WITH_EDITOR
	FActorComponentTickFunction::ExecuteTickHelper(Target, Target->bUpdateClothInEditor, DeltaTime, TickType, [this](float DilatedTime)
#else
	FActorComponentTickFunction::ExecuteTickHelper(Target,true, DeltaTime, TickType, [this](float DilatedTime)
#endif
	{
		Target->TickClothing(DilatedTime, *this);
	});
}

FString FSkeletalMeshComponentClothTickFunction::DiagnosticMessage()
{
	if (Target)
	{
		return Target->GetFullName() + TEXT("[ClothTick]");
	}
	return TEXT("<NULL>[ClothTick]");
}

FName FSkeletalMeshComponentClothTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("SkeletalMeshComponentClothTick"));
}

void FSkeletalMeshComponentEndPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FSkeletalMeshComponentEndPhysicsTickFunction_ExecuteTick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Physics);

	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->EndPhysicsTickComponent(*this);
	});
}

FString FSkeletalMeshComponentEndPhysicsTickFunction::DiagnosticMessage()
{
	if (Target)
	{
		return Target->GetFullName() + TEXT("[EndPhysicsTick]");
	}
	return TEXT("<NULL>[EndPhysicsTick]");
}

FName FSkeletalMeshComponentEndPhysicsTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("SkeletalMeshComponentEndPhysicsTick"));
}

FClothCollisionSource::FClothCollisionSource(USkeletalMeshComponent* InSourceComponent, UPhysicsAsset* InSourcePhysicsAsset, const FOnBoneTransformsFinalizedMultiCast::FDelegate& InOnBoneTransformsFinalizedDelegate)
	: SourceComponent(InSourceComponent)
	, SourcePhysicsAsset(InSourcePhysicsAsset)
	, bCached(false)
{
	if (SourceComponent.IsValid())
	{
		OnBoneTransformsFinalizedHandle = InSourceComponent->RegisterOnBoneTransformsFinalizedDelegate(InOnBoneTransformsFinalizedDelegate);
	}
}

FClothCollisionSource::~FClothCollisionSource()
{
	if (SourceComponent.IsValid() && OnBoneTransformsFinalizedHandle.IsValid())
	{
		SourceComponent->UnregisterOnBoneTransformsFinalizedDelegate(OnBoneTransformsFinalizedHandle);
	}
}

void USkeletalMeshComponent::CreateBodySetup()
{
	if (BodySetup == NULL)
	{
		BodySetup = NewObject<UBodySetup>(this);
	}

	if (GetSkeletalMeshAsset())
	{
		const USkeletalMesh* SkeletalMeshConst = GetSkeletalMeshAsset();
		GetSkeletalMeshAsset()->CreateBodySetup();
		UBodySetup* OriginalBodySetup = SkeletalMeshConst->GetBodySetup();
		BodySetup->CopyBodyPropertiesFrom(OriginalBodySetup);
		BodySetup->CookedFormatDataOverride = &OriginalBodySetup->CookedFormatData;
	}

	BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

	//need to recreate meshes
	BodySetup->ClearPhysicsMeshes();
	BodySetup->CreatePhysicsMeshes();
}

//
//	USkeletalMeshComponent
//
UBodySetup* USkeletalMeshComponent::GetBodySetup()
{
	if (bEnablePerPolyCollision == false)
	{
		UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
		if (GetSkeletalMeshAsset() && PhysicsAsset)
		{
			for (int32 i = 0; i < GetSkeletalMeshAsset()->GetRefSkeleton().GetNum(); i++)
			{
				int32 BodyIndex = PhysicsAsset->FindBodyIndex(GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(i));
				if (BodyIndex != INDEX_NONE)
				{
					return PhysicsAsset->SkeletalBodySetups[BodyIndex];
				}
			}
		}
	}
	else
	{
		if (BodySetup == NULL)
		{
			CreateBodySetup();
		}

		return BodySetup;
	}


	return NULL;
}

bool USkeletalMeshComponent::CanEditSimulatePhysics()
{
	return GetPhysicsAsset() != nullptr;
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bSimulate)
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	BodyInstance.bSimulatePhysics = bSimulate;

	// enable blending physics
	bBlendPhysics = bSimulate;

	//Go through body setups and see which bodies should be turned on and off
	if (UPhysicsAsset * PhysAsset = GetPhysicsAsset())
	{
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (FBodyInstance* BodyInst = Bodies[BodyIdx])
			{
				if (UBodySetup * PhysAssetBodySetup = PhysAsset->SkeletalBodySetups[BodyIdx])
				{
					if (PhysAssetBodySetup->PhysicsType == EPhysicsType::PhysType_Default)
					{
						BodyInst->SetInstanceSimulatePhysics(bSimulate, false, true);
					}
				}
			}
		}
	}

	if(IsSimulatingPhysics())
	{
		SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint
	}

 	UpdateEndPhysicsTickRegisteredState();
 	UpdateClothTickRegisteredState();
}

void USkeletalMeshComponent::OnComponentCollisionSettingsChanged(bool bUpdateOverlaps)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		if (ensure(Bodies[i]))
		{
			Bodies[i]->UpdatePhysicsFilterData();
		}
	}

	if (SceneProxy)
	{
		((FSkeletalMeshSceneProxy*)SceneProxy)->SetCollisionEnabled_GameThread(IsCollisionEnabled());
	}

	Super::OnComponentCollisionSettingsChanged(bUpdateOverlaps);
}

void USkeletalMeshComponent::AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

	PendingRadialForces.Emplace(Origin, Radius, Strength, Falloff, bVelChange, FPendingRadialForces::EType::AddImpulse);
	
	const float StrengthPerMass = Strength / FMath::Max(GetMass(), UE_KINDA_SMALL_NUMBER);
	for(FBodyInstance* Body : Bodies)
	{
		const float StrengthPerBody = bVelChange ? Strength : (StrengthPerMass * Body->GetBodyMass());
		Body->AddRadialImpulseToBody(Origin, Radius, StrengthPerBody, Falloff, bVelChange);
	}
}



void USkeletalMeshComponent::AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bAccelChange)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

	PendingRadialForces.Emplace(Origin, Radius, Strength, Falloff, bAccelChange, FPendingRadialForces::EType::AddForce);

	const float StrengthPerMass = Strength / FMath::Max(GetMass(), UE_KINDA_SMALL_NUMBER);
	for (FBodyInstance* Body : Bodies)
	{
		const float StrengthPerBody = bAccelChange ? Strength : (StrengthPerMass * Body->GetBodyMass());
		Body->AddRadialForceToBody(Origin, Radius, StrengthPerBody, Falloff, bAccelChange);
	}
}

void USkeletalMeshComponent::WakeAllRigidBodies()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->WakeInstance();
	}
}

void USkeletalMeshComponent::PutAllRigidBodiesToSleep()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->PutInstanceToSleep();
	}
}


bool USkeletalMeshComponent::IsAnyRigidBodyAwake()
{
	bool bAwake = false;

	// ..iterate over each body to find any that are awak
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		if(BI->IsInstanceAwake())
		{
			// Found an awake one - so mesh is considered 'awake'
			bAwake = true;
			break;
		}
	}

	return bAwake;
}


void USkeletalMeshComponent::SetAllPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent)
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BodyInst = Bodies[i];
		check(BodyInst);
		BodyInst->SetLinearVelocity(NewVel, bAddToCurrent);
	}
}

void USkeletalMeshComponent::SetAllPhysicsAngularVelocityInRadians(FVector const& NewAngVel, bool bAddToCurrent)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// Find the root actor. We use its location as the center of the rotation.
		FBodyInstance* RootBodyInst = Bodies[ RootBodyData.BodyIndex ];
		check(RootBodyInst);
		FTransform RootTM = RootBodyInst->GetUnrealWorldTransform();

		FVector RootPos = RootTM.GetLocation();

		// Iterate over each bone, updating its velocity
		for (int32 i = 0; i < Bodies.Num(); i++)
		{
			FBodyInstance* const BI = Bodies[i];
			check(BI);

			BI->SetAngularVelocityInRadians(NewAngVel, bAddToCurrent);
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsPosition(FVector NewPos)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewPos
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FVector DeltaLoc = NewPos - RootBodyTM.GetLocation();
			RootBodyTM.SetTranslation(NewPos);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

#if DO_CHECK
			FVector RelativeVector = (RootBI->GetUnrealWorldTransform().GetLocation() - NewPos);
			check(RelativeVector.SizeSquared() < 1.f);
#endif

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetTranslation(BodyTM.GetTranslation() + DeltaLoc);
					BI->SetBodyTransform(BodyTM, ETeleportType::TeleportPhysics);
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsRotation(FRotator NewRot)
{
	SetAllPhysicsRotation(NewRot.Quaternion());
}

void USkeletalMeshComponent::SetAllPhysicsRotation(const FQuat& NewRot)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRot.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("USkeletalMeshComponent::SetAllPhysicsRotation found NaN in parameter NewRot: %s"), *NewRot.ToString());
	}
#endif
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewRot
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FQuat DeltaQuat = RootBodyTM.GetRotation().Inverse() * NewRot;
			RootBodyTM.SetRotation(NewRot);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetRotation(BodyTM.GetRotation() * DeltaQuat);
					BI->SetBodyTransform( BodyTM, ETeleportType::TeleportPhysics );
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::ApplyDeltaToAllPhysicsTransforms(const FVector& DeltaLocation, const FQuat& DeltaRotation)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewRot
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			RootBodyTM.SetRotation(DeltaRotation * RootBodyTM.GetRotation());
			RootBodyTM.SetTranslation(RootBodyTM.GetTranslation() + DeltaLocation);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetRotation(DeltaRotation * BodyTM.GetRotation());
					BodyTM.SetTranslation(BodyTM.GetTranslation() + DeltaLocation);
					BI->SetBodyTransform( BodyTM, ETeleportType::TeleportPhysics );
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
	// Single-body case - just use PrimComp code.
	UPrimitiveComponent::SetPhysMaterialOverride(NewPhysMaterial);

	// Now update any child bodies
	for( int32 i = 0; i < Bodies.Num(); i++ )
	{
		FBodyInstance* BI = Bodies[i];
		BI->UpdatePhysicalMaterials();
	}
}

void USkeletalMeshComponent::SetEnableGravity(bool bGravityEnabled)
{
	if (!bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer())
	{
		return;
	}

	BodyInstance.bEnableGravity = bGravityEnabled;

	if (UPhysicsAsset * PhysAsset = GetPhysicsAsset())
	{
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (FBodyInstance* BodyInst = Bodies[BodyIdx])
			{
				if (UBodySetup * PhysAssetBodySetup = PhysAsset->SkeletalBodySetups[BodyIdx])
				{
					bool bUseGravityEnabled = bGravityEnabled;
					
					//If the default body instance has gravity turned off then turning it ON for skeletal mesh component does not turn the instance on
					if(bUseGravityEnabled && !PhysAssetBodySetup->DefaultInstance.bEnableGravity)	
					{
						bUseGravityEnabled = false;
					}
				
					BodyInst->SetEnableGravity(bUseGravityEnabled);
				}
			}
		}
	}
}

bool USkeletalMeshComponent::IsGravityEnabled() const
{
	return BodyInstance.bEnableGravity;
}

void USkeletalMeshComponent::OnConstraintBrokenWrapper(int32 ConstraintIndex)
{
	OnConstraintBroken.Broadcast(ConstraintIndex);
}

void USkeletalMeshComponent::OnPlasticDeformationWrapper(int32 ConstraintIndex)
{
	OnPlasticDeformation.Broadcast(ConstraintIndex);
}

void USkeletalMeshComponent::InitCollisionRelationships()
{
	if (UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		int32 NumDisabledCollisions = PhysicsAsset->CollisionDisableTable.Num();
		if (NumDisabledCollisions)
		{
			TMap<FPhysicsActorHandle, TArray< FPhysicsActorHandle > > DisabledCollisions;
			for (auto& Elem : PhysicsAsset->CollisionDisableTable)
			{		
				// @question : PhysicsAsset->CollisionDisableTable should contain direct indices into the Bodies list?
				//             I saw some OOB errors in a baked build that seemed to indicate that is not the case.

				int32 SourceIndex = Elem.Key.Indices[0];
				int32 TargetIndex = Elem.Key.Indices[1];
				bool bDoCollide = !Elem.Value;
				if (0 <= SourceIndex && SourceIndex < Bodies.Num())
				{
					if (auto* SourceBody = Bodies[SourceIndex])
					{
						if (0 <= TargetIndex && TargetIndex < Bodies.Num())
						{
							if (auto* TargetBody = Bodies[TargetIndex])
							{
								if (FPhysicsActorHandle SourceHandle = SourceBody->GetPhysicsActorHandle())
								{
									if (FPhysicsActorHandle TargetHandle = TargetBody->GetPhysicsActorHandle())
									{
										if (!DisabledCollisions.Contains(SourceHandle))
										{
											DisabledCollisions.Add(SourceHandle, TArray<FPhysicsActorHandle>());
											DisabledCollisions[SourceHandle].Reserve(NumDisabledCollisions);
										}

										checkSlow(!DisabledCollisions[SourceHandle].Contains(TargetHandle));
										DisabledCollisions[SourceHandle].Add(TargetHandle);
									}
								}
							}
						}
					}
				}
			}
			FPhysicsCommand::ExecuteWrite(this, [&]()
				{
					FChaosEngineInterface::AddDisabledCollisionsFor_AssumesLocked(DisabledCollisions);
				});
		}
	}
}

void USkeletalMeshComponent::TermCollisionRelationships()
{
	if (UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		int32 NumDisabledCollisions = PhysicsAsset->CollisionDisableTable.Num();
		if (NumDisabledCollisions)
		{
			TArray< FPhysicsActorHandle > CollisionRelationships;
			CollisionRelationships.Reserve(Bodies.Num());

			for (auto& Body : Bodies)
			{
				if (Body)
				{
					if (FPhysicsActorHandle Handle = Body->GetPhysicsActorHandle())
					{
						CollisionRelationships.Add(Handle);
					}
				}
			}

			FPhysicsCommand::ExecuteWrite(this, [&]()
			{
				FChaosEngineInterface::RemoveDisabledCollisionsFor_AssumesLocked(CollisionRelationships);
			});
		}
	}
}


DECLARE_CYCLE_STAT(TEXT("Init Articulated"), STAT_InitArticulated, STATGROUP_Physics);

int32 USkeletalMeshComponent::FindRootBodyIndex() const
{
	// Find root physics body
	int32 RootBodyIndex = RootBodyData.BodyIndex;
	if(RootBodyIndex == INDEX_NONE && GetSkeletalMeshAsset())
	{
		if(const UPhysicsAsset* PhysicsAsset = GetPhysicsAsset())
		{
			for (int32 i = 0; i< GetSkeletalMeshAsset()->GetRefSkeleton().GetNum(); i++)
			{
				int32 BodyInstIndex = PhysicsAsset->FindBodyIndex(GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(i));
				if (BodyInstIndex != INDEX_NONE)
				{
					RootBodyIndex = BodyInstIndex;
					break;
				}
			}
		}
	}

	return RootBodyIndex;
}

static int32 bAllowNotForDedServerPhysicsAssets = 1;
static FAutoConsoleVariableRef CVarAllowNotForDedServerPhysicsAssets(
	TEXT("p.AllowNotForDedServerPhysicsAssets"),
	bAllowNotForDedServerPhysicsAssets,
	TEXT("Allow 'Not For Dedicated Server' flag on PhysicsAssets\n")
	TEXT("0: ignore flag, 1: obey flag (default)"),
	ECVF_Default);

void USkeletalMeshComponent::InitArticulated(FPhysScene* PhysScene)
{
	SCOPE_CYCLE_COUNTER(STAT_InitArticulated);

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	if (PhysScene == nullptr || PhysicsAsset == nullptr || GetSkeletalMeshAsset() == nullptr || !ShouldCreatePhysicsState())
	{
		return;
	}

	if (Bodies.Num() > 0)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Bodies already created (%s) - call TermArticulated first."), *GetPathName());
		return;
	}

	// Skip if not desired on dedicated server
	UWorld* World = GetWorld();
	if (PhysicsAsset->bNotForDedicatedServer && World && (World->GetNetMode() == NM_DedicatedServer) && bAllowNotForDedServerPhysicsAssets)
	{
		UE_LOG(LogSkeletalMesh, Verbose, TEXT("Skipping PhysicsAsset creation on dedicated server (%s : %s) %s"), *GetNameSafe(GetOuter()), *GetName(), *PhysicsAsset->GetName());
		return;
	}

	FVector Scale3D = GetComponentTransform().GetScale3D();

	// Find root physics body
	RootBodyData.BodyIndex = INDEX_NONE;	//Reset the root body index just in case we need to refind a new one
	const int32 RootBodyIndex = FindRootBodyIndex();

	if (RootBodyIndex == INDEX_NONE)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Could not find root physics body: '%s'"), *GetPathName());
		return;
	}

	// Set up the map from skelmeshcomp ID to collision disable table
	int32 NumShapes = 0;
	const int32 NumBodies = PhysicsAsset->SkeletalBodySetups.Num();
	for (int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
	{
		if (PhysicsAsset->SkeletalBodySetups[BodyIndex])
		{
			NumShapes += PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom.GetElementCount();
		}
	}

	if (!Aggregate.IsValid() && NumShapes > RagdollAggregateThreshold&& NumShapes <= AggregateMaxSize)
	{
		Aggregate = FPhysicsInterface::CreateAggregate(PhysicsAsset->SkeletalBodySetups.Num());
	}
	else if (Aggregate.IsValid() && NumShapes > AggregateMaxSize)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Too many shapes to create aggregate, Max: %u, This: %d"), AggregateMaxSize, NumShapes);
	}

	InstantiatePhysicsAsset(*PhysicsAsset, Scale3D, Bodies, Constraints, PhysScene, this, RootBodyIndex, Aggregate);
	for (int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
	{
		if (FBodyInstance* Body = Bodies[BodyIndex])
		{
			if (!Body->ActorHandle)
			{
				continue;
			}

			Chaos::FPhysicsObject* PhysicsObject = Body->ActorHandle->GetPhysicsObject();
			if (UBodySetup* Setup = Body->GetBodySetup())
			{
				Chaos::FPhysicsObjectInterface::SetName(PhysicsObject, Setup->BoneName);
			}
			Chaos::FPhysicsObjectInterface::SetId(PhysicsObject, BodyIndex);
		}
	}

	// now update root body index because body has BodySetup now
	SetRootBodyIndex(RootBodyIndex);

	InitCollisionRelationships();

	// Update Flag
	PrevRootBoneMatrix = GetBoneMatrix(0); // save the root bone transform

	// pre-compute cloth teleport thresholds for performance
	ComputeTeleportDistanceThresholdInRadians();
	ComputeTeleportRotationThresholdInRadians();
}

TAutoConsoleVariable<int32> CVarEnableRagdollPhysics(TEXT("p.RagdollPhysics"), 1, TEXT("If 1, ragdoll physics will be used. Otherwise just root body is simulated"));

void USkeletalMeshComponent::InstantiatePhysicsAsset(const UPhysicsAsset& PhysAsset, const FVector& Scale3D, TArray<FBodyInstance*>& OutBodies, TArray<FConstraintInstance*>& OutConstraints, FPhysScene* PhysScene, USkeletalMeshComponent* OwningComponent, int32 UseRootBodyIndex, const FPhysicsAggregateHandle& UseAggregate) const
{
	auto BoneTMCallable = [this](int32 BoneIndex)
	{
		return GetBoneTransform(BoneIndex);
	};

	InstantiatePhysicsAsset_Internal(PhysAsset, Scale3D, OutBodies, OutConstraints, BoneTMCallable, PhysScene, OwningComponent, UseRootBodyIndex, UseAggregate);
}

void USkeletalMeshComponent::InstantiatePhysicsAssetBodies(const UPhysicsAsset& PhysAsset, TArray<FBodyInstance*>& OutBodies, FPhysScene* PhysScene /*= nullptr*/, USkeletalMeshComponent* OwningComponent /*= nullptr*/, int32 UseRootBodyIndex /*= INDEX_NONE*/, const FPhysicsAggregateHandle& UseAggregate /*= FPhysicsAggregateHandle()*/) const
{
	auto BoneTMCallable = [this](int32 BoneIndex)
	{
		return GetBoneTransform(BoneIndex);
	};

	InstantiatePhysicsAssetBodies_Internal(PhysAsset, OutBodies, BoneTMCallable, nullptr, PhysScene, OwningComponent, UseRootBodyIndex, UseAggregate);
}

void USkeletalMeshComponent::InstantiatePhysicsAssetRefPose(const UPhysicsAsset& PhysAsset, const FVector& Scale3D, TArray<FBodyInstance*>& OutBodies, TArray<FConstraintInstance*>& OutConstraints, FPhysScene* PhysScene /*= nullptr*/, USkeletalMeshComponent* OwningComponent /*= nullptr*/, int32 UseRootBodyIndex /*= INDEX_NONE*/, const FPhysicsAggregateHandle& UseAggregate, bool bCreateBodiesInRefPose) const
{
	if(GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkeleton = GetSkeletalMeshAsset()->GetRefSkeleton();

		if (bCreateBodiesInRefPose)
		{
			// Create the bodies posed in component-space with the ref pose
			TArray<FTransform> CachedTransforms;
			TArray<bool> CachedTransformReady;
			CachedTransforms.SetNumUninitialized(RefSkeleton.GetRefBonePose().Num());
			CachedTransformReady.SetNumZeroed(RefSkeleton.GetRefBonePose().Num());

			auto BoneTMCallable = [&](int32 BoneIndex)
			{
				return FAnimationRuntime::GetComponentSpaceTransformWithCache(RefSkeleton, RefSkeleton.GetRefBonePose(), BoneIndex, CachedTransforms, CachedTransformReady);
			};

			InstantiatePhysicsAsset_Internal(PhysAsset, Scale3D, OutBodies, OutConstraints, BoneTMCallable, PhysScene, OwningComponent, UseRootBodyIndex, UseAggregate);
		}
		else
		{
			// Create the bodies with each body in its local ref pose (all bodies will be on top of each other close to the origin, but should still have the correct scale)
			auto BoneTMCallable = [&](int32 BoneIndex)
			{
				if (RefSkeleton.IsValidIndex(BoneIndex))
				{
					return RefSkeleton.GetRefBonePose()[BoneIndex];
				}
				return FTransform::Identity;
			};;

			InstantiatePhysicsAsset_Internal(PhysAsset, Scale3D, OutBodies, OutConstraints, BoneTMCallable, PhysScene, OwningComponent, UseRootBodyIndex, UseAggregate);
		}
	}
}

void USkeletalMeshComponent::InstantiatePhysicsAssetBodies_Internal(const UPhysicsAsset& PhysAsset, TArray<FBodyInstance*>& OutBodies, TFunctionRef<FTransform(int32)> BoneTransformGetter, TMap<FName, FBodyInstance*>* OutNameToBodyMap, FPhysScene* PhysScene /*= nullptr*/, USkeletalMeshComponent* OwningComponent /*= nullptr*/, int32 UseRootBodyIndex /*= INDEX_NONE*/, const FPhysicsAggregateHandle& UseAggregate) const
{
	const int32 NumOutBodies = PhysAsset.SkeletalBodySetups.Num();

	// Create all the OutBodies.
	check(OutBodies.Num() == 0);
	OutBodies.AddZeroed(NumOutBodies);
	
	if (OutNameToBodyMap)
	{
		OutNameToBodyMap->Reserve(NumOutBodies);
	}

	for(int32 BodyIdx = 0; BodyIdx < NumOutBodies; BodyIdx++)
	{
		UBodySetup* PhysicsAssetBodySetup = PhysAsset.SkeletalBodySetups[BodyIdx];
		if (!ensure(PhysicsAssetBodySetup))
		{
			continue;
		}
		OutBodies[BodyIdx] = new FBodyInstance;
		FBodyInstance* BodyInst = OutBodies[BodyIdx];
		check(BodyInst);

		// Get transform of bone by name.
		int32 BoneIndex = GetBoneIndex(PhysicsAssetBodySetup->BoneName);
		if(BoneIndex != INDEX_NONE)
		{
			// Copy body setup default instance properties
			BodyInst->CopyBodyInstancePropertiesFrom(&PhysicsAssetBodySetup->DefaultInstance);
			// we don't allow them to use this in editor. For physics asset, this set up is overridden by Physics Type. 
			// but before we hide in the detail customization, we saved with this being true, causing the simulate always happens for some OutBodies
			// so adding initialization here to disable this. 
			// to check, please check BodySetupDetails.cpp, if (ChildProperty->GetProperty()->GetName() == TEXT("bSimulatePhysics"))
			// we hide this property, so it should always be false initially. 
			// this is not true for all other BodyInstance, but for physics assets it is true. 
			BodyInst->bSimulatePhysics = false;
			BodyInst->InstanceBodyIndex = BodyIdx; // Set body index 
			BodyInst->InstanceBoneIndex = BoneIndex; // Set bone index

			BodyInst->bStartAwake = UseRootBodyIndex >= 0 ? BodyInstance.bStartAwake : true;	//We don't allow customization here. Just use whatever the component is set to

			if(BodyIdx == UseRootBodyIndex)
			{
				BodyInst->DOFMode = BodyInstance.DOFMode;
				BodyInst->CustomDOFPlaneNormal = BodyInstance.CustomDOFPlaneNormal;
				BodyInst->bLockXTranslation = BodyInstance.bLockXTranslation;
				BodyInst->bLockYTranslation = BodyInstance.bLockYTranslation;
				BodyInst->bLockZTranslation = BodyInstance.bLockZTranslation;
				BodyInst->bLockXRotation = BodyInstance.bLockXRotation;
				BodyInst->bLockYRotation = BodyInstance.bLockYRotation;
				BodyInst->bLockZRotation = BodyInstance.bLockZRotation;
				BodyInst->bLockTranslation = BodyInstance.bLockTranslation;
				BodyInst->bLockRotation = BodyInstance.bLockRotation;

				BodyInst->COMNudge = BodyInstance.COMNudge;
			}
			else
			{
				BodyInst->DOFMode = EDOFMode::None;
				if(PhysScene != nullptr && !CVarEnableRagdollPhysics.GetValueOnGameThread())	//We only limit creation of the global physx scenes and not assets related to immedate mode
				{
					continue;
				}
			}

			// Create physics body instance.
			FTransform BoneTransform = BoneTransformGetter(BoneIndex);

			FInitBodySpawnParams SpawnParams(OwningComponent);

			if(OwningComponent == nullptr)
			{
				//special case where we don't use the skel mesh, but we still want to do certain logic like skeletal mesh
				SpawnParams.bStaticPhysics = false;
				SpawnParams.bPhysicsTypeDeterminesSimulation = true;
			}

			SpawnParams.Aggregate = UseAggregate;
			BodyInst->InitBody(PhysicsAssetBodySetup, BoneTransform, OwningComponent, PhysScene, SpawnParams);

			if (OutNameToBodyMap)
			{
				OutNameToBodyMap->Add(PhysicsAssetBodySetup->BoneName, BodyInst);
			}
		}
	}
}

void USkeletalMeshComponent::InstantiatePhysicsAsset_Internal(const UPhysicsAsset& PhysAsset, const FVector& Scale3D, TArray<FBodyInstance*>& OutBodies, TArray<FConstraintInstance*>& OutConstraints, TFunctionRef<FTransform(int32)> BoneTransformGetter, FPhysScene* PhysScene /*= nullptr*/, USkeletalMeshComponent* OwningComponent /*= nullptr*/, int32 UseRootBodyIndex /*= INDEX_NONE*/, const FPhysicsAggregateHandle& UseAggregate) const
{
	const float ActualScale = Scale3D.GetAbsMin();
	const float Scale = ActualScale == 0.f ? UE_KINDA_SMALL_NUMBER : ActualScale;

	TMap<FName, FBodyInstance*> NameToBodyMap;

	// Create all the OutBodies.
	InstantiatePhysicsAssetBodies_Internal(PhysAsset, OutBodies, BoneTransformGetter, &NameToBodyMap, PhysScene, OwningComponent, UseRootBodyIndex, UseAggregate);

	if(PhysScene && Aggregate.IsValid())
	{
		PhysScene->AddAggregateToScene(Aggregate);
	}
	
	// Create all the OutConstraints.
	check(OutConstraints.Num() == 0);
	int32 NumOutConstraints = PhysAsset.ConstraintSetup.Num();
	OutConstraints.AddZeroed(NumOutConstraints);
	for(int32 ConstraintIdx = 0; ConstraintIdx < NumOutConstraints; ConstraintIdx++)
	{
		const UPhysicsConstraintTemplate* OutConstraintsetup = PhysAsset.ConstraintSetup[ConstraintIdx];
		OutConstraints[ConstraintIdx] = new FConstraintInstance;
		FConstraintInstance* ConInst = OutConstraints[ConstraintIdx];
		check(ConInst);
		ConInst->CopyConstraintParamsFrom(&OutConstraintsetup->DefaultInstance);
		ConInst->ConstraintIndex = ConstraintIdx; // Set the ConstraintIndex property in the ConstraintInstance.
		ConInst->PhysScene = PhysScene;
#if WITH_EDITOR
		UWorld* World = GetWorld();
		if(World && World->IsGameWorld())
		{
			//In the editor we may be currently editing the physics asset, so make sure to use the default profile
			OutConstraintsetup->ApplyConstraintProfile(NAME_None, *ConInst, /*bDefaultIfNotFound=*/true);
		}
#endif

		// Get OutBodies we want to joint
		FBodyInstance* Body1 = NameToBodyMap.FindRef(ConInst->ConstraintBone1);
		FBodyInstance* Body2 = NameToBodyMap.FindRef(ConInst->ConstraintBone2);

		// If we have 2, joint 'em
		if(Body1 && Body2)
		{
			// Validates the body. OutBodies could be invalid due to outdated PhysAssets / bad constraint bone (or body) names.
			auto ValidateBody = [this](const FBodyInstance* InBody, const FName& InBoneName)
			{
				if(!InBody->IsValidBodyInstance())
				{
					// Disable log for now.
					// UE_LOG(LogSkeletalMesh, Warning, TEXT("USkeletalMeshComponent::InitArticulated : Unable to initialize constraint (%s) -  Body Invalid %s."), *(this->GetPathName()), *(InBoneName.ToString()));
					return false;
				}

				return true;
			};

			// Applies the adjusted / relative scale of the body instance.
			// Also, remove component scale as it will be reapplied in InitConstraint.
			// GetBoneTransform already accounts for component scale.
			auto ScalePosition = [](const FBodyInstance* InBody, const float InScale, FVector& OutPosition)
			{
				const FBodyInstance& DefaultBody = InBody->GetBodySetup()->DefaultInstance;
				const FVector ScaledDefaultBodyScale = DefaultBody.Scale3D * InScale;
				const FVector AdjustedBodyScale = InBody->Scale3D * ScaledDefaultBodyScale.Reciprocal();
				OutPosition *= AdjustedBodyScale;
			};

			// Do this separately so both are logged if invalid.
			const bool Body1Valid = ValidateBody(Body1, ConInst->ConstraintBone1);
			const bool Body2Valid = ValidateBody(Body2, ConInst->ConstraintBone2);

			if(Body1Valid && Body2Valid)
			{
				ScalePosition(Body1, Scale, ConInst->Pos1);
				ScalePosition(Body2, Scale, ConInst->Pos2);
				ConInst->InitConstraint(Body1, Body2, Scale, OwningComponent
					, OwningComponent ? FOnConstraintBroken::CreateUObject(OwningComponent, &USkeletalMeshComponent::OnConstraintBrokenWrapper) : FOnConstraintBroken()
					, OwningComponent ? FOnPlasticDeformation::CreateUObject(OwningComponent, &USkeletalMeshComponent::OnPlasticDeformationWrapper) : FOnPlasticDeformation());
			}
		}
	}
}


void USkeletalMeshComponent::TermArticulated()
{
	ResetRootBodyIndex();

	TermCollisionRelationships();

	FPhysicsCommand::ExecuteWrite(this, [&]()
	{
	// We shut down the physics for each body and constraint here. 
	// The actual UObjects will get GC'd

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		check( Constraints[i] );
		Constraints[i]->TermConstraint();
		delete Constraints[i];
	}

	Constraints.Empty();

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		if (ensure(Bodies[i]))
		{
			Bodies[i]->TermBody();
			delete Bodies[i];
		}
	}
	
	Bodies.Empty();

	// releasing Aggregate, it shouldn't contain any Bodies now, because they are released above
		if(Aggregate.IsValid())
	{
			check(FPhysicsInterface::GetNumActorsInAggregate(Aggregate) == 0);
			FPhysicsInterface::ReleaseAggregate(Aggregate);
	}

	});
}

void USkeletalMeshComponent::TermBodiesBelow(FName ParentBoneName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && GetSkeletalMeshAsset() && Bodies.Num() > 0)
	{
		check(Bodies.Num() == PhysicsAsset->SkeletalBodySetups.Num());

		// Get index of parent bone
		int32 ParentBoneIndex = GetBoneIndex(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			UE_LOG(LogSkeletalMesh, Log, TEXT("TermBodiesBelow: ParentBoneName '%s' is invalid"), *ParentBoneName.ToString());
			return;
		}

		// First terminate any constraints at below this bone
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			// Get bone index of constraint
			FName JointChildBoneName = Constraints[i]->GetChildBoneName();
			int32 JointBoneIndex = GetBoneIndex(JointChildBoneName);

			// If constraint has bone in mesh, and is either the parent or child of it, term it
			if(	JointBoneIndex != INDEX_NONE && (JointChildBoneName == ParentBoneName || GetSkeletalMeshAsset()->GetRefSkeleton().BoneIsChildOf(JointBoneIndex, ParentBoneIndex)) )
			{
				Constraints[i]->TermConstraint();
			}
		}

		// Then iterate over bodies looking for any which are children of supplied parent
		for(int32 i=0; i<Bodies.Num(); i++)
		{
			// Get bone index of body
			if (Bodies[i]->IsValidBodyInstance())
			{
				FName BodyName = Bodies[i]->BodySetup->BoneName;
				int32 BodyBoneIndex = GetBoneIndex(BodyName);

				// If body has bone in mesh, and is either the parent or child of it, term it
				if(	BodyBoneIndex != INDEX_NONE && (BodyName == ParentBoneName || GetSkeletalMeshAsset()->GetRefSkeleton().BoneIsChildOf(BodyBoneIndex, ParentBoneIndex)) )
				{
					Bodies[i]->TermBody();
				}
			}
		}
	}
}

float USkeletalMeshComponent::GetTotalMassBelowBone(FName InBoneName)
{
	float TotalBodyMass = 0.f;

	ForEachBodyBelow(InBoneName, /*bIncludeSelf=*/true, /*bSkipCustomPhysics=*/false, [&TotalBodyMass](FBodyInstance* BI)
	{
		TotalBodyMass += BI->GetBodyMass();
	});

	return TotalBodyMass;
}

void USkeletalMeshComponent::SetAllBodiesSimulatePhysics(bool bNewSimulate)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceSimulatePhysics(bNewSimulate, false, true);
	}

	SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();
}

void USkeletalMeshComponent::SetCollisionObjectType(ECollisionChannel NewChannel)
{
	SetAllBodiesCollisionObjectType(NewChannel);
}

void USkeletalMeshComponent::SetAllBodiesCollisionObjectType(ECollisionChannel NewChannel)
{
	BodyInstance.SetObjectType(NewChannel);	//children bodies use the skeletal mesh override so make sure root is set properly

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetObjectType(NewChannel);
	}
}

void USkeletalMeshComponent::SetAllBodiesNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	BodyInstance.SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision); //children bodies use the skeletal mesh override so make sure root is set properly

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	}
}

void USkeletalMeshComponent::SetAllBodiesBelowPhysicsDisabled(const FName& InBoneName, bool bDisabled, bool bIncludeSelf)
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, bIncludeSelf, /*bSkipCustomPhysicsType=*/ false, [bDisabled, this](FBodyInstance* BI)
	{
		BI->SetPhysicsDisabled(bDisabled);
		if (bDisabled == false)
		{
			FTransform BoneTransform(GetBoneMatrix(BI->InstanceBoneIndex));
			// if we re-enable it, let's make sure the body transform is up to date 
			BI->SetBodyTransform(BoneTransform, ETeleportType::TeleportPhysics, true);
		}
	});
}

void USkeletalMeshComponent::SetAllBodiesBelowLinearVelocity(const FName& InBoneName, const FVector& LinearVelocity, bool bIncludeSelf)
{
	if (const FBodyInstance* ParentBodyInstance = GetBodyInstance(InBoneName))
	{
		ForEachBodyBelow(InBoneName, bIncludeSelf, /*bSkipCustomPhysicsType=*/ false, [LinearVelocity, this](FBodyInstance* BI)
			{
				BI->SetLinearVelocity(LinearVelocity, false);
			});
	}
}

FVector USkeletalMeshComponent::GetBoneLinearVelocity(const FName& InBoneName)
{
	FVector OutVelocity = FVector::ZeroVector;

	if (const FBodyInstance* ParentBodyInstance = GetBodyInstance(InBoneName))
	{
		OutVelocity = ParentBodyInstance->GetUnrealWorldVelocity();
	}

	return OutVelocity;
}


void USkeletalMeshComponent::SetAllBodiesBelowSimulatePhysics( const FName& InBoneName, bool bNewSimulate, bool bIncludeSelf )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, bIncludeSelf, /*bSkipCustomPhysicsType=*/ false, [bNewSimulate](FBodyInstance* BI)
	{
		BI->SetInstanceSimulatePhysics(bNewSimulate, false, true);
	});

	if (NumBodiesFound)
	{
		if (IsSimulatingPhysics())
		{
			SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint
		}

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}

void USkeletalMeshComponent::SetBodySimulatePhysics(const FName& InBoneName, bool bSimulate)
{
	FBodyInstance* BI = GetBodyInstance(InBoneName);
	if (BI)
	{
		BI->SetInstanceSimulatePhysics(bSimulate, false, true);

		if (IsSimulatingPhysics())
		{
			SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint
		}

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}

void USkeletalMeshComponent::SetAllMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->GetChildBoneName());
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default)
			{
				continue;
			}
		}

		Constraints[i]->SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->GetChildBoneName()) )
		{
			Constraints[i]->SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetOrientationDriveTwistAndSwing(!bEnableTwistDrive, !bEnableSwingDrive);
		}
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->GetChildBoneName()) )
		{
			Constraints[i]->SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetAngularVelocityDriveTwistAndSwing(!bEnableTwistDrive, !bEnableSwingDrive);
		}
	}
}

void USkeletalMeshComponent::SetAllMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->GetChildBoneName());
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}

		Constraints[i]->SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void USkeletalMeshComponent::SetConstraintProfile(FName JointName, FName ProfileName, bool bDefaultIfNotFound)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	for (int32 i = 0; i < Constraints.Num(); i++)
	{
		FConstraintInstance* ConstraintInstance = Constraints[i];
		if(ConstraintInstance->JointName == JointName)
		{
			PhysicsAsset->ConstraintSetup[i]->ApplyConstraintProfile(ProfileName, *ConstraintInstance, bDefaultIfNotFound);
		}
	}
}

void USkeletalMeshComponent::SetConstraintProfileForAll(FName ProfileName, bool bDefaultIfNotFound)
{
	if(UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		for (int32 i = 0; i < Constraints.Num(); i++)
		{
			if(FConstraintInstance* ConstraintInstance = Constraints[i])
			{
				PhysicsAsset->ConstraintSetup[i]->ApplyConstraintProfile(ProfileName, *ConstraintInstance, bDefaultIfNotFound);
			}
		}
	}
}

bool USkeletalMeshComponent::GetConstraintProfilePropertiesOrDefault(
	FConstraintProfileProperties& OutProperties, FName JointName, FName ProfileName)
{
	if (UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		for (int32 i = 0; i < Constraints.Num(); i++)
		{
			FConstraintInstance* ConstraintInstance = Constraints[i];
			if (ConstraintInstance->JointName == JointName)
			{
				OutProperties = PhysicsAsset->ConstraintSetup[i]->GetConstraintProfilePropertiesOrDefault(ProfileName);
				return true;
			}
		}
	}
	return false;
}

void USkeletalMeshComponent::SetAllMotorsAngularDriveParams(float InSpring, float InDamping, float InForceLimit, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->GetChildBoneName());
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}
		Constraints[i]->SetAngularDriveParams(InSpring, InDamping, InForceLimit);
	}
}

void USkeletalMeshComponent::ResetAllBodiesSimulatePhysics()
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	if(CollisionEnabledHasPhysics(GetCollisionEnabled()))
	{
		// Fix / Unfix bones
		for (int32 i = 0; i<Bodies.Num(); i++)
		{
			FBodyInstance*	BodyInst = Bodies[i];
			if (!ensure(BodyInst))
			{
				continue;
			}
			UBodySetup*	BodyInstSetup = BodyInst->GetBodySetup();

			// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
			if (BodyInstSetup && BodyInstSetup->PhysicsType != PhysType_Default)
			{
				if (BodyInstSetup->PhysicsType == PhysType_Simulated)
				{
					BodyInst->SetInstanceSimulatePhysics(true, false, true);
				}
				else
				{
					BodyInst->SetInstanceSimulatePhysics(false, false, true);
				}
			}
		}
	}
}

void USkeletalMeshComponent::SetEnablePhysicsBlending(bool bNewBlendPhysics)
{
	bBlendPhysics = bNewBlendPhysics;
}

void USkeletalMeshComponent::SetPhysicsBlendWeight(float PhysicsBlendWeight)
{
	bool bShouldSimulate = PhysicsBlendWeight > 0.f;
	if (bShouldSimulate != IsSimulatingPhysics())
	{
		SetSimulatePhysics(bShouldSimulate);
	}

	// if blend weight is not 1, set manual weight
	if ( PhysicsBlendWeight < 1.f )
	{
		bBlendPhysics = false;
		SetAllBodiesPhysicsBlendWeight (PhysicsBlendWeight, true);
	}
}

void USkeletalMeshComponent::SetAllBodiesPhysicsBlendWeight(float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	// Fix / Unfix bones
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance*	BodyInst	= Bodies[i];
		if (!ensure(BodyInst))
		{
			continue;
		}
		UBodySetup*	BodyInstSetup	= BodyInst->GetBodySetup();

		// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
		if(BodyInstSetup && (!bSkipCustomPhysicsType || BodyInstSetup->PhysicsType == PhysType_Default) )
		{
			BodyInst->PhysicsBlendWeight = PhysicsBlendWeight;
		}
	}

	bBlendPhysics = false;

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();
}


void USkeletalMeshComponent::SetAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType, bool bIncludeSelf )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, bIncludeSelf, bSkipCustomPhysicsType, [PhysicsBlendWeight](FBodyInstance* BI)
	{
		BI->PhysicsBlendWeight = PhysicsBlendWeight;
	});

	if (NumBodiesFound)
	{
		bBlendPhysics = false;

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}


void USkeletalMeshComponent::AccumulateAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, /*bIncludeSelf=*/ true, /*bSkipCustomPhysicsType=*/ bSkipCustomPhysicsType, [PhysicsBlendWeight](FBodyInstance* BI)
	{
		BI->PhysicsBlendWeight = FMath::Min(BI->PhysicsBlendWeight + PhysicsBlendWeight, 1.f);
	});

	if (NumBodiesFound)
	{
		bBlendPhysics = false;

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}

FConstraintInstance* USkeletalMeshComponent::FindConstraintInstance(FName ConName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && PhysicsAsset->ConstraintSetup.Num() == Constraints.Num())
	{
		int32 ConIndex = PhysicsAsset->FindConstraintIndex(ConName);
		if(ConIndex != INDEX_NONE)
		{
			return Constraints[ConIndex];
		}
	}

	return NULL;
}

FConstraintInstance* USkeletalMeshComponent::GetConstraintInstanceByIndex(uint32 Index)
{
	if (Index < (uint32)Constraints.Num())
	{
		return Constraints[Index];
	}
	return nullptr;
}

void USkeletalMeshComponent::AddForceToAllBodiesBelow(FVector Force, FName BoneName, bool bAccelChange, bool bIncludeSelf)
{
	ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomPhysics=*/false, [Force, bAccelChange](FBodyInstance* BI)
	{
		BI->AddForce(Force, /*bAllowSubstepping=*/ true, bAccelChange);
	});
}

void USkeletalMeshComponent::AddImpulseToAllBodiesBelow(FVector Impulse, FName BoneName, bool bVelChange, bool bIncludeSelf)
{
	ForEachBodyBelow(BoneName, bIncludeSelf,/*bSkipCustomPhysics=*/false, [Impulse, bVelChange](FBodyInstance* BI)
	{
		BI->AddImpulse(Impulse, bVelChange);
	});
}

#ifndef OLD_FORCE_UPDATE_BEHAVIOR
#define OLD_FORCE_UPDATE_BEHAVIOR 0
#endif

void USkeletalMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	// We are handling the physics move below, so don't handle it at higher levels
	Super::OnUpdateTransform(UpdateTransformFlags | EUpdateTransformFlags::SkipPhysicsUpdate, Teleport);

	// Always send new transform to physics
	if(bPhysicsStateCreated && !(UpdateTransformFlags&EUpdateTransformFlags::SkipPhysicsUpdate))
	{
#if !OLD_FORCE_UPDATE_BEHAVIOR
		// Animation from the skeletal mesh happens during TG_PrePhysics.
		// Deferred kinematic updates are applied during TG_StartPhysics.
		// Propagation from the parent movement happens during TG_EndPhysics (for physics objects... could in theory be from any tick group).
		// Therefore, deferred kinematic updates are safe from animation, but not from parent movement.
		EAllowKinematicDeferral AllowDeferral = EAllowKinematicDeferral::AllowDeferral;

		if (!!(UpdateTransformFlags & EUpdateTransformFlags::PropagateFromParent))
		{
			AllowDeferral = EAllowKinematicDeferral::DisallowDeferral;
			
			// If enabled, allow deferral of PropagateFromParent updates in prephysics only.
			// Probably should rework this entire condition to be more concrete, but do not want to introduce that much risk.
			UWorld* World = GetWorld();
			if (CVarEnableKinematicDeferralPrePhysicsCondition.GetValueOnGameThread())
			{
				if (World && (World->TickGroup == ETickingGroup::TG_PrePhysics))
				{
					AllowDeferral = EAllowKinematicDeferral::AllowDeferral;
				}
			}

			if(GEnableKinematicDeferralStartPhysicsCondition)
			{
				if (World && (World->TickGroup == ETickingGroup::TG_StartPhysics))
				{
					AllowDeferral = EAllowKinematicDeferral::AllowDeferral;
				}
			}
		}

		UpdateKinematicBonesToAnim(GetComponentSpaceTransforms(), Teleport, false, AllowDeferral);
#else
		UpdateKinematicBonesToAnim(GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, false);
#endif
	}

	// Pass teleports on to anything in the animation tree that might be interested (e.g. AnimDynamics, RigidBody Node, etc.)
	if(Teleport != ETeleportType::None)
	{
		ResetAnimInstanceDynamics(Teleport);
	}

	// Mark the cloth simulation transform update as pending (the actual update will happen in the cloth thread if the simulation is running)
	UpdateClothTransform(Teleport);
}

bool USkeletalMeshComponent::UpdateOverlapsImpl(const TOverlapArrayView* PendingOverlaps, bool bDoNotifies, const TOverlapArrayView* OverlapsAtEndLocation)
{
	// Parent class (USkinnedMeshComponent) routes only to children, but we really do want to test our own bodies for overlaps.
	return UPrimitiveComponent::UpdateOverlapsImpl(PendingOverlaps, bDoNotifies, OverlapsAtEndLocation);
}

bool USkeletalMeshComponent::ShouldCreatePhysicsState() const
{
	bool bShouldCreatePhysicsState = Super::ShouldCreatePhysicsState();
	bShouldCreatePhysicsState &= (LeaderPoseComponent.IsValid() == false);
	
	return bShouldCreatePhysicsState;
}

void USkeletalMeshComponent::OnCreatePhysicsState()
{
	// Init physics
	if (bEnablePerPolyCollision == false)
	{
		InitArticulated(GetWorld()->GetPhysicsScene());
		USceneComponent::OnCreatePhysicsState(); // Need to route CreatePhysicsState, skip PrimitiveComponent
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SendRenderDebugPhysics();
#endif
	}
	else
	{
		CreateBodySetup();
		BodySetup->CreatePhysicsMeshes();
		Super::OnCreatePhysicsState();	//If we're doing per poly we'll use the body instance of the primitive component
	}

	// Notify physics created
	OnSkelMeshPhysicsCreated.Broadcast();
}


void USkeletalMeshComponent::OnDestroyPhysicsState()
{
	if (bEnablePerPolyCollision == false)
	{
		UnWeldFromParent();
		UnWeldChildren();
		TermArticulated();
	}

	Super::OnDestroyPhysicsState();
}



#if 0 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define DEBUGBROKENCONSTRAINTUPDATE(x) { ##x }
#else
#define DEBUGBROKENCONSTRAINTUPDATE(x)
#endif

#if UE_ENABLE_DEBUG_DRAWING
void USkeletalMeshComponent::SendRenderDebugPhysics(FPrimitiveSceneProxy* OverrideSceneProxy)
{
	FPrimitiveSceneProxy* UseSceneProxy = OverrideSceneProxy ? OverrideSceneProxy : SceneProxy;
	if (UseSceneProxy)
	{
		TArray<FPrimitiveSceneProxy::FDebugMassData> DebugMassData;
		DebugMassData.Reserve(Bodies.Num());
		
		for (FBodyInstance* BI : Bodies)
		{
			if (BI && BI->IsValidBodyInstance())
			{
				const int32 BoneIndex = BI->InstanceBoneIndex;
				if (BoneIndex >= GetComponentSpaceTransforms().Num())
				{
					UE_LOG(LogSkeletalMesh, Log, TEXT("SkeletalMeshComponent : (%d) Bone index out of bounds"), BoneIndex);
					continue;
				}
				DebugMassData.AddDefaulted();
				FPrimitiveSceneProxy::FDebugMassData& MassData = DebugMassData.Last();
				const FTransform MassToWorld = BI->GetMassSpaceToWorldSpace();
				const FTransform& BoneTM = GetComponentSpaceTransforms()[BoneIndex];
				const FTransform BoneToWorld = BoneTM * GetComponentTransform();

				MassData.LocalCenterOfMass = BoneToWorld.InverseTransformPosition(MassToWorld.GetLocation());
				MassData.LocalTensorOrientation = MassToWorld.GetRotation() * BoneToWorld.GetRotation().Inverse();
				MassData.MassSpaceInertiaTensor = BI->GetBodyInertiaTensor();
				MassData.BoneIndex = BoneIndex;
			}
		}

		ENQUEUE_RENDER_COMMAND(SkeletalMesh_SendRenderDebugPhysics)(UE::RenderCommandPipe::SkeletalMesh,
			[UseSceneProxy, DebugMassData]
			{
				UseSceneProxy->SetDebugMassData(DebugMassData);
			}
		);
		
	}
}
#endif

void USkeletalMeshComponent::UpdateMeshForBrokenConstraints()
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	// Needs to have a SkeletalMesh, and PhysicsAsset.
	if( !GetSkeletalMeshAsset() || !PhysicsAsset )
	{
		return;
	}

	DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("%3.3f UpdateMeshForBrokenConstraints"), GetWorld()->GetTimeSeconds());)

	// Iterate through list of constraints in the physics asset
	for(int32 ConstraintInstIndex = 0; ConstraintInstIndex < Constraints.Num(); ConstraintInstIndex++)
	{
		// See if we can find a constraint that has been terminated (broken)
		FConstraintInstance* ConstraintInst = Constraints[ConstraintInstIndex];
		if( ConstraintInst && ConstraintInst->IsTerminated() )
		{
			// Get the associated joint bone index.
			int32 JointBoneIndex = GetBoneIndex(ConstraintInst->GetChildBoneName());
			if( JointBoneIndex == INDEX_NONE )
			{
				continue;
			}

			DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("  Found Broken Constraint: (%d) %s"), JointBoneIndex, *PhysicsAsset->ConstraintSetup(ConstraintInstIndex)->JointName.ToString());)

			// Get child bodies of this joint
			for(int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodySetupIndex++)
			{
				UBodySetup* PhysicsAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodySetupIndex];
				int32 BoneIndex = GetBoneIndex(PhysicsAssetBodySetup->BoneName);
				if( BoneIndex != INDEX_NONE && 
					(BoneIndex == JointBoneIndex || GetSkeletalMeshAsset()->GetRefSkeleton().BoneIsChildOf(BoneIndex, JointBoneIndex)) )
				{
					DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("    Found Child Bone: (%d) %s"), BoneIndex, *PhysicsAssetBodySetup->BoneName.ToString());)

					FBodyInstance* ChildBodyInst = Bodies[BodySetupIndex];
					if( ChildBodyInst )
					{
						// Unfix Body so, it is purely physical, not kinematic.
						if( !ChildBodyInst->IsInstanceSimulatingPhysics() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Unfixing body."));)
							ChildBodyInst->SetInstanceSimulatePhysics(true, false, true);
						}
					}

					FConstraintInstance* ChildConstraintInst = FindConstraintInstance(PhysicsAssetBodySetup->BoneName);
					if( ChildConstraintInst )
					{
						if( ChildConstraintInst->IsLinearPositionDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearPositionDrive."));)
							ChildConstraintInst->SetLinearPositionDrive(false, false, false);
						}
						if( ChildConstraintInst->IsLinearVelocityDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearVelocityDrive."));)
							ChildConstraintInst->SetLinearVelocityDrive(false, false, false);
						}
						if( ChildConstraintInst->IsAngularOrientationDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularPositionDrive."));)
							ChildConstraintInst->SetOrientationDriveTwistAndSwing(false, false);
						}
						if( ChildConstraintInst->IsAngularVelocityDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularVelocityDrive."));)
							ChildConstraintInst->SetAngularVelocityDriveTwistAndSwing(false, false);
						}
					}
				}
			}
		}
	}
}


int32 USkeletalMeshComponent::FindConstraintIndex( FName ConstraintName )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintIndex(ConstraintName) : INDEX_NONE;
}


FName USkeletalMeshComponent::FindConstraintBoneName( int32 ConstraintIndex )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintBoneName(ConstraintIndex) : NAME_None;
}

bool USkeletalMeshComponent::IsSimulatingPhysics(FName BoneName) const
{
	// If no bone name is specified, then we respond referring to the component.
	// If the component is not set to follow physics, then the component is not controlled by simulation.
	if (BoneName == NAME_None && 
		PhysicsTransformUpdateMode == EPhysicsTransformUpdateMode::ComponentTransformIsKinematic)
	{
		return false;
	}

	// We respond based on either the body (if a bone is specified), or the root body (if no bone is
	// specified, and the component is controlled by simulation).
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if (BI)
	{
		return BI->IsInstanceSimulatingPhysics();
	}
	return false;
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(FName BoneName, bool, int32) const
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	FBodyInstance* BodyInst = NULL;

	if(PhysicsAsset != NULL)
	{
		// A name of NAME_None indicates 'root body'
		if(BoneName == NAME_None)
		{
			if (Bodies.IsValidIndex(RootBodyData.BodyIndex))
			{
				BodyInst = Bodies[RootBodyData.BodyIndex];
			}
		}
		// otherwise, look for the body
		else
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if(Bodies.IsValidIndex(BodyIndex))
			{
				BodyInst = Bodies[BodyIndex];
			}
		}

	}

	return BodyInst;
}

void USkeletalMeshComponent::GetWeldedBodies(TArray<FBodyInstance*> & OutWeldedBodies, TArray<FName> & OutLabels, bool bIncludingAutoWeld)
{
	UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();

	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		FBodyInstance* BI = Bodies[BodyIdx];
		if (BI && (BI->WeldParent != nullptr || (bIncludingAutoWeld && BI->bAutoWeld)))
		{
			OutWeldedBodies.Add(BI);
			if (PhysicsAsset)
			{
				if (UBodySetup * PhysicsAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIdx])
				{
					OutLabels.Add(PhysicsAssetBodySetup->BoneName);
				}
				else
				{
					OutLabels.Add(NAME_None);
				}
			}
			else
			{
				OutLabels.Add(NAME_None);
			}

			for (USceneComponent * Child : GetAttachChildren())
			{
				if (UPrimitiveComponent * PrimChild = Cast<UPrimitiveComponent>(Child))
				{
					PrimChild->GetWeldedBodies(OutWeldedBodies, OutLabels, bIncludingAutoWeld);
				}
			}
		}
	}
}

int32 USkeletalMeshComponent::ForEachBodyBelow(FName BoneName, bool bIncludeSelf, bool bSkipCustomType, TFunctionRef<void(FBodyInstance*)> Func)
{
	if (BoneName == NAME_None && bIncludeSelf && !bSkipCustomType)
	{
		for (FBodyInstance* BI : Bodies)	//we want all bodies so just iterate the regular array
		{
			Func(BI);
		}

		return Bodies.Num();
	}
	else
	{
		UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
		if (!PhysicsAsset || !GetSkeletalMeshAsset())
		{
			return 0;
		}

		// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
		if (!IsPhysicsStateCreated() || !bHasValidBodies)
		{
			FMessageLog("PIE").Warning(LOCTEXT("InvalidBodies", "Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset."));
			return 0;
		}

		TArray<int32> BodyIndices;
		BodyIndices.Reserve(Bodies.Num());
		PhysicsAsset->GetBodyIndicesBelow(BodyIndices, BoneName, GetSkeletalMeshAsset(), bIncludeSelf);

		int32 NumBodiesFound = 0;
		for (int32 BodyIdx : BodyIndices)
		{
			FBodyInstance* BI = Bodies[BodyIdx];
			if (bSkipCustomType)
			{
				if (UBodySetup* PhysAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIdx])
				{
					if (PhysAssetBodySetup->PhysicsType != EPhysicsType::PhysType_Default)
					{
						continue;
					}
				}
			}

			++NumBodiesFound;
			Func(BI);
		}

		return NumBodiesFound;
	}

	return 0;
}

void USkeletalMeshComponent::SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	for(FBodyInstance* BI : Bodies)
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	}

	if(Bodies.Num() > 0)
	{
		OnComponentCollisionSettingsChanged();
	}
}

void USkeletalMeshComponent::SetBodyNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision, FName BoneName /* = NAME_None */)
{
	if(FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);

		OnComponentCollisionSettingsChanged();
	}
}

void USkeletalMeshComponent::SetNotifyRigidBodyCollisionBelow(bool bNewNotifyRigidBodyCollision, FName BoneName, bool bIncludeSelf)
{
	const int32 NumBodiesFound = ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomType=*/false, [bNewNotifyRigidBodyCollision](FBodyInstance* BI)
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	});
	
	if(NumBodiesFound > 0)
	{
		OnComponentCollisionSettingsChanged();
	}
}

FConstraintInstanceAccessor USkeletalMeshComponent::GetConstraintByName(FName ConstraintName, bool bIncludesTerminated)
{
	int32 ConstraintIndex = FindConstraintIndex(ConstraintName);
	if (ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num())
	{
		return FConstraintInstanceAccessor();
	}

	if (FConstraintInstance* Constraint = Constraints[ConstraintIndex])
	{
		if (bIncludesTerminated || !Constraint->IsTerminated())
		{
			return FConstraintInstanceAccessor(this, ConstraintIndex);
		}
	}
	return FConstraintInstanceAccessor();
}

void USkeletalMeshComponent::GetConstraints(bool bIncludesTerminated, TArray<FConstraintInstanceAccessor>& OutConstraints)
{
	if (UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		for (int32 i = 0; i < Constraints.Num(); i++)
		{
			if (FConstraintInstance* ConstraintInstance = Constraints[i])
			{
				if (bIncludesTerminated || !ConstraintInstance->IsTerminated())
				{
					OutConstraints.Add(FConstraintInstanceAccessor(this, i));
				}
			}
		}
	}
}

void  USkeletalMeshComponent::GetConstraintsFromBody(FName BodyName, bool bParentConstraints, bool bChildConstraints, bool bIncludesTerminated, TArray<FConstraintInstanceAccessor>& OutConstraints)
{
	if (UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		for (int32 i = 0; i < Constraints.Num(); i++)
		{
			if (FConstraintInstance* ConstraintInstance = Constraints[i])
			{
				if (bIncludesTerminated || !ConstraintInstance->IsTerminated())
				{
					if (bParentConstraints && ConstraintInstance->GetChildBoneName() == BodyName)
					{
						OutConstraints.Add(FConstraintInstanceAccessor(this, i));
					}
					if (bChildConstraints && ConstraintInstance->GetParentBoneName() == BodyName)
					{
						OutConstraints.Add(FConstraintInstanceAccessor(this, i));
					}
				}
			}
		}
	}
}

void USkeletalMeshComponent::BreakConstraint(FVector Impulse, FVector HitLocation, FName InBoneName)
{
	// you can enable/disable the instanced weights by calling
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if( ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num() )
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if( Constraint->IsTerminated() )
	{
		return;
	}

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// Figure out if Body is fixed or not
	FBodyInstance* Body = GetBodyInstance(Constraint->GetChildBoneName());

	if( Body != NULL && !Body->IsInstanceSimulatingPhysics() )
	{
		// Unfix body so it can be broken.
		Body->SetInstanceSimulatePhysics(true, false, true);
	}

	// Break Constraint
	Constraint->TermConstraint();
	// Make sure child bodies and constraints are released and turned to physics.
	UpdateMeshForBrokenConstraints();
	// Add impulse to broken limb
	AddImpulseAtLocation(Impulse, HitLocation, InBoneName);
}


void USkeletalMeshComponent::SetAngularLimits(FName InBoneName, float Swing1LimitAngle, float TwistLimitAngle, float Swing2LimitAngle)
{
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if (ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num())
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if (Constraint->IsTerminated())
	{
		return;
	}

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// Figure out if Body is fixed or not
	FBodyInstance* Body = GetBodyInstance(Constraint->GetChildBoneName());

	if (Body != NULL && Body->IsInstanceSimulatingPhysics())
	{
		// Unfix body so it can be broken.
		Body->SetInstanceSimulatePhysics(true, false, true);
	}

	// update limits
	Constraint->SetAngularSwing1Limit(Swing1LimitAngle == 0 ? ACM_Locked : (Swing1LimitAngle >= 180) ? ACM_Free : ACM_Limited, Swing1LimitAngle);
	Constraint->SetAngularTwistLimit(TwistLimitAngle == 0 ? ACM_Locked : (TwistLimitAngle >= 180) ? ACM_Free : ACM_Limited, TwistLimitAngle);
	Constraint->SetAngularSwing2Limit(Swing2LimitAngle == 0 ? ACM_Locked : (Swing2LimitAngle >= 180) ? ACM_Free : ACM_Limited, Swing2LimitAngle);
}


void USkeletalMeshComponent::GetCurrentJointAngles(FName InBoneName, float &Swing1Angle, float &TwistAngle, float &Swing2Angle)
{
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if (ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num())
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	
	Swing1Angle = FMath::RadiansToDegrees(Constraint->GetCurrentSwing1());
	Swing2Angle = FMath::RadiansToDegrees(Constraint->GetCurrentSwing2());
	TwistAngle = FMath::RadiansToDegrees(Constraint->GetCurrentTwist());
}


void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset, bool bForceReInit)
{
	// If this is different from what we have now, or we should have an instance but for whatever reason it failed last time, teardown/recreate now.
	if(bForceReInit || InPhysicsAsset != GetPhysicsAsset())
	{
		// SkelComp had a physics instance, then terminate it.
		TermArticulated();

		// Need to update scene proxy, because it keeps a ref to the PhysicsAsset.
		Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);
		MarkRenderStateDirty();

		// Update bHasValidBodies flag
		UpdateHasValidBodies();

		// Component should be re-attached here, so create physics.
		if(GetSkeletalMeshAsset())
		{
			// Because we don't know what bones the new PhysicsAsset might want, we have to force an update to _all_ bones in the skeleton.
			RequiredBones.Reset(GetSkeletalMeshAsset()->GetRefSkeleton().GetNum());
			RequiredBones.AddUninitialized(GetSkeletalMeshAsset()->GetRefSkeleton().GetNum() );
			for(int32 i=0; i< GetSkeletalMeshAsset()->GetRefSkeleton().GetNum(); i++)
			{
				RequiredBones[i] = (FBoneIndexType)i;
			}
			RefreshBoneTransforms();

			// Initialize new Physics Asset
			UWorld* World = GetWorld();
			if(World && World->GetPhysicsScene() != nullptr)
			{
			//	UE_LOG(LogSkeletalMesh, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());			
				InitArticulated(World->GetPhysicsScene());
			}
		}
		else
		{
			// If PhysicsAsset hasn't been instanced yet, just update the template.
			Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);

			// Update bHasValidBodies flag
			UpdateHasValidBodies();
		}

		// Indicate that 'required bones' array will need to be recalculated.
		bRequiredBonesUpToDate = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SendRenderDebugPhysics();
#endif
	}
}


void USkeletalMeshComponent::UpdateHasValidBodies()
{
	// First clear out old data
	bHasValidBodies = false;

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	// If we have a physics asset..
	if(PhysicsAsset != NULL)
	{
		// For each body in physics asset..
		for( int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++ )
		{
			// .. find the matching graphics bone index
			int32 BoneIndex = GetBoneIndex( PhysicsAsset->SkeletalBodySetups[ BodyIndex ]->BoneName );

			// If we found a valid graphics bone, set the 'valid' flag
			if(BoneIndex != INDEX_NONE)
			{
				bHasValidBodies = true;
				break;
			}
		}
	}
}

void USkeletalMeshComponent::UpdateBoneBodyMapping()
{
	if (Bodies.Num() > 0)	//If using per poly then there's no bodies to update indices on
	{
		// If we have a physics asset..
		if (const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
		{
			bool bNeedsReInit = false;

			// For each body in physics asset..
			for (int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				// .. find the matching graphics bone index
				int32 BoneIndex = GetBoneIndex(PhysicsAsset->SkeletalBodySetups[BodyIndex]->BoneName);
				FBodyInstance* Inst = Bodies[BodyIndex];
				check(Inst);

				// Make sure physics state matches presence of bone
				bool bHasValidBone = (BoneIndex != INDEX_NONE);
				if (bHasValidBone != Inst->IsValidBodyInstance())
				{
					// If not, we need to recreate physics asset to clean up bodies or create new ones
					bNeedsReInit = true;
				}

				Inst->InstanceBoneIndex = BoneIndex;
			}

			// If the set of bodies needs to change, we recreate physics asset
			if (bNeedsReInit)
			{
				RecreatePhysicsState();
			}
		}
	}
}

void USkeletalMeshComponent::UpdatePhysicsToRBChannels()
{
	// Iterate over each bone/body.
	for (int32 i = 0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->UpdatePhysicsFilterData();
	}
}

template<bool bCachedMatrices>
FVector GetTypedSkinnedVertexPositionWithCloth(USkeletalMeshComponent* Component, int32 VertexIndex, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer, TArray<FMatrix44f>& CachedRefToLocals)
{
	// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
	int32 SectionIndex;
	int32 VertIndexInChunk;
	LODData.GetSectionFromVertexIndex(VertexIndex, SectionIndex, VertIndexInChunk);
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

	// only if this component has clothing and is showing simulated results	
	if (Component->GetSkeletalMeshAsset() &&
		Component->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num() > 0 &&
		!Component->bDisableClothSimulation &&
		Component->ClothBlendWeight > 0.0f // if cloth blend weight is 0.0, only showing skinned vertices regardless of simulation positions
		)
	{
		bool bClothVertex = false;
		FGuid ClothAssetGuid;

		// if this section corresponds to a cloth section, returns corresponding cloth section's info instead
		// if this chunk has cloth data
		if (Section.HasClothingData())
		{
			bClothVertex = true;
			ClothAssetGuid = Section.ClothingData.AssetGuid;
		}

		if (bClothVertex)
		{
			FVector SimulatedPos;
			if (Component->GetClothSimulatedPosition_GameThread(ClothAssetGuid, VertIndexInChunk, SimulatedPos))
			{
				// a simulated position is in world space and convert this to local space
				// because SkinnedMeshComponent::GetSkinnedVertexPosition() returns the position in local space
				SimulatedPos = Component->GetComponentTransform().InverseTransformPosition(SimulatedPos);

				// if blend weight is 1.0, doesn't need to blend with a skinned position
				if (Component->ClothBlendWeight < 1.0f)
				{
					// blend with a skinned position
					FVector SkinnedPos = (FVector)GetTypedSkinnedVertexPosition<bCachedMatrices>(Component, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightBuffer, VertIndexInChunk, CachedRefToLocals);

					SimulatedPos = SimulatedPos*Component->ClothBlendWeight + SkinnedPos*(1.0f - Component->ClothBlendWeight);
				}
				return SimulatedPos;
			}
		}
	}

	return (FVector)GetTypedSkinnedVertexPosition<bCachedMatrices>(Component, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightBuffer, VertIndexInChunk, CachedRefToLocals);
}

FVector3f USkeletalMeshComponent::GetSkinnedVertexPosition(USkeletalMeshComponent* Component, int32 VertexIndex, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer)
{
	TArray<FMatrix44f> Dummy;
	return (FVector3f)GetTypedSkinnedVertexPositionWithCloth<false>(Component, VertexIndex, LODData, SkinWeightBuffer, Dummy);
}

FVector3f USkeletalMeshComponent::GetSkinnedVertexPosition(USkeletalMeshComponent* Component, int32 VertexIndex, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer, TArray<FMatrix44f>& CachedRefToLocals)
{
	return (FVector3f)GetTypedSkinnedVertexPositionWithCloth<true>(Component, VertexIndex, LODData, SkinWeightBuffer, CachedRefToLocals);
}

void USkeletalMeshComponent::ComputeSkinnedPositions(USkeletalMeshComponent* Component, TArray<FVector3f> & OutPositions, TArray<FMatrix44f>& CachedRefToLocals, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer)
{
	// Fail if no mesh
	if (!Component->GetSkeletalMeshAsset())
	{
		return;
	}

	if (Component->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num() > 0 &&
		!Component->bDisableClothSimulation &&
		Component->ClothBlendWeight > 0.0f // if cloth blend weight is 0.0, only showing skinned vertices regardless of simulation positions
		)
	{
		// Fail if cloth data not available
		const TMap<int32, FClothSimulData>& ClothData = Component->GetCurrentClothingData_GameThread();
		if (ClothData.Num() == 0)
		{
			return;
		}

		OutPositions.Empty();
		OutPositions.AddUninitialized(LODData.GetNumVertices());

		//update positions
		for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIdx];

			bool bClothVertex = false;
			int32 ClothAssetIndex = -1;
			FGuid ClothAssetGuid;

			// if this section corresponds to a cloth section, returns corresponding cloth section's info instead
			// if this chunk has cloth data
			if (Section.HasClothingData())
			{
				bClothVertex = true;
				ClothAssetIndex = Section.CorrespondClothAssetIndex;
				ClothAssetGuid = Section.ClothingData.AssetGuid;
			}

			if (bClothVertex)
			{
				int32 AssetIndex = Component->GetSkeletalMeshAsset()->GetClothingAssetIndex(ClothAssetGuid);
				if (AssetIndex != INDEX_NONE)
				{
					const FClothSimulData* ActorData = ClothData.Find(AssetIndex);

					if (ActorData)
					{
						const uint32 SoftOffset = Section.GetVertexBufferIndex();
						const uint32 NumSoftVerts = FMath::Min(Section.GetNumVertices(), ActorData->Positions.Num());

						// if blend weight is 1.0, doesn't need to blend with a skinned position
						if (Component->ClothBlendWeight < 1.0f)
						{
							for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
							{
								FVector3f SimulatedPos = ActorData->Positions[SoftIdx];

								FVector3f SkinnedPosition = GetTypedSkinnedVertexPosition<true>(Component, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightBuffer, SoftIdx, CachedRefToLocals);

								OutPositions[SoftOffset + SoftIdx] = SimulatedPos*Component->ClothBlendWeight + SkinnedPosition*(1.0f - Component->ClothBlendWeight);
							}
						}
						else
						{
							for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
							{
								OutPositions[SoftOffset + SoftIdx] = ActorData->Positions[SoftIdx];
							}
						}

						return;
					}
				}
			}

			//fall back to just regular skinning.
			const uint32 SoftOffset = Section.GetVertexBufferIndex();
			const uint32 NumSoftVerts = Section.GetNumVertices();
			for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
			{
				FVector3f SkinnedPosition = GetTypedSkinnedVertexPosition<true>(Component, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightBuffer, SoftIdx, CachedRefToLocals);
				OutPositions[SoftOffset + SoftIdx] = SkinnedPosition;
			}
		}
	}
	else
	{
		OutPositions.Empty();
		OutPositions.AddUninitialized(LODData.GetNumVertices());

		for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIdx];

			const uint32 SoftOffset = Section.GetVertexBufferIndex();
			const uint32 NumSoftVerts = Section.GetNumVertices();
			for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
			{
				FVector3f SkinnedPosition = GetTypedSkinnedVertexPosition<true>(Component, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightBuffer, SoftIdx, CachedRefToLocals);
				OutPositions[SoftOffset + SoftIdx] = SkinnedPosition;
			}
		}
	}
}

void USkeletalMeshComponent::GetSkinnedTangentBasis(USkeletalMeshComponent* Component, int32 VertexIndex, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer, TArray<FMatrix44f>& CachedRefToLocals, FVector3f& OutTangentX, FVector3f& OutTangentY, FVector3f& OutTangentZ)
{
	int32 SectionIndex;
	int32 VertIndexInChunk;
	LODData.GetSectionFromVertexIndex(VertexIndex, SectionIndex, VertIndexInChunk);

	check(SectionIndex < LODData.RenderSections.Num());
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

	return GetTypedSkinnedTangentBasis(Component, Section, LODData.StaticVertexBuffers, SkinWeightBuffer, VertIndexInChunk, CachedRefToLocals, OutTangentX, OutTangentY, OutTangentZ);
}

void USkeletalMeshComponent::ComputeSkinnedTangentBasis(USkeletalMeshComponent* Component, TArray<FVector3f>& OutTangenXYZ, TArray<FMatrix44f>& CachedRefToLocals, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightBuffer)
{
	// Fail if no mesh
	if (!Component->GetSkeletalMeshAsset())
	{
		return;
	}

	OutTangenXYZ.Empty();
	OutTangenXYZ.AddUninitialized(LODData.GetNumVertices() * 3);

	for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
	{
		const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIdx];

		const uint32 SoftOffset = Section.GetVertexBufferIndex();
		const uint32 NumSoftVerts = Section.GetNumVertices();
		for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
		{
			const uint32 TangentOffset = (SoftOffset + SoftIdx) * 3;
			GetTypedSkinnedTangentBasis(Component, Section, LODData.StaticVertexBuffers, SkinWeightBuffer, SoftIdx, CachedRefToLocals, OutTangenXYZ[TangentOffset + 0], OutTangenXYZ[TangentOffset + 1], OutTangenXYZ[TangentOffset + 2]);
		}
	}
}

void USkeletalMeshComponent::SetEnableBodyGravity(bool bEnableGravity, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		BI->SetEnableGravity(bEnableGravity);
	}
}

bool USkeletalMeshComponent::IsBodyGravityEnabled(FName BoneName)
{
	const FBodyInstance* BI = GetBodyInstance(BoneName);
	return BI && BI->bEnableGravity;
}

void USkeletalMeshComponent::SetEnableGravityOnAllBodiesBelow(bool bEnableGravity, FName BoneName, bool bIncludeSelf)
{
    ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomPhysics=*/false, [bEnableGravity](FBodyInstance* BI)
	{
		BI->SetEnableGravity(bEnableGravity);
	});
}

//////////////////////////////////////////////////////////////////////////
// COLLISION

extern float DebugLineLifetime;

bool USkeletalMeshComponent::GetSquaredDistanceToCollision(const FVector& Point, float& OutSquaredDistance, FVector& OutClosestPointOnCollision) const
{
	OutClosestPointOnCollision = Point;
	bool bHasResult = false;

	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		FBodyInstance* BodyInst = Bodies[BodyIdx];
		if (BodyInst && BodyInst->IsValidBodyInstance() && (BodyInst->GetCollisionEnabled() != ECollisionEnabled::NoCollision))
		{
			FVector ClosestPoint;
			float DistanceSqr = -1.f;

			if (!Bodies[BodyIdx]->GetSquaredDistanceToBody(Point, DistanceSqr, ClosestPoint))
			{
				// Invalid result, impossible to be better than ClosestPointDistance
				continue;
			}

			if (!bHasResult || (DistanceSqr < OutSquaredDistance))
			{
				bHasResult = true;
				OutSquaredDistance = DistanceSqr;
				OutClosestPointOnCollision = ClosestPoint;

				// If we're inside collision, we're not going to find anything better, so abort search we've got our best find.
				if (DistanceSqr <= UE_KINDA_SMALL_NUMBER)
				{
					break;
				}
			}
		}
	}

	return bHasResult;
}

DECLARE_CYCLE_STAT(TEXT("GetClosestPointOnPhysicsAsset"), STAT_GetClosestPointOnPhysicsAsset, STATGROUP_Physics);

bool USkeletalMeshComponent::GetClosestPointOnPhysicsAsset(const FVector& WorldPosition, FClosestPointOnPhysicsAsset& ClosestPointOnPhysicsAsset, bool bApproximate) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetClosestPointOnPhysicsAsset);

	bool bSuccess = false;
	const UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();
	const FReferenceSkeleton* RefSkeleton = GetSkeletalMeshAsset() ? &GetSkeletalMeshAsset()->GetRefSkeleton() : nullptr;
	if(PhysicsAsset && RefSkeleton)
	{
		const TArray<FTransform>& BoneTransforms = GetComponentSpaceTransforms();
		const bool bHasLeaderPoseComponent = LeaderPoseComponent.IsValid();
		const FVector ComponentPosition = GetComponentTransform().InverseTransformPosition(WorldPosition);
	
		float CurrentClosestDistance = FLT_MAX;
		int32 CurrentClosestBoneIndex = INDEX_NONE;
		const UBodySetup* CurrentClosestBodySetup = nullptr;

		for(const UBodySetup* BodySetupInstance : PhysicsAsset->SkeletalBodySetups)
		{
			ClosestPointOnPhysicsAsset.Distance = FLT_MAX;
			const FName BoneName = BodySetupInstance->BoneName;
			const int32 BoneIndex = RefSkeleton->FindBoneIndex(BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				const FTransform BoneTM = bHasLeaderPoseComponent ? GetBoneTransform(BoneIndex) : BoneTransforms[BoneIndex];
				const float Dist = bApproximate ? (BoneTM.GetLocation() - ComponentPosition).SizeSquared() : BodySetupInstance->GetShortestDistanceToPoint(ComponentPosition, BoneTM);

				if (Dist < CurrentClosestDistance)
				{
					CurrentClosestDistance = Dist;
					CurrentClosestBoneIndex = BoneIndex;
					CurrentClosestBodySetup = BodySetupInstance;

					if(Dist <= 0.f) { break; }
				}
			}
		}

		if(CurrentClosestBoneIndex >= 0)
		{
			bSuccess = true;

			const FTransform BoneTM = bHasLeaderPoseComponent ? GetBoneTransform(CurrentClosestBoneIndex) : (BoneTransforms[CurrentClosestBoneIndex] * GetComponentTransform());
			ClosestPointOnPhysicsAsset.Distance = CurrentClosestBodySetup->GetClosestPointAndNormal(WorldPosition, BoneTM, ClosestPointOnPhysicsAsset.ClosestWorldPosition, ClosestPointOnPhysicsAsset.Normal);
			ClosestPointOnPhysicsAsset.BoneName = CurrentClosestBodySetup->BoneName;
		}
	}

	return bSuccess;
}

bool USkeletalMeshComponent::K2_GetClosestPointOnPhysicsAsset(const FVector& WorldPosition, FVector& ClosestWorldPosition, FVector& Normal, FName& BoneName, float& Distance) const
{
	FClosestPointOnPhysicsAsset ClosestPointOnPhysicsAsset;
	bool bSuccess = GetClosestPointOnPhysicsAsset(WorldPosition, ClosestPointOnPhysicsAsset, /*bApproximate =*/ false);
	if(bSuccess)
	{
		ClosestWorldPosition = ClosestPointOnPhysicsAsset.ClosestWorldPosition;
		Normal = ClosestPointOnPhysicsAsset.Normal;
		BoneName = ClosestPointOnPhysicsAsset.BoneName;
		Distance = ClosestPointOnPhysicsAsset.Distance;
	}
	else
	{
		ClosestWorldPosition = FVector::ZeroVector;
		Normal = FVector::ZeroVector;
		BoneName = NAME_None;
		Distance = -1;
	}

	return bSuccess;
}

bool USkeletalMeshComponent::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params)
{
	UWorld* const World = GetWorld();
	bool bHaveHit = false;

	if (bEnablePerPolyCollision)
	{
		// Using PrimitiveComponent implementation
		//as it intersects against mesh polys.
		bHaveHit = UPrimitiveComponent::LineTraceComponent(OutHit, Start, End, Params);
	}
	else
	{
		float MinTime = UE_MAX_FLT;
		FHitResult Hit;
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (Bodies[BodyIdx] && Bodies[BodyIdx]->LineTrace(Hit, Start, End, Params.bTraceComplex, Params.bReturnPhysicalMaterial))
			{
				bHaveHit = true;
				if (MinTime > Hit.Time)
				{
					MinTime = Hit.Time;
					OutHit = Hit;
				}
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(World && World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveHit)
		{
			Hits.Add(OutHit);
		}
		DrawLineTraces(GetWorld(), Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return bHaveHit;
}

bool USkeletalMeshComponent::SweepComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FCollisionShape& CollisionShape, bool bTraceComplex)
{
	bool bHaveHit = false;

	if (bEnablePerPolyCollision)
	{
		// Using PrimitiveComponent implementation
		//as it intersects against mesh polys.
		bHaveHit =  UPrimitiveComponent::SweepComponent(OutHit, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex);
	}
	else
	{
		FHitResult Hit;
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (Bodies[BodyIdx] && Bodies[BodyIdx]->Sweep(Hit, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex))
			{
				if (!bHaveHit || Hit.Time < OutHit.Time)
				{
					OutHit = Hit;
				}
				bHaveHit = true;
			}
		}
	}

	return bHaveHit;
}

bool USkeletalMeshComponent::ComponentOverlapComponentImpl(class UPrimitiveComponent* PrimComp,const FVector Pos,const FQuat& Quat,const struct FCollisionQueryParams& Params)
{
	//we do not support skeletal mesh vs skeletal mesh overlap test
	if (PrimComp->IsA<USkeletalMeshComponent>())
	{
		UE_LOG(LogCollision, Warning, TEXT("ComponentOverlapComponent : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	//We do not support skeletal mesh vs Instanced static meshes
	if (PrimComp->IsA<UInstancedStaticMeshComponent>())
	{
		UE_LOG(LogCollision, Warning, TEXT("ComponentOverlapComponent : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	if (FBodyInstance* BI = PrimComp->GetBodyInstance())
	{
		return BI->OverlapTestForBodies(Pos, Quat, Bodies);
	}

	return false;
}

bool USkeletalMeshComponent::OverlapComponent(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape) const
{
	for (FBodyInstance* Body : Bodies)
	{
		if (Body->OverlapTest(Pos, Rot, CollisionShape))
		{
			return true;
		}
	}

	return false;
}

bool USkeletalMeshComponent::ComponentOverlapMultiImpl(TArray<struct FOverlapResult>& OutOverlaps, const UWorld* World, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	OutOverlaps.Reset();

	if (!Bodies.IsValidIndex(RootBodyData.BodyIndex))
	{
		return false;
	}

	const FTransform WorldToComponent(GetComponentTransform().Inverse());
	const FCollisionResponseParams ResponseParams(GetCollisionResponseToChannels());

	FComponentQueryParams ParamsWithSelf = Params;
	ParamsWithSelf.AddIgnoredComponent(this);

	bool bHaveBlockingHit = false;
	for (const FBodyInstance* Body : Bodies)
	{
		checkSlow(Body);
		if (Body->OverlapMulti(OutOverlaps, World, &WorldToComponent, Pos, Quat, TestChannel, ParamsWithSelf, ResponseParams, ObjectQueryParams))
		{
			bHaveBlockingHit = true;
		}
	}

	return bHaveBlockingHit;
}

void USkeletalMeshComponent::AddClothingBounds(FBoxSphereBounds& InOutBounds, const FTransform& LocalToWorld) const
{
	if(ClothingSimulation && ClothingSimulation->ShouldSimulate())
	{
		InOutBounds = InOutBounds + ClothingSimulation->GetBounds(this).TransformBy(LocalToWorld);
	}
}

void USkeletalMeshComponent::RecreateClothingActors()
{
	CSV_SCOPED_TIMING_STAT(Animation, ClothInit);

	ReleaseAllClothingResources();

	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if(!bAllowClothActors || SkelMesh == nullptr || !IsRegistered())
	{
		return;
	}

	if(CVarEnableClothPhysics.GetValueOnGameThread() && (SkelMesh->GetMeshClothingAssets().Num() > 0))
	{
		UClass* SimFactoryClass = *ClothingSimulationFactory;
		if (SimFactoryClass)
		{
			UClothingSimulationFactory* SimFactory = SimFactoryClass->GetDefaultObject<UClothingSimulationFactory>();
			if(ClothingSimulation == nullptr)
			{
				ClothingSimulation = SimFactory->CreateSimulation();

				if(ClothingSimulation)
				{
					ClothingSimulation->Initialize();
					ClothingSimulationContext = ClothingSimulation->CreateContext();

					if(SimFactory->SupportsRuntimeInteraction())
					{
						ClothingInteractor = SimFactory->CreateInteractor();
					}
				}
			}
		}

		if (ClothingSimulation)
		{
			// Only create cloth sim actors when the world is ready for it
			const UWorld* const World = GetWorld();
#if WITH_EDITORONLY_DATA
			if (World && (World->bShouldSimulatePhysics || bUpdateClothInEditor) && World->GetPhysicsScene())
#else
			if (World && World->bShouldSimulatePhysics && World->GetPhysicsScene())
#endif
			{
				TArray<UClothingAssetBase*> AssetsInUse;
				SkelMesh->GetClothingAssetsInUse(AssetsInUse);

				const bool bIsInitialization = true;
				ClothingSimulation->FillContext(this, 0.f, ClothingSimulationContext, bIsInitialization);

				const int32 NumMeshAssets = SkelMesh->GetMeshClothingAssets().Num();
				for (int32 BaseAssetIndex = 0; BaseAssetIndex < NumMeshAssets; ++BaseAssetIndex)
				{
					UClothingAssetBase* const Asset = SkelMesh->GetMeshClothingAssets()[BaseAssetIndex];
					if (Asset && AssetsInUse.Contains(Asset))
					{
						ClothingSimulation->CreateActor(this, Asset, BaseAssetIndex);
						
						if (ClothingInteractor)
						{
							ClothingInteractor->CreateClothingInteractor(Asset, BaseAssetIndex);
						}
					}
				}
			}

			// Retrieve the cloth sim data, or clear the data if the world isn't ready to sim
			WritebackClothingSimulationData();
		}
	}
	else
	{
		// No clothing assets, so destroy any clothing sim we have
		UClothingSimulationFactory* SimFactory = GetClothingSimFactory();
		if(ClothingSimulation && SimFactory)
		{
			ClothingSimulation->DestroyContext(ClothingSimulationContext);
			ClothingSimulation->DestroyActors();
			ClothingSimulation->Shutdown();

			SimFactory->DestroySimulation(ClothingSimulation);
			ClothingSimulation = nullptr;
			ClothingSimulationContext = nullptr;
		}
	}
}

void USkeletalMeshComponent::RemoveAllClothingActors()
{
	if(ClothingSimulation)
	{
		// Can't destroy our actors if we're still simulating
		HandleExistingParallelClothSimulation();

		if(ClothingInteractor)
		{
			ClothingInteractor->DestroyClothingInteractors();
		}

		ClothingSimulation->DestroyActors();
	}
}

void USkeletalMeshComponent::ReleaseAllClothingResources()
{
#if WITH_CLOTH_COLLISION_DETECTION
	if(ClothingSimulation)
	{
		// Ensure no running simulation first
		HandleExistingParallelClothSimulation();

		ClothingSimulation->ClearExternalCollisions();
	}
#endif // #if WITH_CLOTH_COLLISION_DETECTION

	RemoveAllClothingActors();
}

void USkeletalMeshComponent::GetWindForCloth_GameThread(FVector& WindDirection, float& WindAdaption) const
{
	check(IsInGameThread());

	WindDirection = FVector::ZeroVector;
	WindAdaption = 2.f;	//not sure where this const comes from, but that's what the old code did
	
	UWorld* World = GetWorld();
	if(World && World->Scene)
	{
		// set wind
		if(IsWindEnabled())
		{
			FVector Position = GetComponentTransform().GetTranslation();

			float WindSpeed;
			float WindMinGust;
			float WindMaxGust;
			World->Scene->GetWindParameters_GameThread(Position, WindDirection, WindSpeed, WindMinGust, WindMaxGust);

			WindDirection *= WindSpeed;
			WindAdaption = FMath::Rand() % 20 * 0.1f; // make range from 0 to 2
		}
	}
}

#if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::FindClothCollisions(FClothCollisionData& OutCollisions)
{
	if(ClothingSimulation)
	{
		// Get collisions for this simulation, ignoring any externally added collisions
		// (i.e. on grab the asset collisions, not environment etc.)
		ClothingSimulation->GetCollisions(OutCollisions, false);
	}
}

void USkeletalMeshComponent::ExtractCollisionsForCloth(
	USkeletalMeshComponent* SourceComponent, 
	UPhysicsAsset* PhysicsAsset, 
	USkeletalMeshComponent* DestClothComponent, 
	FClothCollisionData& OutCollisions, 
	FClothCollisionSource& ClothCollisionSource)
{
	// Extract collisions from this mesh 'raw', as this isn't a mesh that has cloth simulation
	// (but we want it to affect other meshes with cloth simulation)
	if(SourceComponent->GetSkeletalMeshAsset() && PhysicsAsset)
	{
		FTransform ComponentToComponentTransform;
		if(SourceComponent != DestClothComponent)
		{
			FTransform DestClothComponentTransform = DestClothComponent->GetComponentTransform();
			DestClothComponentTransform.RemoveScaling();  // The collision source doesn't need the scale of the cloth skeletal mesh applied to it (but it does need the source scale from the component transform)
			ComponentToComponentTransform = SourceComponent->GetComponentTransform() * DestClothComponentTransform.Inverse();
		}

		// Init cache on first copy
		if(!ClothCollisionSource.bCached || ClothCollisionSource.CachedSkeletalMesh.Get() != SourceComponent->GetSkeletalMeshAsset())
		{
			// Clear previous cached data
			ClothCollisionSource.CachedSpheres.Reset();
			ClothCollisionSource.CachedSphereConnections.Reset();

			for(const USkeletalBodySetup* SkeletalBodySetup : PhysicsAsset->SkeletalBodySetups)
			{
				// Cache bones
				int32 MeshBoneIndex = SourceComponent->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(SkeletalBodySetup->BoneName);
				if(MeshBoneIndex != INDEX_NONE)
				{
					// Cache spheres & capsules form physics asset
					for(const FKSphereElem& Sphere : SkeletalBodySetup->AggGeom.SphereElems)
					{
						FClothCollisionPrim_Sphere NewSphere;
						NewSphere.LocalPosition = Sphere.Center;
						NewSphere.Radius = Sphere.Radius;
						NewSphere.BoneIndex = MeshBoneIndex;

						ClothCollisionSource.CachedSpheres.Add(NewSphere);
					}

					for(const FKSphylElem& Sphyl : SkeletalBodySetup->AggGeom.SphylElems)
					{
						FClothCollisionPrim_Sphere Sphere0;
						FClothCollisionPrim_Sphere Sphere1;
						FVector OrientedDirection = Sphyl.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
						FVector HalfDim = OrientedDirection * (Sphyl.Length / 2.0f);
						Sphere0.LocalPosition = Sphyl.Center - HalfDim;
						Sphere1.LocalPosition = Sphyl.Center + HalfDim;
						Sphere0.Radius = Sphyl.Radius;
						Sphere1.Radius = Sphyl.Radius;
						Sphere0.BoneIndex = MeshBoneIndex;
						Sphere1.BoneIndex = MeshBoneIndex;

						ClothCollisionSource.CachedSpheres.Add(Sphere0);
						ClothCollisionSource.CachedSpheres.Add(Sphere1);

						FClothCollisionPrim_SphereConnection Connection;
						Connection.SphereIndices[0] = ClothCollisionSource.CachedSpheres.Num() - 2;
						Connection.SphereIndices[1] = ClothCollisionSource.CachedSpheres.Num() - 1;

						ClothCollisionSource.CachedSphereConnections.Add(Connection);
					}

					for(const FKTaperedCapsuleElem& TaperedCapsuleElems : SkeletalBodySetup->AggGeom.TaperedCapsuleElems)
					{
						FClothCollisionPrim_Sphere Sphere0;
						FClothCollisionPrim_Sphere Sphere1;
						FVector OrientedDirection = TaperedCapsuleElems.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
						FVector HalfDim = OrientedDirection * (TaperedCapsuleElems.Length / 2.0f);
						Sphere0.LocalPosition = TaperedCapsuleElems.Center + HalfDim;
						Sphere1.LocalPosition = TaperedCapsuleElems.Center - HalfDim;
						Sphere0.Radius = TaperedCapsuleElems.Radius0;
						Sphere1.Radius = TaperedCapsuleElems.Radius1;
						Sphere0.BoneIndex = MeshBoneIndex;
						Sphere1.BoneIndex = MeshBoneIndex;

						ClothCollisionSource.CachedSpheres.Add(Sphere0);
						ClothCollisionSource.CachedSpheres.Add(Sphere1);

						FClothCollisionPrim_SphereConnection Connection;
						Connection.SphereIndices[0] = ClothCollisionSource.CachedSpheres.Num() - 2;
						Connection.SphereIndices[1] = ClothCollisionSource.CachedSpheres.Num() - 1;

						ClothCollisionSource.CachedSphereConnections.Add(Connection);
					}
				}
			}

			ClothCollisionSource.CachedSkeletalMesh = SourceComponent->GetSkeletalMeshAsset();
			ClothCollisionSource.bCached = true;
		}

		// presize array allocations
		OutCollisions.Spheres.Reserve(OutCollisions.Spheres.Num() + ClothCollisionSource.CachedSpheres.Num());
		OutCollisions.SphereConnections.Reserve(OutCollisions.SphereConnections.Num() + ClothCollisionSource.CachedSphereConnections.Num());

		// Now transform output data
		for(const FClothCollisionPrim_Sphere& CachedSphere : ClothCollisionSource.CachedSpheres)
		{
			FClothCollisionPrim_Sphere& OutSphere = OutCollisions.Spheres.Add_GetRef(CachedSphere);

			const FTransform BoneTransform = SourceComponent->GetBoneTransform(OutSphere.BoneIndex, FTransform::Identity) * ComponentToComponentTransform;
			OutSphere.LocalPosition = BoneTransform.TransformPosition(OutSphere.LocalPosition);
			OutSphere.Radius *= BoneTransform.GetScale3D().X;  // Cloth collisions only uniformly scale
			OutSphere.BoneIndex = INDEX_NONE;
		}

		// Offset connections
		int32 ConnectionBaseIndex = OutCollisions.SphereConnections.Num();
		for(const FClothCollisionPrim_SphereConnection& CachedSphereConnection : ClothCollisionSource.CachedSphereConnections)
		{
			FClothCollisionPrim_SphereConnection& OutSphereConnection = OutCollisions.SphereConnections.Add_GetRef(CachedSphereConnection);
			OutSphereConnection.SphereIndices[0] += ConnectionBaseIndex;
			OutSphereConnection.SphereIndices[1] += ConnectionBaseIndex;
		}
	}
}

void USkeletalMeshComponent::CopyClothCollisionsToChildren()
{
	// 3 steps
	// 1. release all previous parent collisions
	// 2. find new collisions from parent(this class)
	// 3. add new collisions to children

	TArray<USkeletalMeshComponent*> ClothChildren;

	for (USceneComponent* AttachedChild : GetAttachChildren())
	{
		USkeletalMeshComponent* pChild = Cast<USkeletalMeshComponent>(AttachedChild);
		if(pChild && pChild->ClothingSimulation && pChild->ClothingSimulation->ShouldSimulate())
		{
			ClothChildren.Add(pChild);
		}
	}

	const int32 NumClothChildren = ClothChildren.Num();

	if(NumClothChildren == 0)
	{
		return;
	}

	FClothCollisionData NewCollisions;

	FindClothCollisions(NewCollisions);

	for(USkeletalMeshComponent* Child : ClothChildren)
	{
		Child->ClothingSimulation->AddExternalCollisions(NewCollisions);
	}
}

void USkeletalMeshComponent::CopyClothCollisionSources()
{
	FClothCollisionData ExternalCollisions;

	for(FClothCollisionSource& ClothCollisionSource : ClothCollisionSources)
	{
		USkeletalMeshComponent* SourceComponent = ClothCollisionSource.SourceComponent.Get();
		UPhysicsAsset* SourcePhysicsAsset = ClothCollisionSource.SourcePhysicsAsset.Get();

		if(SourceComponent && SourcePhysicsAsset)
		{
			ExtractCollisionsForCloth(SourceComponent, SourcePhysicsAsset, this, ExternalCollisions, ClothCollisionSource);
		}
	}

	ClothingSimulation->AddExternalCollisions(ExternalCollisions);
}

// children's collisions can affect to parent's cloth reversely
void USkeletalMeshComponent::CopyChildrenClothCollisionsToParent()
{
	// 3 steps
	// 1. release all previous children collisions
	// 2. find new collisions from children
	// 3. add new collisions to parent (this component)

	TArray<USkeletalMeshComponent*> ClothCollisionChildren;

	FClothCollisionData NewCollisions;

	for (USceneComponent* AttachedChild : GetAttachChildren())
	{
		USkeletalMeshComponent* Child = Cast<USkeletalMeshComponent>(AttachedChild);
		if(Child)
		{
			Child->FindClothCollisions(NewCollisions);
		}
	}

	ClothingSimulation->AddExternalCollisions(NewCollisions);
}

void USkeletalMeshComponent::ProcessClothCollisionWithEnvironment()
{
	// Read config on first call
	struct FEnvironmentCollisionConfig
	{
		float Padding = 2.f;  // Extra padding added to the bounds so that the collision can still be detected after being resolved
		float Thickness = 2.f;  // Extra thickness added to edgy collision shapes (cubes & convexes)
		int32 MaxShapes = 32;  // Limit the number of extracted shapes per component as these collisions are very expensive

		FEnvironmentCollisionConfig()
		{
			if (GConfig)
			{
				GConfig->GetFloat(TEXT("ClothSettings"), TEXT("EnvironmentCollisionPadding"), Padding, GEngineIni);
				GConfig->GetFloat(TEXT("ClothSettings"), TEXT("EnvironmentCollisionThickness"), Thickness, GEngineIni);
				GConfig->GetInt(TEXT("ClothSettings"), TEXT("EnvironmentCollisionMaxShapes"), MaxShapes, GEngineIni);
			}
		}
	};
	static FEnvironmentCollisionConfig EnvironmentCollisionConfig;

	// don't handle collision detection if this component is in editor
	if(!GetWorld()->IsGameWorld() || !ClothingSimulation)
	{
		return;
	}

	FClothCollisionData NewCollisionData;

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;

	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	// to collide with other clothing objects
	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ClothOverlapComponents), false);
	const FBoxSphereBounds PaddedBounds = Bounds.ExpandBy(EnvironmentCollisionConfig.Padding);

	GetWorld()->OverlapMultiByObjectType(Overlaps, PaddedBounds.Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(PaddedBounds.BoxExtent), Params);

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		const TWeakObjectPtr<UPrimitiveComponent>& Component = Overlaps[OverlapIdx].Component;
		if (Component.IsValid())
		{ 
			ECollisionChannel Channel = Component->GetCollisionObjectType();

			if(Channel == ECollisionChannel::ECC_WorldStatic)
			{
				// Static world geo
				if(!Component->BodyInstance.IsValidBodyInstance())
				{
					// Move to next component, this one has no valid physics
					continue;
				}

				bool bSuccessfulRead = false;

				const FPhysicsActorHandle& ActorRef = Component->BodyInstance.GetActorReferenceWithWelding();
				FPhysicsCommand::ExecuteRead(ActorRef, [&](const FPhysicsActorHandle& Actor)
				{
					TArray<FPhysicsShapeHandle> AllShapes;
					const int32 NumSyncShapes = Component->BodyInstance.GetAllShapes_AssumesLocked(AllShapes);

					if (NumSyncShapes == 0 || NumSyncShapes > EnvironmentCollisionConfig.MaxShapes)
					{
						// Either no shapes or too complicated to consider
						return;
					}

					using namespace Chaos;

					const FReal Thickness = (FReal)EnvironmentCollisionConfig.Thickness;

					auto AddSphere = [&NewCollisionData](const FImplicitSphere3& ImplicitSphere, const FTransform& ComponentToClothTransform, const FVec3& Scale = FVec3::OneVector)
					{
						NewCollisionData.Spheres.Emplace(
							ImplicitSphere.GetRadius() * Scale.X,  // Assumes uniform scale
							ComponentToClothTransform.TransformPosition(ImplicitSphere.GetCenter()));
					};

					auto AddBox = [&NewCollisionData, Thickness](const FImplicitBox3& ImplicitBox, const FTransform& ComponentToClothTransform, const FVec3& Scale = FVec3::OneVector)
						{
							NewCollisionData.Boxes.Emplace(
								ComponentToClothTransform.TransformPosition(ImplicitBox.GetCenter()),
								ComponentToClothTransform.GetRotation(),
								ImplicitBox.Extents() * Scale * (FReal)0.5 + Thickness);
						};

					auto AddCapsule = [&NewCollisionData](const FImplicitCapsule3& ImplicitCapsule, const FTransform& ComponentToClothTransform, const FVec3& Scale = FVec3::OneVector)
						{
							const int32 BaseSphereIndex = NewCollisionData.Spheres.Num();

							const FReal Radius = ImplicitCapsule.GetRadius() * Scale.X;  // Assumes uniform scale
							const FReal HalfHeight = ImplicitCapsule.GetHeight() * (FReal)0.5;
							const FVector HalfSegment = ComponentToClothTransform.GetUnitAxis(EAxis::X) * HalfHeight * Scale.X;  // Assumes uniform scale
							const FVector TransformedCenter = ComponentToClothTransform.TransformPosition(ImplicitCapsule.GetCenter());

							NewCollisionData.Spheres.Emplace(Radius, TransformedCenter + HalfSegment);
							NewCollisionData.Spheres.Emplace(Radius, TransformedCenter - HalfSegment);

							NewCollisionData.SphereConnections.Emplace(BaseSphereIndex, BaseSphereIndex + 1);
						};

					auto AddConvex = [&NewCollisionData, Thickness](const FImplicitConvex3& ImplicitConvex, const FMatrix& ComponentToClothMatrix, const FVec3* const Scale = nullptr)
						{
							TArray<FClothCollisionPrim_ConvexFace> Faces;
							const int32 NumPlanes = ImplicitConvex.NumPlanes();
							Faces.SetNum(NumPlanes);

							TArray<FVector> SurfacePoints;
							const int32 NumSurfacePoints = ImplicitConvex.NumVertices();
							SurfacePoints.SetNumUninitialized(NumSurfacePoints);

							TArray<TArray<int32, TInlineAllocator<4>>, TInlineAllocator<16>> PointFaces;
							PointFaces.SetNum(NumSurfacePoints);

							for (int32 FaceIndex = 0; FaceIndex < NumPlanes; ++FaceIndex)
							{
								const TPlaneConcrete<FReal, 3>& Plane = ImplicitConvex.GetPlane(FaceIndex);
								const FVec3& Normal = Plane.Normal();
								const FVec3 Base = Plane.X() + Normal * (FReal)Thickness;

								Faces[FaceIndex].Plane = FPlane(FVector(Base), FVector(Normal)).TransformBy(ComponentToClothMatrix);

								const int32 NumFaceIndices = ImplicitConvex.NumPlaneVertices(FaceIndex);
								Faces[FaceIndex].Indices.SetNumUninitialized(NumFaceIndices);

								for (int32 Index = 0; Index < NumFaceIndices; ++Index)
								{
									const int32 PointIndex = ImplicitConvex.GetPlaneVertex(FaceIndex, Index);
									Faces[FaceIndex].Indices[Index] = PointIndex;

									PointFaces[PointIndex].Add(FaceIndex);
								}
							}

							for (int32 PointIndex = 0; PointIndex < NumSurfacePoints; ++PointIndex)
							{
								check(PointFaces[PointIndex].Num() >= 3);
								const int32 Index0 = PointFaces[PointIndex][0];
								const int32 Index1 = PointFaces[PointIndex][1];
								const int32 Index2 = PointFaces[PointIndex][2];

								if (!FMath::IntersectPlanes3(SurfacePoints[PointIndex], Faces[Index0].Plane, Faces[Index1].Plane, Faces[Index2].Plane))
								{
									SurfacePoints[PointIndex] = ComponentToClothMatrix.TransformPosition(FVector(ImplicitConvex.GetVertex(PointIndex)));
								}
							}

							NewCollisionData.Convexes.Emplace(MoveTemp(Faces), MoveTemp(SurfacePoints));
						};

					FTransform ClothComponentTransform = GetComponentTransform();
					ClothComponentTransform.RemoveScaling();  // The environment collision shape doesn't need the scale of the cloth skeletal mesh applied to it (but it does need the source scale from its component transform)
					const FTransform ComponentToClothBaseTransform = Component->GetComponentTransform() * ClothComponentTransform.Inverse();

					bool bHasSimpleCollision = false;

					for (FPhysicsShapeHandle& ShapeHandle : AllShapes)
					{
						FTransform ComponentToClothTransform = ComponentToClothBaseTransform;

						const FImplicitObject* ImplicitObject = &ShapeHandle.GetGeometry();
						EImplicitObjectType ImplicitType = ImplicitObject->GetType();

						// Transformed implicits
						if (ImplicitType == ImplicitObjectType::Transformed)
						{
							const TImplicitObjectTransformed<FReal, 3>& ImplicitTransformed = ImplicitObject->GetObjectChecked<TImplicitObjectTransformed<FReal, 3>>();
							ImplicitObject = ImplicitTransformed.GetTransformedObject();
							ImplicitType = ImplicitObject->GetType();

							ComponentToClothTransform = ImplicitTransformed.GetTransform() * ComponentToClothTransform;
							UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found transformed environmental collision"));
						}

						switch (ImplicitType)
						{
						// Base implicits
						case ImplicitObjectType::Sphere:
							{
								const FImplicitSphere3& ImplicitSphere = ImplicitObject->GetObjectChecked<FImplicitSphere3>();;
								AddSphere(ImplicitSphere, ComponentToClothTransform);
							}
							UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Sphere cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
							bHasSimpleCollision = true;
							break;
						case ImplicitObjectType::Box:
							{
								const FImplicitBox3& ImplicitBox = ImplicitObject->GetObjectChecked<FImplicitBox3>();
								AddBox(ImplicitBox, ComponentToClothTransform);
							}
							UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Box cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
							bHasSimpleCollision = true;
							break;
						case ImplicitObjectType::Capsule:
							{
								const FImplicitCapsule3& ImplicitCapsule = ImplicitObject->GetObjectChecked<FImplicitCapsule3>();
								AddCapsule(ImplicitCapsule, ComponentToClothTransform);
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Capsule cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::Convex:
							{
								const FImplicitConvex3& ImplicitConvex = ImplicitObject->GetObjectChecked<FImplicitConvex3>();
								AddConvex(ImplicitConvex, ComponentToClothTransform.ToMatrixNoScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Convex cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;

						// Instanced implicits
						case ImplicitObjectType::IsInstanced | ImplicitObjectType::Sphere:
							{
								const TImplicitObjectInstanced<FImplicitSphere3>& ImplicitInstanced = ImplicitObject->GetObjectChecked<TImplicitObjectInstanced<FImplicitSphere3>>();
								check(ImplicitInstanced.Object());
								const FImplicitSphere3& ImplicitSphere = *ImplicitInstanced.GetInstancedObject();
								AddSphere(ImplicitSphere, ComponentToClothTransform);
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Instanced Sphere cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsInstanced | ImplicitObjectType::Box:
							{
								const TImplicitObjectInstanced<FImplicitBox3>& ImplicitInstanced = ImplicitObject->GetObjectChecked<TImplicitObjectInstanced<FImplicitBox3>>();
								check(ImplicitInstanced.Object());
								const FImplicitBox3& ImplicitBox = *ImplicitInstanced.GetInstancedObject();
								AddBox(ImplicitBox, ComponentToClothTransform);
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Instanced Box cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsInstanced | ImplicitObjectType::Capsule:
							{
								const TImplicitObjectInstanced<FImplicitCapsule3>& ImplicitInstanced = ImplicitObject->GetObjectChecked<TImplicitObjectInstanced<FImplicitCapsule3>>();
								check(ImplicitInstanced.Object());
								const FImplicitCapsule3& ImplicitCapsule = *ImplicitInstanced.GetInstancedObject();
								AddCapsule(ImplicitCapsule, ComponentToClothTransform);
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Instanced Capsule cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsInstanced | ImplicitObjectType::Convex:
							{
								const TImplicitObjectInstanced<FImplicitConvex3>& ImplicitInstanced = ImplicitObject->GetObjectChecked<TImplicitObjectInstanced<FImplicitConvex3>>();
								check(ImplicitInstanced.Object());
								const FImplicitConvex3& ImplicitConvex = *ImplicitInstanced.GetInstancedObject();
								AddConvex(ImplicitConvex, ComponentToClothTransform.ToMatrixNoScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Instanced Convex cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;

						// Scaled implicits
						case ImplicitObjectType::IsScaled | ImplicitObjectType::Sphere:
							{
								const TImplicitObjectScaled<FImplicitSphere3>& ImplicitScaled = ImplicitObject->GetObjectChecked<TImplicitObjectScaled<FImplicitSphere3>>();
								check(ImplicitScaled.Object());
								const FImplicitSphere3& ImplicitSphere = *ImplicitScaled.GetUnscaledObject();
								ensure(FVector::DistSquared(ComponentToClothTransform.GetScale3D(), FVector(ImplicitScaled.GetScale())) < UE_KINDA_SMALL_NUMBER);
								AddSphere(ImplicitSphere, ComponentToClothTransform, ImplicitScaled.GetScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Scaled Sphere cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsScaled | ImplicitObjectType::Box:
							{
								const TImplicitObjectScaled<FImplicitBox3>& ImplicitScaled = ImplicitObject->GetObjectChecked<TImplicitObjectScaled<FImplicitBox3>>();
								check(ImplicitScaled.Object());
								const FImplicitBox3& ImplicitBox = *ImplicitScaled.GetUnscaledObject();
								ensure(FVector::DistSquared(ComponentToClothTransform.GetScale3D(), FVector(ImplicitScaled.GetScale())) < UE_KINDA_SMALL_NUMBER);
								AddBox(ImplicitBox, ComponentToClothTransform, ImplicitScaled.GetScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Scaled Box cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsScaled | ImplicitObjectType::Capsule:
							{
								const TImplicitObjectScaled<FImplicitCapsule3>& ImplicitScaled = ImplicitObject->GetObjectChecked<TImplicitObjectScaled<FImplicitCapsule3>>();
								check(ImplicitScaled.Object());
								const FImplicitCapsule3& ImplicitCapsule = *ImplicitScaled.GetUnscaledObject();
								ensure(FVector::DistSquared(ComponentToClothTransform.GetScale3D(), FVector(ImplicitScaled.GetScale())) < UE_KINDA_SMALL_NUMBER);
								AddCapsule(ImplicitCapsule, ComponentToClothTransform, ImplicitScaled.GetScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Scaled Capsule cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;
						case ImplicitObjectType::IsScaled | ImplicitObjectType::Convex:
							{
								const TImplicitObjectScaled<FImplicitConvex3>& ImplicitScaled = ImplicitObject->GetObjectChecked<TImplicitObjectScaled<FImplicitConvex3>>();
								check(ImplicitScaled.Object());
								const FImplicitConvex3& ImplicitConvex = *ImplicitScaled.GetUnscaledObject();
								ensure(FVector::DistSquared(ComponentToClothTransform.GetScale3D(), FVector(ImplicitScaled.GetScale())) < UE_KINDA_SMALL_NUMBER);
								AddConvex(ImplicitConvex, ComponentToClothTransform.ToMatrixWithScale());
								UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found Scaled Convex cloth environmental collision in [%s]"), *Component->GetOwner()->GetFName().ToString());
								bHasSimpleCollision = true;
							}
							break;

						// Triangle mesh
						case ImplicitObjectType::TriangleMesh:
						case ImplicitObjectType::IsInstanced | ImplicitObjectType::TriangleMesh:
						case ImplicitObjectType::IsScaled | ImplicitObjectType::TriangleMesh:
							// TODO: We could eventually want to collide cloth against triangle meshes,
							//       however the concept of simple vs complex shape might need to be clarified
							//       as it currently iterates over all shape to discard the triangle mesh ones.
							UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found unusable Triangle Mesh cloth environmental collision in [%s]"), !Component->GetOwner() ? TEXT("Unknown") : *Component->GetOwner()->GetFName().ToString());
							break;

						default: 
							UE_LOG(LogSkeletalMesh, Verbose, TEXT("Found unsupported collision type during environmental collision with the cloth in [%s]"), !Component->GetOwner() ? TEXT("Unknown") : *Component->GetOwner()->GetFName().ToString());
							break;
						}
					}
					bSuccessfulRead = true;
				});
			}
			else if(Channel == ECollisionChannel::ECC_PhysicsBody)
			{
				// Possibly a skeletal mesh, extract it's clothing collisions if necessary
				USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Component.Get());

				if(SkelComp && SkelComp->GetSkeletalMeshAsset())
				{
					if(SkelComp == this)
					{
						// Same mesh, move to next component
						continue;
					}

					if(SkelComp->ClothingSimulation)
					{
						// append skeletal component collisions
						FClothCollisionData SkelCollisionData;
						SkelComp->ClothingSimulation->GetCollisions(SkelCollisionData, false);
						NewCollisionData.Append(SkelCollisionData);
					}
				}
			}
		}
	}

	ClothingSimulation->AddExternalCollisions(NewCollisionData);
}

void USkeletalMeshComponent::AddClothCollisionSource(USkeletalMeshComponent* InSourceComponent, UPhysicsAsset* InSourcePhysicsAsset)
{
	if(InSourceComponent && InSourcePhysicsAsset)
	{
		FClothCollisionSource* FoundCollisionSource = ClothCollisionSources.FindByPredicate(
			[InSourceComponent, InSourcePhysicsAsset](const FClothCollisionSource& InCollisionSource)
			{ 
				return InCollisionSource.SourceComponent.Get() == InSourceComponent && InCollisionSource.SourcePhysicsAsset.Get() == InSourcePhysicsAsset; 
			}
		);

		if(FoundCollisionSource == nullptr)
		{
			// Add an UpdateClothTransform delegate after the transform buffer flip, so that the cloths' transform gets updated when the component owning the cloth isn't moving, but the collision source is
			const FOnBoneTransformsFinalizedMultiCast::FDelegate OnBoneTransformsFinalizedDelegate =
				FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &USkeletalMeshComponent::UpdateClothTransform);

			// Add the new collision source
			ClothCollisionSources.Emplace(InSourceComponent, InSourcePhysicsAsset, OnBoneTransformsFinalizedDelegate);

			// Add prerequisite so we don't end up with a frame delay
			ClothTickFunction.AddPrerequisite(InSourceComponent, InSourceComponent->PrimaryComponentTick);
		}
	}
}

void USkeletalMeshComponent::RemoveClothCollisionSource(USkeletalMeshComponent* InSourceComponent)
{
	if(InSourceComponent)
	{
		ClothCollisionSources.RemoveAll([InSourceComponent](const FClothCollisionSource& InCollisionSource)
		{ 
			return !InCollisionSource.SourceComponent.IsValid() || InCollisionSource.SourceComponent.Get() == InSourceComponent; 
		});
	}
}

void USkeletalMeshComponent::RemoveClothCollisionSource(USkeletalMeshComponent* InSourceComponent, UPhysicsAsset* InSourcePhysicsAsset)
{
	if(InSourceComponent && InSourcePhysicsAsset)
	{
		ClothCollisionSources.RemoveAll([InSourceComponent, InSourcePhysicsAsset](const FClothCollisionSource& InCollisionSource)
		{ 
			return !InCollisionSource.SourceComponent.IsValid() || (InCollisionSource.SourceComponent.Get() == InSourceComponent && InCollisionSource.SourcePhysicsAsset.Get() == InSourcePhysicsAsset); 
		});
	}
}

void USkeletalMeshComponent::ResetClothCollisionSources()
{
	ClothCollisionSources.Reset();
}

#endif// #if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::EndPhysicsTickComponent(FSkeletalMeshComponentEndPhysicsTickFunction& ThisTickFunction)
{
	//IMPORTANT!
	//
	// The decision on whether to use EndPhysicsTickComponent or not is made by ShouldRunEndPhysicsTick()
	// Any changes that are made to EndPhysicsTickComponent that affect whether it should be run or not
	// have to be reflected in ShouldRunEndPhysicsTick() as well
	
	// if physics is disabled on dedicated server, no reason to be here. 
	if (!bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer())
	{
		FinalizeBoneTransform();
		return;
	}

	if (IsRegistered() && IsSimulatingPhysics() && RigidBodyIsAwake())
	{
		if (bNotifySyncComponentToRBPhysics)
		{
			OnSyncComponentToRBPhysics();
		}

		SyncComponentToRBPhysics();
	}

	// this used to not run if not rendered, but that causes issues such as bounds not updated
	// causing it to not rendered, at the end, I think we should blend body positions
	// for example if you're only simulating, this has to happen all the time
	// whether looking at it or not, otherwise
	// @todo better solution is to check if it has moved by changing SyncComponentToRBPhysics to return true if anything modified
	// and run this if that is true or rendered
	// that will at least reduce the chance of mismatch
	// generally if you move your actor position, this has to happen to approximately match their bounds
	if (ShouldBlendPhysicsBones())
	{
		if (IsRegistered())
		{
			BlendInPhysicsInternal(ThisTickFunction);
		}
	}
}

void USkeletalMeshComponent::UpdateClothTransformImp()
{
#if WITH_CLOTH_COLLISION_DETECTION
	if (ClothingSimulation && ClothingSimulation->ShouldSimulate())
	{
		// Component has moved, and there is something to simulate, update all external cloth collisions
		ClothingSimulation->ClearExternalCollisions();

		if (bCollideWithAttachedChildren)
		{
			CopyClothCollisionsToChildren();
		}

		if (ClothCollisionSources.Num() > 0)
		{
			CopyClothCollisionSources();
		}

		if (bCollideWithEnvironment)
		{
			ProcessClothCollisionWithEnvironment();
		}
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

#if !(UE_BUILD_SHIPPING)
	FTransform ComponentTransform = GetComponentTransform();
	if (ComponentTransform.GetRotation().ContainsNaN())
	{
		logOrEnsureNanError(TEXT("SkeletalMeshComponent::UpdateClothTransform found NaN in GetComponentTransform().GetRotation()"));
		ComponentTransform.SetRotation(FQuat(0.0f, 0.0f, 0.0f, 1.0f));
		SetComponentToWorld(ComponentTransform);
	}
	if (ComponentTransform.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("SkeletalMeshComponent::UpdateClothTransform still found NaN in GetComponentTransform() (wasn't the rotation)"));
		SetComponentToWorld(FTransform::Identity);
	}
#endif
}

void USkeletalMeshComponent::UpdateClothTransform(ETeleportType TeleportType)
{
	//Note that it's not safe to run the update here. This is because cloth sim could still be running on another thread. We defer it
	bPendingClothTransformUpdate = true;
	PendingTeleportType = ((TeleportType > PendingTeleportType) ? TeleportType : PendingTeleportType); 
}

void USkeletalMeshComponent::CheckClothTeleport()
{
	// Get the root bone transform
	FMatrix CurRootBoneMat = GetBoneMatrix(0);

	// CVar overrides
	bool bResetAfterTeleportOverride;
	float TeleportDistanceThresholdOverride;
	float ClothTeleportDistThresholdSquaredOverride;
	float TeleportRotationThresholdOverride;
	float ClothTeleportCosineThresholdInRadOverride;
	if (CVarClothTeleportOverride.GetValueOnGameThread())
	{
		bResetAfterTeleportOverride = CVarClothResetAfterTeleport.GetValueOnGameThread();
		TeleportDistanceThresholdOverride = CVarClothTeleportDistanceThreshold.GetValueOnGameThread();
		ClothTeleportDistThresholdSquaredOverride = FMath::Square(TeleportDistanceThresholdOverride);
		TeleportRotationThresholdOverride = CVarClothTeleportRotationThreshold.GetValueOnGameThread();
		ClothTeleportCosineThresholdInRadOverride = FMath::Cos(FMath::DegreesToRadians(TeleportRotationThresholdOverride));
	}
	else
	{
		bResetAfterTeleportOverride = bResetAfterTeleport;
		TeleportDistanceThresholdOverride = TeleportDistanceThreshold;
		ClothTeleportDistThresholdSquaredOverride = ClothTeleportDistThresholdSquared;
		TeleportRotationThresholdOverride = TeleportRotationThreshold;
		ClothTeleportCosineThresholdInRadOverride = ClothTeleportCosineThresholdInRad;
	}

	// distance check 
	// TeleportDistanceThreshold is greater than Zero and not teleported yet
	if(TeleportDistanceThresholdOverride > 0 && ClothTeleportMode == EClothingTeleportMode::None)
	{
		float DistSquared = FVector::DistSquared(PrevRootBoneMatrix.GetOrigin(), CurRootBoneMat.GetOrigin());
		if ( DistSquared > ClothTeleportDistThresholdSquaredOverride ) // if it has traveled too far
		{
			ClothTeleportMode = bResetAfterTeleportOverride ? EClothingTeleportMode::TeleportAndReset : EClothingTeleportMode::Teleport;
		}
	}

	// rotation check
	// if TeleportRotationThreshold is greater than Zero and the user didn't do force teleport
	if(TeleportRotationThresholdOverride > 0 && ClothTeleportMode == EClothingTeleportMode::None)
	{
		// Detect whether teleportation is needed or not
		// Rotation matrix's transpose means an inverse but can't use a transpose because this matrix includes scales
		FMatrix AInvB = CurRootBoneMat * PrevRootBoneMatrix.InverseFast();
		float Trace = AInvB.M[0][0] + AInvB.M[1][1] + AInvB.M[2][2];
		float CosineTheta = (Trace - 1.0f) / 2.0f; // trace = 1+2cos(theta) for a 3x3 matrix

		if ( CosineTheta < ClothTeleportCosineThresholdInRadOverride ) // has the root bone rotated too much
		{
			ClothTeleportMode = bResetAfterTeleportOverride ? EClothingTeleportMode::TeleportAndReset : EClothingTeleportMode::Teleport;
		}
	}

	PrevRootBoneMatrix = CurRootBoneMat;
}

FAutoConsoleTaskPriority CPrio_FParallelClothTask(
	TEXT("TaskGraph.TaskPriorities.ParallelClothTask"),
	TEXT("Task and thread priority for parallel cloth."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FParallelClothTask
{
	const USkeletalMeshComponent& SkeletalMeshComponent;
	float DeltaTime;

public:
	FParallelClothTask(const USkeletalMeshComponent& InSkeletalMeshComponent, float InDeltaTime)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
		, DeltaTime(InDeltaTime)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelClothTask, STATGROUP_TaskGraphTasks);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		if (CVarEnableClothPhysicsUseTaskThread.GetValueOnAnyThread() != 0)
		{
			return CPrio_FParallelClothTask.Get();
		}
		return ENamedThreads::GameThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FScopeCycleCounterUObject ContextScope(SkeletalMeshComponent.GetSkeletalMeshAsset());
		SCOPE_CYCLE_COUNTER(STAT_ClothTotalTime);
		CSV_SCOPED_TIMING_STAT(Animation, Cloth);

		if(SkeletalMeshComponent.ClothingSimulation)
		{
			SkeletalMeshComponent.ClothingSimulation->Simulate(SkeletalMeshComponent.ClothingSimulationContext);
		}
	}
};

// This task runs after the clothing task to perform a writeback of data from the simulation to the
// component. This is done on the gamethread as that is where the data is to be valid (ensures no other
// component ticks will be accessing during the writeback)
class FParallelClothCompletionTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelClothCompletionTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelClothCompletionTask, STATGROUP_TaskGraphTasks);
	}

	static ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothWriteback)
		// Perform the data writeback
		if(USkeletalMeshComponent* MeshComp = SkeletalMeshComponent.Get())
		{
			MeshComp->CompleteParallelClothSimulation();
		}
	}
};

bool USkeletalMeshComponent::RequiresPreEndOfFrameSync() const
{
	if ((ClothingSimulation != nullptr) && (CVarEnableClothPhysics.GetValueOnGameThread() != 0))
	{
		// By default we await the cloth task in the ClothTickFunction, but...
		// If we have cloth and have no game-thread dependencies on the cloth output, 
		// then we will wait for the cloth task in SendAllEndOfFrameUpdates.
		if (!ShouldWaitForClothInTickFunction())
		{
			return true;
		}
	}
	return Super::RequiresPreEndOfFrameSync();
}

void USkeletalMeshComponent::OnPreEndOfFrameSync()
{
	Super::OnPreEndOfFrameSync();

	HandleExistingParallelClothSimulation();
}


bool USkeletalMeshComponent::ShouldWaitForClothInTickFunction() const
{
	return bWaitForParallelClothTask || (CVarClothPhysicsTickWaitForParallelClothTask.GetValueOnAnyThread() != 0);
}

const TMap<int32, FClothSimulData>& USkeletalMeshComponent::GetCurrentClothingData_GameThread() const
{
	// We require the cloth tick to wait for the simulation results if we want to use them for some reason other than rendering.
	if (!ShouldWaitForClothInTickFunction())
	{
		// Log a one-time warning
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Use of USkeletalMeshComponent::GetCurrentClothingData_GameThread requires that property bWaitForParallelClothTask be set to true"));

		// Make it work for next frame
		const_cast<USkeletalMeshComponent*>(this)->bWaitForParallelClothTask = true;

		// Return an empty dataset
		return SEmptyClothSimulationData;
	}

	return CurrentSimulationData;
}

const TMap<int32, FClothSimulData>& USkeletalMeshComponent::GetCurrentClothingData_AnyThread() const
{
	// This is called during EndOfFrameUpdates, usually in a parallel-for loop. We need to be sure that
	// the cloth task (if there is one) is complete, but it cannpt be waited for here. See OnPreEndOfFrameUpdateSync
	// which is called just before EOF updates and is where we would have waited for the cloth task.
	if (!IsValidRef(ParallelClothTask) || ParallelClothTask->IsComplete())
	{
		return CurrentSimulationData;
	}

	return SEmptyClothSimulationData;
}

void USkeletalMeshComponent::UpdateClothStateAndSimulate(float DeltaTime, FTickFunction& ThisTickFunction)
{
	check(IsInGameThread());

	// If disabled or no simulation
	if (CVarEnableClothPhysics.GetValueOnGameThread() == 0 || !ClothingSimulation || bDisableClothSimulation)
	{
		return;
	}

	// If we simulate a clothing actor at 0s it will fill simulated positions and normals with NaNs.
	// we can skip all the work it is still doing, and get the desired result (frozen sim) by not
	// updating and simulating.
	if(DeltaTime == 0.0f)
	{
		return;
	}

	// Make sure we aren't already in flight from previous frame
	HandleExistingParallelClothSimulation();

#if WITH_CLOTH_COLLISION_DETECTION
	if (bCollideWithAttachedChildren)
	{
		ClothingSimulation->ClearExternalCollisions();

		CopyClothCollisionsToChildren();
		CopyChildrenClothCollisionsToParent();
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

	UpdateClothSimulationContext(DeltaTime);

	ParallelClothTask = TGraphTask<FParallelClothTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, DeltaTime);

	if (ShouldWaitForClothInTickFunction())
	{
		FGraphEventArray Prerequisites;
		Prerequisites.Add(ParallelClothTask);
		FGraphEventRef ClothCompletionEvent = TGraphTask<FParallelClothCompletionTask>::CreateTask(&Prerequisites, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this);
		ThisTickFunction.GetCompletionHandle()->DontCompleteUntil(ClothCompletionEvent);
	}
}

//This is the total cloth time split up among multiple computation (updating gpu, updating sim, etc...)
DECLARE_CYCLE_STAT(TEXT("Cloth Sim"), STAT_ClothSimTime, STATGROUP_Physics);

bool USkeletalMeshComponent::GetClothSimulatedPosition_GameThread(const FGuid& AssetGuid, int32 VertexIndex, FVector& OutSimulPos) const
{
	if(!GetSkeletalMeshAsset())
	{
		// Can't proceed without a mesh
		return false;
	}
		
	bool bSucceed = false;

	int32 AssetIndex = GetSkeletalMeshAsset()->GetClothingAssetIndex(AssetGuid);

	if(AssetIndex != INDEX_NONE)
	{
		const FClothSimulData* ActorData = GetCurrentClothingData_GameThread().Find(AssetIndex);

		if(ActorData && ActorData->Positions.IsValidIndex(VertexIndex))
		{
			OutSimulPos = (FVector)ActorData->Positions[VertexIndex];

			bSucceed = true;
		}
	}
	return bSucceed;
}

void USkeletalMeshComponent::TickClothing(float DeltaTime, FTickFunction& ThisTickFunction)
{
	bool bIsCompiling = false;
#if WITH_EDITOR
	bIsCompiling = (GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling());
#endif
	if (GetSkeletalMeshAsset() == nullptr || !ClothingSimulation || CVarEnableClothPhysics.GetValueOnGameThread() == 0 || bIsCompiling)
	{
		return;
	}

	// Use the component update flag to gate simulation to respect the always tick options
	bool bShouldTick = ((VisibilityBasedAnimTickOption < EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered) || bRecentlyRendered);

	if (bShouldTick)
	{
		UpdateClothStateAndSimulate(DeltaTime, ThisTickFunction);
	}
	else
	{
		ForceClothNextUpdateTeleportAndReset();
	}
}

void USkeletalMeshComponent::GetUpdateClothSimulationData(TMap<int32, FClothSimulData>& OutClothSimData, USkeletalMeshComponent* OverrideLocalRootComponent)
{
	if(CVarEnableClothPhysics.GetValueOnAnyThread() == 0)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_ClothTotalTime);
	CSV_SCOPED_TIMING_STAT(Animation, Cloth);

	if(bDisableClothSimulation)
	{
		OutClothSimData.Reset();
		return;
	}

	if(ClothingSimulation)
	{
		ClothingSimulation->GetSimulationData(OutClothSimData, this, OverrideLocalRootComponent);
		return;
	}
}

void USkeletalMeshComponent::GetUpdateClothSimulationData_AnyThread(TMap<int32, FClothSimulData>& OutClothSimulData, FMatrix& OutLocalToWorld, float& OutClothBlendWeight)
{
	OutLocalToWorld = GetComponentToWorld().ToMatrixWithScale();

	const USkeletalMeshComponent* const LeaderPoseSkeletalMeshComponent = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get());
	if (LeaderPoseSkeletalMeshComponent && bBindClothToLeaderComponent)
	{
		OutClothBlendWeight = ClothBlendWeight;
		OutClothSimulData = LeaderPoseSkeletalMeshComponent->GetCurrentClothingData_AnyThread();
	}
	else if (!bDisableClothSimulation && !bBindClothToLeaderComponent)
	{
		OutClothBlendWeight = ClothBlendWeight;
		OutClothSimulData = GetCurrentClothingData_AnyThread();
	}
	else
	{
		OutClothSimulData.Reset();
	}

	// Blend cloth out whenever the simulation data is invalid
	if (!OutClothSimulData.Num())
	{
		OutClothBlendWeight = 0.0f;
	}
}

void USkeletalMeshComponent::WaitForExistingParallelClothSimulation_GameThread()
{
	// Should only kick new parallel cloth simulations from game thread, so should be safe to also wait for existing ones there.
	check(IsInGameThread());
	HandleExistingParallelClothSimulation();
}

void USkeletalMeshComponent::DebugDrawClothing(FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR && ENABLE_DRAW_DEBUG

	if(ClothingSimulation)
	{
		FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

		if(ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(ClothingSimulationFactory->GetFName()))
		{
			Extender->DebugDrawSimulation(ClothingSimulation, this, PDI);
		}

		return;
	}

#endif
}

void USkeletalMeshComponent::DebugDrawClothingTexts(FCanvas* Canvas, const FSceneView* SceneView)
{
#if WITH_EDITOR && ENABLE_DRAW_DEBUG

	if(ClothingSimulation)
	{
		FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

		if(ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(ClothingSimulationFactory->GetFName()))
		{
			Extender->DebugDrawSimulationTexts(ClothingSimulation, this, Canvas, SceneView);
		}

		return;
	}

#endif
}

void USkeletalMeshComponent::SetAllMassScale(float InMassScale)
{
	// Apply mass scale to each child body
	for(FBodyInstance* BI : Bodies)
	{
		if (BI->IsValidBodyInstance())
		{
			BI->SetMassScale(InMassScale);
		}
	}
}


float USkeletalMeshComponent::GetMass() const
{
	float Mass = 0.0f;
	for (int32 i=0; i < Bodies.Num(); ++i)
	{
		FBodyInstance* BI = Bodies[i];

		if (BI->IsValidBodyInstance())
		{
			Mass += BI->GetBodyMass();
		}
	}
	return Mass;
}

float USkeletalMeshComponent::GetBoneMass(FName BoneName, bool bScaleMass) const
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		float Scale = 1.0f;
		if (bScaleMass)
		{
			Scale = BI->MassScale;
		}
		return Scale*BI->GetBodyMass();
	}

	return 0.0f;
}

FVector USkeletalMeshComponent::GetSkeletalCenterOfMass() const
{
	FVector Location = FVector::ZeroVector;
	float Mass = 0.0f;
	for (int32 i = 0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		if (BI->IsValidBodyInstance())
		{
			float BodyMass = BI->MassScale*BI->GetBodyMass();
			Location += BodyMass*BI->GetCOMPosition();
			Mass += BodyMass;
		}
	}

	return Location / Mass;
}

void USkeletalMeshComponent::SetAllUseCCD(bool InUseCCD)
{
	// Apply CCD setting to each child body
	for (FBodyInstance* BI : Bodies)
	{
		if (BI->IsValidBodyInstance())
		{
			BI->SetUseCCD(InUseCCD);
		}
	}
}

// blueprint callable methods 
float USkeletalMeshComponent::GetClothMaxDistanceScale() const
{
	return ClothMaxDistanceScale;
}

void USkeletalMeshComponent::SetClothMaxDistanceScale(float Scale)
{
	ClothMaxDistanceScale = Scale;
}

void USkeletalMeshComponent::ResetClothTeleportMode()
{
	ClothTeleportMode = EClothingTeleportMode::None;
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleport()
{
	ClothTeleportMode = EClothingTeleportMode::Teleport;
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleportAndReset()
{
	ClothTeleportMode = EClothingTeleportMode::TeleportAndReset;
}

FTransform USkeletalMeshComponent::GetComponentTransformFromBodyInstance(FBodyInstance* UseBI)
{
	if (PhysicsTransformUpdateMode == EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform)
	{
		// undo root transform so that it only moves according to what actor itself suppose to move
		const FTransform& BodyTransform = UseBI->GetUnrealWorldTransform();
		return RootBodyData.TransformToRoot * BodyTransform;
	}
	else
	{
		return GetComponentTransform();
	}
}


Chaos::FPhysicsObject* USkeletalMeshComponent::GetPhysicsObjectById(Chaos::FPhysicsObjectId Id) const
{
	if (!Bodies.IsValidIndex(Id) || !Bodies[Id] || !Bodies[Id]->ActorHandle)
	{
		return nullptr;
	}
	return Bodies[Id]->ActorHandle->GetPhysicsObject();
}

Chaos::FPhysicsObject* USkeletalMeshComponent::GetPhysicsObjectByName(const FName& Name) const
{
	FBodyInstance* Body = GetBodyInstance(Name);
	if (!Body || !Body->ActorHandle)
	{
		return nullptr;
	}

	return Body->ActorHandle->GetPhysicsObject();
}

TArray<Chaos::FPhysicsObject*> USkeletalMeshComponent::GetAllPhysicsObjects() const
{
	TArray<Chaos::FPhysicsObject*> Objects;
	Objects.Reserve(Bodies.Num());
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		Objects.Add(GetPhysicsObjectById(Index));
	}
	return Objects;
}

#undef LOCTEXT_NAMESPACE
