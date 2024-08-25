// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/Core.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/Serializable.h"
#include "Containers/Set.h"

#include "Chaos/ChaosArchive.h"
#include "ChaosVDContextProvider.h"
#include "ChaosVDOptionalDataChannel.h"
#include "Chaos/ParticleIterator.h"
#include "HAL/ThreadSafeBool.h"

#ifndef CHAOS_VISUAL_DEBUGGER_ENABLED
	#define CHAOS_VISUAL_DEBUGGER_ENABLED WITH_CHAOS_VISUAL_DEBUGGER
#endif

#include "ChaosVisualDebugger/ChaosVDTraceMacros.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "ChaosVDMemWriterReader.h"
#include "ChaosVDRuntimeModule.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

namespace Chaos
{
class FPBDConstraintContainer;
class FPBDJointConstraints;
}

UE_TRACE_CHANNEL_EXTERN(ChaosVDChannel, CHAOS_API)

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDSolverFrameStart)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, DebugName)
	UE_TRACE_EVENT_FIELD(bool, IsKeyFrame)
	UE_TRACE_EVENT_FIELD(bool, IsReSimulated)
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

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDNonSolverLocation)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	CVD_DEFINE_TRACE_VECTOR(Chaos::FReal, Position)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, DebugName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDNonSolverTransform)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	CVD_DEFINE_TRACE_VECTOR(Chaos::FReal, Position)
	CVD_DEFINE_TRACE_VECTOR(Chaos::FReal, Scale)
	CVD_DEFINE_TRACE_ROTATOR(Chaos::FReal, Rotation)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, DebugName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN_EXTERN(ChaosVDLogger, ChaosVDDummyEvent)
	UE_TRACE_EVENT_FIELD(int32, SolverID)
UE_TRACE_EVENT_END()

struct FChaosVDContext;
struct FChaosVDQueryVisitStep;
struct FChaosVDCollisionResponseParams;
struct FChaosVDCollisionObjectQueryParams;
struct FChaosVDCollisionQueryParams;
enum class EChaosVDSceneQueryMode;
enum class EChaosVDSceneQueryType;
struct FCollisionObjectQueryParams;
struct FCollisionResponseParams;
struct FCollisionQueryParams;
enum ECollisionChannel : int;

namespace Chaos
{
	namespace VisualDebugger
	{
		class FChaosVDSerializableNameTable;
	}

	class FPBDCollisionConstraints;
	class FPBDRigidsSOAs;
	class FImplicitObject;
	class FPhysicsSolverBase;
	template <typename T, int d>
	class TGeometryParticleHandles;

	class FPBDCollisionConstraint;
	class FParticlePairMidPhase;
}

using FChaosVDImplicitObjectWrapper = FChaosVDImplicitObjectDataWrapper<Chaos::FImplicitObjectPtr, Chaos::FChaosArchive>;
using FChaosVDSerializableNameTable = Chaos::VisualDebugger::FChaosVDSerializableNameTable;

enum class EChaosVDTraceBinaryDataOptions
{
	None = 0,
	ForceTrace = 1 << 0
};
ENUM_CLASS_FLAGS(EChaosVDTraceBinaryDataOptions)

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

	/** Traces all joint constraints in the provided container */
	static CHAOS_API void TraceJointsConstraints(Chaos::FPBDJointConstraints& InJointConstraints);

	/** Traces a Particle pair MidPhase as binary data */
	static CHAOS_API void TraceCollisionConstraint(const Chaos::FPBDCollisionConstraint* CollisionConstraint);

	/** Traces a Particle pair MidPhase as binary data in parallel */
	static CHAOS_API void TraceCollisionConstraintView(TArrayView<Chaos::FPBDCollisionConstraint* const> CollisionConstraintView);

	/** Traces all supported constraints in the provided containers view */
	static CHAOS_API void TraceConstraintsContainer(TConstArrayView<Chaos::FPBDConstraintContainer*> ConstraintContainersView);

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
	static CHAOS_API void TraceBinaryData(TConstArrayView<uint8> InData, FStringView TypeName, EChaosVDTraceBinaryDataOptions Options = EChaosVDTraceBinaryDataOptions::None);

	/**
	 * Serializes the implicit object contained in the wrapper and trace its it as binary data
	 * The trace event is not tied to a particular Solver Frame/Step
	 *  @param WrappedGeometryData Wrapper containing a ptr to the implicit and its ID
	 */
	static CHAOS_API void TraceImplicitObject(FChaosVDImplicitObjectWrapper WrappedGeometryData);

	/**
	 * Removes an implicit object from the serialized geometry IDs cache, to ensure we re-serialize it with any new changes
	 *  @param CachedGeometryToInvalidate Ptr to the Geometry we want to invalidate from the cache
	 */
	static CHAOS_API void InvalidateGeometryFromCache(const Chaos::FImplicitObject* CachedGeometryToInvalidate);

	static CHAOS_API void TraceSceneQueryStart(const Chaos::FImplicitObject* InputGeometry, const FQuat& GeometryOrientation, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, FChaosVDCollisionQueryParams&& Params, FChaosVDCollisionResponseParams&& ResponseParams, FChaosVDCollisionObjectQueryParams&& ObjectParams, EChaosVDSceneQueryType QueryType, EChaosVDSceneQueryMode QueryMode, int32 SolverID, bool bIsRetry);
	static CHAOS_API void TraceSceneQueryVisit(FChaosVDQueryVisitStep&& InQueryVisitData);

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
	static CHAOS_API bool IsTracing();

	/** Binds to the static events triggered by the ChaosVD Runtime module */
	static void RegisterEventHandlers();
	
	/** Unbinds to the static events triggered by the ChaosVD Runtime module */
	static void UnregisterEventHandlers();

	static TSharedRef<FChaosVDSerializableNameTable>& GetNameTableInstance() { return CVDNameTable; }

private:

	/**
	 * Traces data from a Particle Handle using the provided CVD Context
	 * @param ParticleHandle Handle to process and Trace
	 * @param ContextData Context to be used to tied this Trace event to a specific solver frame and step
	 */
	static CHAOS_API void TraceParticle(Chaos::FGeometryParticleHandle* ParticleHandle, const FChaosVDContext& ContextData);

	/**
	 * Traces the provided location using with the provided ID -
	 * These are not tied to any solver step so they will be recorded as part of the current game frame data
	 * @param InLocation Location to Trace
	 * @param DebugNameID Name to use as ID
	 */
	static void TraceNonSolverLocation(const FVector& InLocation, FStringView DebugNameID);

	/**
	 * Traces the provided transform using with the provided ID -
	 * These are not tied to any solver step so they will be recorded as part of the current game frame data
	 * @param InTransform Transform to Trace
	 * @param DebugNameID Name to use as ID
	 */
	static void TraceNonSolverTransform(const FTransform& InTransform, FStringView DebugNameID);
	
	/** Resets the state of the CVD Tracer */
	static void Reset();

	static void HandleRecordingStop();
	static void TraceArchiveHeader();
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
	static TSharedRef<FChaosVDSerializableNameTable> CVDNameTable;

	static std::atomic<bool> bIsTracing;

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

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!ensure(CVDContextData))
	{
		return;
	}

	FChaosVDContext CopyContext = *CVDContextData;
	
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

struct FChaosVDScopeSceneQueryVisit
{
	FChaosVDScopeSceneQueryVisit(FChaosVDQueryVisitStep& InVisitData) : VisitData(InVisitData)
	{
	}

	~FChaosVDScopeSceneQueryVisit()
	{
		CVD_TRACE_SCENE_QUERY_VISIT(MoveTemp(VisitData));
	}

	FChaosVDQueryVisitStep& VisitData;
};

namespace Chaos::VisualDebugger
{
	template<typename TDataToSerialize>
	void WriteDataToBuffer(TArray<uint8>& InOutDataBuffer, TDataToSerialize& Data)
	{
		FChaosVDMemoryWriter MemWriterAr(InOutDataBuffer, FChaosVisualDebuggerTrace::GetNameTableInstance());
		MemWriterAr.SetShouldSkipUpdateCustomVersion(true);

		Data.Serialize(MemWriterAr);
	}

	template<typename TDataToSerialize, typename TArchive>
	void WriteDataToBuffer(TArray<uint8>& InOutDataBuffer, TDataToSerialize& Data)
	{
		FChaosVDMemoryWriter MemWriterAr(InOutDataBuffer, FChaosVisualDebuggerTrace::GetNameTableInstance());
		TArchive Ar(MemWriterAr);
		Ar.SetShouldSkipUpdateCustomVersion(true);

		Data.Serialize(Ar);
	}
}
#endif // WITH_CHAOS_VISUAL_DEBUGGER
