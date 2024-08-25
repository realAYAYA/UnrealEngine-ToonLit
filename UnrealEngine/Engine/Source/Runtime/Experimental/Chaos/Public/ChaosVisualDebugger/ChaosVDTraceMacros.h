// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CHAOS_VISUAL_DEBUGGER

	#ifndef CVD_DEFINE_TRACE_VECTOR
		#define CVD_DEFINE_TRACE_VECTOR(Type, Name) \
		UE_TRACE_EVENT_FIELD(Type, Name##X) \
		UE_TRACE_EVENT_FIELD(Type, Name##Y) \
		UE_TRACE_EVENT_FIELD(Type, Name##Z)
	#endif

	#ifndef CVD_DEFINE_TRACE_ROTATOR
		#define CVD_DEFINE_TRACE_ROTATOR(Type, Name) \
		UE_TRACE_EVENT_FIELD(Type, Name##X) \
		UE_TRACE_EVENT_FIELD(Type, Name##Y) \
		UE_TRACE_EVENT_FIELD(Type, Name##Z) \
		UE_TRACE_EVENT_FIELD(Type, Name##W)
	#endif

	#ifndef CVD_TRACE_VECTOR_ON_EVENT
		#define CVD_TRACE_VECTOR_ON_EVENT(EventName, Name, Vector) \
		EventName.Name##X(Vector.X) \
		<< EventName.Name##Y(Vector.Y) \
		<< EventName.Name##Z(Vector.Z)
	#endif

	#ifndef CVD_TRACE_ROTATOR_ON_EVENT
		#define CVD_TRACE_ROTATOR_ON_EVENT(EventName, Name, Rotator) \
		EventName.Name##X(Rotator.X) \
		<< EventName.Name##Y(Rotator.Y) \
		<< EventName.Name##Z(Rotator.Z) \
		<< EventName.Name##W(Rotator.W)
	#endif

	#ifndef CVD_TRACE_PARTICLE
		#define CVD_TRACE_PARTICLE(ParticleHandle) \
			FChaosVisualDebuggerTrace::TraceParticle(ParticleHandle);
	#endif

	#ifndef CVD_TRACE_PARTICLES
		#define CVD_TRACE_PARTICLES(ParticleHandles) \
			FChaosVisualDebuggerTrace::TraceParticles(ParticleHandles);
	#endif

	#ifndef CVD_TRACE_PARTICLES_VIEW
		#define CVD_TRACE_PARTICLES_VIEW(ParticleHandlesView) \
		FChaosVisualDebuggerTrace::TraceParticlesView(ParticleHandlesView);
	#endif

	#ifndef CVD_TRACE_PARTICLES_SOA
		#define CVD_TRACE_PARTICLES_SOA(ParticleSoA) \
		FChaosVisualDebuggerTrace::TraceParticlesSoA(ParticleSoA);
	#endif

	#ifndef CVD_TRACE_SOLVER_START_FRAME
		#define CVD_TRACE_SOLVER_START_FRAME(SolverType, SolverRef) \
			FChaosVDContext StartContextData; \
			FChaosVisualDebuggerTrace::GetCVDContext<SolverType>(SolverRef, StartContextData); \
			FChaosVisualDebuggerTrace::TraceSolverFrameStart(StartContextData, FChaosVisualDebuggerTrace::GetDebugName<SolverType>(SolverRef));
	#endif

	#ifndef CVD_TRACE_SOLVER_END_FRAME
		#define CVD_TRACE_SOLVER_END_FRAME(SolverType, SolverRef) \
			FChaosVDContext EndContextData; \
			FChaosVisualDebuggerTrace::GetCVDContext<SolverType>(SolverRef, EndContextData); \
			FChaosVisualDebuggerTrace::TraceSolverFrameEnd(EndContextData);
	#endif

	#ifndef CVD_SCOPE_TRACE_SOLVER_FRAME
		#define CVD_SCOPE_TRACE_SOLVER_FRAME(SolverType, SolverRef) \
			FChaosVDScopeSolverFrame<SolverType> ScopeSolverFrame(SolverRef);
	#endif

	#ifndef CVD_TRACE_SOLVER_STEP_START
		#define CVD_TRACE_SOLVER_STEP_START(StepName) \
			FChaosVisualDebuggerTrace::TraceSolverStepStart(StepName);
	#endif

	#ifndef CVD_TRACE_SOLVER_STEP_END
		#define CVD_TRACE_SOLVER_STEP_END() \
			FChaosVisualDebuggerTrace::TraceSolverStepEnd();
	#endif

	#ifndef CVD_SCOPE_TRACE_SOLVER_STEP
		#define CVD_SCOPE_TRACE_SOLVER_STEP(DataChannel, StepName) \
			CVD_SCOPED_DATA_CHANNEL_OVERRIDE(DataChannel) \
			FChaosVDScopeSolverStep ScopeSolverStep(StepName)
	#endif

	#ifndef CVD_TRACE_BINARY_DATA
		#define CVD_TRACE_BINARY_DATA(InData, TypeName) \
		FChaosVisualDebuggerTrace::TraceBinaryData(InData, TypeName);
	#endif

	#ifndef CVD_TRACE_SOLVER_SIMULATION_SPACE
		#define CVD_TRACE_SOLVER_SIMULATION_SPACE(InSimulationSpace) \
		FChaosVisualDebuggerTrace::TraceSolverSimulationSpace(InSimulationSpace);
	#endif

	#ifndef CVD_TRACE_PARTICLE_DESTROYED
		#define CVD_TRACE_PARTICLE_DESTROYED(DestroyedParticleHandle) \
		FChaosVisualDebuggerTrace::TraceParticleDestroyed(DestroyedParticleHandle);
	#endif

	#ifndef CVD_TRACE_MID_PHASE
		#define CVD_TRACE_MID_PHASE(MidPhase) \
		FChaosVisualDebuggerTrace::TraceMidPhase(MidPhase);
	#endif

	#ifndef CVD_TRACE_COLLISION_CONSTRAINT
		#define CVD_TRACE_COLLISION_CONSTRAINT(Constraint) \
		FChaosVisualDebuggerTrace::TraceCollisionConstraint(Constraint);
	#endif

	#ifndef CVD_TRACE_COLLISION_CONSTRAINT_VIEW
		#define CVD_TRACE_COLLISION_CONSTRAINT_VIEW(ConstraintView) \
		FChaosVisualDebuggerTrace::TraceCollisionConstraintView(ConstraintView);
	#endif

	#ifndef CVD_TRACE_STEP_MID_PHASES_FROM_COLLISION_CONSTRAINTS
		#define CVD_TRACE_STEP_MID_PHASES_FROM_COLLISION_CONSTRAINTS(DataChannel, CollisionConstraints) \
		CVD_SCOPED_DATA_CHANNEL_OVERRIDE(DataChannel) \
		FChaosVisualDebuggerTrace::TraceMidPhasesFromCollisionConstraints(CollisionConstraints);
	#endif

	#ifndef CVD_TRACE_INVALIDATE_CACHED_GEOMETRY
		#define CVD_TRACE_INVALIDATE_CACHED_GEOMETRY(ImplicitObjectPtr) \
		FChaosVisualDebuggerTrace::InvalidateGeometryFromCache(ImplicitObjectPtr);
	#endif

	#ifndef CVD_TRACE_SCOPED_SCENE_QUERY
		#define CVD_TRACE_SCENE_QUERY_START(InputGeometry, GeometryOrientation, Start, End, TraceChannel, Params, ResponseParams, ObjectParams, QueryType, QueryMode, SolverID, bIsRetry) \
		FChaosVisualDebuggerTrace::TraceSceneQueryStart(InputGeometry, GeometryOrientation, Start, End, TraceChannel, Params, ResponseParams, ObjectParams, QueryType, QueryMode, SolverID, bIsRetry);
	#endif

	#ifndef CVD_TRACE_SCOPED_SCENE_QUERY_VISIT
		#define CVD_TRACE_SCOPED_SCENE_QUERY_VISIT(InQueryVisitData) \
		FChaosVDScopeSceneQueryVisit CVDQueryVisit(InQueryVisitData);
	#endif

	#ifndef CVD_TRACE_SCENE_QUERY_VISIT
		#define CVD_TRACE_SCENE_QUERY_VISIT(InQueryVisitData) \
		FChaosVisualDebuggerTrace::TraceSceneQueryVisit(InQueryVisitData);
	#endif

	#ifndef CVD_TRACE_JOINT_CONSTRAINTS
		#define CVD_TRACE_JOINT_CONSTRAINTS(DataChannel, InJointConstraints) \
			{ \
				CVD_SCOPED_DATA_CHANNEL_OVERRIDE(DataChannel) \
				FChaosVisualDebuggerTrace::TraceJointsConstraints(InJointConstraints); \
			}
	#endif

	#ifndef CVD_TRACE_CONSTRAINTS_CONTAINER
		#define CVD_TRACE_CONSTRAINTS_CONTAINER(ContainerView) \
			FChaosVisualDebuggerTrace::TraceConstraintsContainer(ContainerView);
	#endif

#else // WITH_CHAOS_VISUAL_DEBUGGER

	#ifndef CVD_TRACE_PARTICLE
		#define CVD_TRACE_PARTICLE(ParticleHandle)
	#endif

	#ifndef CVD_TRACE_PARTICLES
		#define CVD_TRACE_PARTICLES(ParticleHandles)
	#endif

	#ifndef CVD_TRACE_SOLVER_START_FRAME
		#define CVD_TRACE_SOLVER_START_FRAME(SolverType, SolverRef)
	#endif

	#ifndef CVD_TRACE_SOLVER_END_FRAME
		#define CVD_TRACE_SOLVER_END_FRAME(SolverType, SolverRef)
	#endif

	#ifndef CVD_SCOPE_TRACE_SOLVER_FRAME
		#define CVD_SCOPE_TRACE_SOLVER_FRAME(SolverType, SolverRef)
	#endif

	#ifndef CVD_TRACE_SOLVER_STEP_START
		#define CVD_TRACE_SOLVER_STEP_START(StepName)
	#endif

	#ifndef CVD_TRACE_SOLVER_STEP_END
		#define CVD_TRACE_SOLVER_STEP_END()
	#endif

	#ifndef CVD_SCOPE_TRACE_SOLVER_STEP
		#define CVD_SCOPE_TRACE_SOLVER_STEP(DataChannel, StepName)
	#endif

	#ifndef CVD_TRACE_BINARY_DATA
		#define CVD_TRACE_BINARY_DATA(InData, TypeName)
	#endif

	#ifndef CVD_TRACE_SOLVER_SIMULATION_SPACE
			#define CVD_TRACE_SOLVER_SIMULATION_SPACE(InSimulationSpace)
	#endif

	#ifndef CVD_TRACE_PARTICLES_SOA
		#define CVD_TRACE_PARTICLES_SOA(ParticleSoA)
	#endif

	#ifndef CVD_TRACE_PARTICLE_DESTROYED
		#define CVD_TRACE_PARTICLE_DESTROYED(DestroyedParticleHandle)
	#endif

	#ifndef CVD_TRACE_MID_PHASE
		#define CVD_TRACE_MID_PHASE(MidPhase)
	#endif
	#ifndef CVD_TRACE_COLLISION_CONSTRAINT
		#define CVD_TRACE_COLLISION_CONSTRAINT(Constraint)
	#endif
	#ifndef CVD_TRACE_COLLISION_CONSTRAINT_VIEW
		#define CVD_TRACE_COLLISION_CONSTRAINT_VIEW(ConstraintView)
	#endif

	#ifndef CVD_TRACE_PARTICLES_VIEW
		#define CVD_TRACE_PARTICLES_VIEW(ParticleHandlesView)
	#endif

	#ifndef CVD_TRACE_STEP_MID_PHASES_FROM_COLLISION_CONSTRAINTS
		#define CVD_TRACE_STEP_MID_PHASES_FROM_COLLISION_CONSTRAINTS(DataChannel, CollisionConstraints)
	#endif

	#ifndef CVD_TRACE_INVALIDATE_CACHED_GEOMETRY
		#define CVD_TRACE_INVALIDATE_CACHED_GEOMETRY(ImplicitObjectPtr)
	#endif

	#ifndef CVD_TRACE_SCOPED_SCENE_QUERY
		#define CVD_TRACE_SCENE_QUERY_START(InputGeometry, GeometryOrientation, Start, End, TraceChannel, Params, ResponseParams, ObjectParams, QueryType, QueryMode, SolverID)
	#endif

	#ifndef CVD_TRACE_SCOPED_SCENE_QUERY_VISIT
		#define CVD_TRACE_SCOPED_SCENE_QUERY_VISIT(InQueryVisitData)
	#endif

	#ifndef CVD_TRACE_SCENE_QUERY_VISIT
		#define CVD_TRACE_SCENE_QUERY_VISIT(InQueryVisitData)
	#endif

	#ifndef CVD_TRACE_JOINT_CONSTRAINTS
		#define CVD_TRACE_JOINT_CONSTRAINTS(DataChannel, InJointConstraints)
	#endif

	#ifndef CVD_TRACE_CONSTRAINTS_CONTAINER
		#define CVD_TRACE_CONSTRAINTS_CONTAINER(ContainerView)
	#endif

#endif // WITH_CHAOS_VISUAL_DEBUGGER