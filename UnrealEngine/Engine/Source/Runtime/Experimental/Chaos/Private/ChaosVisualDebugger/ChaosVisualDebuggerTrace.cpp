// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "ChaosVisualDebugger/ChaosVDDataWrapperUtils.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "DataWrappers/ChaosVDJointDataWrappers.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/UniquePtr.h"

UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDSolverFrameStart)

UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDSolverFrameEnd)

UE_TRACE_CHANNEL_DEFINE(ChaosVDChannel);
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDParticle)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDParticleDestroyed)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDSolverStepStart)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDSolverStepEnd)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDBinaryDataStart)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDBinaryDataContent)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDBinaryDataEnd)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDSolverSimulationSpace)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDDummyEvent)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDNonSolverLocation)
UE_TRACE_EVENT_DEFINE(ChaosVDLogger, ChaosVDNonSolverTransform)

namespace Chaos::VisualDebugger::Cvars
{
	static bool bCompressBinaryData = false;
	FAutoConsoleVariableRef CVarChaosVDCompressBinaryData(
	TEXT("p.Chaos.VD.CompressBinaryData"),
	bCompressBinaryData,
	TEXT("If true, serialized binary data will be compressed using Oodle on the fly before being traced"));

	static int32 CompressionMode = 2;
	FAutoConsoleVariableRef CVarChaosVDCompressionMode(
	TEXT("p.Chaos.VD.CompressionMode"),
	CompressionMode,
	TEXT("Oodle compression mode to use, 4 is by default which equsals to ECompressionLevel::VeryFast"));
}

/** Struct where we keep track of the geometry we are tracing */
struct FChaosVDGeometryTraceContext
{
	FRWLock TracedGeometrySetLock;
	FRWLock CachedGeometryHashesLock;
	TSet<uint32> GeometryTracedIDs;

	uint32 GetGeometryHashForImplicit(const Chaos::FImplicitObject* Implicit)
	{
		if (Implicit == nullptr)
		{
			return 0;
		}

		{
			FReadScopeLock ReadLock(CachedGeometryHashesLock);
			if (uint32* FoundHashPtr = CachedGeometryHashes.Find((void*) Implicit))
			{
				return *FoundHashPtr;
			}
		}

		{
			uint32 Hash = Implicit->GetTypeHash();

			FWriteScopeLock WriteLock(CachedGeometryHashesLock);
			CachedGeometryHashes.Add((void*)Implicit, Hash);
			return Hash;
		}
	}

	void RemoveCachedGeometryHash(const Chaos::FImplicitObject* Implicit)
	{
		if (Implicit == nullptr)
		{
			return;
		}

		FWriteScopeLock WriteLock(CachedGeometryHashesLock);
		CachedGeometryHashes.Remove((void*)Implicit);
	}

	//TODO: Remove this when/if we have stable geometry hashes that can be serialized
	// Right now it is too costly calculate them each time at runtime
	TMap<void*, uint32> CachedGeometryHashes;
};

static FChaosVDGeometryTraceContext GeometryTracerObject = FChaosVDGeometryTraceContext();

FDelegateHandle FChaosVisualDebuggerTrace::RecordingStartedDelegateHandle = FDelegateHandle();
FDelegateHandle FChaosVisualDebuggerTrace::RecordingStoppedDelegateHandle = FDelegateHandle();
FDelegateHandle FChaosVisualDebuggerTrace::RecordingFullCaptureRequestedHandle = FDelegateHandle();

FRWLock FChaosVisualDebuggerTrace::DeltaRecordingStatesLock = FRWLock();
TSet<int32> FChaosVisualDebuggerTrace::SolverIDsForDeltaRecording = TSet<int32>();
TSet<int32> FChaosVisualDebuggerTrace::RequestedFullCaptureSolverIDs = TSet<int32>();
TSharedRef<FChaosVDSerializableNameTable> FChaosVisualDebuggerTrace::CVDNameTable = MakeShared<FChaosVDSerializableNameTable>();
std::atomic<bool> FChaosVisualDebuggerTrace::bIsTracing = false;

void FChaosVisualDebuggerTrace::TraceParticle(const Chaos::FGeometryParticleHandle* ParticleHandle)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	TraceParticle(const_cast<Chaos::FGeometryParticleHandle*>(ParticleHandle), *CVDContextData);
}

void FChaosVisualDebuggerTrace::TraceParticle(Chaos::FGeometryParticleHandle* ParticleHandle, const FChaosVDContext& ContextData)
{
	if (!IsTracing())
	{
		return;
	}

	if (!ParticleHandle)
	{
		UE_LOG(LogChaos, Warning, TEXT("Tried to Trace a null particle %hs"), __FUNCTION__);
		return;
	}

	const uint32 GeometryHash = GeometryTracerObject.GetGeometryHashForImplicit(ParticleHandle->GetGeometry());
	
	TraceImplicitObject({ GeometryHash, ParticleHandle->GetGeometry() });

	{
		FChaosVDParticleDataWrapper ParticleDataWrapper = FChaosVDDataWrapperUtils::BuildParticleDataWrapperFromParticle(ParticleHandle, CVDNameTable);
		ParticleDataWrapper.GeometryHash = GeometryHash;
		ParticleDataWrapper.SolverID = ContextData.Id;
		
		const Chaos::FShapeInstanceArray& ShapesInstancesArray = ParticleHandle->ShapeInstances();
		ParticleDataWrapper.CollisionDataPerShape.Reserve(ShapesInstancesArray.Num());
		
		for (const Chaos::FShapeInstancePtr& ShapeData : ShapesInstancesArray)
		{
			FChaosVDShapeCollisionData CVDCollisionData;
			FChaosVDDataWrapperUtils::CopyShapeDataToWrapper(ShapeData, CVDCollisionData);
			ParticleDataWrapper.CollisionDataPerShape.Add(MoveTemp(CVDCollisionData));
		}
		
		FChaosVDScopedTLSBufferAccessor TLSDataBuffer;

		Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, ParticleDataWrapper);

		TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDParticleDataWrapper::WrapperTypeName);
	}
}

void FChaosVisualDebuggerTrace::TraceParticles(const Chaos::TGeometryParticleHandles<Chaos::FReal, 3>& ParticleHandles)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	ParallelFor(ParticleHandles.Size(),[&ParticleHandles, CopyContext = *CVDContextData](int32 ParticleIndex)
	{
		CVD_SCOPE_CONTEXT(CopyContext)
		TraceParticle(ParticleHandles.Handle(ParticleIndex).Get(), CopyContext);
	});
}

void FChaosVisualDebuggerTrace::TraceParticleDestroyed(const Chaos::FGeometryParticleHandle* ParticleHandle)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	if (!ParticleHandle)
	{
		UE_LOG(LogChaos, Warning, TEXT("Tried to Trace a null particle %hs"), __FUNCTION__);
		return;
	}

	GeometryTracerObject.RemoveCachedGeometryHash(ParticleHandle->GetGeometry());
	
	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDParticleDestroyed, ChaosVDChannel)
		<< ChaosVDParticleDestroyed.SolverID(CVDContextData->Id)
		<< ChaosVDParticleDestroyed.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDParticleDestroyed.ParticleID(ParticleHandle->UniqueIdx().Idx);
}

void FChaosVisualDebuggerTrace::TraceParticlesSoA(const Chaos::FPBDRigidsSOAs& ParticlesSoA)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	// If this solver is not being delta recorded, Trace all the particles
	if (ShouldPerformFullCapture(CVDContextData->Id))
	{
		TraceParticlesView(ParticlesSoA.GetAllParticlesView());
		return;
	}

	TraceParticlesView(ParticlesSoA.GetDirtyParticlesView());
}

void FChaosVisualDebuggerTrace::SetupForFullCaptureIfNeeded(int32 SolverID, bool& bOutFullCaptureRequested)
{
	DeltaRecordingStatesLock.ReadLock();
	bOutFullCaptureRequested = RequestedFullCaptureSolverIDs.Contains(SolverID) || !SolverIDsForDeltaRecording.Contains(SolverID);
	DeltaRecordingStatesLock.ReadUnlock();

	if (bOutFullCaptureRequested)
	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
		SolverIDsForDeltaRecording.Remove(SolverID);
		RequestedFullCaptureSolverIDs.Remove(SolverID);
	}
}

bool FChaosVisualDebuggerTrace::ShouldPerformFullCapture(int32 SolverID)
{
	FReadScopeLock ReadLock(DeltaRecordingStatesLock);
	int32* FoundSolverID = SolverIDsForDeltaRecording.Find(SolverID);

	// If the solver ID is on the SolverIDsForDeltaRecording set, it means we should NOT perform a full capture
	return FoundSolverID == nullptr;
}

void FChaosVisualDebuggerTrace::TraceMidPhase(const Chaos::FParticlePairMidPhase* MidPhase)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	if (!MidPhase->IsValid())
	{
		return;
	}

	FChaosVDParticlePairMidPhase CVDMidPhase = FChaosVDDataWrapperUtils::BuildMidPhaseDataWrapperFromMidPhase(*MidPhase);
	CVDMidPhase.SolverID = CVDContextData->Id;

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, CVDMidPhase);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDParticlePairMidPhase::WrapperTypeName);
}

void FChaosVisualDebuggerTrace::TraceMidPhasesFromCollisionConstraints(Chaos::FPBDCollisionConstraints& InCollisionConstraints)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	InCollisionConstraints.GetConstraintAllocator().VisitMidPhases([CopyContext = *CVDContextData](const Chaos::FParticlePairMidPhase& MidPhase) -> Chaos::ECollisionVisitorResult
	{
		CVD_SCOPE_CONTEXT(CopyContext)
		CVD_TRACE_MID_PHASE(&MidPhase);
		return Chaos::ECollisionVisitorResult::Continue;
	});
}

void FChaosVisualDebuggerTrace::TraceJointsConstraints(Chaos::FPBDJointConstraints& InJointConstraints)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	const Chaos::FPBDJointConstraints::FHandles& JointHandles = InJointConstraints.GetConstConstraintHandles();

	ParallelFor(JointHandles.Num(), [&JointHandles, CopyContext = *CVDContextData](int32 ConstraintIndex)
	{
		CVD_SCOPE_CONTEXT(CopyContext);
		
		FChaosVDJointConstraint WrappedJointConstraintData = FChaosVDDataWrapperUtils::BuildJointDataWrapper(JointHandles[ConstraintIndex]);

		WrappedJointConstraintData.SolverID = CopyContext.Id;

		FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
		Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, WrappedJointConstraintData);

		TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDJointConstraint::WrapperTypeName);
	});
}

void FChaosVisualDebuggerTrace::TraceCollisionConstraint(const Chaos::FPBDCollisionConstraint* CollisionConstraint)
{
	using namespace Chaos::VisualDebugger::Utils;
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	FChaosVDConstraint CVDConstraint = FChaosVDDataWrapperUtils::BuildConstraintDataWrapperFromConstraint(*CollisionConstraint);
	CVDConstraint.SolverID = CVDContextData->Id;

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, CVDConstraint);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDConstraint::WrapperTypeName);
}

void FChaosVisualDebuggerTrace::TraceCollisionConstraintView(TArrayView<Chaos::FPBDCollisionConstraint* const> CollisionConstraintView)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);

	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	ParallelFor(CollisionConstraintView.Num(), [&CollisionConstraintView, CopyContext = *CVDContextData](int32 ConstraintIndex)
	{
		CVD_SCOPE_CONTEXT(CopyContext);
		TraceCollisionConstraint(CollisionConstraintView[ConstraintIndex]);
	});
}

void FChaosVisualDebuggerTrace::TraceConstraintsContainer(TConstArrayView<Chaos::FPBDConstraintContainer*> ConstraintContainersView)
{
	if (!IsTracing())
	{
		return;
	}

	for (Chaos::FPBDConstraintContainer* ConstraintContainer : ConstraintContainersView)
	{
		if (ConstraintContainer)
		{
			if (ConstraintContainer->GetConstraintHandleType().IsA(Chaos::FPBDJointConstraintHandle::StaticType()))
			{
				CVD_TRACE_JOINT_CONSTRAINTS(CVDDC_JointConstraints, *static_cast<Chaos::FPBDJointConstraints*>(ConstraintContainer));
			}
			else if (ConstraintContainer->GetConstraintHandleType().IsA(Chaos::FPBDCollisionConstraint::StaticType()))
			{
				CVD_TRACE_STEP_MID_PHASES_FROM_COLLISION_CONSTRAINTS(CVDDC_EndOfEvolutionCollisionConstraints, *static_cast<Chaos::FPBDCollisionConstraints*>(ConstraintContainer));
			}
		}
	}
}

void FChaosVisualDebuggerTrace::TraceSolverFrameStart(const FChaosVDContext& ContextData, const FString& InDebugName)
{
	if (!IsTracing())
	{
		return;
	}

	if (!ensure(ContextData.Id != INDEX_NONE))
	{
		return;
	}

	if (!ensure(ContextData.Type == static_cast<int32>(EChaosVDContextType::Solver)))
	{
		return;
	}

	FChaosVDThreadContext::Get().PushContext(ContextData);

	bool bIsReSimulatedFrame = EnumHasAnyFlags(static_cast<EChaosVDContextAttributes>(ContextData.Attributes), EChaosVDContextAttributes::Resimulated);

	// Check if we need to do a full capture for this solver, and setup accordingly
	bool bOutIsFullCaptureRequested;
	SetupForFullCaptureIfNeeded(ContextData.Id, bOutIsFullCaptureRequested);

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverFrameStart, ChaosVDChannel)
		<< ChaosVDSolverFrameStart.SolverID(ContextData.Id)
		<< ChaosVDSolverFrameStart.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverFrameStart.DebugName(*InDebugName, InDebugName.Len())
		<< ChaosVDSolverFrameStart.IsKeyFrame(bOutIsFullCaptureRequested)
		<< ChaosVDSolverFrameStart.IsReSimulated(bIsReSimulatedFrame);
}

void FChaosVisualDebuggerTrace::TraceSolverFrameEnd(const FChaosVDContext& ContextData)
{
	if (!IsTracing())
	{
		return;
	}

	FChaosVDThreadContext::Get().PopContext();

	if (!ensure(ContextData.Id != INDEX_NONE))
	{
		return;
	}

	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
		if (!SolverIDsForDeltaRecording.Contains(ContextData.Id))
		{
			SolverIDsForDeltaRecording.Add(ContextData.Id);
		}
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverFrameEnd, ChaosVDChannel)
		<< ChaosVDSolverFrameEnd.SolverID(ContextData.Id)
		<< ChaosVDSolverFrameEnd.Cycle(FPlatformTime::Cycles64());
}

void FChaosVisualDebuggerTrace::TraceSolverStepStart(FStringView StepName)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverStepStart, ChaosVDChannel)
		<< ChaosVDSolverStepStart.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverStepStart.SolverID(CVDContextData->Id)
		<< ChaosVDSolverStepStart.StepName(StepName.GetData(), GetNum(StepName));
}

void FChaosVisualDebuggerTrace::TraceSolverStepEnd()
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverStepEnd, ChaosVDChannel)
		<< ChaosVDSolverStepEnd.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverStepEnd.SolverID(CVDContextData->Id);
}

void FChaosVisualDebuggerTrace::TraceSolverSimulationSpace(const Chaos::FRigidTransform3& Transform)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext(EChaosVDContextType::Solver);
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}
	
	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverSimulationSpace, ChaosVDChannel)
		<< ChaosVDSolverSimulationSpace.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverSimulationSpace.SolverID(CVDContextData->Id)
		<< CVD_TRACE_VECTOR_ON_EVENT(ChaosVDSolverSimulationSpace, Position, Transform.GetLocation())
		<< CVD_TRACE_ROTATOR_ON_EVENT(ChaosVDSolverSimulationSpace, Rotation, Transform.GetRotation());
}

void FChaosVisualDebuggerTrace::TraceBinaryData(TConstArrayView<uint8> InData, FStringView TypeName, EChaosVDTraceBinaryDataOptions Options)
{
	if (!IsTracing() && !EnumHasAnyFlags(Options, EChaosVDTraceBinaryDataOptions::ForceTrace))
	{
		return;
	}

	//TODO: This might overflow
	static FThreadSafeCounter LastDataID;

	const int32 DataID = LastDataID.Increment();

	ensure(DataID < TNumericLimits<int32>::Max());

	TConstArrayView<uint8> DataViewToTrace = InData;

	// Handle Compression if enabled
	const bool bIsCompressed = Chaos::VisualDebugger::Cvars::bCompressBinaryData;
	TArray<uint8> CompressedData;
	if (bIsCompressed)
	{
		CompressedData.Reserve(CompressedData.Num());
		FOodleCompressedArray::CompressData(CompressedData, InData.GetData(),InData.Num(), FOodleDataCompression::ECompressor::Kraken,
			static_cast<FOodleDataCompression::ECompressionLevel>(Chaos::VisualDebugger::Cvars::CompressionMode));

		DataViewToTrace = CompressedData;
	}

	const uint32 DataSize = static_cast<uint32>(DataViewToTrace.Num());
	constexpr uint32 MaxChunkSize = TNumericLimits<uint16>::Max();
	const uint32 ChunkNum = (DataSize + MaxChunkSize - 1) / MaxChunkSize;

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDBinaryDataStart, ChaosVDChannel)
		<< ChaosVDBinaryDataStart.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDBinaryDataStart.TypeName(TypeName.GetData(), TypeName.Len())
		<< ChaosVDBinaryDataStart.DataID(DataID)
		<< ChaosVDBinaryDataStart.DataSize(DataSize)
		<< ChaosVDBinaryDataStart.OriginalSize(InData.Num())
		<< ChaosVDBinaryDataStart.IsCompressed(bIsCompressed);

	uint32 RemainingSize = DataSize;
	for (uint32 Index = 0; Index < ChunkNum; ++Index)
	{
		const uint16 Size = static_cast<uint16>(FMath::Min(RemainingSize, MaxChunkSize));
		const uint8* ChunkData = DataViewToTrace.GetData() + MaxChunkSize * Index;

		UE_TRACE_LOG(ChaosVDLogger, ChaosVDBinaryDataContent, ChaosVDChannel)
			<< ChaosVDBinaryDataContent.Cycle(FPlatformTime::Cycles64())
			<< ChaosVDBinaryDataContent.DataID(DataID)
			<< ChaosVDBinaryDataContent.RawData(ChunkData, Size);

		RemainingSize -= Size;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDBinaryDataEnd, ChaosVDChannel)
		<< ChaosVDBinaryDataEnd.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDBinaryDataEnd.DataID(DataID);

	ensure(RemainingSize == 0);
}

void FChaosVisualDebuggerTrace::TraceImplicitObject(FChaosVDImplicitObjectWrapper WrappedGeometryData)
{
	if (!IsTracing())
	{
		return;
	}

	uint32 GeometryID = WrappedGeometryData.Hash;
	{
		FReadScopeLock ReadLock(GeometryTracerObject.TracedGeometrySetLock);
		if (GeometryTracerObject.GeometryTracedIDs.Find(GeometryID))
		{
			return;
		}
	}

	{
		FWriteScopeLock WriteLock(GeometryTracerObject.TracedGeometrySetLock);
		GeometryTracerObject.GeometryTracedIDs.Add(GeometryID);
	}

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer<FChaosVDImplicitObjectWrapper, Chaos::FChaosArchive>(TLSDataBuffer.BufferRef, WrappedGeometryData);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDImplicitObjectWrapper::WrapperTypeName);
}

void FChaosVisualDebuggerTrace::InvalidateGeometryFromCache(const Chaos::FImplicitObject* CachedGeometryToInvalidate)
{
	if (!IsTracing())
	{
		return;
	}

	GeometryTracerObject.RemoveCachedGeometryHash(CachedGeometryToInvalidate);
}

void FChaosVisualDebuggerTrace::TraceNonSolverLocation(const FVector& InLocation, FStringView DebugNameID)
{
	if (!IsTracing())
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDNonSolverLocation, ChaosVDChannel)
			<< ChaosVDNonSolverLocation.Cycle(FPlatformTime::Cycles64())
			<< CVD_TRACE_VECTOR_ON_EVENT(ChaosVDNonSolverLocation, Position, InLocation)
			<< ChaosVDNonSolverLocation.DebugName(DebugNameID.GetData(), DebugNameID.Len());
}

void FChaosVisualDebuggerTrace::TraceNonSolverTransform(const FTransform& InTransform, FStringView DebugNameID)
{
	if (!IsTracing())
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDNonSolverTransform, ChaosVDChannel)
		<< ChaosVDNonSolverTransform.Cycle(FPlatformTime::Cycles64())
		<< CVD_TRACE_VECTOR_ON_EVENT(ChaosVDNonSolverTransform, Position, InTransform.GetLocation())
		<< CVD_TRACE_VECTOR_ON_EVENT(ChaosVDNonSolverTransform, Scale, InTransform.GetScale3D())
		<< CVD_TRACE_ROTATOR_ON_EVENT(ChaosVDNonSolverTransform, Rotation, InTransform.GetRotation())
		<< ChaosVDNonSolverTransform.DebugName(DebugNameID.GetData(), DebugNameID.Len());
}

void FChaosVisualDebuggerTrace::TraceSceneQueryStart(const Chaos::FImplicitObject* InputGeometry, const FQuat& GeometryOrientation,  const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, FChaosVDCollisionQueryParams&& Params, FChaosVDCollisionResponseParams&& ResponseParams, FChaosVDCollisionObjectQueryParams&& ObjectParams, EChaosVDSceneQueryType QueryType, EChaosVDSceneQueryMode QueryMode, int32 SolverID, bool bIsRetry)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}

	const bool bIsQueryContext = CVDContextData->Type == static_cast<int32>(EChaosVDContextType::Query) ||  CVDContextData->Type == static_cast<int32>(EChaosVDContextType::SubTraceQuery);

	if (!ensure(bIsQueryContext))
	{
		return;
	}

	FChaosVDQueryDataWrapper WrappedQueryData;

	if (InputGeometry)
	{
		const uint32 GeometryHash = GeometryTracerObject.GetGeometryHashForImplicit(InputGeometry);
		TraceImplicitObject({ GeometryHash, const_cast<Chaos::FImplicitObject*>(InputGeometry) });
	
		WrappedQueryData.InputGeometryKey = GeometryHash;
	}
	
	WrappedQueryData.ID = CVDContextData->Id;
	WrappedQueryData.ParentQueryID = CVDContextData->OwnerID;
	WrappedQueryData.WorldSolverID = SolverID;
	WrappedQueryData.bIsRetryQuery = bIsRetry;
	
	WrappedQueryData.GeometryOrientation = GeometryOrientation;

	WrappedQueryData.CollisionChannel = TraceChannel;
	WrappedQueryData.StartLocation = Start;
	WrappedQueryData.EndLocation = End;

	WrappedQueryData.CollisionQueryParams = MoveTemp(Params);
	WrappedQueryData.CollisionResponseParams = MoveTemp(ResponseParams);
	WrappedQueryData.CollisionObjectQueryParams = MoveTemp(ObjectParams);

	WrappedQueryData.Mode = QueryMode;
	WrappedQueryData.Type = QueryType;

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, WrappedQueryData);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDQueryDataWrapper::WrapperTypeName);
}

void FChaosVisualDebuggerTrace::TraceSceneQueryVisit(FChaosVDQueryVisitStep&& InQueryVisitData)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!IsContextEnabledAndValid(CVDContextData))
	{
		return;
	}
	const bool bIsQueryContext = CVDContextData->Type == static_cast<int32>(EChaosVDContextType::Query) ||  CVDContextData->Type == static_cast<int32>(EChaosVDContextType::SubTraceQuery);

	if (!ensure(bIsQueryContext))
	{
		return;
	}

	InQueryVisitData.OwningQueryID = CVDContextData->Id;

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, InQueryVisitData);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDQueryVisitStep::WrapperTypeName);
}

bool FChaosVisualDebuggerTrace::IsTracing()
{
	return bIsTracing;
}

void FChaosVisualDebuggerTrace::RegisterEventHandlers()
{
	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);

		
		if (!RecordingStartedDelegateHandle.IsValid())
		{
			RecordingStartedDelegateHandle = FChaosVDRuntimeModule::RegisterRecordingStartedCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateStatic(&FChaosVisualDebuggerTrace::HandleRecordingStart));
		}

		if (!RecordingStoppedDelegateHandle.IsValid())
		{
			RecordingStoppedDelegateHandle = FChaosVDRuntimeModule::RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateStatic(&FChaosVisualDebuggerTrace::HandleRecordingStop));
		}

		if (!RecordingFullCaptureRequestedHandle.IsValid())
		{
			RecordingFullCaptureRequestedHandle = FChaosVDRuntimeModule::RegisterFullCaptureRequestedCallback(FChaosVDCaptureRequestDelegate::FDelegate::CreateStatic(&FChaosVisualDebuggerTrace::PerformFullCapture));
		}
	}
}

void FChaosVisualDebuggerTrace::UnregisterEventHandlers()
{
	FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
	if (RecordingStartedDelegateHandle.IsValid())
	{
		FChaosVDRuntimeModule::RemoveRecordingStartedCallback(RecordingStartedDelegateHandle);
	}

	if (RecordingStoppedDelegateHandle.IsValid())
	{
		FChaosVDRuntimeModule::RemoveRecordingStopCallback(RecordingStoppedDelegateHandle);
	}

	if (RecordingFullCaptureRequestedHandle.IsValid())
	{
		FChaosVDRuntimeModule::RemoveFullCaptureRequestedCallback(RecordingFullCaptureRequestedHandle);
	}

	bIsTracing = false;
}

void FChaosVisualDebuggerTrace::Reset()
{
	CVDNameTable->ResetTable();

	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
		RequestedFullCaptureSolverIDs.Reset();
		SolverIDsForDeltaRecording.Reset();
	}

	{
		FWriteScopeLock GeometryWriteLock(GeometryTracerObject.TracedGeometrySetLock);
		GeometryTracerObject.GeometryTracedIDs.Reset();

		FWriteScopeLock GeometryHashWriteLock(GeometryTracerObject.CachedGeometryHashesLock);
		GeometryTracerObject.CachedGeometryHashes.Reset();
	}
}

void FChaosVisualDebuggerTrace::HandleRecordingStop()
{
	bIsTracing = false;
	Reset();
}

void FChaosVisualDebuggerTrace::TraceArchiveHeader()
{
	using namespace Chaos::VisualDebugger;

	TArray<uint8> HeaderDataBuffer;

	FMemoryWriter MemWriterAr(HeaderDataBuffer);

	FChaosVDArchiveHeader::Current().Serialize(MemWriterAr);

	// We intentionally trace the header when the recording start was requested but we are not in a tracing state
	// So we need to force a trace
	// We do this to ensure the header is traced before any other binary data is generated, as we will need it to be read first on load 
	TraceBinaryData(HeaderDataBuffer, FChaosVDArchiveHeader::WrapperTypeName, EChaosVDTraceBinaryDataOptions::ForceTrace);
}

void FChaosVisualDebuggerTrace::HandleRecordingStart()
{
	Reset();

	FString CommandlineEnabledCVDChannels;
	constexpr bool bStopOnSeparator = false;
	if (FParse::Value(FCommandLine::Get(), TEXT("CVDDataChannelsOverride="), CommandlineEnabledCVDChannels, bStopOnSeparator))
	{
		TArray<FString> ParsedChannels;
		Chaos::VisualDebugger::ParseChannelListFromCommandArgument(ParsedChannels, CommandlineEnabledCVDChannels);

		UE_LOG(LogChaos, Log, TEXT("[%s] Channel list override provided via commandline - Enabling [%d] Requested channels..."), ANSI_TO_TCHAR(__FUNCTION__), ParsedChannels.Num());

		using namespace Chaos::VisualDebugger;
		FChaosVDDataChannelsManager::Get().EnumerateChannels([&ParsedChannels](const TSharedRef<FChaosVDOptionalDataChannel>& Channel)
		{
			if (Channel->CanChangeEnabledState())
			{
				// This is far from efficient, but this will be called once when the recording start command is executed, and we only have a handful of channels
				const FString ChannelIdAsString = Channel->GetId().ToString();
				const bool bChannelShouldBeEnabled = ParsedChannels.Contains(ChannelIdAsString);
				Channel->SetChannelEnabled(bChannelShouldBeEnabled);

				UE_LOG(LogChaos, Log, TEXT("[%s] Setting enabled state for channel [%s] to [%s]..."), ANSI_TO_TCHAR(__FUNCTION__), *ChannelIdAsString, bChannelShouldBeEnabled ? TEXT("True") : TEXT("False"));
			}
			return true;
		});
	}

	TraceArchiveHeader();

	bIsTracing = true;
}

void FChaosVisualDebuggerTrace::PerformFullCapture(EChaosVDFullCaptureFlags CaptureOptions)
{
	if (EnumHasAnyFlags(CaptureOptions, EChaosVDFullCaptureFlags::Particles))
	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
		RequestedFullCaptureSolverIDs.Append(SolverIDsForDeltaRecording);
	}

	if (EnumHasAnyFlags(CaptureOptions, EChaosVDFullCaptureFlags::Geometry))
	{
		FWriteScopeLock GeometryWriteLock(GeometryTracerObject.TracedGeometrySetLock);
		GeometryTracerObject.GeometryTracedIDs.Reset();
	}
}

#endif //WITH_CHAOS_VISUAL_DEBUGGER
