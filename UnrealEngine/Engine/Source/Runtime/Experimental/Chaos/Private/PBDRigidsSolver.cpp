// Copyright Epic Games, Inc. All Rights Reserved.

#include "PBDRigidsSolver.h"

#include "Async/AsyncWork.h"
#include "Chaos/ChaosArchive.h"
#include "Chaos/PBDCollisionConstraintsUtil.h"
#include "Chaos/Utilities.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "ChaosStats.h"
#include "ChaosSolversModule.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "EventDefaults.h"
#include "EventsData.h"
#include "RewindData.h"
#include "ChaosSolverConfiguration.h"
#include "Chaos/PullPhysicsDataImp.h"
#include "Chaos/PhysicsSolverBaseImpl.h"

#include "ProfilingDebugging/CsvProfiler.h"
#include "ChaosLog.h"

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY_STATIC(LogPBDRigidsSolver, Log, All);

// Stat Counters
DECLARE_DWORD_COUNTER_STAT(TEXT("NumDisabledBodies"), STAT_ChaosCounter_NumDisabledBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumBodies"), STAT_ChaosCounter_NumBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumDynamicBodies"), STAT_ChaosCounter_NumDynamicBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumActiveDynamicBodies"), STAT_ChaosCounter_NumActiveDynamicBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumKinematicBodies"), STAT_ChaosCounter_NumKinematicBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumStaticBodies"), STAT_ChaosCounter_NumStaticBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumGeomCollBodies"), STAT_ChaosCounter_NumGeometryCollectionBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumIslands"), STAT_ChaosCounter_NumIslands, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumIslandGroups"), STAT_ChaosCounter_NumIslandGroups, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumContacts"), STAT_ChaosCounter_NumContacts, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumValidConstraints"), STAT_ChaosCounter_NumValidConstraints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumActiveConstraints"), STAT_ChaosCounter_NumActiveConstraints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumRestoredConstraints"), STAT_ChaosCounter_NumRestoredConstraints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumManifoldPoints"), STAT_ChaosCounter_NumManifoldPoints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumActiveManifoldPoints"), STAT_ChaosCounter_NumActiveManifoldPoints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumRestoredManifoldPoints"), STAT_ChaosCounter_NumRestoredManifoldPoints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumUpdatedManifoldPoints"), STAT_ChaosCounter_NumUpdatedManifoldPoints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumJoints"), STAT_ChaosCounter_NumJoints, STATGROUP_ChaosCounters);

// Stat Iteration counters
DECLARE_DWORD_COUNTER_STAT(TEXT("NumPositionIterations"), STAT_ChaosCounter_NumPositionIterations, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumVelocityIterations"), STAT_ChaosCounter_NumVelocityIterations, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumProjectionIterations"), STAT_ChaosCounter_NumProjectionIterations, STATGROUP_ChaosCounters);


// DebugDraw CVars
#if CHAOS_DEBUG_DRAW

// Must be 0 when checked in...
#define CHAOS_SOLVER_ENABLE_DEBUG_DRAW 0

namespace Chaos
{
	namespace CVars
	{
		int32 ChaosSolverDebugDrawShapes = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
		int32 ChaosSolverDebugDrawCollisions = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
		int32 ChaosSolverDebugDrawCollidingShapes = 0;
		int32 ChaosSolverDebugDrawBounds = 0;
		int32 ChaosSolverDrawTransforms = 0;
		int32 ChaosSolverDrawIslands = 0;
		int32 ChaosSolverDrawCCDInteractions = 0;
		int32 ChaosSolverDrawCCDThresholds = 0;
		int32 ChaosSolverDrawShapesShowStatic = 1;
		int32 ChaosSolverDrawShapesShowKinematic = 1;
		int32 ChaosSolverDrawShapesShowDynamic = 1;
		int32 ChaosSolverDrawJoints = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructure = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructureShowLeaves = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructureShowNodes = 0;
		int32 ChaosSolverDebugDrawSuspensionConstraints = 0;
		int32 ChaosSolverDrawClusterConstraints = 0;
		int32 ChaosSolverDebugDrawMeshContacts = 0;
		int32 ChaosSolverDebugDrawColorShapeByClientServer = 0;
		DebugDraw::FChaosDebugDrawJointFeatures ChaosSolverDrawJointFeatures = DebugDraw::FChaosDebugDrawJointFeatures::MakeDefault();
		FAutoConsoleVariableRef CVarChaosSolverDrawShapes(TEXT("p.Chaos.Solver.DebugDrawShapes"), ChaosSolverDebugDrawShapes, TEXT("Draw Shapes (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCollisions(TEXT("p.Chaos.Solver.DebugDrawCollisions"), ChaosSolverDebugDrawCollisions, TEXT("Draw Collisions (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCollidingShapes(TEXT("p.Chaos.Solver.DebugDrawCollidingShapes"), ChaosSolverDebugDrawCollidingShapes, TEXT("Draw Shapes that have collisions on them (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawBounds(TEXT("p.Chaos.Solver.DebugDrawBounds"), ChaosSolverDebugDrawBounds, TEXT("Draw bounding volumes inside the broadphase (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawTransforms(TEXT("p.Chaos.Solver.DebugDrawTransforms"), ChaosSolverDrawTransforms, TEXT("Draw particle transforms (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawIslands(TEXT("p.Chaos.Solver.DebugDrawIslands"), ChaosSolverDrawIslands, TEXT("Draw solver islands (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCCD(TEXT("p.Chaos.Solver.DebugDrawCCDInteractions"), ChaosSolverDrawCCDInteractions, TEXT("Draw CCD interactions."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCCDThresholds(TEXT("p.Chaos.Solver.DebugDrawCCDThresholds"), ChaosSolverDrawCCDThresholds, TEXT("Draw CCD swept thresholds."));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesStatic(TEXT("p.Chaos.Solver.DebugDraw.ShowStatics"), ChaosSolverDrawShapesShowStatic, TEXT("If DebugDrawShapes is enabled, whether to show static objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesKinematic(TEXT("p.Chaos.Solver.DebugDraw.ShowKinematics"), ChaosSolverDrawShapesShowKinematic, TEXT("If DebugDrawShapes is enabled, whether to show kinematic objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesDynamic(TEXT("p.Chaos.Solver.DebugDraw.ShowDynamics"), ChaosSolverDrawShapesShowDynamic, TEXT("If DebugDrawShapes is enabled, whether to show dynamic objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawJoints(TEXT("p.Chaos.Solver.DebugDrawJoints"), ChaosSolverDrawJoints, TEXT("Draw joints"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawSpatialAccelerationStructure(TEXT("p.Chaos.Solver.DebugDrawSpatialAccelerationStructure"), ChaosSolverDebugDrawSpatialAccelerationStructure, TEXT("Draw spatial acceleration structure"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawSpatialAccelerationStructureShowLeaves(TEXT("p.Chaos.Solver.DebugDrawSpatialAccelerationStructure.ShowLeaves"), ChaosSolverDebugDrawSpatialAccelerationStructureShowLeaves, TEXT("Show spatial acceleration structure leaves when its debug draw is enabled"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawSpatialAccelerationStructureShowNodes(TEXT("p.Chaos.Solver.DebugDrawSpatialAccelerationStructure.ShowNodes"), ChaosSolverDebugDrawSpatialAccelerationStructureShowNodes, TEXT("Show spatial acceleration structure nodes when its debug draw is enabled"));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesCoMConnector(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.CoMConnector"), ChaosSolverDrawJointFeatures.bCoMConnector, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesActorConnector(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.ActorConnector"), ChaosSolverDrawJointFeatures.bActorConnector, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesStretch(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Stretch"), ChaosSolverDrawJointFeatures.bStretch, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesAxes(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Axes"), ChaosSolverDrawJointFeatures.bAxes, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesLevel(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Level"), ChaosSolverDrawJointFeatures.bLevel, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesIndex(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Index"), ChaosSolverDrawJointFeatures.bIndex, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesColor(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Color"), ChaosSolverDrawJointFeatures.bColor, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawJointFeaturesIsland(TEXT("p.Chaos.Solver.DebugDraw.JointFeatures.Island"), ChaosSolverDrawJointFeatures.bIsland, TEXT("Joint features mask (see FDebugDrawJointFeatures)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawSuspensionConstraints(TEXT("p.Chaos.Solver.DebugDrawSuspension"), ChaosSolverDebugDrawSuspensionConstraints, TEXT("Draw Suspension (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawClusterConstraints(TEXT("p.Chaos.Solver.DebugDraw.Cluster.Constraints"), ChaosSolverDrawClusterConstraints, TEXT("Draw Active Cluster Constraints (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawMeshContacts(TEXT("p.Chaos.Solver.DebugDrawMeshContacts"), ChaosSolverDebugDrawMeshContacts, TEXT("Draw Mesh contacts"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawColorShapeByClientServer(TEXT("p.Chaos.Solver.DebugDraw.ColorShapeByClientServer"), ChaosSolverDebugDrawColorShapeByClientServer, TEXT("Color shape according to client and server: red = server / blue = client "));


		DebugDraw::FChaosDebugDrawSettings ChaosSolverDebugDebugDrawSettings(
			/* ArrowSize =					*/ 10.0f,
			/* BodyAxisLen =				*/ 30.0f,
			/* ContactLen =					*/ 30.0f,
			/* ContactWidth =				*/ 6.0f,
			/* ContactPhiWidth =			*/ 0.0f,
			/* ContactInfoWidth				*/ 6.0f,
			/* ContactOwnerWidth =			*/ 0.0f,
			/* ConstraintAxisLen =			*/ 30.0f,
			/* JointComSize =				*/ 2.0f,
			/* LineThickness =				*/ 1.0f,
			/* DrawScale =					*/ 1.0f,
			/* FontHeight =					*/ 10.0f,
			/* FontScale =					*/ 1.5f,
			/* ShapeThicknesScale =			*/ 1.0f,
			/* PointSize =					*/ 5.0f,
			/* VelScale =					*/ 0.0f,
			/* AngVelScale =				*/ 0.0f,
			/* ImpulseScale =				*/ 0.0f,
			/* PushOutScale =				*/ 0.0f,
			/* InertiaScale =				*/ 1.0f,
			/* DrawPriority =				*/ 10,
			/* bShowSimple =				*/ true,
			/* bShowComplex =				*/ false,
			/* bInShowLevelSetCollision =	*/ true,
			/* InShapesColorsPerState =     */ DebugDraw::GetDefaultShapesColorsByState(),
			/* InShapesColorsPerShaepType=  */ DebugDraw::GetDefaultShapesColorsByShapeType(),
			/* InBoundsColorsPerState =     */ DebugDraw::GetDefaultBoundsColorsByState(),
			/* InBoundsColorsPerShapeType=  */ DebugDraw::GetDefaultBoundsColorsByShapeType()
		);

		static DebugDraw::FChaosDebugDrawColorsByState GetSolverShapesColorsByState_Server()
		{
			static DebugDraw::FChaosDebugDrawColorsByState SolverShapesColorsByState_Server
			{
				/* InDynamicColor =	  */ FColor(255, 0, 0),
				/* InSleepingColor =  */ FColor(128, 0, 0),
				/* InKinematicColor = */ FColor(255, 0, 0),
				/* InStaticColor =	  */ FColor(255, 0, 0),
			};
			return SolverShapesColorsByState_Server;
		}

		static DebugDraw::FChaosDebugDrawColorsByState GetSolverShapesColorsByState_Client()
		{
			static DebugDraw::FChaosDebugDrawColorsByState SolverShapesColorsByState_Client
			{
				/* InDynamicColor =	  */ FColor(0, 0, 255),
				/* InSleepingColor =  */ FColor(0, 0, 128),
				/* InKinematicColor = */ FColor(0, 0, 255),
				/* InStaticColor =	  */ FColor(0, 0, 255),
			};
			return SolverShapesColorsByState_Client;
		}

		FAutoConsoleVariableRef CVarChaosSolverArrowSize(TEXT("p.Chaos.Solver.DebugDraw.ArrowSize"), ChaosSolverDebugDebugDrawSettings.ArrowSize, TEXT("ArrowSize."));
		FAutoConsoleVariableRef CVarChaosSolverBodyAxisLen(TEXT("p.Chaos.Solver.DebugDraw.BodyAxisLen"), ChaosSolverDebugDebugDrawSettings.BodyAxisLen, TEXT("BodyAxisLen."));
		FAutoConsoleVariableRef CVarChaosSolverContactLen(TEXT("p.Chaos.Solver.DebugDraw.ContactLen"), ChaosSolverDebugDebugDrawSettings.ContactLen, TEXT("ContactLen."));
		FAutoConsoleVariableRef CVarChaosSolverContactWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactWidth"), ChaosSolverDebugDebugDrawSettings.ContactWidth, TEXT("ContactWidth."));
		FAutoConsoleVariableRef CVarChaosSolverContactInfoWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactInfoWidth"), ChaosSolverDebugDebugDrawSettings.ContactInfoWidth, TEXT("ContactInfoWidth."));
		FAutoConsoleVariableRef CVarChaosSolverContactPhiWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactPhiWidth"), ChaosSolverDebugDebugDrawSettings.ContactPhiWidth, TEXT("ContactPhiWidth."));
		FAutoConsoleVariableRef CVarChaosSolverContactOwnerWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactOwnerWidth"), ChaosSolverDebugDebugDrawSettings.ContactOwnerWidth, TEXT("ContactOwnerWidth."));
		FAutoConsoleVariableRef CVarChaosSolverConstraintAxisLen(TEXT("p.Chaos.Solver.DebugDraw.ConstraintAxisLen"), ChaosSolverDebugDebugDrawSettings.ConstraintAxisLen, TEXT("ConstraintAxisLen."));
		FAutoConsoleVariableRef CVarChaosSolverLineThickness(TEXT("p.Chaos.Solver.DebugDraw.LineThickness"), ChaosSolverDebugDebugDrawSettings.LineThickness, TEXT("LineThickness."));
		FAutoConsoleVariableRef CVarChaosSolverLineShapeThickness(TEXT("p.Chaos.Solver.DebugDraw.ShapeLineThicknessScale"), ChaosSolverDebugDebugDrawSettings.ShapeThicknesScale, TEXT("Shape lineThickness multiplier."));
		FAutoConsoleVariableRef CVarChaosSolverPointSize(TEXT("p.Chaos.Solver.DebugDraw.PointSize"), ChaosSolverDebugDebugDrawSettings.PointSize, TEXT("Point size."));
		FAutoConsoleVariableRef CVarChaosSolverVelScale(TEXT("p.Chaos.Solver.DebugDraw.VelScale"), ChaosSolverDebugDebugDrawSettings.VelScale, TEXT("If >0 show velocity when drawing particle transforms."));
		FAutoConsoleVariableRef CVarChaosSolverAngVelScale(TEXT("p.Chaos.Solver.DebugDraw.AngVelScale"), ChaosSolverDebugDebugDrawSettings.AngVelScale, TEXT("If >0 show angular velocity when drawing particle transforms."));
		FAutoConsoleVariableRef CVarChaosSolverImpulseScale(TEXT("p.Chaos.Solver.DebugDraw.ImpulseScale"), ChaosSolverDebugDebugDrawSettings.ImpulseScale, TEXT("If >0 show impulses when drawing collisions."));
		FAutoConsoleVariableRef CVarChaosSolverPushOutScale(TEXT("p.Chaos.Solver.DebugDraw.PushOutScale"), ChaosSolverDebugDebugDrawSettings.PushOutScale, TEXT("If >0 show pushouts when drawing collisions."));
		FAutoConsoleVariableRef CVarChaosSolverInertiaScale(TEXT("p.Chaos.Solver.DebugDraw.InertiaScale"), ChaosSolverDebugDebugDrawSettings.InertiaScale, TEXT("When DebugDrawTransforms is enabled, show the mass-normalized inertia matrix scaled by this amount."));
		FAutoConsoleVariableRef CVarChaosSolverScale(TEXT("p.Chaos.Solver.DebugDraw.Scale"), ChaosSolverDebugDebugDrawSettings.DrawScale, TEXT("Scale applied to all Chaos Debug Draw line lengths etc."));
		FAutoConsoleVariableRef CVarChaosSolverShowSimple(TEXT("p.Chaos.Solver.DebugDraw.ShowSimple"), ChaosSolverDebugDebugDrawSettings.bShowSimpleCollision, TEXT("Whether to show simple collision is shape drawing is enabled"));
		FAutoConsoleVariableRef CVarChaosSolverShowComplex(TEXT("p.Chaos.Solver.DebugDraw.ShowComplex"), ChaosSolverDebugDebugDrawSettings.bShowComplexCollision, TEXT("Whether to show complex collision is shape drawing is enabled"));
		FAutoConsoleVariableRef CVarChaosSolverShowLevelSet(TEXT("p.Chaos.Solver.DebugDraw.ShowLevelSet"), ChaosSolverDebugDebugDrawSettings.bShowLevelSetCollision, TEXT("Whether to show levelset collision is shape drawing is enabled"));
	}
}
#endif

namespace Chaos
{
	namespace CVars
	{
		bool ChaosSolverUseParticlePool = true;
		FAutoConsoleVariableRef CVarChaosSolverUseParticlePool(TEXT("p.Chaos.Solver.UseParticlePool"), ChaosSolverUseParticlePool, TEXT("Whether or not to use dirty particle pool (Optim)"));

		int32 ChaosSolverParticlePoolNumFrameUntilShrink = 30;
		FAutoConsoleVariableRef CVarChaosSolverParticlePoolNumFrameUntilShrink(TEXT("p.Chaos.Solver.ParticlePoolNumFrameUntilShrink"), ChaosSolverParticlePoolNumFrameUntilShrink, TEXT("Num Frame until we can potentially shrink the pool"));

		// Joint solver mode (linear vs non-linear)
		bool bChaosSolverJointUseLinearSolver = true;
		FAutoConsoleVariableRef CVarChaosSolverJointUseCachedSolver(TEXT("p.Chaos.Solver.Joint.UseLinearSolver"), bChaosSolverJointUseLinearSolver, TEXT("Use linear version of joint solver. (default is true"));

		// Enable/Disable collisions
		bool bChaosSolverCollisionEnabled = true;
		FAutoConsoleVariableRef CVarChaosSolverCollisionDisable(TEXT("p.Chaos.Solver.Collision.Enabled"), bChaosSolverCollisionEnabled, TEXT("Enable/Disable collisions in the main scene."));


		// Iteration count cvars
		// These override the engine config if >= 0

		int32 ChaosSolverCollisionPositionFrictionIterations = -1;
		int32 ChaosSolverCollisionVelocityFrictionIterations = -1;
		int32 ChaosSolverCollisionPositionShockPropagationIterations = -1;
		int32 ChaosSolverCollisionVelocityShockPropagationIterations = -1;
		FAutoConsoleVariableRef CVarChaosSolverCollisionPositionFrictionIterations(TEXT("p.Chaos.Solver.Collision.PositionFrictionIterations"), ChaosSolverCollisionPositionFrictionIterations, TEXT("Override number of position iterations where friction is applied (if >= 0)"));
		FAutoConsoleVariableRef CVarChaosSolverCollisionVelocityFrictionIterations(TEXT("p.Chaos.Solver.Collision.VelocityFrictionIterations"), ChaosSolverCollisionVelocityFrictionIterations, TEXT("Override number of velocity iterations where friction is applied (if >= 0)"));
		FAutoConsoleVariableRef CVarChaosSolverCollisionPositionShockPropagationIterations(TEXT("p.Chaos.Solver.Collision.PositionShockPropagationIterations"), ChaosSolverCollisionPositionShockPropagationIterations, TEXT("Override number of position iterations where shock propagation is applied (if >= 0)"));
		FAutoConsoleVariableRef CVarChaosSolverCollisionVelocityShockPropagationIterations(TEXT("p.Chaos.Solver.Collision.VelocityShockPropagationIterations"), ChaosSolverCollisionVelocityShockPropagationIterations, TEXT("Override number of velocity iterations where shock propagation is applied (if >= 0)"));

		int32 ChaosSolverPositionIterations = -1;
		FAutoConsoleVariableRef CVarChaosSolverIterations(TEXT("p.Chaos.Solver.Iterations.Position"), ChaosSolverPositionIterations, TEXT("Override number of solver position iterations (-1 to use config)"));

		int32 ChaosSolverVelocityIterations = -1;
		FAutoConsoleVariableRef CVarChaosSolverPushOutIterations(TEXT("p.Chaos.Solver.Iterations.Velocity"), ChaosSolverVelocityIterations, TEXT("Override number of solver velocity iterations (-1 to use config)"));

		int32 ChaosSolverProjectionIterations = -1;
		FAutoConsoleVariableRef CVarChaosSolverProjectionIterations(TEXT("p.Chaos.Solver.Iterations.Projection"), ChaosSolverProjectionIterations, TEXT("Override number of solver projection iterations (-1 to use config)"));

		int32 ChaosSolverDeterministic = -1;
		FAutoConsoleVariableRef CVarChaosSolverDeterministic(TEXT("p.Chaos.Solver.Deterministic"), ChaosSolverDeterministic, TEXT("Override determinism. 0: disabled; 1: enabled; -1: use config"));

		// Copied from RBAN
		Chaos::FRealSingle ChaosSolverJointPositionTolerance = 0.025f;
		FAutoConsoleVariableRef CVarChaosSolverJointPositionTolerance(TEXT("p.Chaos.Solver.Joint.PositionTolerance"), ChaosSolverJointPositionTolerance, TEXT("PositionTolerance."));
		Chaos::FRealSingle ChaosSolverJointAngleTolerance = 0.001f;
		FAutoConsoleVariableRef CVarChaosSolverJointAngleTolerance(TEXT("p.Chaos.Solver.Joint.AngleTolerance"), ChaosSolverJointAngleTolerance, TEXT("AngleTolerance."));
		Chaos::FRealSingle ChaosSolverJointMinParentMassRatio = 0.2f;
		FAutoConsoleVariableRef CVarChaosSolverJointMinParentMassRatio(TEXT("p.Chaos.Solver.Joint.MinParentMassRatio"), ChaosSolverJointMinParentMassRatio, TEXT("6Dof joint MinParentMassRatio (if > 0)"));
		Chaos::FRealSingle ChaosSolverJointMaxInertiaRatio = 5.0f;
		FAutoConsoleVariableRef CVarChaosSolverJointMaxInertiaRatio(TEXT("p.Chaos.Solver.Joint.MaxInertiaRatio"), ChaosSolverJointMaxInertiaRatio, TEXT("6Dof joint MaxInertiaRatio (if > 0)"));

		// Collision detection cvars
		// These override the engine config if >= 0
		Chaos::FRealSingle ChaosSolverCullDistance = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverCullDistance(TEXT("p.Chaos.Solver.Collision.CullDistance"), ChaosSolverCullDistance, TEXT("Override cull distance (if >= 0)"));

		Chaos::FRealSingle ChaosSolverMaxPushOutVelocity = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverMaxPushOutVelocity(TEXT("p.Chaos.Solver.Collision.MaxPushOutVelocity"), ChaosSolverMaxPushOutVelocity, TEXT("Override max pushout velocity (if >= 0)"));

		int32 ChaosSolverCleanupCommandsOnDestruction = 1;
		FAutoConsoleVariableRef CVarChaosSolverCleanupCommandsOnDestruction(TEXT("p.Chaos.Solver.CleanupCommandsOnDestruction"), ChaosSolverCleanupCommandsOnDestruction, TEXT("Whether or not to run internal command queue cleanup on solver destruction (0 = no cleanup, >0 = cleanup all commands)"));

		int32 ChaosSolverCollisionDeferNarrowPhase = 0;
		FAutoConsoleVariableRef CVarChaosSolverCollisionDeferNarrowPhase(TEXT("p.Chaos.Solver.Collision.DeferNarrowPhase"), ChaosSolverCollisionDeferNarrowPhase, TEXT("Create contacts for all broadphase pairs, perform NarrowPhase later."));

		// Allow one-shot or incremental manifolds where supported (which depends on shape pair types)
		int32 ChaosSolverCollisionUseManifolds = 1;
		FAutoConsoleVariableRef CVarChaosSolverCollisionUseManifolds(TEXT("p.Chaos.Solver.Collision.UseManifolds"), ChaosSolverCollisionUseManifolds, TEXT("Enable/Disable use of manifolds in collision."));

		// Allow manifolds to be reused between ticks
		int32 ChaosSolverCollisionAllowManifoldUpdate = 1;
		FAutoConsoleVariableRef CVarChaosSolverCollisionAllowManifoldUpdate(TEXT("p.Chaos.Solver.Collision.AllowManifoldUpdate"), ChaosSolverCollisionAllowManifoldUpdate, TEXT("Enable/Disable reuse of manifolds between ticks (for small movement)."));

		// Enable/Disable CCD
		bool bChaosUseCCD = true;
		FAutoConsoleVariableRef  CVarChaosUseCCD(TEXT("p.Chaos.Solver.UseCCD"), bChaosUseCCD, TEXT("Global flag to turn CCD on or off. Default is true"));

		// Joint cvars
		float ChaosSolverJointMinSolverStiffness = 1.0f;
		float ChaosSolverJointMaxSolverStiffness = 1.0f;
		int32 ChaosSolverJointNumIterationsAtMaxSolverStiffness = 1;
		bool bChaosSolverJointSolvePositionLast = true;
		int32 ChaosSolverJointNumShockProagationIterations = 0;
		FRealSingle ChaosSolverJointShockPropagation = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverJointMinSolverStiffness(TEXT("p.Chaos.Solver.Joint.MinSolverStiffness"), ChaosSolverJointMinSolverStiffness, TEXT("Solver stiffness on first iteration, increases each iteration toward MaxSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointMaxSolverStiffness(TEXT("p.Chaos.Solver.Joint.MaxSolverStiffness"), ChaosSolverJointMaxSolverStiffness, TEXT("Solver stiffness on last iteration, increases each iteration from MinSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointNumIterationsAtMaxSolverStiffness(TEXT("p.Chaos.Solver.Joint.NumIterationsAtMaxSolverStiffness"), ChaosSolverJointNumIterationsAtMaxSolverStiffness, TEXT("How many iterations we want at MaxSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointSolvePositionFirst(TEXT("p.Chaos.Solver.Joint.SolvePositionLast"), bChaosSolverJointSolvePositionLast, TEXT("Should we solve joints in position-then-rotation order (false) or rotation-then-position order (true, default)"));
		FAutoConsoleVariableRef CVarChaosSolverJointNumShockPropagationIterations(TEXT("p.Chaos.Solver.Joint.NumShockPropagationIterations"), ChaosSolverJointNumShockProagationIterations, TEXT("How many iterations to enable SHockProagation for."));
		FAutoConsoleVariableRef CVarChaosSolverJointShockPropagation(TEXT("p.Chaos.Solver.Joint.ShockPropagation"), ChaosSolverJointShockPropagation, TEXT("6Dof joint shock propagation override (if >= 0)."));

		int32 ChaosVisualDebuggerEnable = 1;
		FAutoConsoleVariableRef CVarChaosVisualDebuggerEnable(TEXT("p.Chaos.VisualDebuggerEnable"), ChaosVisualDebuggerEnable, TEXT("Enable/Disable pushing/saving data to the visual debugger"));
	}
}

namespace Chaos
{
	using namespace CVars;

	class AdvanceOneTimeStepTask : public FNonAbandonableTask
	{
		friend class FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>;
	public:
		AdvanceOneTimeStepTask(
			FPBDRigidsSolver* Scene
			, const FReal DeltaTime
			, const FSubStepInfo& SubStepInfo)
			: MSolver(Scene)
			, MDeltaTime(DeltaTime)
			, MSubStepInfo(SubStepInfo)
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("AdvanceOneTimeStepTask::AdvanceOneTimeStepTask()"));
		}

		void DoWork()
		{
			LLM_SCOPE(ELLMTag::ChaosUpdate);
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("AdvanceOneTimeStepTask::DoWork()"));

			MSolver->ApplyCallbacks_Internal();
			
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateParams);
				Chaos::FPBDPositionConstraints PositionTarget; // Dummy for now
				TMap<int32, int32> TargetedParticles;
				{
					MSolver->FieldParameterUpdateCallback(PositionTarget, TargetedParticles);
				}

				for (FGeometryCollectionPhysicsProxy* GeoclObj : MSolver->GetGeometryCollectionPhysicsProxies_Internal())
				{
					GeoclObj->FieldParameterUpdateCallback(MSolver);
				}

				MSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().ProcessPendingQueues();
			}

			{
				//SCOPE_CYCLE_COUNTER(STAT_BeginFrame);
				//MSolver->StartFrameCallback(MDeltaTime, MSolver->GetSolverTime());
			}


			if(FRewindData* RewindData = MSolver->GetRewindData())
			{
				RewindData->AdvanceFrame(MDeltaTime,[Evolution = MSolver->GetEvolution()]()
				{
					return Evolution->CreateExternalResimCache();
				});
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EvolutionAndKinematicUpdate);

				// clear out the collision constraints as they will be stale from last frame if AdvanceOneTimeStep never gets called due to TimeRemaining being less than MinDeltaTime 
				// @todo(chaos): maybe we can pull data at a better time instead to avoid collision-specific code here for event dispatch
				MSolver->GetEvolution()->GetCollisionConstraints().BeginFrame();
			
				// This outer loop can potentially cause the system to lose energy over integration
				// in a couple of different cases.
				//
				// * If we have a timestep that's smaller than MinDeltaTime, then we just won't step.
				//   Yes, we'll lose some teeny amount of energy, but we'll avoid 1/dt issues.
				//
				// * If we have used all of our substeps but still have time remaining, then some
				//   energy will be lost.
				const FReal MinDeltaTime = MSolver->GetMinDeltaTime_External();
				const FReal MaxDeltaTime = MSolver->GetMaxDeltaTime_External();
				int32 StepsRemaining = MSubStepInfo.bSolverSubstepped ? 1 : MSolver->GetMaxSubSteps_External();
				FReal TimeRemaining = MDeltaTime;
				bool bFirstStep = true;
				while (StepsRemaining > 0 && TimeRemaining > MinDeltaTime)
				{
					--StepsRemaining;
					const FReal DeltaTime = MaxDeltaTime > 0.f ? FMath::Min(TimeRemaining, MaxDeltaTime) : TimeRemaining;
					TimeRemaining -= DeltaTime;

					{
						MSolver->FieldForcesUpdateCallback();
					}

					for (FGeometryCollectionPhysicsProxy* GeoCollectionObj : MSolver->GetGeometryCollectionPhysicsProxies_Internal())
					{
						GeoCollectionObj->FieldForcesUpdateCallback(MSolver);
					}

					if(FRewindData* RewindData = MSolver->GetRewindData())
					{
						//todo: make this work with sub-stepping
						MSolver->GetEvolution()->SetCurrentStepResimCache(bFirstStep ? RewindData->GetCurrentStepResimCache() : nullptr);
					}

					MSolver->GetEvolution()->AdvanceOneTimeStep(DeltaTime, MSubStepInfo);
					MSolver->PostEvolutionVDBPush();
					bFirstStep = false;
				}

				// Editor will tick with 0 DT, this will guarantee acceleration structure is still processing even if we don't advance evolution.
				if (MDeltaTime < MinDeltaTime)
				{
					MSolver->GetEvolution()->ComputeIntermediateSpatialAcceleration();
				}


#if CHAOS_CHECKED
				// If time remains, then log why we have lost energy over the timestep.
				if (TimeRemaining > 0.f)
				{
					if (StepsRemaining == 0)
					{
						UE_LOG(LogPBDRigidsSolver, Warning, TEXT("AdvanceOneTimeStepTask::DoWork() - Energy lost over %fs due to too many substeps over large timestep"), TimeRemaining);
					}
					else
					{
						UE_LOG(LogPBDRigidsSolver, Warning, TEXT("AdvanceOneTimeStepTask::DoWork() - Energy lost over %fs due to small timestep remainder"), TimeRemaining);
					}
				}
#endif
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EventDataGathering);
				{
					SCOPE_CYCLE_COUNTER(STAT_FillProducerData);
					bool ResetData = (MSubStepInfo.Step == 0);
					MSolver->GetEventManager()->FillProducerData(MSolver, ResetData);
					MSolver->GetEvolution()->ResetAllRemovals();
				}

				// flip on last sub-step of frame
				if (MSubStepInfo.Step == MSubStepInfo.NumSteps - 1)
				{
					SCOPE_CYCLE_COUNTER(STAT_FlipBuffersIfRequired);
					MSolver->GetEventManager()->FlipBuffersIfRequired();
				}
			}
			
			{
				SCOPE_CYCLE_COUNTER(STAT_EndFrame);
				MSolver->GetEvolution()->EndFrame(MDeltaTime);
			}

			MSolver->FinalizeCallbackData_Internal();

			MSolver->SetSolverTime(MSolver->GetSolverTime() + MDeltaTime );
			MSolver->GetCurrentFrame()++;
			MSolver->PostTickDebugDraw(MDeltaTime);

			MSolver->UpdateStatCounters();

			//Editor ticks with 0 dt. We don't want to buffer any dirty data from this since it won't be consumed
			//TODO: handle this more gracefully
			if(MDeltaTime > 0)
			{
				MSolver->CompleteSceneSimulation();
			}

			// reset all clustering event needs to be after CompleteSceneSimulation to make sure the cache recording gets them before they get removed
			// they cannot be right after the presolve callback ( as they were before ) because they will cause geometry collection replicated clients to miss them
			// Todo(chaos) we should probably move all of the solver event reset here in the future
			MSolver->GetEvolution()->GetRigidClustering().ResetAllClusterBreakings();
			MSolver->GetEvolution()->GetRigidClustering().ResetAllClusterCrumblings();

			if (FRewindData* RewindData = MSolver->GetRewindData())
			{
				RewindData->FinishFrame();
			}
		}

	protected:

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(AdvanceOneTimeStepTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		FPBDRigidsSolver* MSolver;
		FReal MDeltaTime;
		FSubStepInfo MSubStepInfo;
		TSharedPtr<FCriticalSection> PrevLock, CurrentLock;
		TSharedPtr<FEvent> PrevEvent, CurrentEvent;
	};

	FPBDRigidsSolver::FPBDRigidsSolver(const EMultiBufferMode BufferingModeIn, UObject* InOwner, FReal InAsyncDt)
		: Super(BufferingModeIn, BufferingModeIn == EMultiBufferMode::Single ? EThreadingModeTemp::SingleThread : EThreadingModeTemp::TaskGraph, InOwner, InAsyncDt)
		, CurrentFrame(0)
		, bHasFloor(true)
		, bIsFloorAnalytic(false)
		, FloorHeight(0.f)
		, bIsDeterministic(false)
		, Particles(UniqueIndices)
		, MEvolution(new FPBDRigidsEvolution(Particles, SimMaterials, &ContactModifiers, BufferingModeIn == Chaos::EMultiBufferMode::Single))
		, MEventManager(new FEventManager(BufferingModeIn))
		, MSolverEventFilters(new FSolverEventFilters())
		, MDirtyParticlesBuffer(new FDirtyParticlesBuffer(BufferingModeIn, BufferingModeIn == Chaos::EMultiBufferMode::Single))
		, MCurrentLock(new FCriticalSection())

		, PerSolverField(nullptr)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::PBDRigidsSolver()"));
		Reset();

		MEvolution->SetInternalParticleInitilizationFunction(
			[this](const Chaos::FGeometryParticleHandle* OldParticle, Chaos::FGeometryParticleHandle* NewParticle) 
				{
				IPhysicsProxyBase* Proxy = const_cast<IPhysicsProxyBase*>(OldParticle->PhysicsProxy());
				if (Chaos::FPBDRigidClusteredParticleHandle* NewClusteredParticle = NewParticle->CastToClustered())
					{
					NewClusteredParticle->AddPhysicsProxy(Proxy);
				}
				NewParticle->SetPhysicsProxy(Proxy);
			});
	}

	FRealSingle MaxBoundsForTree = (FRealSingle)10000;
	FAutoConsoleVariableRef CVarMaxBoundsForTree(
		TEXT("p.MaxBoundsForTree"),
		MaxBoundsForTree,
		TEXT("The max bounds before moving object into a large objects structure. Only applies on object registration")
		TEXT(""),
		ECVF_Default);

	void FPBDRigidsSolver::RegisterObject(FSingleParticlePhysicsProxy* Proxy)
	{
		LLM_SCOPE(ELLMTag::ChaosBody);

		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("FPBDRigidsSolver::RegisterObject()"));
		auto& RigidBody_External = Proxy->GetGameThreadAPI();

		if (BroadPhaseConfig.BroadphaseType < FBroadPhaseConfig::TreeAndGrid)
		{
			FSpatialAccelerationIdx Idx = RigidBody_External.SpatialIdx();
			Idx.Bucket = 0;
			RigidBody_External.SetSpatialIdx(Idx);
		}
		else if (RigidBody_External.Geometry() && RigidBody_External.Geometry()->HasBoundingBox() && RigidBody_External.Geometry()->BoundingBox().Extents().Max() >= MaxBoundsForTree)
		{
			RigidBody_External.SetSpatialIdx(FSpatialAccelerationIdx{ 1,0 });
		}
		if (!ensure(Proxy->GetParticle_LowLevel()->IsParticleValid()))
		{
			return;
		}

		// NOTE: Do we really need these lists of proxies if we can just
		// access them through the GTParticles list?
		

		RigidBody_External.SetUniqueIdx(GetEvolution()->GenerateUniqueIdx());
		TrackGTParticle_External(*Proxy->GetParticle_LowLevel());	//todo: remove this
		//Chaos::FParticlePropertiesData& RemoteParticleData = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteParticleProperties();
		//Chaos::FShapeRemoteDataContainer& RemoteShapeContainer = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteShapeContainer();

		Proxy->SetSolver(this);
		Proxy->GetParticle_LowLevel()->SetProxy(Proxy);
		AddDirtyProxy(Proxy);

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), /*bDelete=*/false);
	}

	int32 LogCorruptMap = 0;
	FAutoConsoleVariableRef CVarLogCorruptMap(TEXT("p.LogCorruptMap"), LogCorruptMap, TEXT(""));


	void FPBDRigidsSolver::UnregisterObject(FSingleParticlePhysicsProxy* Proxy)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("FPBDRigidsSolver::UnregisterObject()"));

		PullResultsManager->RemoveProxy_External(Proxy);

		ClearGTParticle_External(*Proxy->GetParticle_LowLevel());	//todo: remove this

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), /*bDelete=*/true);

		// remove the proxy from the invalidation list
		RemoveDirtyProxy(Proxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		Proxy->MarkDeleted();

		// Null out the particle's proxy pointer
		Proxy->GetParticle_LowLevel()->SetProxy(nullptr);	//todo: use TUniquePtr for better ownership

		// Remove the proxy from the GT proxy map
		FUniqueIdx UniqueIdx = Proxy->GetGameThreadAPI().UniqueIdx();

		Chaos::FIgnoreCollisionManager& CollisionManager = GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();
		{
			int32 ExternalTimestamp = GetMarshallingManager().GetExternalTimestamp_External();
			Chaos::FIgnoreCollisionManager::FDeactivationSet& PendingMap = CollisionManager.GetPendingDeactivationsForGameThread(ExternalTimestamp);
			PendingMap.Add(UniqueIdx);
		}

		// Enqueue a command to remove the particle and delete the proxy
		EnqueueCommandImmediate([Proxy, UniqueIdx, this]()
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("FPBDRigidsSolver::UnregisterObject() ~ Dequeue"));

				// Generally need to remove stale events for particles that no longer exist
				GetEventManager()->template ClearEvents<FCollisionEventData>(EEventType::Collision, [Proxy]
				(FCollisionEventData& EventDataInOut)
				{
					Chaos::FCollisionDataArray const& CollisionData = EventDataInOut.CollisionData.AllCollisionsArray;
					if (CollisionData.Num() > 0)
					{
						check(Proxy);
						TArray<int32> const* const CollisionIndices = EventDataInOut.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Find(Proxy);
						if (CollisionIndices)
						{
							EventDataInOut.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Remove(Proxy);
						}
					}

				});

			FGeometryParticleHandle* Handle = Proxy->GetHandle_LowLevel();
			Proxy->SetHandle(nullptr);
			const int32 OffsetForRewind = MRewindData ? MRewindData->Capacity() : 0;
			PendingDestroyPhysicsProxy.Add(FPendingDestroyInfo{Proxy, GetCurrentFrame() + OffsetForRewind, Handle, UniqueIdx});
			
			//If particle was created and destroyed before commands were enqueued just skip. I suspect we can skip entire lambda, but too much code to verify right now
			if(Handle)
			{
				//Disable until particle is finally destroyed
				GetEvolution()->DisableParticle(Handle);
			}

			// Remove the proxy for this particle
			if (SingleParticlePhysicsProxies_PT.IsValidIndex(UniqueIdx.Idx))
			{
				SingleParticlePhysicsProxies_PT.RemoveAt(UniqueIdx.Idx);
			}
		});
	}

	void FPBDRigidsSolver::RegisterObject(FGeometryCollectionPhysicsProxy* InProxy)
	{
		LLM_SCOPE(ELLMTag::ChaosBody);
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("FPBDRigidsSolver::RegisterObject(FGeometryCollectionPhysicsProxy*)"));
		InProxy->SetSolver(this);
		InProxy->Initialize(GetEvolution());
		InProxy->NewData(); // Buffers data on the proxy.
		FParticlesType* InParticles = &GetParticles();

		// Finish registration on the physics thread...
		EnqueueCommandImmediate([InParticles, InProxy, this]()
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, 
				TEXT("FPBDRigidsSolver::RegisterObject(FGeometryCollectionPhysicsProxy*)"));
			check(InParticles);
			InProxy->InitializeBodiesPT(this, *InParticles);
			GeometryCollectionPhysicsProxies_Internal.Add(InProxy);
		});
	}
	
	void FPBDRigidsSolver::UnregisterObject(FGeometryCollectionPhysicsProxy* InProxy)
	{
		check(InProxy);
		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		InProxy->MarkDeleted();

		RemoveDirtyProxy(InProxy);

		// Particles are removed from acceleration structure in FPhysScene_Chaos::RemoveObject.


		EnqueueCommandImmediate([InProxy, this]()
			{
				GeometryCollectionPhysicsProxies_Internal.RemoveSingle(InProxy);
				InProxy->SyncBeforeDestroy();
				InProxy->OnRemoveFromSolver(this);
				InProxy->ResetDirtyIdx();
				PendingDestroyGeometryCollectionPhysicsProxy.Add(InProxy);
			});
	}

	void FPBDRigidsSolver::RegisterObject(Chaos::FJointConstraint* GTConstraint)
	{
		LLM_SCOPE(ELLMTag::ChaosConstraint);
		FJointConstraintPhysicsProxy* JointProxy = new FJointConstraintPhysicsProxy(GTConstraint, nullptr);
		JointProxy->SetSolver(this);

		AddDirtyProxy(JointProxy);
	}

	void FPBDRigidsSolver::UnregisterObject(Chaos::FJointConstraint* GTConstraint)
	{
		FJointConstraintPhysicsProxy* JointProxy = GTConstraint->GetProxy<FJointConstraintPhysicsProxy>();
		check(JointProxy);

		RemoveDirtyProxy(JointProxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		GTConstraint->GetProxy()->MarkDeleted();


		GTConstraint->SetProxy(static_cast<FJointConstraintPhysicsProxy*>(nullptr));

		GTConstraint->ReleaseKinematicEndPoint(this);

		JointProxy->DestroyOnGameThread();	//destroy the game thread portion of the proxy
		
		// Finish de-registration on the physics thread...
		EnqueueCommandImmediate([JointProxy, this]()
		{
			//TODO: consider deferring this so that rewind can resim with joint until moment of destruction
			//For now we assume this always comes from server update
			if(FRewindData* RewindData = GetRewindData())
			{
				RewindData->RemoveObject(JointProxy->GetHandle());
			}

			JointProxy->DestroyOnPhysicsThread(this);
			JointConstraintPhysicsProxies_Internal.RemoveSingle(JointProxy);
			delete JointProxy;
		});
	}

	void FPBDRigidsSolver::RegisterObject(Chaos::FSuspensionConstraint* GTConstraint)
	{
		LLM_SCOPE(ELLMTag::ChaosConstraint);
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = new FSuspensionConstraintPhysicsProxy(GTConstraint, nullptr);
		SuspensionProxy->SetSolver(this);

		AddDirtyProxy(SuspensionProxy);
	}

	void FPBDRigidsSolver::UnregisterObject(Chaos::FSuspensionConstraint* GTConstraint)
	{
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = GTConstraint->GetProxy<FSuspensionConstraintPhysicsProxy>();
		check(SuspensionProxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		SuspensionProxy->MarkDeleted();

		RemoveDirtyProxy(SuspensionProxy);

		GTConstraint->SetProxy(static_cast<FSuspensionConstraintPhysicsProxy*>(nullptr));

		FParticlesType* InParticles = &GetParticles();
		SuspensionProxy->DestroyOnGameThread();	//destroy the game thread portion of the proxy

		// Finish registration on the physics thread...
		EnqueueCommandImmediate([InParticles, SuspensionProxy, this]()
			{
				SuspensionProxy->DestroyOnPhysicsThread(this);
				delete SuspensionProxy;
			});
	}

	void FPBDRigidsSolver::SetSuspensionTarget(Chaos::FSuspensionConstraint* GTConstraint, const FVector& TargetPos, const FVector& Normal, bool Enabled)
	{
		EnsureIsInPhysicsThreadContext();
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = GTConstraint->GetProxy<FSuspensionConstraintPhysicsProxy>();
		check(SuspensionProxy);
		SuspensionProxy->UpdateTargetOnPhysicsThread(this, TargetPos, Normal, Enabled);
	}

	CHAOS_API int32 RewindCaptureNumFrames = -1;
	FAutoConsoleVariableRef CVarRewindCaptureNumFrames(TEXT("p.RewindCaptureNumFrames"),RewindCaptureNumFrames,TEXT("The number of frames to capture rewind for. Requires restart of solver"));

	int32 UseResimCache = 0;
	FAutoConsoleVariableRef CVarUseResimCache(TEXT("p.UseResimCache"),UseResimCache,TEXT("Whether resim uses cache to skip work, requires recreating world to take effect"));

	void FPBDRigidsSolver::EnableRewindCapture(int32 NumFrames, bool InUseCollisionResimCache, TUniquePtr<IRewindCallback>&& RewindCallback)
	{
		//TODO: this function calls both internal and extrnal - sort of assumed during initialization. Should decide what thread it's called on and mark it as either external or internal
		MRewindData = MakeUnique<FRewindData>(((FPBDRigidsSolver*)this), NumFrames, InUseCollisionResimCache, ((FPBDRigidsSolver*)this)->GetCurrentFrame()); // FIXME
		bUseCollisionResimCache = InUseCollisionResimCache;
		MRewindCallback = MoveTemp(RewindCallback);
		MarshallingManager.SetHistoryLength_Internal(NumFrames);
		MEvolution->SetRewindData(GetRewindData());
		
		UpdateIsDeterministic();
	}

	void FPBDRigidsSolver::Reset()
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::Reset()"));

		MTime = 0;
		MLastDt = 0.0f;
		CurrentFrame = 0;
		SetMaxDeltaTime_External(1.0f);
		SetMinDeltaTime_External(UE_SMALL_NUMBER);
		SetMaxSubSteps_External(1);
		MEvolution = TUniquePtr<FPBDRigidsEvolution>(new FPBDRigidsEvolution(Particles, SimMaterials, &ContactModifiers, BufferMode == EMultiBufferMode::Single)); 

		PerSolverField = MakeUnique<FPerSolverFieldSystem>();

		//todo: do we need this?
		//MarshallingManager.Reset();

		if(RewindCaptureNumFrames >= 0)
		{
			EnableRewindCapture(RewindCaptureNumFrames, bUseCollisionResimCache || !!UseResimCache);
		}

		MEvolution->SetCaptureRewindDataFunction([this](const TParticleView<TPBDRigidParticles<FReal,3>>& ActiveParticles)
		{
			FinalizeRewindData(ActiveParticles);
		});

		FEventDefaults::RegisterSystemEvents(*GetEventManager());
	}

	void FPBDRigidsSolver::ChangeBufferMode(Chaos::EMultiBufferMode InBufferMode)
	{
		// This seems unused inside the solver? #BH
		BufferMode = InBufferMode;

		SetThreadingMode_External(BufferMode == EMultiBufferMode::Single ? EThreadingModeTemp::SingleThread : EThreadingModeTemp::TaskGraph);
	}

	void FPBDRigidsSolver::StartingSceneSimulation()
	{
		LLM_SCOPE(ELLMTag::Chaos);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StartedSceneSimulation);

		GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().PopStorageData_Internal(GetEvolution()->LatestExternalTimestampConsumed_Internal);
	}

	void FPBDRigidsSolver::DestroyPendingProxies_Internal()
	{
		// If we have any callback objects watching for particle deregistrations,
		// send an array of proxies that are about to be deleted.
		if (UnregistrationWatchers.Num() > 0)
		{
			TArray<TTuple<FUniqueIdx, FSingleParticlePhysicsProxy*>> Proxies;
			Proxies.Reserve(PendingDestroyPhysicsProxy.Num());
			for (FPendingDestroyInfo& Info : PendingDestroyPhysicsProxy)
			{
				Proxies.Add({ Info.UniqueIdx, Info.Proxy });
			}
			for (ISimCallbackObject* Callback : UnregistrationWatchers)
			{
				Callback->OnParticleUnregistered_Internal(Proxies);
			}
		}

		// Do the actual destruction
		for(int32 Idx = PendingDestroyPhysicsProxy.Num() - 1; Idx >= 0; --Idx)
		{
			FPendingDestroyInfo& Info = PendingDestroyPhysicsProxy[Idx];
			if(Info.DestroyOnStep <= GetCurrentFrame() || IsShuttingDown())
			{
				// finally let's release the unique index
				GetEvolution()->ReleaseUniqueIdx(Info.UniqueIdx);

				if (Info.Handle)
				{
					// Use the handle to destroy the particle data
					GetEvolution()->DestroyParticle(Info.Handle);
				}

				ensure(Info.Proxy->GetHandle_LowLevel() == nullptr);	//should have already cleared this out
				delete Info.Proxy;
				PendingDestroyPhysicsProxy.RemoveAtSwap(Idx, 1, false);
			}
		}

		bool bResetCollisionConstraints=false;
		for (auto Proxy : PendingDestroyGeometryCollectionPhysicsProxy)
		{
			//ensure(Proxy->GetHandle_LowLevel() == nullptr);	//should have already cleared this out
			MarshallingManager.GetCurrentPullData_Internal()->DirtyGeometryCollections.Reset();
			bResetCollisionConstraints = true;
			delete Proxy;
		}
		PendingDestroyGeometryCollectionPhysicsProxy.Reset();

		if(bResetCollisionConstraints)
			GetEvolution()->GetCollisionConstraints();
	}

	void FPBDRigidsSolver::PrepareAdvanceBy(const FReal DeltaTime)
	{
		MEvolution->GetCollisionConstraints().SetCollisionsEnabled(bChaosSolverCollisionEnabled);

		FCollisionDetectorSettings CollisionDetectorSettings = MEvolution->GetCollisionDetector().GetSettings();
		CollisionDetectorSettings.bAllowManifoldReuse = (ChaosSolverCollisionAllowManifoldUpdate != 0);
		CollisionDetectorSettings.bDeferNarrowPhase = (ChaosSolverCollisionDeferNarrowPhase != 0);
		CollisionDetectorSettings.bAllowManifolds = (ChaosSolverCollisionUseManifolds != 0);
		CollisionDetectorSettings.bAllowCCD = bChaosUseCCD;
		MEvolution->GetCollisionDetector().SetSettings(CollisionDetectorSettings);
		
		FPBDJointSolverSettings JointsSettings = MEvolution->GetJointConstraints().GetSettings();
		JointsSettings.MinSolverStiffness = ChaosSolverJointMinSolverStiffness;
		JointsSettings.MaxSolverStiffness = ChaosSolverJointMaxSolverStiffness;
		JointsSettings.NumIterationsAtMaxSolverStiffness = ChaosSolverJointNumIterationsAtMaxSolverStiffness;
		JointsSettings.PositionTolerance = ChaosSolverJointPositionTolerance;
		JointsSettings.AngleTolerance = ChaosSolverJointAngleTolerance;
		JointsSettings.MinParentMassRatio = ChaosSolverJointMinParentMassRatio;
		JointsSettings.MaxInertiaRatio = ChaosSolverJointMaxInertiaRatio;
		JointsSettings.bSolvePositionLast = bChaosSolverJointSolvePositionLast;
		JointsSettings.NumShockPropagationIterations = ChaosSolverJointNumShockProagationIterations;
		JointsSettings.ShockPropagationOverride = ChaosSolverJointShockPropagation;
		JointsSettings.bUseLinearSolver = bChaosSolverJointUseLinearSolver;
		MEvolution->GetJointConstraints().SetSettings(JointsSettings);

		// Apply CVAR overrides if set
		{
			if (ChaosSolverCollisionPositionFrictionIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetPositionFrictionIterations(ChaosSolverCollisionPositionFrictionIterations);
			}
			if (ChaosSolverCollisionVelocityFrictionIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetVelocityFrictionIterations(ChaosSolverCollisionVelocityFrictionIterations);
			}
			if (ChaosSolverCollisionPositionShockPropagationIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetPositionShockPropagationIterations(ChaosSolverCollisionPositionShockPropagationIterations);
			}
			if (ChaosSolverCollisionVelocityShockPropagationIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetVelocityShockPropagationIterations(ChaosSolverCollisionVelocityShockPropagationIterations);
			}
			if (ChaosSolverPositionIterations >= 0)
			{
				SetPositionIterations(ChaosSolverPositionIterations);
			}
			if (ChaosSolverVelocityIterations >= 0)
			{
				SetVelocityIterations(ChaosSolverVelocityIterations);
			}
			if (ChaosSolverProjectionIterations >= 0)
			{
				SetProjectionIterations(ChaosSolverProjectionIterations);
			}
			if (ChaosSolverCullDistance >= 0.0f)
			{
				SetCollisionCullDistance(ChaosSolverCullDistance);
			}
			if (ChaosSolverMaxPushOutVelocity >= 0.0f)
			{
				SetCollisionMaxPushOutVelocity(ChaosSolverMaxPushOutVelocity);
			}
			if (ChaosSolverDeterministic >= 0)
			{
				UpdateIsDeterministic();
			}
		}

		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::Tick(%3.5f)"), DeltaTime);
		MLastDt = DeltaTime;
		EventPreSolve.Broadcast(DeltaTime);

		StartingSceneSimulation();
	}

	void FPBDRigidsSolver::AdvanceSolverBy(const FSubStepInfo& SubStepInfo)
	{
		const FReal StartSimTime = GetSolverTime();

		AdvanceOneTimeStepTask(this, MLastDt, SubStepInfo).DoWork();

		if (MLastDt > 0)
		{
			//pass information back to external thread
			//we skip dt=0 case because sync data should be identical if dt = 0
			MarshallingManager.FinalizePullData_Internal(MEvolution->LatestExternalTimestampConsumed_Internal, StartSimTime, MLastDt);
		}

		if(SubStepInfo.Step == SubStepInfo.NumSteps - 1)
		{
			//final step so we can destroy proxies
			DestroyPendingProxies_Internal();
		}
	}

	void FPBDRigidsSolver::SetExternalTimestampConsumed_Internal(const int32 Timestamp)
	{
		MEvolution->LatestExternalTimestampConsumed_Internal = Timestamp;
	}

	void FPBDRigidsSolver::SyncEvents_GameThread()
	{
		GetEventManager()->DispatchEvents();
	}

	int32 LogDirtyParticles = 0;
	FAutoConsoleVariableRef CVarLogDirtyParticles(TEXT("p.LogDirtyParticles"), LogDirtyParticles, TEXT("Logs out which particles are dirty every frame"));

	void FPBDRigidsSolver::PushPhysicsState(const FReal DeltaTime, const int32 NumSteps, const int32 NumExternalSteps)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PushPhysicsState);
		ensure(NumSteps > 0);
		ensure(NumExternalSteps > 0);
		//TODO: interpolate some data based on num steps

		FPushPhysicsData* PushData = MarshallingManager.GetProducerData_External();
		const FReal DynamicsWeight = FReal(1) / FReal(NumExternalSteps);
		FDirtySet* DirtyProxiesData = &PushData->DirtyProxiesDataBuffer;
		FDirtyPropertiesManager* Manager = &PushData->DirtyPropertiesManager;

		Manager->PrepareBuckets(DirtyProxiesData->GetDirtyProxyBucketInfo());
		Manager->SetNumShapes(DirtyProxiesData->NumDirtyShapes());
		FShapeDirtyData* ShapeDirtyData = DirtyProxiesData->GetShapesDirtyData();

		DirtyProxiesData->ParallelForEachProxy([this, DynamicsWeight, Manager, ShapeDirtyData](int32 DataIdx, FDirtyProxy& Dirty)
		{
			switch(Dirty.Proxy->GetType())
			{
			case EPhysicsProxyType::SingleParticleProxy:
			{
				auto Proxy = static_cast<FSingleParticlePhysicsProxy*>(Dirty.Proxy);
				auto Particle = Proxy->GetParticle_LowLevel();
				if(auto Rigid = Particle->CastToRigidParticle())
				{
					Rigid->ApplyDynamicsWeight(DynamicsWeight);
				}
				Particle->SyncRemoteData(*Manager, DataIdx, Dirty.PropertyData, Dirty.ShapeDataIndices, ShapeDirtyData);
				Proxy->ClearAccumulatedData();
				Proxy->ResetDirtyIdx();
				break;
			}
			case EPhysicsProxyType::GeometryCollectionType:
			{
				auto Proxy = static_cast<FGeometryCollectionPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(this);
				break;
			}
			case EPhysicsProxyType::JointConstraintType:
			{
				auto Proxy = static_cast<FJointConstraintPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(*Manager, DataIdx, Dirty.PropertyData);
				Proxy->ResetDirtyIdx();
				break;
			}
			case EPhysicsProxyType::SuspensionConstraintType:
			{
				auto Proxy = static_cast<FSuspensionConstraintPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(*Manager, DataIdx, Dirty.PropertyData);
				Proxy->ResetDirtyIdx();
				break;
			}

			default:
			ensure(0 && TEXT("Unknown proxy type in physics solver."));
			}
		});

		if(!!LogDirtyParticles)
		{
			int32 NumJoints = 0;
			int32 NumSuspension = 0;
			int32 NumParticles = 0;
			UE_LOG(LogChaos, Warning, TEXT("LogDirtyParticles:"));
			DirtyProxiesData->ForEachProxy([&NumJoints, &NumSuspension, &NumParticles](int32 DataIdx, FDirtyProxy& Dirty)
			{
				switch (Dirty.Proxy->GetType())
				{
					case EPhysicsProxyType::SingleParticleProxy:
					{
						auto Proxy = static_cast<FSingleParticlePhysicsProxy*>(Dirty.Proxy);
#if CHAOS_DEBUG_NAME
						UE_LOG(LogChaos, Warning, TEXT("\t%s"), **Proxy->GetParticle_LowLevel()->DebugName());
#endif
						++NumParticles;
						break;
					}
					case EPhysicsProxyType::JointConstraintType:
					{
						auto Proxy = static_cast<FJointConstraintPhysicsProxy*>(Dirty.Proxy);
						++NumJoints;
						break;
					}
					case EPhysicsProxyType::SuspensionConstraintType:
					{
						++NumSuspension;
						break;
					}

					default: break;
				}
			});

			UE_LOG(LogChaos, Warning, TEXT("Num Particles:%d Num Shapes:%d Num Joints:%d Num Suspensions:%d"), NumParticles, DirtyProxiesData->NumDirtyShapes(), NumJoints, NumSuspension);
		}

		GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().PushProducerStorageData_External(MarshallingManager.GetExternalTimestamp_External());

		if (MRewindCallback && !IsShuttingDown())
		{
			MRewindCallback->InjectInputs_External(MarshallingManager.GetInternalStep_External(), NumSteps);
		}

		MarshallingManager.Step_External(DeltaTime, NumSteps, GetSolverSubstep_External());
	}

	void FPBDRigidsSolver::ProcessSinglePushedData_Internal(FPushPhysicsData& PushData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ProcessSinglePushedData_Internal);

		FRewindData* RewindData = GetRewindData();

		FDirtySet* DirtyProxiesData = &PushData.DirtyProxiesDataBuffer;
		FDirtyPropertiesManager* Manager = &PushData.DirtyPropertiesManager;
		FShapeDirtyData* ShapeDirtyData = DirtyProxiesData->GetShapesDirtyData();
		FReal ExternalDt = PushData.ExternalDt;

		TArray<FSingleParticlePhysicsProxy*> RegisteredProxies;

		auto ProcessProxyPT = [Manager, ShapeDirtyData, RewindData, ExternalDt, &RegisteredProxies, this](auto& Proxy,int32 DataIdx,FDirtyProxy& Dirty,const auto& CreateHandleFunc)
		{
			const bool bIsNew = !Proxy->IsInitialized();
			if(bIsNew)
			{
				const auto* NonFrequentData = Dirty.PropertyData.FindNonFrequentData(*Manager,DataIdx);
				const FUniqueIdx* UniqueIdx = NonFrequentData ? &NonFrequentData->UniqueIdx() : nullptr;
				Proxy->SetHandle(CreateHandleFunc(UniqueIdx));

				auto Handle = Proxy->GetHandle_LowLevel();
				Handle->GTGeometryParticle() = Proxy->GetParticle_LowLevel();

				// If anybody's listening for proxy creations, build a list of proxies that have been
				// added in this step.
				if (RegistrationWatchers.Num() > 0)
				{
					RegisteredProxies.Add(Proxy);
				}

				// Track proxies
				if (UniqueIdx)
				{
					SingleParticlePhysicsProxies_PT.EmplaceAt(UniqueIdx->Idx, Proxy);
				}
			}

			if(Proxy->GetHandle_LowLevel())
			{
				if (RewindData)
				{
					RewindData->PushGTDirtyData(*Manager, DataIdx, Dirty, ShapeDirtyData);
				}
				Proxy->PushToPhysicsState(*Manager, DataIdx, Dirty, ShapeDirtyData, *GetEvolution(), ExternalDt);
			}
			else
			{
				//The only valid time for a handle to not exist is during a resim, when the proxy was already deleted
				//Another way would be to sanitize pending push data, but this would be expensive
				ensure(RewindData && RewindData->IsResim());
			}

			if(bIsNew)
			{
				auto Handle = Proxy->GetHandle_LowLevel();
				if(auto Rigid = Handle->CastToRigidParticle())
				{
					Rigid->SetPreObjectStateLowLevel(Rigid->ObjectState());	//created this frame so pre is the initial value
				}
				Handle->SetPhysicsProxy(Proxy);
				GetEvolution()->RegisterParticle(Handle);
				Proxy->SetInitialized(GetCurrentFrame());
			}
		};

		//need to create new particle handles
		DirtyProxiesData->ForEachProxy([this,&ProcessProxyPT](int32 DataIdx,FDirtyProxy& Dirty)
		{
			if(Dirty.Proxy->GetIgnoreDataOnStep_Internal() != CurrentFrame)
			{
				switch(Dirty.Proxy->GetType())
				{
					case EPhysicsProxyType::SingleParticleProxy:
					{
						auto Proxy = static_cast<FSingleParticlePhysicsProxy*>(Dirty.Proxy);
						ProcessProxyPT(Proxy, DataIdx, Dirty, [this, &Dirty](const FUniqueIdx* UniqueIdx) -> TGeometryParticleHandle<FReal,3>*
						{
							switch (Dirty.PropertyData.GetParticleBufferType())
							{
								case EParticleType::Static: return Particles.CreateStaticParticles(1, UniqueIdx)[0];
								case EParticleType::Kinematic: return Particles.CreateKinematicParticles(1, UniqueIdx)[0];
								case EParticleType::Rigid: return Particles.CreateDynamicParticles(1, UniqueIdx)[0];
								default: check(false); return nullptr;
							}
						});
						break;
					}
			
					case EPhysicsProxyType::GeometryCollectionType:
					{
						auto Proxy = static_cast<FGeometryCollectionPhysicsProxy*>(Dirty.Proxy);
						Proxy->PushToPhysicsState();
						// Currently no push needed for geometry collections and they handle the particle creation internally
						// #TODO This skips the rewind data push so GC will not be rewindable until resolved.
						Dirty.Proxy->ResetDirtyIdx();
						break;
					}
					case EPhysicsProxyType::JointConstraintType:
					case EPhysicsProxyType::SuspensionConstraintType:
					{
						// Pass until after all bodies are created. 
						break;
					}
					default:
					{
						ensure(0 && TEXT("Unknown proxy type in physics solver."));
						//Can't use, but we can still mark as "clean"
						Dirty.Proxy->ResetDirtyIdx();
					}
				}
			}
		});

		//need to create new constraint handles
		DirtyProxiesData->ForEachProxy([this, Manager, RewindData](int32 DataIdx, FDirtyProxy& Dirty)
		{
			if (Dirty.Proxy->GetIgnoreDataOnStep_Internal() != CurrentFrame)
			{
				switch (Dirty.Proxy->GetType())
				{
				case EPhysicsProxyType::JointConstraintType:
				{
					auto JointProxy = static_cast<FJointConstraintPhysicsProxy*>(Dirty.Proxy);
					const bool bIsNew = !JointProxy->IsInitialized();
					if (bIsNew)
					{
						JointConstraintPhysicsProxies_Internal.Add(JointProxy);
						JointProxy->InitializeOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
						JointProxy->SetInitialized(GetCurrentFrame());
					}

					//TODO: if we support predicting creation / destruction of joints need to handle null joint case
					if (RewindData)
					{
						RewindData->PushGTDirtyData(*Manager, DataIdx, Dirty, nullptr);
					}
				
					JointProxy->PushStateOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
					Dirty.Proxy->ResetDirtyIdx();
					break;
				}

				case EPhysicsProxyType::SuspensionConstraintType:
				{
					auto SuspensionProxy = static_cast<FSuspensionConstraintPhysicsProxy*>(Dirty.Proxy);
					const bool bIsNew = !SuspensionProxy->IsInitialized();
					if (bIsNew)
					{
						SuspensionProxy->InitializeOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
						SuspensionProxy->SetInitialized();
					}
					SuspensionProxy->PushStateOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
					Dirty.Proxy->ResetDirtyIdx();
					break;
				}

				}
			}
		});

		// If we have callbacks watching for particle registrations, send them the array
		// of newly added proxies
		if (RegistrationWatchers.Num() > 0)
		{
			for (ISimCallbackObject* Callback : RegistrationWatchers)
			{
				Callback->OnParticlesRegistered_Internal(RegisteredProxies);
			}
		}

		//MarshallingManager.FreeData_Internal(&PushData);
	}

	void FPBDRigidsSolver::ProcessPushedData_Internal(FPushPhysicsData& PushData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(ChaosPushData);
		ensure(PushData.InternalStep == CurrentFrame);	//push data was generated for this specific frame

		//update callbacks
		SimCallbackObjects.Reserve(SimCallbackObjects.Num() + PushData.SimCallbackObjectsToAdd.Num());
		for(ISimCallbackObject* SimCallbackObject : PushData.SimCallbackObjectsToAdd)
		{
			if (SimCallbackObject->HasOption(ESimCallbackOptions::Presimulate))
			{
				SimCallbackObjects.Add(SimCallbackObject);
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::ContactModification))
			{
				ContactModifiers.Add(SimCallbackObject);
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::ParticleRegister))
			{
				RegistrationWatchers.Add(SimCallbackObject);
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::ParticleUnregister))
			{
				UnregistrationWatchers.Add(SimCallbackObject);
			}
		}

		//save any pending data for this particular interval
		for (const FSimCallbackInputAndObject& InputAndCallbackObj : PushData.SimCallbackInputs)
		{
			InputAndCallbackObj.CallbackObject->SetCurrentInput_Internal(InputAndCallbackObj.Input);
		}

		//remove any callbacks that are unregistered
		for (ISimCallbackObject* RemovedCallbackObject : PushData.SimCallbackObjectsToRemove)
		{
			RemovedCallbackObject->bPendingDelete = true;
		}

		for (int32 Idx = ContactModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			ISimCallbackObject* Callback = ContactModifiers[Idx];
			if (Callback->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				ContactModifiers.RemoveAtSwap(Idx, 1, false);
			}
		}

		for (int32 Idx = SimCallbackObjects.Num() - 1; Idx >= 0; --Idx)
		{
			ISimCallbackObject* Callback = SimCallbackObjects[Idx];
			if (Callback->bPendingDelete)
			{
				SimCallbackObjects.RemoveAtSwap(Idx, 1, false);
			}
		}

		ProcessSinglePushedData_Internal(PushData);

		//run any commands passed in. These don't generate outputs and are a one off so just do them here
		//note: commands run before sim callbacks. This is important for sub-stepping since we want each sub-step to have a consistent view
		//so for example if the user deletes a floor surface, we want all sub-steps to see that in the same way
		//also note, the commands run after data is marshalled over. This is important because data marshalling ensures any GT property changes are seen by command
		//for example a particle may not be created until marshalling occurs, and then a command could explicitly modify something like a collision setting
		for (FSimCallbackCommandObject* SimCallbackObject : PushData.SimCommands)
		{
			SimCallbackObject->PreSimulate_Internal();
			delete SimCallbackObject;
		}
		PushData.SimCommands.Reset();

		if(MRewindCallback && !IsShuttingDown())
		{
			MRewindCallback->ProcessInputs_Internal(MRewindData->CurrentFrame(), PushData.SimCallbackInputs);
		}
	}

	void FPBDRigidsSolver::ConditionalApplyRewind_Internal()
	{
		// Note: checking MRewindData->IsResim() can lead to recursion into this function on the last resim frame since the call to AdvanceSolver is what advances RewindData's internal frame
		if(!IsShuttingDown() && MRewindCallback && !GetEvolution()->IsResimming())
		{
			const int32 LastStep = MRewindData->CurrentFrame() - 1;
			const int32 ResimStep = MRewindCallback->TriggerRewindIfNeeded_Internal(LastStep);
			if(ResimStep != INDEX_NONE)
			{
				FResimDebugInfo DebugInfo;
				QUICK_SCOPE_CYCLE_COUNTER(ChaosRewindAndResim);
				if(ensure(MRewindData->RewindToFrame(ResimStep)))
				{
					GetEvolution()->SetResim(true);
					CurrentFrame = ResimStep;
					const int32 NumResimSteps = LastStep - ResimStep + 1;
					TArray<FPushPhysicsData*> RecordedPushData = MarshallingManager.StealHistory_Internal(NumResimSteps);
					bool bFirst = true;

					FDurationTimer ResimTimer(DebugInfo.ResimTime);
					// Do rollback as necessary
					for (int32 Step = ResimStep; Step <= LastStep; ++Step)
					{
						FPushPhysicsData* PushData = RecordedPushData[LastStep - Step];	//push data is sorted as latest first
						if(bFirst)
						{
							MTime = PushData->StartTime;	//not sure if sub-steps have proper StartTime so just do this once and let solver evolve remaining time
						}

						MRewindCallback->PreResimStep_Internal(Step, bFirst);
						FAllSolverTasks ImmediateTask(*this, PushData);
						ensure(bSolverHasFrozenGameThreadCallbacks == false);	//We don't support this for resim as it's very expensive and difficult to schedule
						ImmediateTask.AdvanceSolver();
						MRewindCallback->PostResimStep_Internal(Step);

						bFirst = false;
					}

					GetEvolution()->SetResim(false);

					ResimTimer.Stop();
					MRewindCallback->SetResimDebugInfo_Internal(DebugInfo);
				}
			}

		}
	}

	void FPBDRigidsSolver::CompleteSceneSimulation()
	{
		LLM_SCOPE(ELLMTag::Chaos);
		SCOPE_CYCLE_COUNTER(STAT_BufferPhysicsResults);

		EventPreBuffer.Broadcast(MLastDt);
		GetDirtyParticlesBuffer()->CaptureSolverData(this);
		BufferPhysicsResults();
	}

	void FPBDRigidsSolver::BufferPhysicsResults()
	{
		//ensure(IsInPhysicsThread());
		TArray<FGeometryCollectionPhysicsProxy*> ActiveGC;
		ActiveGC.Reserve(GeometryCollectionPhysicsProxies_Internal.Num());

		FPullPhysicsData* PullData = MarshallingManager.GetCurrentPullData_Internal();

		TParticleView<FPBDRigidParticles>& DirtyParticles = GetParticles().GetDirtyParticlesView();
		const bool bIsResim = GetEvolution()->IsResimming();

		//todo: should be able to go wide just add defaulted etc...
		{
			ensure(PullData->DirtyRigids.Num() == 0);	//we only fill this once per frame
			int32 BufferIdx = 0;
			PullData->DirtyRigids.Reserve(DirtyParticles.Num());

			for (Chaos::TPBDRigidParticleHandleImp<FReal, 3, false>& DirtyParticle : DirtyParticles)
			{
				if(IPhysicsProxyBase* Proxy = DirtyParticle.Handle()->PhysicsProxy())
				{
					switch(DirtyParticle.GetParticleType())
					{
						case Chaos::EParticleType::Rigid:
						{
							if(!bIsResim || DirtyParticle.SyncState() == ESyncState::HardDesync)
							{
								PullData->DirtyRigids.AddDefaulted();
								((FSingleParticlePhysicsProxy*)(Proxy))->BufferPhysicsResults(PullData->DirtyRigids.Last());
							}
							break;
						}
						case Chaos::EParticleType::Kinematic:
						case Chaos::EParticleType::Static:
							ensure(false);
							break;
						case Chaos::EParticleType::GeometryCollection:
							ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(Proxy));
							break;
						case Chaos::EParticleType::Clustered:
							if (auto ClusterParticle = DirtyParticle.CastToClustered())
							{
								if (ClusterParticle->InternalCluster())
								{
									const TSet<IPhysicsProxyBase*> Proxies = ClusterParticle->PhysicsProxies();
									for (IPhysicsProxyBase* ClusterProxy : Proxies)
									{
										ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(ClusterProxy));
									}
								}
								else
								{
									ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(Proxy));
								}
							}
							break;
						default:
							check(false);
					}
				}	
			}
		}

		{
			ensure(PullData->DirtyGeometryCollections.Num() == 0);	//we only fill this once per frame
			PullData->DirtyGeometryCollections.Reserve(ActiveGC.Num());

			for (int32 Idx = 0; Idx < ActiveGC.Num(); ++Idx)
			{
				PullData->DirtyGeometryCollections.AddDefaulted();
				ActiveGC[Idx]->BufferPhysicsResults_Internal(this, PullData->DirtyGeometryCollections.Last());
			}
		}

		{
			ensure(PullData->DirtyJointConstraints.Num() == 0);	//we only fill this once per frame
			PullData->DirtyJointConstraints.Reserve(JointConstraintPhysicsProxies_Internal.Num());

			for(int32 Idx = 0; Idx < JointConstraintPhysicsProxies_Internal.Num(); ++Idx)
			{
				PullData->DirtyJointConstraints.AddDefaulted();
				JointConstraintPhysicsProxies_Internal[Idx]->BufferPhysicsResults(PullData->DirtyJointConstraints.Last());
			}
		}
		

		
		// Now that results have been buffered we have completed a solve step so we can broadcast that event
		EventPostSolve.Broadcast(MLastDt);


		Particles.ClearTransientDirty();

	}

	void FPBDRigidsSolver::BeginDestroy()
	{
		MEvolution->SetCanStartAsyncTasks(false);
	}
	
	// This function is not called during normal Engine execution.  
	// FPhysScene_ChaosInterface::EndFrame() calls 
	// FPhysScene_ChaosInterface::SyncBodies() instead, and then immediately afterwards 
	// calls FPBDRigidsSovler::SyncEvents_GameThread().  This function is used by tests,
	// however.
	void FPBDRigidsSolver::UpdateGameThreadStructures()
	{
		PullPhysicsStateForEachDirtyProxy_External([](auto){}, [](auto) {});
	}

	int32 FPBDRigidsSolver::NumJointConstraints() const
	{
		return MEvolution->GetJointConstraints().NumConstraints();
	}
	
	int32 FPBDRigidsSolver::NumCollisionConstraints() const
	{
		return GetEvolution()->GetCollisionConstraints().NumConstraints();
	}

#ifndef CHAOS_COUNTER_STAT
#define CHAOS_COUNTER_STAT(Name, Value)\
SET_DWORD_STAT(STAT_ChaosCounter_##Name, Value); \
CSV_CUSTOM_STAT(PhysicsCounters, Name, Value, ECsvCustomStatOp::Set);
#endif


	void FPBDRigidsSolver::UpdateStatCounters() const
	{
		// Particle counts
		CHAOS_COUNTER_STAT(NumDisabledBodies, GetEvolution()->GetParticles().GetAllParticlesView().Num() - GetEvolution()->GetParticles().GetNonDisabledView().Num());
		CHAOS_COUNTER_STAT(NumBodies, GetEvolution()->GetParticles().GetNonDisabledView().Num());
		CHAOS_COUNTER_STAT(NumDynamicBodies, GetEvolution()->GetParticles().GetNonDisabledDynamicView().Num());
		CHAOS_COUNTER_STAT(NumActiveDynamicBodies, GetEvolution()->GetParticles().GetActiveParticlesView().Num());
		CHAOS_COUNTER_STAT(NumKinematicBodies, GetEvolution()->GetParticles().GetActiveKinematicParticlesView().Num());
		CHAOS_COUNTER_STAT(NumStaticBodies, GetEvolution()->GetParticles().GetActiveStaticParticlesView().Num());
		CHAOS_COUNTER_STAT(NumGeometryCollectionBodies, (int32)GetEvolution()->GetParticles().GetGeometryCollectionParticles().Size());

		// Constraint counts
		CHAOS_COUNTER_STAT(NumIslands, GetEvolution()->GetConstraintGraph().NumIslands());
		CHAOS_COUNTER_STAT(NumIslandGroups, GetEvolution()->GetIslandGroupManager().GetNumActiveGroups());
		CHAOS_COUNTER_STAT(NumContacts, NumCollisionConstraints());
		CHAOS_COUNTER_STAT(NumJoints, NumJointConstraints());

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		UpdateExpensiveStatCounters();
#endif

		// Iterations
		CHAOS_COUNTER_STAT(NumPositionIterations, GetEvolution()->GetNumPositionIterations());
		CHAOS_COUNTER_STAT(NumVelocityIterations, GetEvolution()->GetNumVelocityIterations());
		CHAOS_COUNTER_STAT(NumProjectionIterations, GetEvolution()->GetNumProjectionIterations());
	}

	void FPBDRigidsSolver::UpdateExpensiveStatCounters() const
	{
		int32 NumValidCollisions = 0;
		int32 NumActiveCollisions = 0;
		int32 NumRestoredCollisions = 0;
		int32 NumManifoldPoints = 0;
		int32 NumActiveManifoldPoints = 0;
		int32 NumRestoredManifoldPoints = 0;
		int32 NumUpdatedManifoldPoints = 0;
		for (const FPBDCollisionConstraintHandle* Collision : GetEvolution()->GetCollisionConstraints().GetConstraints())
		{
			if (Collision->GetContact().IsEnabled())
			{
				if (Collision->GetContact().GetManifoldPoints().Num() > 0)
				{
					++NumValidCollisions;
				}
				if (!Collision->GetContact().AccumulatedImpulse.IsNearlyZero())
				{
					++NumActiveCollisions;
				}
				if (Collision->GetContact().WasManifoldRestored())
				{
					++NumRestoredCollisions;
				}
				for (const FManifoldPoint& ManifoldPoint : Collision->GetContact().GetManifoldPoints())
				{
					++NumManifoldPoints;
					if (ManifoldPoint.Flags.bWasRestored || Collision->GetContact().WasManifoldRestored())
					{
						++NumRestoredManifoldPoints;
					}
					if (ManifoldPoint.Flags.bWasReplaced)
					{
						++NumUpdatedManifoldPoints;
					}
					if (!ManifoldPoint.NetPushOut.IsNearlyZero())
					{
						++NumActiveManifoldPoints;
					}
				}

			}
		}

		CHAOS_COUNTER_STAT(NumValidConstraints, NumValidCollisions);
		CHAOS_COUNTER_STAT(NumActiveConstraints, NumActiveCollisions);
		CHAOS_COUNTER_STAT(NumRestoredConstraints, NumRestoredCollisions);
		CHAOS_COUNTER_STAT(NumManifoldPoints, NumManifoldPoints);
		CHAOS_COUNTER_STAT(NumActiveManifoldPoints, NumActiveManifoldPoints);
		CHAOS_COUNTER_STAT(NumRestoredManifoldPoints, NumRestoredManifoldPoints);
		CHAOS_COUNTER_STAT(NumUpdatedManifoldPoints, NumUpdatedManifoldPoints);
	}

	void FPBDRigidsSolver::PostTickDebugDraw(FReal Dt) const
	{
#if CHAOS_DEBUG_DRAW
		QUICK_SCOPE_CYCLE_COUNTER(SolverDebugDraw);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		if (ChaosSolverDebugDrawColorShapeByClientServer)
		{
			if (DebugName.ToString().StartsWith(TEXT("Server")))
			{
				ChaosSolverDebugDebugDrawSettings.ShapesColorsPerState = GetSolverShapesColorsByState_Server();
			}
			else
			{
				ChaosSolverDebugDebugDrawSettings.ShapesColorsPerState = GetSolverShapesColorsByState_Client();
			}
		}
		else
		{
			ChaosSolverDebugDebugDrawSettings.ShapesColorsPerState = DebugDraw::GetDefaultShapesColorsByState();
		}
#endif

		if (ChaosSolverDebugDrawShapes == 1)
		{
			if (ChaosSolverDrawShapesShowStatic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveStaticParticlesView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
			}
			if (ChaosSolverDrawShapesShowKinematic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveKinematicParticlesView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
			}
			if (ChaosSolverDrawShapesShowDynamic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetNonDisabledDynamicView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
			}
		}
		if (ChaosSolverDebugDrawCollisions == 1) 
		{
			DebugDraw::DrawCollisions(FRigidTransform3(), GetEvolution()->GetCollisionConstraints().GetConstraintAllocator(), 1.f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawBounds == 1)
		{
			DebugDraw::DrawParticleBounds(FRigidTransform3(), Particles.GetActiveStaticParticlesView(), Dt, &ChaosSolverDebugDebugDrawSettings);
			DebugDraw::DrawParticleBounds(FRigidTransform3(), Particles.GetActiveKinematicParticlesView(), Dt, &ChaosSolverDebugDebugDrawSettings);
			DebugDraw::DrawParticleBounds(FRigidTransform3(), Particles.GetNonDisabledDynamicView(), Dt, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawTransforms == 1)
		{
			DebugDraw::DrawParticleTransforms(FRigidTransform3(), Particles.GetAllParticlesView(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawIslands == 1)
		{
			DebugDraw::DrawConstraintGraph(FRigidTransform3(), GetEvolution()->GetConstraintGraph(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawClusterConstraints == 1)
		{
			DebugDraw::DrawConnectionGraph(MEvolution->GetRigidClustering(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawCollidingShapes == 1)
		{
			DebugDraw::DrawCollidingShapes(FRigidTransform3(), GetEvolution()->GetCollisionConstraints(), 1.f, 0.f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawJoints == 1)
		{
			DebugDraw::DrawJointConstraints(FRigidTransform3(), MEvolution->GetJointConstraints(), 1.0f, ChaosSolverDrawJointFeatures, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawSpatialAccelerationStructure)
		{
			if (const auto SpatialAccelerationStructure = GetEvolution()->GetSpatialAcceleration())
			{
				DebugDraw::DrawSpatialAccelerationStructure(*SpatialAccelerationStructure, &ChaosSolverDebugDebugDrawSettings);
			}
		}
		if (ChaosSolverDebugDrawCollidingShapes == 1)
		{
			DebugDraw::DrawCollidingShapes(FRigidTransform3(), GetEvolution()->GetCollisionConstraints(), 1.f, 0.f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawSuspensionConstraints == 1)
		{
			DebugDraw::DrawSuspensionConstraints(FRigidTransform3(), GetEvolution()->GetSuspensionConstraints(), &ChaosSolverDebugDebugDrawSettings);
		}

#endif
	}

	FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FGeometryParticleHandle& Handle)
	{
		const FUniqueIdx UniqueIdx = Handle.UniqueIdx();
		return SingleParticlePhysicsProxies_PT.IsValidIndex(UniqueIdx.Idx)
			? SingleParticlePhysicsProxies_PT[UniqueIdx.Idx] : nullptr;
	}

	const FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FGeometryParticleHandle& Handle) const
	{
		const FUniqueIdx UniqueIdx = Handle.UniqueIdx();
		return SingleParticlePhysicsProxies_PT.IsValidIndex(UniqueIdx.Idx)
			? SingleParticlePhysicsProxies_PT[UniqueIdx.Idx] : nullptr;
	}

	void FPBDRigidsSolver::PostEvolutionVDBPush() const
	{
#if CHAOS_VISUAL_DEBUGGER_ENABLED
		if (ChaosVisualDebuggerEnable)
		{
			const TGeometryParticleHandles<FReal, 3>&  AllParticleHandles = GetEvolution()->GetParticleHandles();
			for (uint32 ParticelIndex = 0; ParticelIndex < AllParticleHandles.Size(); ParticelIndex++)
			{
				const TUniquePtr<FGeometryParticleHandle>& ParticleHandle = AllParticleHandles.Handle(ParticelIndex);
				ChaosVisualDebugger::ParticlePositionLog(ParticleHandle->X());				
			}
		}
#endif
	}

	void FPBDRigidsSolver::UpdateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterials.Get(InHandle.InnerHandle) = InNewData;
	}

	void FPBDRigidsSolver::CreateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterials.Create(InNewData) == InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::DestroyMaterial(Chaos::FMaterialHandle InHandle)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		SimMaterials.Destroy(InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::UpdateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterialMasks.Get(InHandle.InnerHandle) = InNewData;
	}

	void FPBDRigidsSolver::CreateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterialMasks.Create(InNewData) == InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::DestroyMaterialMask(Chaos::FMaterialMaskHandle InHandle)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		SimMaterialMasks.Destroy(InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::SyncQueryMaterials_External()
	{
		// Using lock on sim material is an imprefect workaround, we may block while physics thread is updating sim materials in callbacks.
		// QueryMaterials may be slightly stale. Need to rethink lifetime + ownership of materials for async case.
		//acquire external data lock
		FPhysicsSceneGuardScopedWrite ScopedWrite(GetExternalDataLock_External());
		TSolverSimMaterialScope<ELockType::Read> SimMatLock(this);
		
		QueryMaterials_External = SimMaterials;
		QueryMaterialMasks_External = SimMaterialMasks;
	}


	void FPBDRigidsSolver::FinalizeRewindData(const TParticleView<TPBDRigidParticles<FReal,3>>& DirtyParticles)
	{
		using namespace Chaos;
		//Simulated objects must have their properties captured for rewind
		if(MRewindData && DirtyParticles.Num())
		{
			QUICK_SCOPE_CYCLE_COUNTER(RecordRewindData);
			
			int32 DataIdx = 0;
			for(TPBDRigidParticleHandleImp<FReal,3,false>& DirtyParticle : DirtyParticles)
			{
				MRewindData->PushPTDirtyData(*DirtyParticle.Handle(), DataIdx++);
			}
		}
	}

	void FPBDRigidsSolver::UpdateExternalAccelerationStructure_External(ISpatialAccelerationCollection<FAccelerationStructureHandle,FReal,3>*& ExternalStructure)
	{
		GetEvolution()->UpdateExternalAccelerationStructure_External(ExternalStructure,*PendingSpatialOperations_External);
	}

	bool FPBDRigidsSolver::IsDetemerministic() const
	{
		return bIsDeterministic || (MRewindData != nullptr) || (ChaosSolverDeterministic >= 1);
	}

	void FPBDRigidsSolver::SetIsDeterministic(const bool bInIsDeterministic)
	{
		if (bIsDeterministic != bInIsDeterministic)
		{
			bIsDeterministic = bInIsDeterministic;
			UpdateIsDeterministic();
		}
	}

	void FPBDRigidsSolver::UpdateIsDeterministic()
	{
		GetEvolution()->SetIsDeterministic(IsDetemerministic());
	}


	Chaos::FClusterCreationParameters::EConnectionMethod ToInternalConnectionMethod(EClusterUnionMethod InMethod)
	{
		using ETargetEnum = Chaos::FClusterCreationParameters::EConnectionMethod;
		switch(InMethod)
		{
		case EClusterUnionMethod::PointImplicit:
			return ETargetEnum::PointImplicit;
		case EClusterUnionMethod::DelaunayTriangulation:
			return ETargetEnum::DelaunayTriangulation;
		case EClusterUnionMethod::MinimalSpanningSubsetDelaunayTriangulation:
			return ETargetEnum::MinimalSpanningSubsetDelaunayTriangulation;
		case EClusterUnionMethod::PointImplicitAugmentedWithMinimalDelaunay:
			return ETargetEnum::PointImplicitAugmentedWithMinimalDelaunay;
		case EClusterUnionMethod::BoundsOverlapFilteredDelaunayTriangulation:
			return ETargetEnum::BoundsOverlapFilteredDelaunayTriangulation;
		}

		return ETargetEnum::None;
	}

	void Chaos::FPBDRigidsSolver::ApplyConfig(const FChaosSolverConfiguration& InConfig)
	{
		GetEvolution()->GetRigidClustering().SetClusterConnectionFactor(InConfig.ClusterConnectionFactor);
		GetEvolution()->GetRigidClustering().SetClusterUnionConnectionType(ToInternalConnectionMethod(InConfig.ClusterUnionConnectionType));
		SetPositionIterations(InConfig.PositionIterations);
		SetVelocityIterations(InConfig.VelocityIterations);
		SetProjectionIterations(InConfig.ProjectionIterations);
		SetCollisionCullDistance(InConfig.CollisionCullDistance);
		SetCollisionMaxPushOutVelocity(InConfig.CollisionMaxPushOutVelocity);
		SetGenerateCollisionData(InConfig.bGenerateCollisionData);
		SetGenerateBreakingData(InConfig.bGenerateBreakData);
		SetGenerateTrailingData(InConfig.bGenerateTrailingData);
		SetCollisionFilterSettings(InConfig.CollisionFilterSettings);
		SetBreakingFilterSettings(InConfig.BreakingFilterSettings);
		SetTrailingFilterSettings(InConfig.TrailingFilterSettings);
	}

	FPBDRigidsSolver::~FPBDRigidsSolver()
	{
		EventTeardown.Broadcast();
	}

	void Chaos::FPBDRigidsSolver::FieldParameterUpdateCallback(
		Chaos::FPBDPositionConstraints& PositionTarget,
		TMap<int32, int32>& TargetedParticles)
	{
		GetPerSolverField().FieldParameterUpdateCallback(this, PositionTarget, TargetedParticles);
	}

	void Chaos::FPBDRigidsSolver::FieldForcesUpdateCallback()
	{
		GetPerSolverField().FieldForcesUpdateCallback(this);
	}

}; // namespace Chaos

