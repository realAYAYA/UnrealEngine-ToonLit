// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"

#if CHAOS_VISUAL_DEBUGGER_ENABLED

#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "ChaosVisualDebugger/ChaosVDDataWrapperUtils.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/MemoryWriter.h"
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

static FAutoConsoleVariable CVarChaosVDCompressBinaryData(
	TEXT("p.Chaos.VD.CompressBinaryData"),
	false,
	TEXT("If true, serialized binary data will be compressed using Oodle on the fly before being traced"));

static FAutoConsoleVariable CVarChaosVDCompressionMode(
	TEXT("p.Chaos.VD.CompressionMode"),
	2,
	TEXT("Oodle compression mode to use, 4 is by default which equsals to ECompressionLevel::VeryFast"));

/** Struct where we keep track of the geometry we are tracing */
struct FChaosVDGeometryTraceContext
{
	FRWLock TracedGeometrySetLock;
	FRWLock CachedGeometryHashesLock;
	TSet<uint32> GeometryTracedIDs;

	uint32 GetGeometryHashForImplicit(const Chaos::FImplicitObject* Implicit)
	{
		if (!ensure(Implicit != nullptr))
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

		FWriteScopeLock WriteLock(TracedGeometrySetLock);
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
FThreadSafeBool FChaosVisualDebuggerTrace::bIsTracing = false;

void FChaosVisualDebuggerTrace::TraceParticle(const Chaos::FGeometryParticleHandle* ParticleHandle)
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

	const uint32 GeometryHash = GeometryTracerObject.GetGeometryHashForImplicit(ParticleHandle->Geometry().Get());
	
	TraceImplicitObject({ GeometryHash, ParticleHandle->Geometry() });

	{
		FChaosVDParticleDataWrapper ParticleDataWrapper = FChaosVDDataWrapperUtils::BuildParticleDataWrapperFromParticle(ParticleHandle);
		ParticleDataWrapper.GeometryHash = GeometryHash;
		ParticleDataWrapper.SolverID = ContextData.Id;
		
		FChaosVDScopedTLSBufferAccessor TLSDataBuffer;

		FMemoryWriter MemWriterAr(TLSDataBuffer.BufferRef);
		MemWriterAr.SetShouldSkipUpdateCustomVersion(true);
		MemWriterAr.SetUseUnversionedPropertySerialization(true);

		ParticleDataWrapper.Serialize(MemWriterAr);

		TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDParticleDataWrapper::WrapperTypeName);
	}
}

void FChaosVisualDebuggerTrace::TraceParticles(const Chaos::TGeometryParticleHandles<Chaos::FReal, 3>& ParticleHandles)
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

	ParallelFor(ParticleHandles.Size(),[&ParticleHandles, CopyContext = *CVDContextData](int32 ParticleIndex)
	{
		CVD_SCOPE_CONTEXT(CopyContext)
		TraceParticle(ParticleHandles.Handle(ParticleIndex).Get(), CopyContext);
	});
}

void FChaosVisualDebuggerTrace::TraceParticleDestroyed(const Chaos::FGeometryParticleHandle* ParticleHandle)
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

	GeometryTracerObject.RemoveCachedGeometryHash(ParticleHandle->Geometry().Get());
	
	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!ensure(CVDContextData))
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
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!ensure(CVDContextData))
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
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!ensure(CVDContextData))
	{
		return;
	}

	FChaosVDParticlePairMidPhase CVDMidPhase = FChaosVDDataWrapperUtils::BuildMidPhaseDataWrapperFromMidPhase(*MidPhase);
	CVDMidPhase.SolverID = CVDContextData->Id;

	FChaosVDScopedTLSBufferAccessor CVDBuffer;
	FMemoryWriter MemWriter(CVDBuffer.BufferRef);
		
	CVDMidPhase.Serialize(MemWriter);

	TraceBinaryData(CVDBuffer.BufferRef, FChaosVDParticlePairMidPhase::WrapperTypeName);
}


void FChaosVisualDebuggerTrace::TraceMidPhasesFromCollisionConstraints(Chaos::FPBDCollisionConstraints& InCollisionConstraints)
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

	InCollisionConstraints.GetConstraintAllocator().VisitMidPhases([CopyContext = *CVDContextData](const Chaos::FParticlePairMidPhase& MidPhase) -> Chaos::ECollisionVisitorResult
	{
		CVD_SCOPE_CONTEXT(CopyContext)
		CVD_TRACE_MID_PHASE(&MidPhase);
		return Chaos::ECollisionVisitorResult::Continue;
	});
}


void FChaosVisualDebuggerTrace::TraceCollisionConstraint(const Chaos::FPBDCollisionConstraint* CollisionConstraint)
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

	FChaosVDConstraint CVDConstraint = FChaosVDDataWrapperUtils::BuildConstraintDataWrapperFromConstraint(*CollisionConstraint);
	CVDConstraint.SolverID = CVDContextData->Id;

	FChaosVDScopedTLSBufferAccessor CVDBuffer;
	FMemoryWriter MemWriter(CVDBuffer.BufferRef);
		
	CVDConstraint.Serialize(MemWriter);

	TraceBinaryData(CVDBuffer.BufferRef, FChaosVDConstraint::WrapperTypeName);
}

void FChaosVisualDebuggerTrace::TraceCollisionConstraintView(TArrayView<Chaos::FPBDCollisionConstraint* const> CollisionConstraintView)
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

	ParallelFor(CollisionConstraintView.Num(), [&CollisionConstraintView, CopyContext = *CVDContextData](int32 ConstraintIndex)
	{
		CVD_SCOPE_CONTEXT(CopyContext);
		TraceCollisionConstraint(CollisionConstraintView[ConstraintIndex]);
	});
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

	FChaosVDThreadContext::Get().PushContext(ContextData);

	// Check if we need to do a full capture for this solver, and setup accordingly
	bool bOutIsFullCaptureRequested;
	SetupForFullCaptureIfNeeded(ContextData.Id, bOutIsFullCaptureRequested);

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverFrameStart, ChaosVDChannel)
		<< ChaosVDSolverFrameStart.SolverID(ContextData.Id)
		<< ChaosVDSolverFrameStart.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverFrameStart.DebugName(*InDebugName, InDebugName.Len())
		<< ChaosVDSolverFrameStart.IsKeyFrame(bOutIsFullCaptureRequested);
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
	if (!IsTracing())
	{
		return;
	}

	const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext();
	if (!ensure(CVDContextData))
	{
		return;
	}

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverStepStart, ChaosVDChannel)
		<< ChaosVDSolverStepStart.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverStepStart.SolverID(CVDContextData->Id)
		<< ChaosVDSolverStepStart.StepName(StepName.GetData(), StepName.Len());
}

void FChaosVisualDebuggerTrace::TraceSolverStepEnd()
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

	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverStepEnd, ChaosVDChannel)
		<< ChaosVDSolverStepEnd.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverStepEnd.SolverID(CVDContextData->Id);
}

void FChaosVisualDebuggerTrace::TraceSolverSimulationSpace(const Chaos::FRigidTransform3& Transform)
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
	
	UE_TRACE_LOG(ChaosVDLogger, ChaosVDSolverSimulationSpace, ChaosVDChannel)
		<< ChaosVDSolverSimulationSpace.Cycle(FPlatformTime::Cycles64())
		<< ChaosVDSolverSimulationSpace.SolverID(CVDContextData->Id)
		<< CVD_TRACE_VECTOR_ON_EVENT(ChaosVDSolverSimulationSpace, Position, Transform.GetLocation())
		<< CVD_TRACE_ROTATOR_ON_EVENT(ChaosVDSolverSimulationSpace, Rotation, Transform.GetRotation());
}

void FChaosVisualDebuggerTrace::TraceBinaryData(const TArray<uint8>& InData, FStringView TypeName)
{
	if (!IsTracing())
	{
		return;
	}

	//TODO: This might overflow
	static FThreadSafeCounter LastDataID;

	const int32 DataID = LastDataID.Increment();

	ensure(DataID < TNumericLimits<int32>::Max());

	const TArray<uint8>* DataToTrace = &InData;

	// Handle Compression if enabled
	const bool bIsCompressed = CVarChaosVDCompressBinaryData->GetBool();
	TArray<uint8> CompressedData;
	if (bIsCompressed)
	{
		CompressedData.Reserve(CompressedData.Num());
		FOodleCompressedArray::CompressTArray(CompressedData, InData, FOodleDataCompression::ECompressor::Kraken,
			static_cast<FOodleDataCompression::ECompressionLevel>(CVarChaosVDCompressionMode->GetInt()));

		DataToTrace = &CompressedData;
	}

	const uint32 DataSize = static_cast<uint32>(DataToTrace->Num());
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
		const uint8* ChunkData = DataToTrace->GetData() + MaxChunkSize * Index;

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

	FMemoryWriter MemWriterAr(TLSDataBuffer.BufferRef);
	Chaos::FChaosArchive Ar(MemWriterAr);

	Ar.SetShouldSkipUpdateCustomVersion(true);

	WrappedGeometryData.Serialize(Ar);

	TraceBinaryData(TLSDataBuffer.BufferRef, FChaosVDImplicitObjectWrapper::WrapperTypeName);
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
	{
		FWriteScopeLock WriteLock(DeltaRecordingStatesLock);
		RequestedFullCaptureSolverIDs.Reset();
		SolverIDsForDeltaRecording.Reset();
	}

	{
		FWriteScopeLock GeometryWriteLock(GeometryTracerObject.TracedGeometrySetLock);
		GeometryTracerObject.GeometryTracedIDs.Reset();
		GeometryTracerObject.CachedGeometryHashes.Reset();
	}
}

void FChaosVisualDebuggerTrace::HandleRecordingStop()
{
	bIsTracing = false;
	Reset();
}

void FChaosVisualDebuggerTrace::HandleRecordingStart()
{
	Reset();
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

#endif
