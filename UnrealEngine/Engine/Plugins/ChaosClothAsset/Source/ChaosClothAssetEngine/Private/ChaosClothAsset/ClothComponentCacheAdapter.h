// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Adapters/CacheAdapter.h"

class UChaosClothComponent;
namespace Chaos
{
	class FClothingSimulationSolver;
};

namespace UE::Chaos::ClothAsset
{	
	class FClothSimulationProxy;

	/**
	 * Skeletal mesh cache adapter to be able to cache cloth simulation datas through the chaos cache system
	 */
	class FClothComponentCacheAdapter : public ::Chaos::FComponentCacheAdapter
	{
	public:
		
		virtual ~FClothComponentCacheAdapter() = default;

		// ~Begin FComponentCacheAdapter interface
		virtual SupportType            SupportsComponentClass(UClass* InComponentClass) const override;
		virtual UClass*                GetDesiredClass() const override;
		virtual uint8                  GetPriority() const override;
		virtual FGuid                  GetGuid() const override;
		virtual bool                   ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const override;
		virtual ::Chaos::FPhysicsSolver* GetComponentSolver(UPrimitiveComponent* InComponent) const override;
		virtual ::Chaos::FPhysicsSolverEvents* BuildEventsSolver(UPrimitiveComponent* InComponent) const override;
		virtual void				   SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, ::Chaos::FReal InTime) const override;
		virtual bool                   InitializeForRecord(UPrimitiveComponent* InComponent, UChaosCache* InCache) override;
		virtual bool                   InitializeForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache, float InTime) override;
		virtual void                   Record_PostSolve(UPrimitiveComponent* InComp, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, ::Chaos::FReal InTime) const override;
		virtual void                   Playback_PreSolve(UPrimitiveComponent* 				InComponent,
										 UChaosCache*										InCache,
										 ::Chaos::FReal										InTime,
										 FPlaybackTickRecord&								TickRecord,
										 TArray<::Chaos::TPBDRigidParticleHandle<::Chaos::FReal, 3>*>& OutUpdatedRigids) const override;
		virtual void                   WaitForSolverTasks(UPrimitiveComponent* InComponent) const override;
		// ~End FComponentCacheAdapter interface

	private :
		/** Return the cloth component.
		 */
		UChaosClothComponent* GetClothComponent(UPrimitiveComponent* InComponent) const;

		FClothSimulationProxy* GetProxy(UPrimitiveComponent* InComponent) const;
		::Chaos::FClothingSimulationSolver* GetClothSolver(UPrimitiveComponent* InComponent) const;
	};
}    // namespace Chaos
