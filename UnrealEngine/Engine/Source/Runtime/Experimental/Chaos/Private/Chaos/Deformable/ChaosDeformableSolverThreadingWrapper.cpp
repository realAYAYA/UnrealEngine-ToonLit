// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Deformable/ChaosDeformableSolver.h"
#include "Chaos/Deformable/ChaosDeformableSolverTypes.h"
#include "Chaos/Deformable/ChaosDeformableSolverProxy.h"

#include "Chaos/TriangleMesh.h"
#include "Chaos/PBDAltitudeSpringConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDTetConstraints.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/XPBDCorotatedConstraints.h"
#include "Chaos/XPBDVolumeConstraints.h"
#include "Chaos/XPBDCorotatedFiberConstraints.h"
#include "Chaos/Plane.h"
#include "Chaos/Utilities.h"
#include "Chaos/PBDEvolution.h"
#include "Containers/StringConv.h"
#include "CoreMinimal.h"
#include "GeometryCollection/Facades/CollectionKinematicBindingFacade.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"


namespace Chaos::Softs
{
	void FDeformableSolver::FPhysicsThreadAccess::Reset(const FDeformableSolverProperties& InProps)
	{
		Solver.Reset(InProps);
	}

	bool FDeformableSolver::FPhysicsThreadAccess::Advance(FSolverReal DeltaTime)
	{
		return Solver.Advance(DeltaTime);
	}

	void FDeformableSolver::FPhysicsThreadAccess::InitializeSimulationObjects()
	{
		Solver.InitializeSimulationObjects();
	}

	void FDeformableSolver::FPhysicsThreadAccess::InitializeSimulationObject(FThreadingProxy& InProxy)
	{
		Solver.InitializeSimulationObject(InProxy);
	}

	void FDeformableSolver::FPhysicsThreadAccess::InitializeCollisionBodies()
	{
		return Solver.InitializeCollisionBodies();
	}

	void FDeformableSolver::FPhysicsThreadAccess::InitializeKinematicState(FThreadingProxy& InProxy)
	{
		Solver.InitializeKinematicState(InProxy);
	}

	void FDeformableSolver::FPhysicsThreadAccess::InitializeSelfCollisionVariables()
	{
		Solver.InitializeSelfCollisionVariables();
	}

	void FDeformableSolver::FGameThreadAccess::PushInputPackage(int32 InFrame, FDeformableDataMap&& InPackage)
	{
		Solver.PushInputPackage(InFrame, MoveTemp(InPackage));
	}

	TUniquePtr<FDeformablePackage> FDeformableSolver::FPhysicsThreadAccess::PullInputPackage()
	{
		return Solver.PullInputPackage();
	}

	void FDeformableSolver::FPhysicsThreadAccess::UpdateProxyInputPackages()
	{
		return Solver.UpdateProxyInputPackages();
	}

	void FDeformableSolver::FPhysicsThreadAccess::TickSimulation(FSolverReal DeltaTime)
	{
		Solver.TickSimulation(DeltaTime);
	}

	void FDeformableSolver::FPhysicsThreadAccess::PushOutputPackage(int32 InFrame, FDeformableDataMap&& InPackage)
	{
		Solver.PushOutputPackage(InFrame, MoveTemp(InPackage));
	}

	TUniquePtr<FDeformablePackage>  FDeformableSolver::FGameThreadAccess::PullOutputPackage()
	{
		return Solver.PullOutputPackage();
	}

	void  FDeformableSolver::FGameThreadAccess::AddProxy(TUniquePtr<FThreadingProxy> InObject)
	{
		return Solver.AddProxy(TUniquePtr<FThreadingProxy>(InObject.Release()));
	}

	void  FDeformableSolver::FPhysicsThreadAccess::UpdateOutputState(FThreadingProxy& InProxy)
	{
		Solver.UpdateOutputState(InProxy);
	}

	void  FDeformableSolver::FPhysicsThreadAccess::WriteFrame(FThreadingProxy& InProxy, const FSolverReal DeltaTime)
	{
		Solver.WriteFrame(InProxy, DeltaTime);
	}

	void  FDeformableSolver::FPhysicsThreadAccess::WriteTrisGEO(const FSolverParticles& Particles, const TArray<TVec3<int32>>& Mesh)
	{
		Solver.WriteTrisGEO(Particles, Mesh);
	}
}; // Namespace Chaos::Softs