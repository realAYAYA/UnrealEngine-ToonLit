// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/ChaosInterfaceWrapper.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"


namespace ChaosInterface
{
	FBodyInstance* GetUserData(const Chaos::FGeometryParticle& Actor)
	{
		void* UserData = Actor.UserData();
		if (UserData)
		{
			FBodyInstance* BodyInstance = FChaosUserData::Get<FBodyInstance>(UserData);
			if (!BodyInstance)
			{
				// Check if we appended a custom entity
				FChaosUserEntityAppend* ChaosUserEntityAppend = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
				if (ChaosUserEntityAppend)
				{
					BodyInstance = FChaosUserData::Get<FBodyInstance>(ChaosUserEntityAppend->ChaosUserData);
				}
			}
			return BodyInstance;
		}
		return nullptr;
	}

	UPhysicalMaterial* GetUserData(const Chaos::FChaosPhysicsMaterial& Material)
	{
		void* UserData = Material.UserData;
		return UserData ? FChaosUserData::Get<UPhysicalMaterial>(UserData) : nullptr;
	}

	UPrimitiveComponent* GetPrimitiveComponentFromUserData(const Chaos::FGeometryParticle& Actor)
	{
		void* UserData = Actor.UserData();
		if (UserData)
		{
			UPrimitiveComponent* PrimitiveComponent = FChaosUserData::Get<UPrimitiveComponent>(UserData);
			if (!PrimitiveComponent)
			{
				// Check if we appended a custom entity
				FChaosUserEntityAppend* ChaosUserEntityAppend = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
				if (ChaosUserEntityAppend)
				{
					PrimitiveComponent = FChaosUserData::Get<UPrimitiveComponent>(ChaosUserEntityAppend->ChaosUserData);
				}
			}
			return PrimitiveComponent;
		}
		return nullptr;
	}

	FScopedSceneReadLock::FScopedSceneReadLock(FPhysScene_Chaos& SceneIn)
		: Solver(SceneIn.GetSolver())
	{
		if(Solver)
		{
			Solver->GetExternalDataLock_External().ReadLock();
		}
	}

	FScopedSceneReadLock::~FScopedSceneReadLock()
	{
		if(Solver)
		{
			Solver->GetExternalDataLock_External().ReadUnlock();
		}
	}
}
