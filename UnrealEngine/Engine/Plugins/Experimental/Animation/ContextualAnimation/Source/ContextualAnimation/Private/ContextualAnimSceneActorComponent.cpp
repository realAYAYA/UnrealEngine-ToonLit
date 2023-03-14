// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimSceneActorComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "ContextualAnimSelectionCriterion.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "ContextualAnimManager.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimUtilities.h"
#include "AnimNotifyState_IKWindow.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContextualAnimSceneActorComponent)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TAutoConsoleVariable<int32> CVarContextualAnimIKDebug(TEXT("a.ContextualAnim.IK.Debug"), 0, TEXT("Draw Debug IK Targets"));
TAutoConsoleVariable<float> CVarContextualAnimIKDrawDebugLifetime(TEXT("a.ContextualAnim.IK.DrawDebugLifetime"), 0, TEXT("Draw Debug Duration"));
TAutoConsoleVariable<float> CVarContextualAnimIKForceAlpha(TEXT("a.ContextualAnim.IK.ForceAlpha"), -1.f, TEXT("Override Alpha value for all the targets. -1 = Disable"));
#endif

UContextualAnimSceneActorComponent::UContextualAnimSceneActorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UContextualAnimSceneActorComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UContextualAnimSceneActorComponent, Bindings, COND_SimulatedOnly);
}

void UContextualAnimSceneActorComponent::OnRep_Bindings()
{
	if(Bindings.Num() > 0)
	{
		if (const FContextualAnimSceneBinding* Binding = Bindings.FindBindingByActor(GetOwner()))
		{
			// Start listening to TickPose if we joined an scene where we need IK
			if (Bindings.GetIKTargetDefContainerFromBinding(*Binding).IKTargetDefs.Num() > 0)
			{
				USkeletalMeshComponent* SkelMeshComp = UContextualAnimUtilities::TryGetSkeletalMeshComponent(GetOwner());
				if (SkelMeshComp && !SkelMeshComp->OnTickPose.IsBoundToObject(this))
				{
					SkelMeshComp->OnTickPose.AddUObject(this, &UContextualAnimSceneActorComponent::OnTickPose);
				}
			}

			OnJoinedSceneDelegate.Broadcast(this);
		}
	}
	else
	{
		USkeletalMeshComponent* SkelMeshComp = UContextualAnimUtilities::TryGetSkeletalMeshComponent(GetOwner());
		if (SkelMeshComp && SkelMeshComp->OnTickPose.IsBoundToObject(this))
		{
			SkelMeshComp->OnTickPose.RemoveAll(this);
		}

		OnLeftSceneDelegate.Broadcast(this);
	}
}

FBoxSphereBounds UContextualAnimSceneActorComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// The option of having an SceneAsset and draw options on this component may go away in the future anyway, replaced by smart objects.
	const float Radius = SceneAsset && SceneAsset->HasValidData() ? SceneAsset->GetRadius() : 0.f;
	return FBoxSphereBounds(FSphere(GetComponentTransform().GetLocation(), Radius));
}

void UContextualAnimSceneActorComponent::OnRegister()
{
	Super::OnRegister();

	UContextualAnimManager* ContextAnimManager = UContextualAnimManager::Get(GetWorld());
	if (ensure(!bRegistered) && ContextAnimManager)
	{
		ContextAnimManager->RegisterSceneActorComponent(this);
		bRegistered = true;
	}
}

void UContextualAnimSceneActorComponent::OnUnregister()
{
	Super::OnUnregister();

	UContextualAnimManager* ContextAnimManager = UContextualAnimManager::Get(GetWorld());
	if (bRegistered && ContextAnimManager)
	{
		ContextAnimManager->UnregisterSceneActorComponent(this);
		bRegistered = false;
	}
}

void UContextualAnimSceneActorComponent::OnJoinedScene(const FContextualAnimSceneBindings& InBindings)
{
	if (const FContextualAnimSceneBinding* Binding = InBindings.FindBindingByActor(GetOwner()))
	{
		Bindings = InBindings;

		// Start listening to TickPose if we joined an scene where we need IK
		if (Bindings.GetIKTargetDefContainerFromBinding(*Binding).IKTargetDefs.Num() > 0)
		{
			USkeletalMeshComponent* SkelMeshComp = UContextualAnimUtilities::TryGetSkeletalMeshComponent(GetOwner());
			if (SkelMeshComp && !SkelMeshComp->OnTickPose.IsBoundToObject(this))
			{
				SkelMeshComp->OnTickPose.AddUObject(this, &UContextualAnimSceneActorComponent::OnTickPose);
			}
		}

		OnJoinedSceneDelegate.Broadcast(this);
	}
}

void UContextualAnimSceneActorComponent::OnLeftScene()
{
	if (const FContextualAnimSceneBinding* Binding = Bindings.FindBindingByActor(GetOwner()))
	{
		OnLeftSceneDelegate.Broadcast(this);

		// Stop listening to TickPose if we were
		USkeletalMeshComponent* SkelMeshComp = UContextualAnimUtilities::TryGetSkeletalMeshComponent(GetOwner());
		if (SkelMeshComp && SkelMeshComp->OnTickPose.IsBoundToObject(this))
		{
			SkelMeshComp->OnTickPose.RemoveAll(this);
		}

		Bindings.Reset();
	}
}

void UContextualAnimSceneActorComponent::OnTickPose(class USkinnedMeshComponent* SkinnedMeshComponent, float DeltaTime, bool bNeedsValidRootMotion)
{
	//@TODO: Check for LOD to prevent this update if the actor is too far away
	UpdateIKTargets();
}

void UContextualAnimSceneActorComponent::UpdateIKTargets()
{
	IKTargets.Reset();

	const FContextualAnimSceneBinding* BindingPtr = Bindings.FindBindingByActor(GetOwner());
	if (BindingPtr == nullptr)
	{
		return;
	}

	const FAnimMontageInstance* MontageInstance = BindingPtr->GetAnimMontageInstance();
	if(MontageInstance == nullptr)
	{
		return;
	}

	const TArray<FContextualAnimIKTargetDefinition>& IKTargetDefs = Bindings.GetIKTargetDefContainerFromBinding(*BindingPtr).IKTargetDefs;
	for (const FContextualAnimIKTargetDefinition& IKTargetDef : IKTargetDefs)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bDrawDebugEnable = CVarContextualAnimIKDebug.GetValueOnGameThread() > 0;
		const float DrawDebugDuration = CVarContextualAnimIKDrawDebugLifetime.GetValueOnGameThread();
		FTransform IKTargetParentTransformForDebug = FTransform::Identity;
#endif

		FTransform IKTargetTransform = FTransform::Identity;

		float Alpha = UAnimNotifyState_IKWindow::GetIKAlphaValue(IKTargetDef.GoalName, MontageInstance);

		// @TODO: IKTargetTransform will be off by 1 frame if we tick before target. 
		// Should we at least add an option to the SceneAsset to setup tick dependencies or should this be entirely up to the user?

		if (const FContextualAnimSceneBinding* TargetBinding = Bindings.FindBindingByRole(IKTargetDef.TargetRoleName))
		{
			if (const USkeletalMeshComponent* TargetSkelMeshComp = TargetBinding->GetSkeletalMeshComponent())
			{
				if (IKTargetDef.Provider == EContextualAnimIKTargetProvider::Autogenerated)
				{
					const FTransform IKTargetParentTransform = TargetSkelMeshComp->GetSocketTransform(IKTargetDef.TargetBoneName);

					const float Time = MontageInstance->GetPosition();
					IKTargetTransform = Bindings.GetIKTargetTransformFromBinding(*BindingPtr, IKTargetDef.GoalName, Time) * IKTargetParentTransform;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (bDrawDebugEnable)
					{
						IKTargetParentTransformForDebug = IKTargetParentTransform;
					}
#endif
				}
				else if (IKTargetDef.Provider == EContextualAnimIKTargetProvider::Bone)
				{
					IKTargetTransform = TargetSkelMeshComp->GetSocketTransform(IKTargetDef.TargetBoneName);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (bDrawDebugEnable)
					{
						IKTargetParentTransformForDebug = TargetSkelMeshComp->GetSocketTransform(TargetSkelMeshComp->GetParentBone(IKTargetDef.TargetBoneName));
					}
#endif
				}
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const float ForcedAlphaValue = CVarContextualAnimIKForceAlpha.GetValueOnGameThread();
		if (ForcedAlphaValue > 0.f)
		{
			Alpha = FMath::Clamp(ForcedAlphaValue, 0.f, 1.f);
		}

		if (bDrawDebugEnable)
		{
			const float DrawThickness = 0.5f;
			const FColor DrawColor = FColor::MakeRedToGreenColorFromScalar(Alpha);

			DrawDebugLine(GetWorld(), IKTargetParentTransformForDebug.GetLocation(), IKTargetTransform.GetLocation(), DrawColor, false, DrawDebugDuration, 0, DrawThickness);
			DrawDebugCoordinateSystem(GetWorld(), IKTargetTransform.GetLocation(), IKTargetTransform.Rotator(), 10.f, false, DrawDebugDuration, 0, DrawThickness);
			//DrawDebugSphere(GetWorld(), IKTargetTransform.GetLocation(), 5.f, 12, DrawColor, false, 0.f, 0, DrawThickness  );

			//DrawDebugString(GetWorld(), IKTargetTransform.GetLocation(), FString::Printf(TEXT("%s (%f)"), *IKTargetDef.AlphaCurveName.ToString(), Alpha));
		}
#endif

		// Convert IK Target to mesh space
		//IKTargetTransform = IKTargetTransform.GetRelativeTransform(BindingPtr->GetSkeletalMeshComponent()->GetComponentTransform());

		IKTargets.Add(FContextualAnimIKTarget(IKTargetDef.GoalName, Alpha, IKTargetTransform));
	}
}

void UContextualAnimSceneActorComponent::AddIKGoals_Implementation(TMap<FName, FIKRigGoal>& OutGoals)
{
	OutGoals.Reserve(IKTargets.Num());

	for(const FContextualAnimIKTarget& IKTarget : IKTargets)
	{
		FIKRigGoal Goal;
		Goal.Name = IKTarget.GoalName;
		Goal.Position = IKTarget.Transform.GetLocation();
		Goal.Rotation = IKTarget.Transform.Rotator();
		Goal.PositionAlpha = IKTarget.Alpha;
		Goal.RotationAlpha = IKTarget.Alpha;
		Goal.PositionSpace = EIKRigGoalSpace::World;
		Goal.RotationSpace = EIKRigGoalSpace::World;
		OutGoals.Add(Goal.Name, Goal);
	}
}

const FContextualAnimIKTarget& UContextualAnimSceneActorComponent::GetIKTargetByGoalName(FName GoalName) const
{
	const FContextualAnimIKTarget* IKTargetPtr = IKTargets.FindByPredicate([GoalName](const FContextualAnimIKTarget& IKTarget){
		return IKTarget.GoalName == GoalName;
	});

	return IKTargetPtr ? *IKTargetPtr : FContextualAnimIKTarget::InvalidIKTarget;
}

FPrimitiveSceneProxy* UContextualAnimSceneActorComponent::CreateSceneProxy()
{
	class FSceneActorCompProxy final : public FPrimitiveSceneProxy
	{
	public:

		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		FSceneActorCompProxy(const UContextualAnimSceneActorComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
			, SceneAssetPtr(InComponent->SceneAsset)
		{
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			const UContextualAnimSceneAsset* Asset = SceneAssetPtr.Get();
			if (Asset == nullptr)
			{
				return;
			}

			const FMatrix& LocalToWorld = GetLocalToWorld();
			const FTransform ToWorldTransform = FTransform(LocalToWorld);

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					// Taking into account the min and maximum drawing distance
					const float DistanceSqr = (View->ViewMatrices.GetViewOrigin() - LocalToWorld.GetOrigin()).SizeSquared();
					if (DistanceSqr < FMath::Square(GetMinDrawDistance()) || DistanceSqr > FMath::Square(GetMaxDrawDistance()))
					{
						continue;
					}

					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

					//DrawCircle(PDI, ToWorldTransform.GetLocation(), FVector(1, 0, 0), FVector(0, 1, 0), FColor::Red, SceneAssetPtr->GetRadius(), 12, SDPG_World, 1.f);

					SceneAssetPtr->ForEachAnimTrack([=](const FContextualAnimTrack& AnimTrack)
					{
						if (AnimTrack.Role != SceneAssetPtr->GetPrimaryRole())
						{
							// Draw Entry Point
							const FTransform EntryTransform = (AnimTrack.GetAlignmentTransformAtEntryTime() * ToWorldTransform);
							DrawCoordinateSystem(PDI, EntryTransform.GetLocation(), EntryTransform.Rotator(), 20.f, SDPG_World, 3.f);

							// Draw Sync Point
							const FTransform SyncPoint = AnimTrack.GetAlignmentTransformAtSyncTime() * ToWorldTransform;
							DrawCoordinateSystem(PDI, SyncPoint.GetLocation(), SyncPoint.Rotator(), 20.f, SDPG_World, 3.f);

							FLinearColor DrawColor = FLinearColor::White;
							for (const UContextualAnimSelectionCriterion* Criterion : AnimTrack.SelectionCriteria)
							{
								if (const UContextualAnimSelectionCriterion_TriggerArea* Spatial = Cast<UContextualAnimSelectionCriterion_TriggerArea>(Criterion))
								{
									const float HalfHeight = Spatial->Height / 2.f;
									const int32 LastIndex = Spatial->PolygonPoints.Num() - 1;
									for (int32 Idx = 0; Idx <= LastIndex; Idx++)
									{
										const FVector P0 = ToWorldTransform.TransformPositionNoScale(Spatial->PolygonPoints[Idx]);
										const FVector P1 = ToWorldTransform.TransformPositionNoScale(Spatial->PolygonPoints[Idx == LastIndex ? 0 : Idx + 1]);

										PDI->DrawLine(P0, P1, DrawColor, SDPG_Foreground, 2.f);
										PDI->DrawLine(P0 + FVector::UpVector * Spatial->Height, P1 + FVector::UpVector * Spatial->Height, DrawColor, SDPG_Foreground, 2.f);

										PDI->DrawLine(P0, P0 + FVector::UpVector * Spatial->Height, DrawColor, SDPG_Foreground, 2.f);
									}
								}
							}
						}

						return UE::ContextualAnim::EForEachResult::Continue;
					});
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			const bool bShowForCollision = View->Family->EngineShowFlags.Collision;
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bNormalTranslucency = Result.bSeparateTranslucency = IsShown(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint(void) const override
		{
			return(sizeof(*this) + GetAllocatedSize());
		}

		uint32 GetAllocatedSize(void) const
		{
			return(FPrimitiveSceneProxy::GetAllocatedSize());
		}

	private:
		TWeakObjectPtr<const UContextualAnimSceneAsset> SceneAssetPtr;
	};

	if(bEnableDebug)
	{
		return new FSceneActorCompProxy(this);
	}

	return nullptr;
}
