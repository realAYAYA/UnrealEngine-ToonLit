// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/Core.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/Serializable.h"
#include "Containers/Set.h"

#include "Chaos/ChaosArchive.h"
#include "ChaosVDContextProvider.h"
#include "Chaos/ParticleIterator.h"
#include "HAL/ThreadSafeBool.h"

#ifndef CHAOS_VISUAL_DEBUGGER_ENABLED
	#define CHAOS_VISUAL_DEBUGGER_ENABLED (WITH_CHAOS_VISUAL_DEBUGGER && UE_TRACE_ENABLED)
#endif

// Define NO-OP versions of our macros if we can't Trace
#if !CHAOS_VISUAL_DEBUGGER_ENABLED
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
		#define CVD_SCOPE_TRACE_SOLVER_STEP(StepName)
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

#ifndef CVD_TRACE_MID_PHASES_FROM_COLLISION_CONSTRAINTS
	#define CVD_TRACE_MID_PHASES_FROM_COLLISION_CONSTRAINTS(CollisionConstraints)
#endif

#else

#include "ChaosVDRuntimeModule.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

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

UE_TRACE_CHANNEL_EXTERN(ChaosVDChannel, CHAOS_API)

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverFrameStart)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, DebugName)
	UE_TRACE_EVENT_FIELD(bool, IsKeyFrame)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverFrameEnd)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDParticleCreated)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, ParticleID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDParticleDestroyed)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, ParticleID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverStepStart)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StepName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverStepEnd)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, StepNumber)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDBinaryDataStart)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, TypeName)
	UE_TRACE_EVENT_FIELD(int32, DataID)
	UE_TRACE_EVENT_FIELD(uint32, DataSize)
	UE_TRACE_EVENT_FIELD(uint32, OriginalSize)
	UE_TRACE_EVENT_FIELD(bool, IsCompressed)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDBinaryDataContent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, DataID)
	UE_TRACE_EVENT_FIELD(uint8[], RawData)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDBinaryDataEnd)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, DataID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverSimulationSpace)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	CVD_DEFINE_TRACE_VECTOR(Chaos::FReal, Position)
	CVD_DEFINE_TRACE_ROTATOR(Chaos::FReal, Rotation)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDDummyEvent)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
UE_TRACE_EVENT_END()

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
	#define CVD_SCOPE_TRACE_SOLVER_STEP(StepName) \
		FChaosVDScopeSolverStep ScopeSolverStep(StepName);
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

#ifndef CVD_TRACE_MID_PHASES_FROM_COLLISION_CONSTRAINTS
	#define CVD_TRACE_MID_PHASES_FROM_COLLISION_CONSTRAINTS(CollisionConstraints) \
	FChaosVisualDebuggerTrace::TraceMidPhasesFromCollisionConstraints(CollisionConstraints);
#endif

struct FChaosVDContext;

namespace Chaos
{
	class FPBDCollisionConstraints;
	class FPBDRigidsSOAs;
	class FImplicitObject;
	class FPhysicsSolverBase;
	template <typename T, int d>
	class TGeometryParticleHandles;

	class FPBDCollisionConstraint;
	class FParticlePairMidPhase;
}

using FChaosVDImplicitObjectWrapper = FChaosVDImplicitObjectDataWrapper<Chaos::TSerializablePtr<Chaos::FImplicitObject>, Chaos::FChaosArchive>;

/** Class containing  all the Tracing logic to record data for the Chaos Visual Debugger tool */
class FChaosVisualDebuggerTrace
{
public:
	/**
	 * Traces data from a Particle Handle. The CVD context currently pushed into will be used to tie this particle data to a specific solver frame and step
	 * @param ParticleHandle Handle to process and Trace
	 */
	static CHAOS_API void TraceParticle(const Chaos::FGeometryParticleHandle* ParticleHandle);
	
	/**
	 * Traces data from a Particle Handle using the provided CVD Context
	 * @param ParticleHandle Handle to process and Trace
	 * @param ContextData Context to be used to tied this Trace event to a specific solver frame and step
	 */
	static CHAOS_API void TraceParticle(Chaos::FGeometryParticleHandle* ParticleHandle, const FChaosVDContext& ContextData);

	/**
	 * Traces data from a collection of Particle Handles using. The CVD context currently pushed into will be used to tie this particle data to a specific solver frame and step.
	 * It does not handle Full and Delta Recording automatically
	 * @param ParticleHandles Handles collection to process and Trace
	 */
	static CHAOS_API void TraceParticles(const Chaos::TGeometryParticleHandles<Chaos::FReal, 3>& ParticleHandles);
	
	/**
	 * Traces the destruction event for the provided particle handled so it can be reproduces in the CVD tool
	 * @param ParticleHandle Handle that is being destroyed
	 */
	static CHAOS_API void TraceParticleDestroyed(const Chaos::FGeometryParticleHandle* ParticleHandle);

	/**
	 * Traces data from particles on the provided FPBDRigidsSOAs. It traces only the DirtyParticles view unless a full capture was requested
	 * @param ParticlesSoA Particles SoA to evaluate and trace
	 */
	static CHAOS_API void TraceParticlesSoA(const Chaos::FPBDRigidsSOAs& ParticlesSoA);

	/** Traces the provided particle view in parallel */
	template<typename ParticleType>
	static void TraceParticlesView(const Chaos::TParticleView<ParticleType>& ParticlesView);

	/** Traces a Particle pair MidPhase as binary data */
	static CHAOS_API void TraceMidPhase(const Chaos::FParticlePairMidPhase* MidPhase);

	/** Traces a Particle pair MidPhase as binary data from a provided CollisionConstraints object */
	static CHAOS_API void TraceMidPhasesFromCollisionConstraints(Chaos::FPBDCollisionConstraints& InCollisionConstraints);

	/** Traces a Particle pair MidPhase as binary data */
	static CHAOS_API void TraceCollisionConstraint(const Chaos::FPBDCollisionConstraint* CollisionConstraint);

	/** Traces a Particle pair MidPhase as binary data in parallel */
	static CHAOS_API void TraceCollisionConstraintView(TArrayView<Chaos::FPBDCollisionConstraint* const> CollisionConstraintView);

	/** Traces the start of a solver frame and it pushes its context data to the CVD TLS context stack */
	static CHAOS_API void TraceSolverFrameStart(const FChaosVDContext& ContextData, const FString& InDebugName);
	
	/** Traces the end of a solver frame and removes its context data to the CVD TLS context stack */
	static CHAOS_API void TraceSolverFrameEnd(const FChaosVDContext& ContextData);

	/** Traces the start of a solver step
	 * @param StepName Name of the step. It will be used in the CVD Tool's UI
	 */
	static CHAOS_API void TraceSolverStepStart(FStringView StepName);
	/** Traces the end of a solver step */
	static CHAOS_API void TraceSolverStepEnd();

	/** Traces the provider transform as simulation space of the solver that is currently on the CVD Context Stack
	 * @param Transform Simulation space Transform
	 */
	static CHAOS_API void TraceSolverSimulationSpace(const Chaos::FRigidTransform3& Transform);

	/**
	 * Traces a binary blob of data outside of any solver frame solver step scope.
	 * @param InData Data to trace
	 * @param TypeName Type name the data represents. It is used during Trace Analysis serialize it back (this is not automatic)
	 */
	static CHAOS_API void TraceBinaryData(const TArray<uint8>& InData, FStringView TypeName);

	/**
	 * Serializes the implicit object contained in the wrapper and trace its it as binary data
	 * The trace event is not tied to a particular Solver Frame/Step
	 *  @param WrappedGeometryData Wrapper containing a ptr to the implicit and its ID
	 */
	static CHAOS_API void TraceImplicitObject(FChaosVDImplicitObjectWrapper WrappedGeometryData);

	/** Returns true if the provided solver ID needs a Full Capture */
	static bool ShouldPerformFullCapture(int32 SolverID);

	/**
	 * Gets the CVD Context data form an object that has such data. Usually Solvers
	 * @tparam T type of the object with CVD context
	 * @param ObjectWithContext A reference to where to get the CVD Context
	 * @param OutCVDContext A reference to the CVD contexts in the provided object
	 */
	template<typename T>
	static void GetCVDContext(T& ObjectWithContext, FChaosVDContext& OutCVDContext);

	/**
	 * Returns the debug name string of the provided object
	 */
	template<typename T>
	static FString GetDebugName(T& ObjectWithDebugName);

	/** Returns true if a CVD trace is running */
	static bool IsTracing() { return bIsTracing;}

private:

	/** Binds to the static events triggered by the ChaosVD Runtime module */
	static void RegisterEventHandlers();
	/** Unbinds to the static events triggered by the ChaosVD Runtime module */
	static void UnregisterEventHandlers();
	
	/** Resets the state of the CVD Tracer */
	static void Reset();

	static void HandleRecordingStop();
	static void HandleRecordingStart();

	/** Sets up the tracer to perform a full capture in the next solver frame */
	static void PerformFullCapture(EChaosVDFullCaptureFlags CaptureOptions);

	/** Setups the current Solver frame for a full capture if needed */
	static void SetupForFullCaptureIfNeeded(int32 SolverID, bool& bOutFullCaptureRequested);

	static FDelegateHandle RecordingStartedDelegateHandle;
	static FDelegateHandle RecordingStoppedDelegateHandle;
	static FDelegateHandle RecordingFullCaptureRequestedHandle;

	static TSet<int32> SolverIDsForDeltaRecording;
	static TSet<int32> RequestedFullCaptureSolverIDs;

	static FThreadSafeBool bIsTracing;

	static FRWLock DeltaRecordingStatesLock;

	friend class FChaosEngineModule;
};

template <typename ParticleType>
void FChaosVisualDebuggerTrace::TraceParticlesView(const Chaos::TParticleView<ParticleType>& ParticlesView)
{
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!ensure(CVDContextData))
	{
		return;
	}

	FChaosVDContext CopyContext = *FChaosVDThreadContext::Get().GetCurrentContext();
	
	ParticlesView.ParallelFor([CopyContext](auto& Particle, int32 Index)
	{
		CVD_SCOPE_CONTEXT(CopyContext);
		TraceParticle(Particle.Handle());
	});
}

template <typename T>
void FChaosVisualDebuggerTrace::GetCVDContext(T& ObjectWithContext, FChaosVDContext& OutCVDContext)
{
	OutCVDContext = ObjectWithContext.GetChaosVDContextData();	
}

template <typename T>
FString FChaosVisualDebuggerTrace::GetDebugName(T& ObjectWithDebugName)
{
#if CHAOS_DEBUG_NAME
	return ObjectWithDebugName.GetDebugName().ToString();
#else
	return FString("COMPILED OUT");
#endif
}

struct FChaosVDScopeSolverStep
{
	FChaosVDScopeSolverStep(FStringView StepName)
	{
		CVD_TRACE_SOLVER_STEP_START(StepName);
	}

	~FChaosVDScopeSolverStep()
	{
		CVD_TRACE_SOLVER_STEP_END();
	}
};

template<typename T>
struct FChaosVDScopeSolverFrame
{
	FChaosVDScopeSolverFrame(T& InSolverRef) : SolverRef(InSolverRef)
	{
		CVD_TRACE_SOLVER_START_FRAME(T, SolverRef);
	}

	~FChaosVDScopeSolverFrame()
	{
		CVD_TRACE_SOLVER_END_FRAME(T, SolverRef);
	}

	T& SolverRef;
};
#endif // CHAOS_VISUAL_DEBUGGER_ENABLED
