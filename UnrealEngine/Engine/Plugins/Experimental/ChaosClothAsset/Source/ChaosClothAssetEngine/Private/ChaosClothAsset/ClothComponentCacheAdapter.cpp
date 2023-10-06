// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothComponentCacheAdapter.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/PBDEvolution.h"
#include "ChaosCloth/ChaosClothingCacheSchema.h"
#include "ChaosCloth/ChaosClothingSimulation.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "Components/SkeletalMeshComponent.h"

namespace UE::Chaos::ClothAsset
{
	::Chaos::FComponentCacheAdapter::SupportType FClothComponentCacheAdapter::SupportsComponentClass(UClass* InComponentClass) const
	{
		const UClass* Desired = GetDesiredClass();
		if(InComponentClass == Desired)
		{
			return ::Chaos::FComponentCacheAdapter::SupportType::Direct;
		}
		else if(InComponentClass->IsChildOf(Desired))
		{
			return ::Chaos::FComponentCacheAdapter::SupportType::Derived;
		}

		return ::Chaos::FComponentCacheAdapter::SupportType::None;
	}

	UClass* FClothComponentCacheAdapter::GetDesiredClass() const
	{
		return UChaosClothComponent::StaticClass();
	}

	uint8 FClothComponentCacheAdapter::GetPriority() const
	{
		return EngineAdapterPriorityBegin;
	}

	void FClothComponentCacheAdapter::Record_PostSolve(UPrimitiveComponent* InComponent, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, ::Chaos::FReal InTime) const
	{
		ensureMsgf(false, TEXT("Record not implemented."));
	}

	void FClothComponentCacheAdapter::Playback_PreSolve(UPrimitiveComponent* InComponent, UChaosCache* InCache, ::Chaos::FReal InTime, FPlaybackTickRecord& TickRecord, TArray<::Chaos::TPBDRigidParticleHandle<::Chaos::FReal, 3>*>& OutUpdatedRigids) const
	{
		ensureMsgf(false, TEXT("Playback not implemented."));
	}

	FGuid FClothComponentCacheAdapter::GetGuid() const
	{
		FGuid NewGuid;
		checkSlow(FGuid::Parse(TEXT("C704F4F536A34CD4973ABBB7BFEEE432"), NewGuid));
		return NewGuid;
	}

	bool FClothComponentCacheAdapter::ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const
	{
		const UChaosClothComponent* ClothComp = GetClothComponent(InComponent);
		return ClothComp && ::Chaos::FClothingCacheSchema::CacheIsValidForPlayback(InCache);
	}

	::Chaos::FPhysicsSolverEvents* FClothComponentCacheAdapter::BuildEventsSolver(UPrimitiveComponent* InComponent) const
	{
		ensureMsgf(false, TEXT("Playback or record not implemented."));
		return nullptr;
	}
	
	::Chaos::FPhysicsSolver* FClothComponentCacheAdapter::GetComponentSolver(UPrimitiveComponent* InComponent) const
	{
		return nullptr;
	}
	
	void FClothComponentCacheAdapter::SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, ::Chaos::FReal InTime) const
	{
		if (!InCache || InCache->GetDuration() == 0.0f)
		{
			return;
		}

		if (UChaosClothComponent* ClothComp = GetClothComponent(InComponent))
		{
			FClothingSimulationCacheData CacheData;
			::Chaos::FClothingCacheSchema::LoadCacheData(InCache, InTime, CacheData);

			FClothSimulationProxy* Proxy = ClothComp->ClothSimulationProxy.Get();

			Proxy->Tick_GameThread(0, &CacheData);
			Proxy->CompleteParallelSimulation_GameThread();
			ClothComp->MarkRenderDynamicDataDirty();
			ClothComp->DoDeferredRenderUpdates_Concurrent();
		}
	}

	bool FClothComponentCacheAdapter::InitializeForRecord(UPrimitiveComponent* InComponent, UChaosCache* InCache)
	{
		return true;
	}

	bool FClothComponentCacheAdapter::InitializeForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache, float InTime)
	{
		::Chaos::EnsureIsInGameThreadContext();
		return true;
	}

	UChaosClothComponent* FClothComponentCacheAdapter::GetClothComponent(UPrimitiveComponent* InComponent) const
	{
		if (InComponent)
		{
			if (UChaosClothComponent* ClothComp = Cast<UChaosClothComponent>(InComponent))
			{
				return ClothComp;
			}
			else if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(InComponent))
			{
				TArray<USceneComponent*> Children;
				SkelComp->GetChildrenComponents(true, Children);
				TArray<USceneComponent*> ClothComps = Children.FilterByPredicate([](USceneComponent* Child) { return Child->IsA<UChaosClothComponent>(); });
				if (ClothComps.Num() > 1)
				{
					ensureMsgf(false, TEXT("Found more than one cloth component attached to a skeletal mesh component. This is not yet supported."));
				}
				else if (ClothComps.Num() == 1)
				{
					return Cast<UChaosClothComponent>(ClothComps[0]);
				}
			}
		}
		return nullptr;
	}

}    // namespace Chaos
