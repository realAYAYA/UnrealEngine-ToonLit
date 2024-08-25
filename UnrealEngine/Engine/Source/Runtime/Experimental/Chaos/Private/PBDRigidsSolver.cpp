// Copyright Epic Games, Inc. All Rights Reserved.

#include "PBDRigidsSolver.h"

#include "Async/AsyncWork.h"
#include "Chaos/ChaosArchive.h"
#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/PBDCollisionConstraintsUtil.h"
#include "Chaos/Utilities.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "ChaosStats.h"
#include "ChaosSolversModule.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
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
#include "Chaos/ConvexOptimizer.h"

#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ChaosLog.h"

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY_STATIC(LogPBDRigidsSolver, Log, All);

// Stat Counters
DECLARE_DWORD_COUNTER_STAT(TEXT("NumDisabledBodies"), STAT_ChaosCounter_NumDisabledBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumBodies"), STAT_ChaosCounter_NumBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumDynamicBodies"), STAT_ChaosCounter_NumDynamicBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumKinematicBodies"), STAT_ChaosCounter_NumKinematicBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumStaticBodies"), STAT_ChaosCounter_NumStaticBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumGeomCollBodies"), STAT_ChaosCounter_NumGeometryCollectionBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumMovingBodies"), STAT_ChaosCounter_NumMovingBodies, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumStaticShapes"), STAT_ChaosCounter_NumStaticShapes, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumKinematicShapes"), STAT_ChaosCounter_NumKinematicShapes, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumDynamicShapes"), STAT_ChaosCounter_NumDynamicShapes, STATGROUP_ChaosCounters);
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
DECLARE_DWORD_COUNTER_STAT(TEXT("NumCharacterGroundConstraints"), STAT_ChaosCounter_NumCharacterGroundConstraints, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumBroadPhasePairs"), STAT_ChaosCounter_NumBroadPhasePairs, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumMidPhases"), STAT_ChaosCounter_NumMidPhases, STATGROUP_ChaosCounters);

TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumDisabledBodies, TEXT("Chaos/Solver/Bodies/NumDisabled"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumBodies, TEXT("Chaos/Solver/Bodies/Num"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumStaticBodies, TEXT("Chaos/Solver/Bodies/NumStatic"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumKinematicBodies, TEXT("Chaos/Solver/Bodies/NumKinematic"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumDynamicBodies, TEXT("Chaos/Solver/Bodies/NumDynamic"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumGeometryCollectionBodies, TEXT("Chaos/Solver/Bodies/NumGC"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumMovingBodies, TEXT("Chaos/Solver/Bodies/NumMoving"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumStaticShapes, TEXT("Chaos/Solver/Collisions/NumStaticShapes"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumKinematicShapes, TEXT("Chaos/Solver/Collisions/NumKinematicShapes"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumDynamicShapes, TEXT("Chaos/Solver/Collisions/NumDynamicShapes"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumIslands, TEXT("Chaos/Solver/Islands/NumIslands"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumIslandGroups, TEXT("Chaos/Solver/Islands/NumIslandGroups"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumContacts, TEXT("Chaos/Solver/Collisions/NumConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumValidConstraints, TEXT("Chaos/Solver/Collisions/NumValidConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumActiveConstraints, TEXT("Chaos/Solver/Collisions/NumActiveConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumRestoredConstraints, TEXT("Chaos/Solver/Collisions/NumRestoredConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumManifoldPoints, TEXT("Chaos/Solver/Collisions/NumManifoldPoints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumActiveManifoldPoints, TEXT("Chaos/Solver/Collisions/NumActiveManifoldPoints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumRestoredManifoldPoints, TEXT("Chaos/Solver/Collisions/NumRestoredManifoldPoints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumUpdatedManifoldPoints, TEXT("Chaos/Solver/Collisions/NumUpdatedManifoldPoints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumJoints, TEXT("Chaos/Solver/Joints/NumConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumCharacterGroundConstraints, TEXT("Chaos/Solver/Character/NumConstraints"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumBroadPhasePairs, TEXT("Chaos/Solver/Collisions/NumBroadPhasePairs"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumMidPhases, TEXT("Chaos/Solver/Collisions/NumMidPhases"));

TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_MidPhase_NumShapePair, TEXT("Chaos/Solver/MidPhase/NumShapePair"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_MidPhase_NumGeneric, TEXT("Chaos/Solver/MidPhase/NumGeneric"));

// Stat Iteration counters
DECLARE_DWORD_COUNTER_STAT(TEXT("NumPositionIterations"), STAT_ChaosCounter_NumPositionIterations, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumVelocityIterations"), STAT_ChaosCounter_NumVelocityIterations, STATGROUP_ChaosCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumProjectionIterations"), STAT_ChaosCounter_NumProjectionIterations, STATGROUP_ChaosCounters);

TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumPositionIterations, TEXT("Chaos/Solver/Iterations/NumPosition"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumVelocityIterations, TEXT("Chaos/Solver/Iterations/NumVelocity"));
TRACE_DECLARE_INT_COUNTER(ChaosTraceCounter_NumProjectionIterations, TEXT("Chaos/Solver/Iterations/NumProjection"));


// DebugDraw CVars
#if CHAOS_DEBUG_DRAW

// Must be 0 when checked in...
#define CHAOS_SOLVER_ENABLE_DEBUG_DRAW 0

namespace Chaos
{
	namespace DebugDraw
	{
		extern const FChaosDebugDrawColorsByState& GetDefaultShapesColorsPreIntegrate();
		extern const FChaosDebugDrawColorsByState& GetDefaultShapesColorsPostIntegrate();
		extern const FChaosDebugDrawColorsByState& GetDefaultShapesColorsCollisionDetection();
	}

	namespace CVars
	{
		int32 ChaosSolverDebugDrawShapes = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
		int32 ChaosSolverDebugDrawMass = 0;
		int32 ChaosSolverDebugDrawBVHs = 0;
		int32 ChaosSolverDebugDrawCollisions = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
		int32 ChaosSolverDebugDrawCollidingShapes = 0;
		int32 ChaosSolverDebugDrawBounds = 0;
		int32 ChaosSolverDrawTransforms = 0;
		int32 ChaosSolverDrawIslands = 0;
		int32 ChaosSolverDebugDrawIslandSleepState = 0;
		int32 ChaosSolverDrawCCDInteractions = 0;
		int32 ChaosSolverDrawCCDThresholds = 0;
		int32 ChaosSolverDrawShapesShowStatic = 1;
		int32 ChaosSolverDrawShapesShowKinematic = 1;
		int32 ChaosSolverDrawShapesShowDynamic = 1;
		int32 ChaosSolverDrawJoints = 0;
		int32 ChaosSolverDrawCharacterGroundConstraints = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructure = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructureShowLeaves = 0;
		int32 ChaosSolverDebugDrawSpatialAccelerationStructureShowNodes = 0;
		int32 ChaosSolverDebugDrawSuspensionConstraints = 0;
		int32 ChaosSolverDrawClusterConstraints = 0;
		int32 ChaosSolverDebugDrawMeshContacts = 0;
		int32 ChaosSolverDebugDrawMeshBVHOverlaps = 0;
		int32 ChaosSolverDebugDrawColorShapeByClientServer = 0;
		int32 ChaosSolverDebugDrawShowServer = 1;
		int32 ChaosSolverDebugDrawShowClient = 1;
		DebugDraw::FChaosDebugDrawJointFeatures ChaosSolverDrawJointFeatures = DebugDraw::FChaosDebugDrawJointFeatures::MakeDefault();
		FAutoConsoleVariableRef CVarChaosSolverDrawShapes(TEXT("p.Chaos.Solver.DebugDrawShapes"), ChaosSolverDebugDrawShapes, TEXT("Draw Shapes (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawMass(TEXT("p.Chaos.Solver.DebugDrawMass"), ChaosSolverDebugDrawMass, TEXT("Draw Mass values in Kg (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawBVHs(TEXT("p.Chaos.Solver.DebugDrawBVHs"), ChaosSolverDebugDrawBVHs, TEXT("Draw Particle BVHs where applicable (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCollisions(TEXT("p.Chaos.Solver.DebugDrawCollisions"), ChaosSolverDebugDrawCollisions, TEXT("Draw Collisions (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCollidingShapes(TEXT("p.Chaos.Solver.DebugDrawCollidingShapes"), ChaosSolverDebugDrawCollidingShapes, TEXT("Draw Shapes that have collisions on them (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawBounds(TEXT("p.Chaos.Solver.DebugDrawBounds"), ChaosSolverDebugDrawBounds, TEXT("Draw bounding volumes inside the broadphase (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawTransforms(TEXT("p.Chaos.Solver.DebugDrawTransforms"), ChaosSolverDrawTransforms, TEXT("Draw particle transforms (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawIslands(TEXT("p.Chaos.Solver.DebugDrawIslands"), ChaosSolverDrawIslands, TEXT("Draw solver islands (0 = never; 1 = end of frame)."));
		FAutoConsoleVariableRef CVarChaosSolverDrawIslandSleepState(TEXT("p.Chaos.Solver.DebugDrawSleepState"), ChaosSolverDebugDrawIslandSleepState, TEXT("Draw island sleep state."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCCD(TEXT("p.Chaos.Solver.DebugDrawCCDInteractions"), ChaosSolverDrawCCDInteractions, TEXT("Draw CCD interactions."));
		FAutoConsoleVariableRef CVarChaosSolverDrawCCDThresholds(TEXT("p.Chaos.Solver.DebugDrawCCDThresholds"), ChaosSolverDrawCCDThresholds, TEXT("Draw CCD swept thresholds."));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesStatic(TEXT("p.Chaos.Solver.DebugDraw.ShowStatics"), ChaosSolverDrawShapesShowStatic, TEXT("If DebugDrawShapes is enabled, whether to show static objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesKinematic(TEXT("p.Chaos.Solver.DebugDraw.ShowKinematics"), ChaosSolverDrawShapesShowKinematic, TEXT("If DebugDrawShapes is enabled, whether to show kinematic objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesDynamic(TEXT("p.Chaos.Solver.DebugDraw.ShowDynamics"), ChaosSolverDrawShapesShowDynamic, TEXT("If DebugDrawShapes is enabled, whether to show dynamic objects"));
		FAutoConsoleVariableRef CVarChaosSolverDrawJoints(TEXT("p.Chaos.Solver.DebugDrawJoints"), ChaosSolverDrawJoints, TEXT("Draw joints"));
		FAutoConsoleVariableRef CVarChaosSolverDrawCharacterGroundConstraints(TEXT("p.Chaos.Solver.DebugDrawCharacterGroundConstraints"), ChaosSolverDrawCharacterGroundConstraints, TEXT("Draw character ground constraints"));
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
		FAutoConsoleVariableRef CVarChaosSolverDrawMeshBVHOverlaps(TEXT("p.Chaos.Solver.DebugDrawMeshBVHOverlaps"), ChaosSolverDebugDrawMeshBVHOverlaps, TEXT("Draw BVH of objects overlapping meshes"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawColorShapeByClientServer(TEXT("p.Chaos.Solver.DebugDraw.ColorShapeByClientServer"), ChaosSolverDebugDrawColorShapeByClientServer, TEXT("Color shape according to client and server: red = server / blue = client "));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawShowServer(TEXT("p.Chaos.Solver.DebugDraw.ShowServer"), ChaosSolverDebugDrawShowServer, TEXT("Draw server related debug data"));
		FAutoConsoleVariableRef CVarChaosSolverDebugDrawShowClient(TEXT("p.Chaos.Solver.DebugDraw.ShowClient"), ChaosSolverDebugDrawShowClient, TEXT("Draw client related debug data"));

		int32 ChaosSolverDebugDrawPreIntegrationShapes = 0;
		int32 ChaosSolverDebugDrawPreIntegrationCollisions = 0;
		FAutoConsoleVariableRef CVarChaosSolverDrawPreIntegrationShapes(TEXT("p.Chaos.Solver.DebugDrawPreIntegrationShapes"), ChaosSolverDebugDrawPreIntegrationShapes, TEXT("Draw Shapes prior to integrate."));
		FAutoConsoleVariableRef CVarChaosSolverDrawPreIntegrationCollisions(TEXT("p.Chaos.Solver.DebugDrawPreIntegrationCollisions"), ChaosSolverDebugDrawPreIntegrationCollisions, TEXT("Draw Collisions prior to integrate."));

		int32 ChaosSolverDebugDrawPostIntegrationShapes = 0;
		int32 ChaosSolverDebugDrawPostIntegrationCollisions = 0;
		FAutoConsoleVariableRef CVarChaosSolverDrawPostIntegrationShapes(TEXT("p.Chaos.Solver.DebugDrawPostIntegrationShapes"), ChaosSolverDebugDrawPostIntegrationShapes, TEXT("Draw Shapes prior to constraint solve phase."));
		FAutoConsoleVariableRef CVarChaosSolverDrawPostIntegrationCollisions(TEXT("p.Chaos.Solver.DebugDrawPostIntegrationCollisions"), ChaosSolverDebugDrawPostIntegrationCollisions, TEXT("Draw Collisions prior to constraint solve phase."));


		DebugDraw::FChaosDebugDrawSettings ChaosSolverDebugDebugDrawSettings(
			/* ArrowSize =					*/ 10.0f,
			/* BodyAxisLen =				*/ 30.0f,
			/* ContactLen =					*/ 30.0f,
			/* ContactWidth =				*/ 6.0f,
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
				/* InDebrisColor =	  */ FColor(255, 0, 0),
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
				/* InDebrisColor =	  */ FColor(0, 0, 255),
			};
			return SolverShapesColorsByState_Client;
		}

		FAutoConsoleVariableRef CVarChaosSolverArrowSize(TEXT("p.Chaos.Solver.DebugDraw.ArrowSize"), ChaosSolverDebugDebugDrawSettings.ArrowSize, TEXT("ArrowSize."));
		FAutoConsoleVariableRef CVarChaosSolverBodyAxisLen(TEXT("p.Chaos.Solver.DebugDraw.BodyAxisLen"), ChaosSolverDebugDebugDrawSettings.BodyAxisLen, TEXT("BodyAxisLen."));
		FAutoConsoleVariableRef CVarChaosSolverContactLen(TEXT("p.Chaos.Solver.DebugDraw.ContactLen"), ChaosSolverDebugDebugDrawSettings.ContactLen, TEXT("ContactLen."));
		FAutoConsoleVariableRef CVarChaosSolverContactWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactWidth"), ChaosSolverDebugDebugDrawSettings.ContactWidth, TEXT("ContactWidth."));
		FAutoConsoleVariableRef CVarChaosSolverContactInfoWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactInfoWidth"), ChaosSolverDebugDebugDrawSettings.ContactInfoWidth, TEXT("ContactInfoWidth."));
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
		FAutoConsoleVariableRef CVarChaosSolverDrawPriority(TEXT("p.Chaos.Solver.DebugDraw.DrawPriority"), ChaosSolverDebugDebugDrawSettings.DrawPriority, TEXT("Draw Priority for debug draw shapes (0 draw at actual Z, +ve is closer to the screen)."));
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

		// Shrink particle arrays every frame to recove rmemory when a scene changes significantly
		bool bChaosSolverShrinkArrays = false;
		float ChaosArrayCollectionMaxSlackFraction = 0.5f;
		int32 ChaosArrayCollectionMinSlack = 100;
		FAutoConsoleVariableRef CVarChaosSolverShrinkArrays(TEXT("p.Chaos.Solver.ShrinkArrays"), bChaosSolverShrinkArrays, TEXT("Enable/Disable particle array shrinking in the main scene"));
		FAutoConsoleVariableRef CVarChaosSolverArrayCollectionMaxSlackFraction(TEXT("p.Chaos.ArrayCollection.MaxSlackFraction"), ChaosArrayCollectionMaxSlackFraction, TEXT("Shrink particle arrays if the number of slack elements exceeds the number of elements by this fraction"));
		FAutoConsoleVariableRef CVarChaosSolverArrayCollectionMinSlack(TEXT("p.Chaos.ArrayCollection.MinSlack"), ChaosArrayCollectionMinSlack, TEXT("Do not reduce the size of particle arrays if it would leave less slack than this"));

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

		int32 ChaosSolverDeterministic = 1;
		FAutoConsoleVariableRef CVarChaosSolverDeterministic(TEXT("p.Chaos.Solver.Deterministic"), ChaosSolverDeterministic, TEXT("Override determinism. 0: disabled; 1: enabled; -1: use config"));

		// Copied from RBAN
		FRealSingle ChaosSolverJointPositionTolerance = 0.025f;
		FAutoConsoleVariableRef CVarChaosSolverJointPositionTolerance(TEXT("p.Chaos.Solver.Joint.PositionTolerance"), ChaosSolverJointPositionTolerance, TEXT("PositionTolerance."));
		FRealSingle ChaosSolverJointAngleTolerance = 0.001f;
		FAutoConsoleVariableRef CVarChaosSolverJointAngleTolerance(TEXT("p.Chaos.Solver.Joint.AngleTolerance"), ChaosSolverJointAngleTolerance, TEXT("AngleTolerance."));
		FRealSingle ChaosSolverJointMinParentMassRatio = 0.2f;
		FAutoConsoleVariableRef CVarChaosSolverJointMinParentMassRatio(TEXT("p.Chaos.Solver.Joint.MinParentMassRatio"), ChaosSolverJointMinParentMassRatio, TEXT("6Dof joint MinParentMassRatio (if > 0)"));
		FRealSingle ChaosSolverJointMaxInertiaRatio = 5.0f;
		FAutoConsoleVariableRef CVarChaosSolverJointMaxInertiaRatio(TEXT("p.Chaos.Solver.Joint.MaxInertiaRatio"), ChaosSolverJointMaxInertiaRatio, TEXT("6Dof joint MaxInertiaRatio (if > 0)"));

		// Collision detection cvars

		// Utility to support runtime changes to some high-level collision configuration that requires we update all existing collisions (actually we just destroy them)
		// This is only intended for testing and debugging and handling the change is not fast.
		bool bChaosCollisionConfigChanged = false;
		FConsoleVariableDelegate OnCollisionConfigCVarChanged = FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar) -> void { bChaosCollisionConfigChanged = true; });

		// These override the engine config if >= 0
		FRealSingle ChaosSolverCullDistance = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverCullDistance(TEXT("p.Chaos.Solver.Collision.CullDistance"), ChaosSolverCullDistance, TEXT("Override cull distance (if >= 0)"), OnCollisionConfigCVarChanged);

		// @todo(chaos): move to physics project settings and set these to -1 when we are settled on values...
		FRealSingle ChaosSolverVelocityBoundsMultiplier = 1.0f;
		FRealSingle ChaosSolverMaxVelocityBoundsExpansion = 3.0f;			// This should probably be a fraction of object size (see FParticlePairMidPhase::GenerateCollisions)
		FRealSingle ChaosSolverVelocityBoundsMultiplierMACD = 1.0f;
		FRealSingle ChaosSolverMaxVelocityBoundsExpansionMACD = 1000.0f;	// For use when Movement-Aware Collision Detection (MACD) is enabled
		FAutoConsoleVariableRef CVarChaosSolverVelocityBoundsMultiplier(TEXT("p.Chaos.Solver.Collision.VelocityBoundsMultiplier"), ChaosSolverVelocityBoundsMultiplier, TEXT("Override velocity bounds multiplier (if >= 0)"), OnCollisionConfigCVarChanged);
		FAutoConsoleVariableRef CVarChaosSolverMaxVelocityBoundsExpansion(TEXT("p.Chaos.Solver.Collision.MaxVelocityBoundsExpansion"), ChaosSolverMaxVelocityBoundsExpansion, TEXT("Override max velocity bounds expansion (if >= 0)"), OnCollisionConfigCVarChanged);
		FAutoConsoleVariableRef CVarChaosSolverVelocityBoundsMultiplierMACD(TEXT("p.Chaos.Solver.Collision.VelocityBoundsMultiplierMACD"), ChaosSolverVelocityBoundsMultiplierMACD, TEXT("Override velocity bounds multiplier for MACD (if >= 0)"), OnCollisionConfigCVarChanged);
		FAutoConsoleVariableRef CVarChaosSolverMaxVelocityBoundsExpansionMACD(TEXT("p.Chaos.Solver.Collision.MaxVelocityBoundsExpansionMACD"), ChaosSolverMaxVelocityBoundsExpansionMACD, TEXT("Override max velocity bounds expansion for MACD (if >= 0)"), OnCollisionConfigCVarChanged);

		FRealSingle ChaosSolverMaxPushOutVelocity = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverMaxPushOutVelocity(TEXT("p.Chaos.Solver.Collision.MaxPushOutVelocity"), ChaosSolverMaxPushOutVelocity, TEXT("Override max pushout velocity (if >= 0)"), OnCollisionConfigCVarChanged);

		FRealSingle ChaosSolverDepenetrationVelocity = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverInitialOverlapDepentrationVelocity(TEXT("p.Chaos.Solver.Collision.DepenetrationVelocity"), ChaosSolverDepenetrationVelocity, TEXT("Override initial overlap depenetration velocity (if >= 0)"), OnCollisionConfigCVarChanged);

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

		// Enable/Disable CCD. Set to false to disable the system, regardless of particle settings
		bool bChaosUseCCD = true;
		FAutoConsoleVariableRef  CVarChaosUseCCD(TEXT("p.Chaos.Solver.UseCCD"), bChaosUseCCD, TEXT("Global flag to turn CCD on or off. Default is true (on)"), OnCollisionConfigCVarChanged);

		// Enable/Disable MACD (Motion-Aware Collision Detection). Set to false to disable the system, regardless of particle settings
		bool bChaosUseMACD = true;
		FAutoConsoleVariableRef CVarChaos_Collision_UseMACD(TEXT("p.Chaos.Solver.UseMACD"), bChaosUseMACD, TEXT("Global flag to turn Movement-Aware Collision Detection (MACD) on or off. Default is true (on)"), OnCollisionConfigCVarChanged);

		// Use to force all collisions to use MACD for testing (must also have bChaosUseMACD enabled)
		bool bChaosForceMACD = false;
		FAutoConsoleVariableRef CVarChaos_Collision_ForceMACD(TEXT("p.Chaos.Solver.bChaosForceMACD"), bChaosForceMACD, TEXT("Force all collisions to use MACD for testing"), OnCollisionConfigCVarChanged);

		// Joint cvars
		float ChaosSolverJointMinSolverStiffness = 1.0f;
		float ChaosSolverJointMaxSolverStiffness = 1.0f;
		int32 ChaosSolverJointNumIterationsAtMaxSolverStiffness = 1;
		bool bChaosSolverJointSolvePositionLast = true;
		bool bChaosSolverJointUsePositionBasedDrives = true;
		int32 ChaosSolverJointNumShockProagationIterations = 0;
		FRealSingle ChaosSolverJointShockPropagation = -1.0f;
		FAutoConsoleVariableRef CVarChaosSolverJointMinSolverStiffness(TEXT("p.Chaos.Solver.Joint.MinSolverStiffness"), ChaosSolverJointMinSolverStiffness, TEXT("Solver stiffness on first iteration, increases each iteration toward MaxSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointMaxSolverStiffness(TEXT("p.Chaos.Solver.Joint.MaxSolverStiffness"), ChaosSolverJointMaxSolverStiffness, TEXT("Solver stiffness on last iteration, increases each iteration from MinSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointNumIterationsAtMaxSolverStiffness(TEXT("p.Chaos.Solver.Joint.NumIterationsAtMaxSolverStiffness"), ChaosSolverJointNumIterationsAtMaxSolverStiffness, TEXT("How many iterations we want at MaxSolverStiffness."));
		FAutoConsoleVariableRef CVarChaosSolverJointSolvePositionFirst(TEXT("p.Chaos.Solver.Joint.SolvePositionLast"), bChaosSolverJointSolvePositionLast, TEXT("Should we solve joints in position-then-rotation order (false) or rotation-then-position order (true, default)"));
		FAutoConsoleVariableRef CVarChaosSolverJointUsePBDVelocityDrives(TEXT("p.Chaos.Solver.Joint.UsePBDDrives"), bChaosSolverJointUsePositionBasedDrives, TEXT("Whether to solve drives in the position or velocity phase of the solver (default true"));
		FAutoConsoleVariableRef CVarChaosSolverJointNumShockPropagationIterations(TEXT("p.Chaos.Solver.Joint.NumShockPropagationIterations"), ChaosSolverJointNumShockProagationIterations, TEXT("How many iterations to enable SHockProagation for."));
		FAutoConsoleVariableRef CVarChaosSolverJointShockPropagation(TEXT("p.Chaos.Solver.Joint.ShockPropagation"), ChaosSolverJointShockPropagation, TEXT("6Dof joint shock propagation override (if >= 0)."));

		int32 ChaosVisualDebuggerEnable = 1;
		FAutoConsoleVariableRef CVarChaosVisualDebuggerEnable(TEXT("p.Chaos.VisualDebuggerEnable"), ChaosVisualDebuggerEnable, TEXT("Enable/Disable pushing/saving data to the visual debugger"));


		// Enable a couple bug fixes with temporary roll-back just in case
		bool bRemoveParticleFromMovingKinematicsOnDisable = true;
		FAutoConsoleVariableRef CVarChaosRemoveParticleFromMovingKinematicsOnDisable(TEXT("p.Chaos.RemoveParticleFromMovingKinematicsOnDisable"), bRemoveParticleFromMovingKinematicsOnDisable, TEXT(""));
	}
}

namespace PhysicsReplicationCVars
{
	namespace ResimulationCVars
	{
		bool bApplyTargetsWhileResimulating = false;
		static FAutoConsoleVariableRef CVarResimApplyTargetsWhileResimulating(TEXT("np2.Resim.ApplyTargetsWhileResimulating"), bApplyTargetsWhileResimulating, TEXT("If false, target states from the server are only applied on rewind. If true, target states from the server are applied during resimulation if there are any available."));
	}
}

namespace Chaos
{
	using namespace CVars;

	CHAOS_API extern int32 SyncKinematicOnGameThread;

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

			MSolver->ResetStatCounters();

			if (FRewindData* RewindData = MSolver->GetRewindData())
			{
				RewindData->ApplyInputs(MSolver->GetCurrentFrame(), MSolver->GetEvolution()->IsResetting());
			}

			MSolver->ApplyCallbacks_Internal();
			
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateParams);
				FPBDPositionConstraints PositionTarget; // Dummy for now
				TMap<int32, int32> TargetedParticles;
				{
					MSolver->FieldParameterUpdateCallback(PositionTarget, TargetedParticles);
				}

				for (FGeometryCollectionPhysicsProxy* GeoclObj : MSolver->GetGeometryCollectionPhysicsProxiesField_Internal())
				{
					GeoclObj->FieldParameterUpdateCallback(MSolver);
				}
				MSolver->GetGeometryCollectionPhysicsProxiesField_Internal().Reset();

				MSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().ProcessPendingQueues(*MSolver);
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

				// This needs to be before BeginFrame since there's a possibility that we'll need to invalidate particles when updating cluster unions.
				// Invalidating particles will cause us to iterate through and remove active collisions. If this is after BeginFrame but before we run
				// the next broadphase, we'll pass the epoch check but have an invalid index into a now-empty active constraints array.
				MSolver->GetEvolution()->GetRigidClustering().UnionClusterGroups();

				// Process any sleep/wake requests that came from the game thread
				// NOTE: Must be before GetCollisionConstraints().BeginFrame() (because its behaviour depends on sleep state)
				MSolver->GetEvolution()->GetIslandManager().UpdateExplicitSleep();

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

					for (FGeometryCollectionPhysicsProxy* GeoCollectionObj : MSolver->GetGeometryCollectionPhysicsProxiesField_Internal())
					{
						GeoCollectionObj->FieldForcesUpdateCallback(MSolver);
					}
					MSolver->GetGeometryCollectionPhysicsProxiesField_Internal().Reset();

					if(FRewindData* RewindData = MSolver->GetRewindData())
					{
						//todo: make this work with sub-stepping
						MSolver->GetEvolution()->SetCurrentStepResimCache(bFirstStep ? RewindData->GetCurrentStepResimCache() : nullptr);
					}

					MSolver->GetEvolution()->AdvanceOneTimeStep(DeltaTime, MSubStepInfo);
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
					// The Game Thread is now in charge to reset the producer buffer
					constexpr bool bResetProducerData = false;
					MSolver->GetEventManager()->FillProducerData(MSolver, bResetProducerData);
					MSolver->GetEvolution()->ResetAllRemovals();
				}
			}
			
			{
				SCOPE_CYCLE_COUNTER(STAT_EndFrame);
				MSolver->GetEvolution()->EndFrame(MDeltaTime);
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_FinalizeCallbacks);
				MSolver->FinalizeCallbackData_Internal();
			}

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

			{
				SCOPE_CYCLE_COUNTER(STAT_ResetClusteringEvents);

				// reset all clustering events needs to be after CompleteSceneSimulation to make sure the cache recording gets them before they get removed
				// they cannot be right after the presolve callback ( as they were before ) because they will cause geometry collection replicated clients to miss them
				// Todo(chaos) we should probably move all of the solver event reset here in the future
				MSolver->GetEvolution()->GetRigidClustering().ResetAllEvents();
			}

			// Recover unused memory from particle arrays, based on the array shrink policy
			if (bChaosSolverShrinkArrays)
			{
				QUICK_SCOPE_CYCLE_COUNTER(ShrinkParticleArrays);
				MSolver->GetParticles().ShrinkArrays(CVars::ChaosArrayCollectionMaxSlackFraction, CVars::ChaosArrayCollectionMinSlack);
			}

			if (FRewindData* RewindData = MSolver->GetRewindData())
			{
				SCOPE_CYCLE_COUNTER(STAT_RewindFinishFrame);
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
		, MEvolution(new FPBDRigidsEvolution(Particles, SimMaterials, &MidPhaseModifiers, &CCDModifiers, &StrainModifiers, &ContactModifiers, BufferingModeIn == EMultiBufferMode::Single))
		, MEventManager(new FEventManager(BufferingModeIn))
		, MSolverEventFilters(new FSolverEventFilters())
		, MDirtyParticlesBuffer(new FDirtyParticlesBuffer(BufferingModeIn, BufferingModeIn == EMultiBufferMode::Single))
		, MCurrentLock(new FCriticalSection())

		, PerSolverField(nullptr)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::PBDRigidsSolver()"));
		Reset();

		MEvolution->SetInternalParticleInitilizationFunction(
			[this](const FGeometryParticleHandle* OldParticle, FGeometryParticleHandle* NewParticle) 
			{
				IPhysicsProxyBase* Proxy = const_cast<IPhysicsProxyBase*>(OldParticle->PhysicsProxy());
				if (FPBDRigidClusteredParticleHandle* NewClusteredParticle = NewParticle->CastToClustered())
				{
					NewClusteredParticle->AddPhysicsProxy(Proxy);
				}
				NewParticle->SetPhysicsProxy(Proxy);
			});

		MEvolution->SetPreIntegrateCallback(
			[this](FReal Dt)
			{
				for (ISimCallbackObject* Callback : SimCallbackObjects)
				{
					if (Callback->HasOption(ESimCallbackOptions::PreIntegrate))
					{
						FScopedTraceSolverCallback TraceCallback(Callback);
						Callback->PreIntegrate_Internal();
					}
				}

				PreIntegrateDebugDraw(Dt);
			});

		MEvolution->SetPostIntegrateCallback(
			[this](FReal Dt)
			{
				for (ISimCallbackObject* Callback : SimCallbackObjects)
				{
					if (Callback->HasOption(ESimCallbackOptions::PostIntegrate))
					{
						FScopedTraceSolverCallback TraceCallback(Callback);
						Callback->PostIntegrate_Internal();
					}
				}
			});

		MEvolution->SetPreSolveCallback(
			[this](FReal Dt)
			{
				for (ISimCallbackObject* Callback : SimCallbackObjects)
				{
					if (Callback->HasOption(ESimCallbackOptions::PreSolve))
					{
						FScopedTraceSolverCallback TraceCallback(Callback);
						Callback->PreSolve_Internal();
					}
				}

				PreSolveDebugDraw(Dt);
			});

		MEvolution->SetPostSolveCallback(
			[this](FReal Dt)
			{
				for (ISimCallbackObject* Callback : SimCallbackObjects)
				{
					if (Callback->HasOption(ESimCallbackOptions::PostSolve))
					{
						FScopedTraceSolverCallback TraceCallback(Callback);
						Callback->PostSolve_Internal();
					}
				}
			});
	}

#if CHAOS_DEBUG_NAME
	void FPBDRigidsSolver::OnDebugNameChanged()
	{
		MEvolution->SetName(GetDebugName().ToString());
	}
#endif

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
		else if (RigidBody_External.GetGeometry() && RigidBody_External.GetGeometry()->HasBoundingBox() && RigidBody_External.GetGeometry()->BoundingBox().Extents().Max() >= MaxBoundsForTree)
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
		//FParticlePropertiesData& RemoteParticleData = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteParticleProperties();
		//FShapeRemoteDataContainer& RemoteShapeContainer = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteShapeContainer();

		Proxy->SetSolver(this);
		Proxy->GetParticle_LowLevel()->SetProxy(Proxy);
		AddDirtyProxy(Proxy);

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), EPendingSpatialDataOperation::Add);
	}

	int32 LogCorruptMap = 0;
	FAutoConsoleVariableRef CVarLogCorruptMap(TEXT("p.LogCorruptMap"), LogCorruptMap, TEXT(""));

	void FPBDRigidsSolver::ApplyCallbacks_Internal()
	{
		Super::ApplyCallbacks_Internal();

		if (MRewindCallback) // Note: Don't use ShouldApplyRewindCallbacks() here since we want this called even if we don't have RewindData enabled.
		{
			if (bGameThreadFrozen)
			{ 
				MRewindCallback->ApplyCallbacks_Internal(GetCurrentFrame(), SimCallbackObjects);
			}
		}
	}

	void FPBDRigidsSolver::UnregisterObject(FSingleParticlePhysicsProxy* Proxy)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("FPBDRigidsSolver::UnregisterObject()"));

		PullResultsManager->RemoveProxy_External(Proxy);

		ClearGTParticle_External(*Proxy->GetParticle_LowLevel());	//todo: remove this

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), EPendingSpatialDataOperation::Delete);

		// remove the proxy from the invalidation list
		RemoveDirtyProxy(Proxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		Proxy->MarkDeleted();

		// Null out the particle's proxy pointer
		Proxy->GetParticle_LowLevel()->SetProxy(nullptr);	//todo: use TUniquePtr for better ownership

		// Remove the proxy from the GT proxy map
		FUniqueIdx UniqueIdx = Proxy->GetGameThreadAPI().UniqueIdx();

		FIgnoreCollisionManager& CollisionManager = GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();
		{
			int32 ExternalTimestamp = GetMarshallingManager().GetExternalTimestamp_External();
			FIgnoreCollisionManager::FDeactivationSet& PendingMap = CollisionManager.GetPendingDeactivationsForGameThread(ExternalTimestamp);
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
					FCollisionDataArray const& CollisionData = EventDataInOut.CollisionData.AllCollisionsArray;
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
			if (MRewindData)
			{
				MRewindData->RemoveObject(Handle);
			}
			
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

		// Need to immediately add to the SQ to make sure that the GC is in the external SQ even after the SQ's flip.
		for (const TUniquePtr<FPBDRigidParticle>& Particle : InProxy->GetUnorderedParticles_External())
		{
			if (Particle && !Particle->Disabled())
			{
				UpdateParticleInAccelerationStructure_External(Particle.Get(), EPendingSpatialDataOperation::Add);
			}
		}

		// There used to be an EnqueueCommandImmediate here to push the initialization of the GC
		// onto the physics thread. We should use AddDirtyProxy here instead to better match up with
		// the initialization order done in the ProcessSinglePushedData_Internal. Using EnqueueCommandImmediate
		// will cause the GC to be initialized after constraints which would preclude it from being
		// used with joint constraints that get spawned on level load with the GC.
		AddDirtyProxy(InProxy);
	}
	
	void FPBDRigidsSolver::UnregisterObject(FGeometryCollectionPhysicsProxy* InProxy)
	{
		check(InProxy);
		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		InProxy->MarkDeleted();

		RemoveDirtyProxy(InProxy);
		InProxy->OnUnregisteredFromSolver();

		// Particles are removed from acceleration structure in FPhysScene_Chaos::RemoveObject.


		EnqueueCommandImmediate([InProxy, this]()
		{
			GeometryCollectionPhysicsProxies_Internal.RemoveSingle(InProxy);
			InProxy->SyncBeforeDestroy();
			PendingDestroyGeometryCollectionPhysicsProxy.Add(InProxy);
		});
	}

	void FPBDRigidsSolver::RegisterObject(FClusterUnionPhysicsProxy* Proxy)
	{
		if (!Proxy)
		{
			return;
		}

		if (FPBDRigidParticle* Particle = Proxy->GetParticle_External())
		{
			if (AccelerationStructureSplitStaticAndDynamic == 1)
			{
				// It needs to be {0, 1}. Things will crash otherwise. I don't know why.
				// Probably similarly to the geometry collection physics proxy.
				Particle->SetSpatialIdx(FSpatialAccelerationIdx{ 0, 1 });
			}
			else
			{
				Particle->SetSpatialIdx(FSpatialAccelerationIdx{ 0, 0 });
			}

			Particle->SetUniqueIdx(GetEvolution()->GenerateUniqueIdx());
			UpdateParticleInAccelerationStructure_External(Particle, EPendingSpatialDataOperation::Add);
		}
		Proxy->SetSolver(this);
		AddDirtyProxy(Proxy);
	}

	void FPBDRigidsSolver::UnregisterObject(FClusterUnionPhysicsProxy* Proxy)
	{
		if (!Proxy)
		{
			return;
		}

		Proxy->MarkDeleted();
		RemoveDirtyProxy(Proxy);

		if (FPBDRigidParticle* GTParticle = Proxy->GetParticle_External())
		{
			GTParticle->SetProxy(nullptr);
		}

		EnqueueCommandImmediate(
			[Proxy, this]()
			{
				if (Proxy->GetParticle_Internal())
				{
					MEvolution->DisableParticle(Proxy->GetParticle_Internal());
				}
				ClusterUnionPhysicsProxies_Internal.RemoveSingle(Proxy);
				Proxy->SyncBeforeDestroy();
				PendingDestroyClusterUnionProxy.Add(Proxy);
			}
		);
	}

	void FPBDRigidsSolver::RegisterObject(FJointConstraint* GTConstraint)
	{
		LLM_SCOPE(ELLMTag::ChaosConstraint);
		FJointConstraintPhysicsProxy* JointProxy = new FJointConstraintPhysicsProxy(GTConstraint, nullptr);
		JointProxy->SetSolver(this);

		AddDirtyProxy(JointProxy);
	}

	void FPBDRigidsSolver::UnregisterObject(FJointConstraint* GTConstraint)
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

	void FPBDRigidsSolver::RegisterObject(FCharacterGroundConstraint* GTConstraint)
	{
		LLM_SCOPE(ELLMTag::ChaosConstraint);
		FCharacterGroundConstraintProxy* ConstraintProxy = new FCharacterGroundConstraintProxy(GTConstraint);
		ConstraintProxy->SetSolver(this);

		AddDirtyProxy(ConstraintProxy);
	}

	void FPBDRigidsSolver::UnregisterObject(FCharacterGroundConstraint* GTConstraint)
	{
		FCharacterGroundConstraintProxy* ConstraintProxy = GTConstraint->GetProxy<FCharacterGroundConstraintProxy>();
		check(ConstraintProxy);

		RemoveDirtyProxy(ConstraintProxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		GTConstraint->GetProxy()->MarkDeleted();
		GTConstraint->SetProxy(static_cast<FCharacterGroundConstraintProxy*>(nullptr));

		ConstraintProxy->DestroyOnGameThread();	//destroy the game thread portion of the proxy

		// Finish de-registration on the physics thread...
		EnqueueCommandImmediate([ConstraintProxy, this]()
			{
				// TODO: Add character ground constraint to rewind data
				//if (FRewindData* RewindData = GetRewindData())
				//{
				//	RewindData->RemoveObject(ConstraintProxy->GetPhysicsThreadAPI());
				//}

				ConstraintProxy->DestroyOnPhysicsThread(this);
				CharacterGroundConstraintProxies_Internal.RemoveSingle(ConstraintProxy);
				delete ConstraintProxy;
			});
	}

	void FPBDRigidsSolver::RegisterObject(FSuspensionConstraint* GTConstraint)
	{
		LLM_SCOPE(ELLMTag::ChaosConstraint);
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = new FSuspensionConstraintPhysicsProxy(GTConstraint, nullptr);
		SuspensionProxy->SetSolver(this);

		AddDirtyProxy(SuspensionProxy);
	}

	void FPBDRigidsSolver::UnregisterObject(FSuspensionConstraint* GTConstraint)
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

	void FPBDRigidsSolver::SetSuspensionTarget(FSuspensionConstraint* GTConstraint, const FVector& TargetPos, const FVector& Normal, bool Enabled)
	{
		EnsureIsInPhysicsThreadContext();
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = GTConstraint->GetProxy<FSuspensionConstraintPhysicsProxy>();
		check(SuspensionProxy);
		SuspensionProxy->UpdateTargetOnPhysicsThread(this, TargetPos, Normal, Enabled);
	}

	void FPBDRigidsSolver::EnableRewindCapture(int32 NumFrames, bool InUseCollisionResimCache, TUniquePtr<IRewindCallback>&& RewindCallback)
	{
		SetRewindCallback(MoveTemp(RewindCallback));
		EnableRewindCapture(NumFrames, InUseCollisionResimCache);
	}

	void FPBDRigidsSolver::EnableRewindCapture(int32 NumFrames, bool InUseCollisionResimCache)
	{
		//TODO: this function calls both internal and external - sort of assumed during initialization. Should decide what thread it's called on and mark it as either external or internal
		if (MRewindData.IsValid())
		{
			MRewindData->Init(((FPBDRigidsSolver*)this), NumFrames, InUseCollisionResimCache, ((FPBDRigidsSolver*)this)->GetCurrentFrame());
		}
		else
		{
			MRewindData = MakeUnique<FRewindData>(((FPBDRigidsSolver*)this), NumFrames, InUseCollisionResimCache, ((FPBDRigidsSolver*)this)->GetCurrentFrame()); // FIXME
		}
		bUseCollisionResimCache = InUseCollisionResimCache;
		const int32 NumFramesSet = GetRewindData() != nullptr ? GetRewindData()->Capacity() : NumFrames;
		MarshallingManager.SetHistoryLength_Internal(NumFramesSet);
		MEvolution->SetRewindData(GetRewindData());
		
		if (MRewindCallback) 
		{
			MRewindCallback->RewindData = GetRewindData();
		}
		
		UpdateIsDeterministic();

		UE_LOG(LogChaos, Log, TEXT("PBDRigidsSolver::EnableRewindCapture - Starting physics data history caching for rewind / resimulation. History Size: %d"), NumFramesSet);
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
		MEvolution = TUniquePtr<FPBDRigidsEvolution>(new FPBDRigidsEvolution(Particles, SimMaterials, &MidPhaseModifiers, &CCDModifiers, &StrainModifiers, &ContactModifiers, BufferMode == EMultiBufferMode::Single)); 

		PerSolverField = MakeUnique<FPerSolverFieldSystem>();

		//todo: do we need this?
		//MarshallingManager.Reset();

		const int32 PhysicsHistoryLength = FChaosSolversModule::GetModule()->GetSettingsProvider().GetPhysicsHistoryCount();

		if (bUseCollisionResimCache && PhysicsHistoryLength >= 0)
		{
			EnableRewindCapture(PhysicsHistoryLength, true);
		}

		MEvolution->SetCaptureRewindDataFunction([this](const TParticleView<TPBDRigidParticles<FReal,3>>& ActiveParticles)
		{
			FinalizeRewindData(ActiveParticles);
		});

		FEventDefaults::RegisterSystemEvents(*GetEventManager());
	}

	void FPBDRigidsSolver::ChangeBufferMode(EMultiBufferMode InBufferMode)
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
		MarshallingManager.GetCurrentPullData_Internal()->DirtyClusterUnions.Reset();
		for (FClusterUnionPhysicsProxy* Proxy : PendingDestroyClusterUnionProxy)
		{
			if (!Proxy)
			{
				continue;
			}

			// Callback for Unregister PhysicsObject
			if (PhysicsObjectUnregistrationWatchers.Num() > 0)
			{
				for (ISimCallbackObject* Callback : PhysicsObjectUnregistrationWatchers)
				{
					Callback->OnPhysicsObjectUnregistered_Internal(Proxy->GetPhysicsObjectHandle());
				}
			}

			if (FPBDRigidsEvolutionGBF* Evolution = GetEvolution())
			{
				// Remove the cluster union this proxy manages which will in turn also the destroy the necessary particles.
				Evolution->GetRigidClustering().GetClusterUnionManager().DestroyClusterUnion(Proxy->GetClusterUnionIndex());
			}
			delete Proxy;
		}
		PendingDestroyClusterUnionProxy.Reset();

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
				// Callback for Unregister PhysicsObject
				if (PhysicsObjectUnregistrationWatchers.Num() > 0)
				{
					for (ISimCallbackObject* Callback : PhysicsObjectUnregistrationWatchers)
					{
						Callback->OnPhysicsObjectUnregistered_Internal(Info.Proxy->GetPhysicsObject());
					}
				}

				// finally let's release the unique index
				GetEvolution()->ReleaseUniqueIdx(Info.UniqueIdx);

				if (Info.Handle)
				{
					// Use the handle to destroy the particle data
					GetEvolution()->DestroyParticle(Info.Handle);
				}

				ensure(Info.Proxy->GetHandle_LowLevel() == nullptr);	//should have already cleared this out
				delete Info.Proxy;
				PendingDestroyPhysicsProxy.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		MarshallingManager.GetCurrentPullData_Internal()->DirtyGeometryCollections.Reset();

		if (!PendingDestroyGeometryCollectionPhysicsProxy.IsEmpty())
		{
			GetEvolution()->GetRigidClustering().CleanupInternalClustersForProxies(TArrayView<IPhysicsProxyBase*>{reinterpret_cast<IPhysicsProxyBase**>(&PendingDestroyGeometryCollectionPhysicsProxy[0]), PendingDestroyGeometryCollectionPhysicsProxy.Num() });
			for (auto Proxy : PendingDestroyGeometryCollectionPhysicsProxy)
			{
				// Callback for Unregister PhysicsObject
				if (PhysicsObjectUnregistrationWatchers.Num() > 0)
				{
					TArray<FPhysicsObjectHandle> PhysicsObjects = Proxy->GetAllPhysicsObjects();
					for (FConstPhysicsObjectHandle PhysicsObject : PhysicsObjects)
					{
						for (ISimCallbackObject* Callback : PhysicsObjectUnregistrationWatchers)
						{
							Callback->OnPhysicsObjectUnregistered_Internal(PhysicsObject);
						}
					}
				}

				// Removing the geometry collection from the solver a bit delayed. This lets the cluster union do its cleanup first before
				// the geometry collection if they're all being destroyed at the same time.
				Proxy->OnRemoveFromSolver(this);
				delete Proxy;
			}
			PendingDestroyGeometryCollectionPhysicsProxy.Reset();
		}
	}

	void FPBDRigidsSolver::SetVelocityBoundsExpansion(const FReal BoundsVelocityMultiplier, const FReal MaxBoundsVelocityExpansion)
	{ 
		GetEvolution()->GetCollisionConstraints().SetVelocityBoundsExpansion(BoundsVelocityMultiplier, MaxBoundsVelocityExpansion);
	}

	void FPBDRigidsSolver::SetVelocityBoundsExpansionMACD(const FReal BoundsVelocityMultiplier, const FReal MaxBoundsVelocityExpansion)
	{
		GetEvolution()->GetCollisionConstraints().SetVelocityBoundsExpansionMACD(BoundsVelocityMultiplier, MaxBoundsVelocityExpansion);
	}

	void FPBDRigidsSolver::PrepareAdvanceBy(const FReal DeltaTime)
	{
		MEvolution->GetCollisionConstraints().SetCollisionsEnabled(bChaosSolverCollisionEnabled);

		FCollisionDetectorSettings CollisionDetectorSettings = MEvolution->GetCollisionConstraints().GetDetectorSettings();
		CollisionDetectorSettings.bAllowManifoldReuse = (ChaosSolverCollisionAllowManifoldUpdate != 0);
		CollisionDetectorSettings.bDeferNarrowPhase = (ChaosSolverCollisionDeferNarrowPhase != 0);
		CollisionDetectorSettings.bAllowManifolds = (ChaosSolverCollisionUseManifolds != 0);
		CollisionDetectorSettings.bAllowCCD = bChaosUseCCD;
		CollisionDetectorSettings.bAllowMACD = bChaosUseMACD;
		MEvolution->GetCollisionConstraints().SetDetectorSettings(CollisionDetectorSettings);
		
		FPBDJointSolverSettings JointsSettings = MEvolution->GetJointConstraints().GetSettings();
		JointsSettings.MinSolverStiffness = ChaosSolverJointMinSolverStiffness;
		JointsSettings.MaxSolverStiffness = ChaosSolverJointMaxSolverStiffness;
		JointsSettings.NumIterationsAtMaxSolverStiffness = ChaosSolverJointNumIterationsAtMaxSolverStiffness;
		JointsSettings.PositionTolerance = ChaosSolverJointPositionTolerance;
		JointsSettings.AngleTolerance = ChaosSolverJointAngleTolerance;
		JointsSettings.MinParentMassRatio = ChaosSolverJointMinParentMassRatio;
		JointsSettings.MaxInertiaRatio = ChaosSolverJointMaxInertiaRatio;
		JointsSettings.bSolvePositionLast = bChaosSolverJointSolvePositionLast;
		JointsSettings.bUsePositionBasedDrives = bChaosSolverJointUsePositionBasedDrives;
		JointsSettings.NumShockPropagationIterations = ChaosSolverJointNumShockProagationIterations;
		JointsSettings.ShockPropagationOverride = ChaosSolverJointShockPropagation;
		JointsSettings.bUseLinearSolver = bChaosSolverJointUseLinearSolver;
		JointsSettings.bSortEnabled = false;
		MEvolution->GetJointConstraints().SetSettings(JointsSettings);

		// Apply CVAR overrides if set
		{
			// To enable runtime support for switching collision features on/off we need to update existing constraints when config changes.
			if (bChaosCollisionConfigChanged)
			{
				// For now destroy the collisions. This is a bit over the top and causes problems for sleeping islands, but it's only for debugging/testing.
				GetEvolution()->DestroyTransientConstraints();
				bChaosCollisionConfigChanged = false;
			}

			if (ChaosSolverCollisionPositionFrictionIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetPositionFrictionIterations(ChaosSolverCollisionPositionFrictionIterations);
			}
			if (ChaosSolverCollisionVelocityFrictionIterations >= 0)
			{
				MEvolution->GetCollisionConstraints().SetVelocityFrictionIterations(ChaosSolverCollisionVelocityFrictionIterations);
			}
			{
				MEvolution->SetShockPropagationIterations(ChaosSolverCollisionPositionShockPropagationIterations, ChaosSolverCollisionVelocityShockPropagationIterations);
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
			if ((ChaosSolverVelocityBoundsMultiplier >= 0.0f) && (ChaosSolverMaxVelocityBoundsExpansion >= 0.0f))
			{
				SetVelocityBoundsExpansion(ChaosSolverVelocityBoundsMultiplier, ChaosSolverMaxVelocityBoundsExpansion);
			}
			if ((ChaosSolverVelocityBoundsMultiplierMACD >= 0.0f) && (ChaosSolverMaxVelocityBoundsExpansionMACD >= 0.0f))
			{
				SetVelocityBoundsExpansionMACD(ChaosSolverVelocityBoundsMultiplierMACD, ChaosSolverMaxVelocityBoundsExpansionMACD);
			}
			if (ChaosSolverMaxPushOutVelocity >= 0.0f)
			{
				SetCollisionMaxPushOutVelocity(ChaosSolverMaxPushOutVelocity);
			}
			if (ChaosSolverDepenetrationVelocity >= 0.0f)
			{
				SetCollisionDepenetrationVelocity(ChaosSolverDepenetrationVelocity);
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(IsNetworkPhysicsPredictionEnabled() && CanDebugNetworkPhysicsPrediction())
		{
			UE_LOG(LogChaos, Log, TEXT("-> Simulating Frame = %d"), CurrentFrame);
		}
#endif

		AdvanceOneTimeStepTask(this, MLastDt, SubStepInfo).DoWork();

		if (MLastDt > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_FinalizePullData);
			//pass information back to external thread
			//we skip dt=0 case because sync data should be identical if dt = 0
			MarshallingManager.FinalizePullData_Internal(MEvolution->LatestExternalTimestampConsumed_Internal, StartSimTime, MLastDt);
		}

		if(SubStepInfo.Step == SubStepInfo.NumSteps - 1)
		{
			SCOPE_CYCLE_COUNTER(STAT_DestroyPendingProxies);
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

				Particle->PrepareBVH();
				Particle->SyncRemoteData(*Manager, DataIdx, Dirty.PropertyData, Dirty.ShapeDataIndices, ShapeDirtyData);
				Proxy->ClearAccumulatedData();
				Proxy->ResetDirtyIdx();
				break;
			}
			case EPhysicsProxyType::GeometryCollectionType:
			{
				auto Proxy = static_cast<FGeometryCollectionPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(this);
				Proxy->ResetDirtyIdx();
				break;
			}
			case EPhysicsProxyType::ClusterUnionProxy:
			{
				FClusterUnionPhysicsProxy* Proxy = static_cast<FClusterUnionPhysicsProxy*>(Dirty.Proxy);
				FClusterUnionPhysicsProxy::FExternalParticle* Particle = Proxy->GetParticle_External();
				Proxy->SyncRemoteData(*Manager, DataIdx, Dirty.PropertyData);
				Proxy->ClearAccumulatedData();
				Proxy->ResetDirtyIdx();
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
			case EPhysicsProxyType::CharacterGroundConstraintType:
			{
				auto Proxy = static_cast<FCharacterGroundConstraintProxy*>(Dirty.Proxy);
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
			int32 NumCharacterGroundConstraints = 0;
			int32 NumParticles = 0;
			UE_LOG(LogChaos, Warning, TEXT("LogDirtyParticles:"));
			DirtyProxiesData->ForEachProxy([&NumJoints, &NumSuspension, &NumCharacterGroundConstraints, &NumParticles](int32 DataIdx, FDirtyProxy& Dirty)
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
					case EPhysicsProxyType::CharacterGroundConstraintType:
					{
						++NumCharacterGroundConstraints;
						break;
					}

					default: break;
				}
			});

			UE_LOG(LogChaos, Warning, TEXT("Num Particles:%d Num Shapes:%d Num Joints:%d Num Suspensions:%d Num CharGround:%d"), NumParticles, DirtyProxiesData->NumDirtyShapes(), NumJoints, NumSuspension, NumCharacterGroundConstraints);
		}

		GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().PushProducerStorageData_External(MarshallingManager.GetExternalTimestamp_External());

		if (ShouldApplyRewindCallbacks() && !IsShuttingDown())
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
				Proxy->PushToPhysicsState(*Manager, DataIdx, Dirty, ShapeDirtyData, ExternalDt);
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
		int32 NumInitializedGCProxies = 0;
		DirtyProxiesData->ForEachProxy([this, &ProcessProxyPT, Manager, &NumInitializedGCProxies](int32 DataIdx,FDirtyProxy& Dirty)
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
						if (!Proxy->IsInitializedOnPhysicsThread())
						{
							// Finish registration on the physics thread...
							Proxy->InitializeBodiesPT(this, GetParticles());
							++NumInitializedGCProxies;

							GeometryCollectionPhysicsProxies_Internal.Add(Proxy);
						}
						Proxy->PushToPhysicsState();
						break;
					}
					case EPhysicsProxyType::ClusterUnionProxy:
					{
						FClusterUnionPhysicsProxy* Proxy = static_cast<FClusterUnionPhysicsProxy*>(Dirty.Proxy);
						if (!Proxy->IsInitializedOnPhysicsThread())
						{
							Proxy->Initialize_Internal(this, GetParticles());
							ClusterUnionPhysicsProxies_Internal.Add(Proxy);
						}
						Proxy->PushToPhysicsState(*Manager, DataIdx, Dirty);
						break;
					}
					case EPhysicsProxyType::JointConstraintType:
					case EPhysicsProxyType::SuspensionConstraintType:
					case EPhysicsProxyType::CharacterGroundConstraintType:
					{
						// Pass until after all bodies are created. 
						break;
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
						break;
					}

					case EPhysicsProxyType::CharacterGroundConstraintType:
					{
						auto ConstraintProxy = static_cast<FCharacterGroundConstraintProxy*>(Dirty.Proxy);
						const bool bIsNew = !ConstraintProxy->IsInitialized();
						if (bIsNew)
						{
							CharacterGroundConstraintProxies_Internal.Add(ConstraintProxy);
							ConstraintProxy->InitializeOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
							ConstraintProxy->SetInitialized(GetCurrentFrame());
						}

						//TODO: Support rewind for character ground constraints
						//if (RewindData)
						//{
						//	RewindData->PushGTDirtyData(*Manager, DataIdx, Dirty, nullptr);
						//}

						ConstraintProxy->PushStateOnPhysicsThread(this, *Manager, DataIdx, Dirty.PropertyData);
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

	template<typename TRigidParticle>
	bool ShouldUpdateFromSimulation(const TRigidParticle& InRigidParticle)
	{
		if (InRigidParticle.ObjectState() == Chaos::EObjectStateType::Kinematic)
		{
			switch (Chaos::SyncKinematicOnGameThread)
			{
			case 0:
				return false;
			case 1:
				return true;
			default:
				return InRigidParticle.UpdateKinematicFromSimulation();
			}
		}
		// We assume that sleeping/static particles etc won't appear repeatedly (over multiple
		// frames) in the dirty list, so we can safely return true here without incurring unwanted costs.
		return true;
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

			if (SimCallbackObject->HasOption(ESimCallbackOptions::MidPhaseModification))
			{
				MidPhaseModifiers.Add(SimCallbackObject);
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::CCDModification))
			{
				CCDModifiers.Add(SimCallbackObject);
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::StrainModification))
			{
				StrainModifiers.Add(SimCallbackObject);
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

			if (SimCallbackObject->HasOption(ESimCallbackOptions::Rewind))
			{
				if (MRewindCallback)
				{
					MRewindCallback->RegisterRewindableSimCallback_Internal(SimCallbackObject);
				}
			}

			if (SimCallbackObject->HasOption(ESimCallbackOptions::PhysicsObjectUnregister))
			{
				PhysicsObjectUnregistrationWatchers.Add(SimCallbackObject);
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

			if (RemovedCallbackObject->HasOption(ESimCallbackOptions::Rewind))
			{
				if (MRewindCallback)
				{
					MRewindCallback->UnregisterRewindableSimCallback_Internal(RemovedCallbackObject);
				}
			}
		}

		for (int32 Idx = SimCallbackObjects.Num() - 1; Idx >= 0; --Idx)
		{
			if (SimCallbackObjects[Idx]->bPendingDelete)
			{
				SimCallbackObjects.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = MidPhaseModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			if (MidPhaseModifiers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				MidPhaseModifiers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = CCDModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			if (CCDModifiers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				CCDModifiers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = StrainModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			if (StrainModifiers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				StrainModifiers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = ContactModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			if (ContactModifiers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				ContactModifiers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = RegistrationWatchers.Num() - 1; Idx >= 0; --Idx)
		{
			if (RegistrationWatchers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				RegistrationWatchers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = UnregistrationWatchers.Num() - 1; Idx >= 0; --Idx)
		{
			if (UnregistrationWatchers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				UnregistrationWatchers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
		}

		for (int32 Idx = PhysicsObjectUnregistrationWatchers.Num() - 1; Idx >= 0; --Idx)
		{
			if (PhysicsObjectUnregistrationWatchers[Idx]->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				PhysicsObjectUnregistrationWatchers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
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
			SimCallbackObject->SetSimAndDeltaTime_Internal(GetSolverTime(), MLastDt);
			SimCallbackObject->PreSimulate_Internal();
			delete SimCallbackObject;
		}
		PushData.SimCommands.Reset();
		
		for (ISimCallbackObject* SimCallbackObject : PushData.SimCallbackObjectsToAdd)
		{
			SimCallbackObject->PostInitialize_Internal();
		}

		if (MRewindCallback && MRewindData && !IsShuttingDown())
		{
			MRewindCallback->ProcessInputs_Internal(MRewindData->CurrentFrame(), PushData.SimCallbackInputs);
		}
	}

	void FPBDRigidsSolver::ConditionalApplyRewind_Internal()
	{
		// Note: checking MRewindData->IsResim() can lead to recursion into this function on the last resim frame since the call to AdvanceSolver is what advances RewindData's internal frame
		if(!IsShuttingDown() && ShouldApplyRewindCallbacks() && MRewindData && !GetEvolution()->IsResimming())
		{
			const int32 LastStep = MRewindData->CurrentFrame() - 1;
			const int32 ResimStep = MRewindCallback->TriggerRewindIfNeeded_Internal(LastStep);
			const int32 NumResimSteps = LastStep - ResimStep + 1;

			if (ResimStep < 0)
			{
				// Clear ResimFrame if no valid resim frame was found
				MRewindData->SetResimFrame(INDEX_NONE);
				return;
			}

			const bool bEnableNetworkPredictionDebug = IsNetworkPhysicsPredictionEnabled() && CanDebugNetworkPhysicsPrediction();
			
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bEnableNetworkPredictionDebug)
			{
				UE_LOG(LogChaos, Log, TEXT("	-> Trying to Rewind Frame = %d | At Time = %f | Num Steps = %d | Resim Step = %d | Last Step = %d | Manager Size = %d"), CurrentFrame,  MTime, NumResimSteps, ResimStep, LastStep, MarshallingManager.GetNumHistory_Internal());
				for(auto& Handle : GetParticles().GetNonDisabledDynamicView())
				{
					if(!Handle.IsSleeping())
					{
						UE_LOG(LogChaos, Log, TEXT("Particle Dynamic At Position = %s | Velocity = %s | Quaternion = %s | Omega = %s"),
							*Handle.GetX().ToString(), *Handle.GetV().ToString(), *Handle.GetR().ToString(), *Handle.GetW().ToString());
					}
					else
					{
						UE_LOG(LogChaos, Log, TEXT("Particle Sleeping At Position = %s | Velocity = %s | Quaternion = %s | Omega = %s"), 
							*Handle.GetX().ToString(), *Handle.GetV().ToString(), *Handle.GetR().ToString(), *Handle.GetW().ToString());
					}
				}
			}
#endif

			if ((ResimStep < LastStep) && NumResimSteps <= MarshallingManager.GetNumHistory_Internal())
			{
				FResimDebugInfo DebugInfo;
				QUICK_SCOPE_CYCLE_COUNTER(ChaosRewindAndResim);
				if (MRewindData->RewindToFrame(ResimStep))
				{
#if DEBUG_REWIND_DATA
					UE_LOG(LogTemp, Warning, TEXT("COMMON | PT | ConditionalApplyRewind_Internal | PERFORMING RESIMULATION | Resim From Frame = %d | Num Steps = %d | To Current Frame: %d"), ResimStep, NumResimSteps, CurrentFrame);
#endif

					SetIsResimming(true);
					CurrentFrame = ResimStep;

					TArray<FPushPhysicsData*> RecordedPushData = MarshallingManager.StealHistory_Internal(NumResimSteps);
					bool bFirst = true;

					FDurationTimer ResimTimer(DebugInfo.ResimTime);
					// Do rollback as necessary
					for (int32 Step = ResimStep; Step <= LastStep; ++Step)
					{
						if ((LastStep - Step) < RecordedPushData.Num())
						{
							if (PhysicsReplicationCVars::ResimulationCVars::bApplyTargetsWhileResimulating || bFirst)
							{
								// Update all the particles having received a target from the server
								MRewindData->ApplyTargets(Step, bFirst);
							}

							FPushPhysicsData* PushData = RecordedPushData[LastStep - Step];	//push data is sorted as latest first
							if (bFirst)
							{
								MTime = PushData->StartTime;	//not sure if sub-steps have proper StartTime so just do this once and let solver evolve remaining time
								GetEvolution()->ResetCollisions();
							}
								
							GetEvolution()->SetReset(bFirst);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							if(bEnableNetworkPredictionDebug)
							{
								UE_LOG(LogChaos, Log, TEXT("		-> Re-simulating Frame = %d "), Step);
								for(auto& Handle : GetParticles().GetNonDisabledDynamicView())
								{
									if (GetEvolution()->GetIslandManager().GetParticleIsland(Handle.Handle()))
									{
										if (!Handle.IsSleeping())
										{
											UE_LOG(LogChaos, Log, TEXT("Particle Dynamic At Position = %s | Velocity = %s | Quaternion = %s | Omega = %s | Resim Frame = %d | Sync State = %d | Needs Resim = %d | Unique Idx = %d"),
												*Handle.GetX().ToString(), *Handle.GetV().ToString(), *Handle.GetR().ToString(), *Handle.GetW().ToString(),
												GetEvolution()->GetIslandManager().GetParticleIsland(Handle.Handle())->GetResimFrame(), (uint8)Handle.SyncState(),
												GetEvolution()->GetIslandManager().GetParticleIsland(Handle.Handle())->NeedsResim(), Handle.UniqueIdx().Idx);
										}
										else
										{
											UE_LOG(LogChaos, Log, TEXT("Particle Sleeping At Position = %s | Velocity = %s | Quaternion = %s | Omega = %s | Resim Frame = %d | Sync State = %d | Needs Resim = %d | Unique Idx = %d"),
												*Handle.GetX().ToString(), *Handle.GetV().ToString(), *Handle.GetR().ToString(), *Handle.GetW().ToString(),
												GetEvolution()->GetIslandManager().GetParticleIsland(Handle.Handle())->GetResimFrame(), (uint8)Handle.SyncState(),
												GetEvolution()->GetIslandManager().GetParticleIsland(Handle.Handle())->NeedsResim(), Handle.UniqueIdx().Idx);
										}
									}
								}
							}
#endif
							MRewindCallback->PreResimStep_Internal(Step, bFirst);

							// Run the advance but omit the game thread callbacks as we're executing during the physics tick
							FSolverTasksPTOnly ImmediateTask(*this, PushData);
							//ensure(bSolverHasFrozenGameThreadCallbacks == false);	//We don't support this for resim as it's very expensive and difficult to schedule
							ImmediateTask.AdvanceSolver();
							MRewindCallback->PostResimStep_Internal(Step);
							
							bFirst = false;
						}
					}
					GetEvolution()->GetIslandManager().ResetParticleResimFrame();

					SetIsResimming(false);
					GetEvolution()->SetReset(false);

					ResimTimer.Stop();
					MRewindCallback->SetResimDebugInfo_Internal(DebugInfo);
				}
#if DEBUG_REWIND_DATA
				else
				{
					UE_LOG(LogTemp, Log, TEXT("COMMON | PT | ConditionalApplyRewind_Internal | Resimulation failed, FRewindData::RewindToFrame returned false | Current Frame = %d | Num Steps = %d | Resim Frame = %d | Last Frame = %d | Rewind History Size = %d"), CurrentFrame, NumResimSteps, ResimStep, LastStep, MarshallingManager.GetNumHistory_Internal());
				}
#endif
			}
#if DEBUG_REWIND_DATA
			else
			{
				UE_LOG(LogTemp, Log, TEXT("COMMON | PT | ConditionalApplyRewind_Internal | Resimulation failed, invalid rewind frame data | Current Frame = %d | Num Steps = %d | Resim Frame = %d | Last Frame = %d | Rewind History Size = %d"), CurrentFrame, NumResimSteps, ResimStep, LastStep, MarshallingManager.GetNumHistory_Internal());
			}
#endif
			// Clear the ResimFrame no matter if resimulation succeeded or failed (if it failed it's not going to succeed next frame either based on the same ResimFrame)
			MRewindData->SetResimFrame(INDEX_NONE);
		}
	}

	void FPBDRigidsSolver::SetIsResimming(bool bIsResimming)
	{
		GetEvolution()->SetResim(bIsResimming);

#if WITH_CHAOS_VISUAL_DEBUGGER
		EChaosVDContextAttributes Attributes = static_cast<EChaosVDContextAttributes>(GetChaosVDContextData().Attributes);
		if (bIsResimming)
		{
			EnumAddFlags(Attributes,  EChaosVDContextAttributes::Resimulated);
		}
		else
		{
			EnumRemoveFlags(Attributes,  EChaosVDContextAttributes::Resimulated);
		}

		GetChaosVDContextData().Attributes = static_cast<int32>(Attributes);
#endif	
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

		TArray<FSingleParticlePhysicsProxy*> ActiveRigid;
		ActiveRigid.Reserve(SingleParticlePhysicsProxies_PT.Num());

		TArray<FClusterUnionPhysicsProxy*> ActiveClusterUnions;
		ActiveClusterUnions.Reserve(ClusterUnionPhysicsProxies_Internal.Num());

		const bool bIsResim = GetEvolution()->IsResimming();

		//todo: should be able to go wide just add defaulted etc...
		{
			ensure(PullData->DirtyRigids.Num() == 0);	//we only fill this once per frame
			int32 BufferIdx = 0;

			for (TPBDRigidParticleHandleImp<FReal, 3, false>& DirtyParticle : DirtyParticles)
			{
				if(IPhysicsProxyBase* Proxy = DirtyParticle.Handle()->PhysicsProxy())
				{
					switch(DirtyParticle.GetParticleType())
					{
						case EParticleType::Rigid:
						{
							if(!bIsResim || DirtyParticle.SyncState() == ESyncState::HardDesync)
							{
								if (ShouldUpdateFromSimulation(DirtyParticle))
								{
									ActiveRigid.AddUnique((FSingleParticlePhysicsProxy*)Proxy);
								}
							}
							break;
						}
						case EParticleType::Kinematic:
						case EParticleType::Static:
							ensure(false);
							break;
						case EParticleType::GeometryCollection:
							ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(Proxy));
							break;
						case EParticleType::Clustered:
							if (auto ClusterParticle = DirtyParticle.CastToClustered())
							{
								if (ClusterParticle->InternalCluster())
								{
									if (Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
									{
										ActiveClusterUnions.AddUnique((FClusterUnionPhysicsProxy*)(Proxy));
									}

									const bool bActivateChildren =
										ClusterParticle->ObjectState() == EObjectStateType::Dynamic ||
										Proxy->GetType() != EPhysicsProxyType::ClusterUnionProxy;

									// If the cluster is dynamic, all children will likely need a transform update
									// otherwise we should not need a full update (if the cluster is still we only
									// need to consider de-clustered objects which should be in other parts of the 
									// dirty view)
									if(bActivateChildren)
									{
										const TSet<IPhysicsProxyBase*> Proxies = ClusterParticle->PhysicsProxies();
										for(IPhysicsProxyBase* ClusterProxy : Proxies)
										{
											if(!ClusterProxy)
											{
												continue;
											}

											switch(ClusterProxy->GetType())
											{
											case EPhysicsProxyType::SingleParticleProxy:
												ActiveRigid.AddUnique((FSingleParticlePhysicsProxy*)ClusterProxy);
												break;
											case EPhysicsProxyType::GeometryCollectionType:
												ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(ClusterProxy));
												break;
											default:
												ensure(false);
												break;
											}
										}
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
			//we only fill this once per frame
			ensure(PullData->DirtyRigids.Num() == 0);
			PullData->DirtyRigids.Reserve(ActiveRigid.Num());

			for (int32 Idx = 0; Idx < ActiveRigid.Num(); ++Idx)
			{
				PullData->DirtyRigids.AddDefaulted();
				ActiveRigid[Idx]->BufferPhysicsResults(PullData->DirtyRigids.Last());
			}
		}

		// Move correction error data from RewindData to FPullPhysicsData for marshalling
		if (FRewindData* RewindData = GetRewindData())
		{
			RewindData->BufferPhysicsResults(PullData->DirtyRigidErrors);
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
			ensure(PullData->DirtyClusterUnions.IsEmpty());
			PullData->DirtyClusterUnions.Reserve(ActiveClusterUnions.Num());

			for (int32 Idx = 0; Idx < ActiveClusterUnions.Num(); ++Idx)
			{
				PullData->DirtyClusterUnions.AddDefaulted();
				ActiveClusterUnions[Idx]->BufferPhysicsResults_Internal(PullData->DirtyClusterUnions.Last());
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

		{
			ensure(PullData->DirtyCharacterGroundConstraints.Num() == 0);	//we only fill this once per frame
			PullData->DirtyCharacterGroundConstraints.Reserve(CharacterGroundConstraintProxies_Internal.Num());

			for (int32 Idx = 0; Idx < CharacterGroundConstraintProxies_Internal.Num(); ++Idx)
			{
				PullData->DirtyCharacterGroundConstraints.AddDefaulted();
				CharacterGroundConstraintProxies_Internal[Idx]->BufferPhysicsResults(PullData->DirtyCharacterGroundConstraints.Last());
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
		struct FDispatcher {} Dispatcher;
		PullPhysicsStateForEachDirtyProxy_External(Dispatcher);
	}

	int32 FPBDRigidsSolver::NumJointConstraints() const
	{
		return MEvolution->GetJointConstraints().NumConstraints();
	}
	
	int32 FPBDRigidsSolver::NumCollisionConstraints() const
	{
		return GetEvolution()->GetCollisionConstraints().NumConstraints();
	}

	void FPBDRigidsSolver::ResetStatCounters()
	{
		TRACE_COUNTER_SET(ChaosTraceCounter_MidPhase_NumShapePair, 0);
		TRACE_COUNTER_SET(ChaosTraceCounter_MidPhase_NumGeneric, 0);
	}

#ifndef CHAOS_COUNTER_STAT
#define CHAOS_COUNTER_STAT(Name, Value)\
SET_DWORD_STAT(STAT_ChaosCounter_##Name, Value); \
CSV_CUSTOM_STAT(PhysicsCounters, Name, Value, ECsvCustomStatOp::Set); \
TRACE_COUNTER_SET(ChaosTraceCounter_##Name, Value)
#endif

	void FPBDRigidsSolver::UpdateStatCounters() const
	{
		const int32 NumStatic = GetEvolution()->GetParticles().GetActiveStaticParticlesView().Num();
		const int32 NumKinematic = GetEvolution()->GetParticles().GetActiveKinematicParticlesView().Num();
		const int32 NumDynamic = GetEvolution()->GetParticles().GetNonDisabledDynamicView().Num();
		const int32 NumMoving = GetEvolution()->GetParticles().GetActiveDynamicMovingKinematicParticlesView().Num();

		// Particle counts
		CHAOS_COUNTER_STAT(NumDisabledBodies, GetEvolution()->GetParticles().GetAllParticlesView().Num() - GetEvolution()->GetParticles().GetNonDisabledView().Num());
		CHAOS_COUNTER_STAT(NumBodies, GetEvolution()->GetParticles().GetNonDisabledView().Num());
		CHAOS_COUNTER_STAT(NumDynamicBodies, NumDynamic);
		CHAOS_COUNTER_STAT(NumKinematicBodies, NumKinematic);
		CHAOS_COUNTER_STAT(NumStaticBodies, NumStatic);
		CHAOS_COUNTER_STAT(NumMovingBodies, NumMoving);
		CHAOS_COUNTER_STAT(NumGeometryCollectionBodies, (int32)GetEvolution()->GetParticles().GetGeometryCollectionParticles().Size());

		// Constraint counts
		CHAOS_COUNTER_STAT(NumIslands, GetEvolution()->GetIslandManager().GetNumIslands());
		CHAOS_COUNTER_STAT(NumIslandGroups, GetEvolution()->GetIslandGroupManager().GetNumActiveGroups());
		CHAOS_COUNTER_STAT(NumContacts, NumCollisionConstraints());
		CHAOS_COUNTER_STAT(NumJoints, NumJointConstraints());
		CHAOS_COUNTER_STAT(NumCharacterGroundConstraints, GetEvolution()->GetCharacterGroundConstraints().GetNumConstraints());

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		UpdateExpensiveStatCounters();
#endif

		// Collision detection info
		CHAOS_COUNTER_STAT(NumBroadPhasePairs, GetEvolution()->GetBroadPhase().GetNumBroadPhasePairs());
		CHAOS_COUNTER_STAT(NumMidPhases, GetEvolution()->GetBroadPhase().GetNumMidPhases());

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
			if (Collision == nullptr)
			{
				continue;
			}

			const FPBDCollisionConstraint& Contact = Collision->GetContact();
			if (Contact.IsEnabled())
			{
				if (Contact.GetManifoldPoints().Num() > 0)
				{
					++NumValidCollisions;
				}
				if (!Contact.AccumulatedImpulse.IsNearlyZero())
				{
					++NumActiveCollisions;
				}
				if (Contact.WasManifoldRestored())
				{
					++NumRestoredCollisions;
				}
				for (int32 PointIndex = 0; PointIndex < Contact.NumManifoldPoints(); ++PointIndex)
				{
					const FManifoldPoint& ManifoldPoint = Contact.GetManifoldPoint(PointIndex);
					const FManifoldPointResult& ManifoldPointResult = Contact.GetManifoldPointResult(PointIndex);
					++NumManifoldPoints;
					if (ManifoldPoint.Flags.bWasRestored || Contact.WasManifoldRestored())
					{
						++NumRestoredManifoldPoints;
					}
					if (ManifoldPoint.Flags.bWasReplaced)
					{
						++NumUpdatedManifoldPoints;
					}
					if (ManifoldPointResult.bIsValid)
					{
						if (!ManifoldPointResult.NetPushOut.IsNearlyZero())
						{
							++NumActiveManifoldPoints;
						}
					}
				}
			}
		}

		int32 NumStaticShapes = 0;
		int32 NumKinematicShapes = 0;
		int32 NumDynamicShapes = 0;
		for (const FTransientPBDRigidParticleHandle& Particle : Particles.GetActiveParticlesView())
		{
			const FConstGenericParticleHandle P = Particle.Handle();
			if (Particle.GetGeometry() != nullptr)
			{
				int32 NumShapes = 1;
				if (const FImplicitObjectUnion* Union = Particle.GetGeometry()->AsA<FImplicitObjectUnion>())
				{
					NumShapes = Union->GetNumLeafObjects();
				}
				if(auto* ClusteredParticle = Particle.CastToClustered())
				{
					if((ClusteredParticle->ConvexOptimizer().Get() != nullptr) && ClusteredParticle->ConvexOptimizer()->IsValid())
					{
						NumShapes = ClusteredParticle->ConvexOptimizer()->NumCollisionObjects();
					}
				}
				if (P->IsDynamic())
				{
					NumDynamicShapes += NumShapes;
				}
				else if (P->IsKinematic())
				{
					NumKinematicShapes += NumShapes;
				}
				else
				{
					NumStaticShapes += NumShapes;
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
		CHAOS_COUNTER_STAT(NumStaticShapes, NumStaticShapes);
		CHAOS_COUNTER_STAT(NumKinematicShapes, NumKinematicShapes);
		CHAOS_COUNTER_STAT(NumDynamicShapes, NumDynamicShapes);
	}

	void FPBDRigidsSolver::DebugDrawShapes(const bool bShowStatic, const bool bShowKinematic, const bool bShowDynamic) const
	{
#if CHAOS_DEBUG_DRAW
		if (bShowStatic)
		{
			DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveStaticParticlesView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (bShowKinematic)
		{
			DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveKinematicParticlesView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (bShowDynamic)
		{
			DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetNonDisabledDynamicView(), 1.0f, &ChaosSolverDebugDebugDrawSettings);
		}
#endif
	}

	void FPBDRigidsSolver::PreIntegrateDebugDraw(FReal Dt) const
	{
#if CHAOS_DEBUG_DRAW
		QUICK_SCOPE_CYCLE_COUNTER(SolverDebugDraw);

		if (ChaosSolverDebugDrawPreIntegrationShapes == 1)
		{
			ChaosSolverDebugDebugDrawSettings.ShapesColorsPerState = DebugDraw::GetDefaultShapesColorsPreIntegrate();

			DebugDrawShapes(false, !!ChaosSolverDrawShapesShowKinematic, !!ChaosSolverDrawShapesShowDynamic);
		}
		if (ChaosSolverDebugDrawPreIntegrationCollisions == 1)
		{
			DebugDraw::DrawCollisions(FRigidTransform3(), GetEvolution()->GetCollisionConstraints().GetConstraintAllocator(), 1.f, &ChaosSolverDebugDebugDrawSettings);
		}
#endif
	}

	void FPBDRigidsSolver::PreSolveDebugDraw(FReal Dt) const
	{
#if CHAOS_DEBUG_DRAW
		QUICK_SCOPE_CYCLE_COUNTER(SolverDebugDraw);

		if (ChaosSolverDebugDrawPostIntegrationShapes == 1)
		{
			ChaosSolverDebugDebugDrawSettings.ShapesColorsPerState = DebugDraw::GetDefaultShapesColorsPostIntegrate();

			DebugDrawShapes(!!ChaosSolverDrawShapesShowStatic, !!ChaosSolverDrawShapesShowKinematic, !!ChaosSolverDrawShapesShowDynamic);
		}
		if (ChaosSolverDebugDrawPostIntegrationCollisions == 1)
		{
			DebugDraw::DrawCollisions(FRigidTransform3(), GetEvolution()->GetCollisionConstraints().GetConstraintAllocator(), 1.f, &ChaosSolverDebugDebugDrawSettings);
		}
#endif
	}

	void FPBDRigidsSolver::PostTickDebugDraw(FReal Dt) const
	{
#if CHAOS_DEBUG_DRAW
		QUICK_SCOPE_CYCLE_COUNTER(SolverDebugDraw);

		const bool bIsServer = GetDebugName().ToString().StartsWith(TEXT("Server"));
		if (bIsServer && !ChaosSolverDebugDrawShowServer)
		{
			return;
		}
		if (!bIsServer && !ChaosSolverDebugDrawShowClient)
		{
			return;
		}

		if (ChaosSolverDebugDrawShapes == 1)
		{
			if (ChaosSolverDebugDrawColorShapeByClientServer)
			{
				if (bIsServer)
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
			DebugDrawShapes(!!ChaosSolverDrawShapesShowStatic, !!ChaosSolverDrawShapesShowKinematic, !!ChaosSolverDrawShapesShowDynamic);
		}
		if (ChaosSolverDebugDrawCollisions == 1)
		{
			DebugDraw::DrawCollisions(FRigidTransform3(), GetEvolution()->GetCollisionConstraints().GetConstraintAllocator(), 1.f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawMass == 1)
		{
			DebugDraw::DrawParticleMass(FRigidTransform3(), Particles.GetActiveKinematicParticlesView(), &ChaosSolverDebugDebugDrawSettings);
			DebugDraw::DrawParticleMass(FRigidTransform3(), Particles.GetNonDisabledDynamicView(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawBVHs == 1)
		{
			for (const FTransientGeometryParticleHandle& Particle : Particles.GetNonDisabledView())
			{
				DebugDraw::DrawParticleBVH(FRigidTransform3(), Particle.Handle(), FColor::Silver, &ChaosSolverDebugDebugDrawSettings);
			}
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
			DebugDraw::DrawConstraintGraph(FRigidTransform3(), GetEvolution()->GetIslandManager(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawIslandSleepState == 1)
		{
			GetEvolution()->GetIslandManager().DebugDrawSleepState(&ChaosSolverDebugDebugDrawSettings);
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
		if (ChaosSolverDrawCharacterGroundConstraints == 1)
		{
			DebugDraw::DrawCharacterGroundConstraints(FRigidTransform3(), MEvolution->GetCharacterGroundConstraints(), &ChaosSolverDebugDebugDrawSettings);
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

	FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FUniqueIdx& Idx)
	{
		return SingleParticlePhysicsProxies_PT.IsValidIndex(Idx.Idx) ? SingleParticlePhysicsProxies_PT[Idx.Idx] : nullptr;
	}

	const FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FUniqueIdx& Idx) const
	{
		return SingleParticlePhysicsProxies_PT.IsValidIndex(Idx.Idx) ? SingleParticlePhysicsProxies_PT[Idx.Idx] : nullptr;
	}

	FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FGeometryParticleHandle& Handle)
	{
		return GetParticleProxy_PT(Handle.UniqueIdx());
	}

	const FSingleParticlePhysicsProxy* FPBDRigidsSolver::GetParticleProxy_PT(const FGeometryParticleHandle& Handle) const
	{
		return GetParticleProxy_PT(Handle.UniqueIdx());
	}

	void FPBDRigidsSolver::UpdateMaterial(FMaterialHandle InHandle, const FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterials.Get(InHandle.InnerHandle) = InNewData;
	}

	void FPBDRigidsSolver::CreateMaterial(FMaterialHandle InHandle, const FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterials.Create(InNewData) == InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::DestroyMaterial(FMaterialHandle InHandle)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		SimMaterials.Destroy(InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::UpdateMaterialMask(FMaterialMaskHandle InHandle, const FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterialMasks.Get(InHandle.InnerHandle) = InNewData;
	}

	void FPBDRigidsSolver::CreateMaterialMask(FMaterialMaskHandle InHandle, const FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterialMasks.Create(InNewData) == InHandle.InnerHandle);
	}

	void FPBDRigidsSolver::DestroyMaterialMask(FMaterialMaskHandle InHandle)
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
		return bIsDeterministic || (MRewindData != nullptr) || (ChaosSolverDeterministic >= 1) || FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled();
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

	void FPBDRigidsSolver::SetParticleDynamicMisc(FPBDRigidParticleHandle* Rigid, const FParticleDynamicMisc& DynamicMisc)
	{
		if (Rigid == nullptr)
		{
			return;
		}

		// Enable or disable the particle
		if (Rigid->Disabled() != DynamicMisc.Disabled())
		{
			if (DynamicMisc.Disabled())
			{
				GetEvolution()->DisableParticle(Rigid);
			}
			else
			{
				GetEvolution()->EnableParticle(Rigid);
			}
		}

		// If we changed kinematics we need to rebuild the inertia conditioning
		const bool bDirtyInertiaConditioning = (Rigid->ObjectState() != DynamicMisc.ObjectState());
		if (bDirtyInertiaConditioning)
		{
			Rigid->SetInertiaConditioningDirty();
		}

		Rigid->SetLinearEtherDrag(DynamicMisc.LinearEtherDrag());
		Rigid->SetAngularEtherDrag(DynamicMisc.AngularEtherDrag());
		Rigid->SetMaxLinearSpeedSq(DynamicMisc.MaxLinearSpeedSq());
		Rigid->SetMaxAngularSpeedSq(DynamicMisc.MaxAngularSpeedSq());
		Rigid->SetInitialOverlapDepenetrationVelocity(DynamicMisc.InitialOverlapDepenetrationVelocity());
		Rigid->SetSleepThresholdMultiplier(DynamicMisc.SleepThresholdMultiplier());
		Rigid->SetCollisionGroup(DynamicMisc.CollisionGroup());
		Rigid->SetDisabled(DynamicMisc.Disabled());
		Rigid->SetCollisionConstraintFlags(DynamicMisc.CollisionConstraintFlags());
		Rigid->SetControlFlags(DynamicMisc.ControlFlags());

		GetEvolution()->SetParticleObjectState(Rigid, DynamicMisc.ObjectState());
		GetEvolution()->SetParticleSleepType(Rigid, DynamicMisc.SleepType());
	}


	FClusterCreationParameters::EConnectionMethod ToInternalConnectionMethod(EClusterUnionMethod InMethod)
	{
		using ETargetEnum = FClusterCreationParameters::EConnectionMethod;
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

	void FPBDRigidsSolver::ApplyConfig(const FChaosSolverConfiguration& InConfig)
	{
		GetEvolution()->GetRigidClustering().SetClusterConnectionFactor(InConfig.ClusterConnectionFactor);
		GetEvolution()->GetRigidClustering().SetClusterUnionConnectionType(ToInternalConnectionMethod(InConfig.ClusterUnionConnectionType));
		SetPositionIterations(InConfig.PositionIterations);
		SetVelocityIterations(InConfig.VelocityIterations);
		SetProjectionIterations(InConfig.ProjectionIterations);
		SetCollisionCullDistance(InConfig.CollisionCullDistance);
		SetCollisionMaxPushOutVelocity(InConfig.CollisionMaxPushOutVelocity);
		SetCollisionDepenetrationVelocity(InConfig.CollisionInitialOverlapDepenetrationVelocity);
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

	void FPBDRigidsSolver::FieldParameterUpdateCallback(
		FPBDPositionConstraints& PositionTarget,
		TMap<int32, int32>& TargetedParticles)
	{
		GetPerSolverField().FieldParameterUpdateCallback(this, PositionTarget, TargetedParticles);
	}

	void FPBDRigidsSolver::FieldForcesUpdateCallback()
	{
		GetPerSolverField().FieldForcesUpdateCallback(this);
	}

}; // namespace Chaos

