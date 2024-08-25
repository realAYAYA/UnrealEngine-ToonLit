// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/MorphTargetVertexInfoBuffers.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Animation/MorphTarget.h"
#include "DataDrivenShaderPlatformInfo.h"

extern int32 GSkinCacheRecomputeTangents;

FMorphTargetVertexInfoBuffers::FMorphTargetVertexInfoBuffers() = default;
FMorphTargetVertexInfoBuffers::~FMorphTargetVertexInfoBuffers() = default;

void FMorphTargetVertexInfoBuffers::InitRHI(FRHICommandListBase& RHICmdList)
{
	SCOPED_LOADTIMER(FFMorphTargetVertexInfoBuffers_InitRHI);

	check(NumTotalBatches > 0);
	check(!bRHIInitialized);

	const static FLazyName ClassName(TEXT("FMorphTargetVertexInfoBuffers"));

	const uint32 BufferSize = MorphData.Num() * sizeof(uint32);
	FRHIResourceCreateInfo CreateInfo(TEXT("MorphData"));
	CreateInfo.ClassName = ClassName;
	CreateInfo.OwnerName = GetOwnerName();
	MorphDataBuffer = RHICmdList.CreateStructuredBuffer(sizeof(uint32), BufferSize, BUF_Static | BUF_ByteAddressBuffer | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
	MorphDataBuffer->SetOwnerName(GetOwnerName());
	
	void* BufferPtr = RHICmdList.LockBuffer(MorphDataBuffer, 0, BufferSize, RLM_WriteOnly);
	FMemory::ParallelMemcpy(BufferPtr, MorphData.GetData(), BufferSize, EMemcpyCachePolicy::StoreUncached);
	RHICmdList.UnlockBuffer(MorphDataBuffer);
	MorphDataSRV = RHICmdList.CreateShaderResourceView(MorphDataBuffer);

	if (bEmptyMorphCPUDataOnInitRHI)
	{
		MorphData.Empty();
		bIsMorphCPUDataValid = false;
	}

	bRHIInitialized = true;
}

void FMorphTargetVertexInfoBuffers::ReleaseRHI()
{
	MorphDataBuffer.SafeRelease();
	MorphDataSRV.SafeRelease();
	bRHIInitialized = false;
}

uint32 FMorphTargetVertexInfoBuffers::GetMaximumThreadGroupSize()
{
	//D3D11 there can be at most 65535 Thread Groups in each dimension of a Dispatch call.
	uint64 MaximumThreadGroupSize = uint64(GMaxComputeDispatchDimension) * 32ull;
	return uint32(FMath::Min<uint64>(MaximumThreadGroupSize, UINT32_MAX));
}

const float FMorphTargetVertexInfoBuffers::CalculatePositionPrecision(float TargetPositionErrorTolerance)
{
	const float UnrealUnitPerMeter = 100.0f;
	return TargetPositionErrorTolerance * 2.0f * 1e-6f * UnrealUnitPerMeter;	// * 2.0 because correct rounding guarantees error is at most half of the cell size.
}

void FMorphTargetVertexInfoBuffers::ResetCPUData()
{
	MorphData.Empty();
	MaximumValuePerMorph.Empty();
	MinimumValuePerMorph.Empty();
	BatchStartOffsetPerMorph.Empty();
	BatchesPerMorph.Empty();
	NumTotalBatches = 0;
	PositionPrecision = 0.0f;
	TangentZPrecision = 0.0f;
	bResourcesInitialized = false;
	bIsMorphCPUDataValid = false;
}

void FMorphTargetVertexInfoBuffers::ValidateVertexBuffers(bool bMorphTargetsShouldBeValid)
{
#if DO_CHECK
	check(BatchesPerMorph.Num() == BatchStartOffsetPerMorph.Num());
	check(BatchesPerMorph.Num() == MaximumValuePerMorph.Num());
	check(BatchesPerMorph.Num() == MinimumValuePerMorph.Num());

	if (bMorphTargetsShouldBeValid)
	{
		if (NumTotalBatches > 0)
		{
			check(MorphData.Num() > 0);
		}
		else
		{
			check(MorphData.Num() == 0);
		}
	}
	
#endif //DO_CHECK
}

void FMorphTargetVertexInfoBuffers::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		check(bResourcesInitialized);
		check(bIsMorphCPUDataValid);
		ValidateVertexBuffers(true);
	}
	else if(Ar.IsLoading())
	{
		ResetCPUData();
	}

	Ar << MorphData
	   << MinimumValuePerMorph
	   << MaximumValuePerMorph
	   << BatchStartOffsetPerMorph
	   << BatchesPerMorph
	   << NumTotalBatches
	   << PositionPrecision
	   << TangentZPrecision;

	if (Ar.IsLoading())
	{
		bRHIInitialized = false;
		bIsMorphCPUDataValid = true;
		bResourcesInitialized = true;
		ValidateVertexBuffers(true);
	}
}

FArchive& operator<<(FArchive& Ar, FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers)
{
	MorphTargetVertexInfoBuffers.Serialize(Ar);
	return Ar;
}

class FDwordBitWriter
{
public:
	FDwordBitWriter(TArray<uint32>& Buffer) :
		Buffer(Buffer),
		PendingBits(0ull),
		NumPendingBits(0)
	{
	}

	void PutBits(uint32 Bits, uint32 NumBits)
	{
		check((uint64)Bits < (1ull << NumBits));
		PendingBits |= (uint64)Bits << NumPendingBits;
		NumPendingBits += NumBits;

		while (NumPendingBits >= 32)
		{
			Buffer.Add((uint32)PendingBits);
			PendingBits >>= 32;
			NumPendingBits -= 32;
		}
	}

	void Flush()
	{
		if (NumPendingBits > 0)
			Buffer.Add((uint32)PendingBits);
		PendingBits = 0;
		NumPendingBits = 0;
	}

private:
	TArray<uint32>& Buffer;
	uint64 			PendingBits;
	int32 			NumPendingBits;
};

void FMorphTargetVertexInfoBuffers::InitMorphResources(EShaderPlatform ShaderPlatform, const TArray<FSkelMeshRenderSection>& RenderSections, const TArray<UMorphTarget*>& MorphTargets, int NumVertices, int32 LODIndex, float TargetPositionErrorTolerance)
{
	check(!IsRHIInitialized());
	check(!IsMorphResourcesInitialized());
	check(!IsMorphCPUDataValid());

	bResourcesInitialized = true;

	// UseGPUMorphTargets() can be toggled only on SM5 atm
	if (!IsPlatformShaderSupported(ShaderPlatform) || MorphTargets.Num() == 0)
	{
		return;
	}

	bIsMorphCPUDataValid = true;

	// Simple Morph compression 0.1
	// Instead of storing vertex deltas individually they are organized into batches of 64.
	// Each batch has a header that describes how many bits are allocated to each of the vertex components.
	// Batches also store an explicit offset to its associated data. This makes it trivial to decode batches
	// in parallel, and because deltas are fixed-width inside a batch, deltas can also be decoded in parallel.
	// The result is a semi-adaptive encoding that functions as a crude substitute for entropy coding, that is
	// fast to decode on parallel hardware.

	// Quantization still happens globally to avoid issues with cracks at duplicate vertices.
	// The quantization is artist controlled on a per LOD basis. Higher error tolerance results in smaller deltas
	// and a smaller compressed size.

	PositionPrecision = CalculatePositionPrecision(TargetPositionErrorTolerance);
	const float RcpPositionPrecision = 1.0f / PositionPrecision;

	TangentZPrecision = 1.0f / 2048.0f;				// Object scale irrelevant here. Let's assume ~12bits per component is plenty.
	const float RcpTangentZPrecision = 1.0f / TangentZPrecision;

	const uint32 BatchSize = 64;
	const uint32 NumBatchHeaderDwords = 10u;

	const uint32 IndexMaxBits = 31u;

	const uint32 PositionMaxBits = 28u;				// Probably more than we need, but let's just allow it to go this high to be safe for now.
													// For larger deltas this can even be more precision than what was in the float input data!
													// Maybe consider float-like or exponential encoding of large values?
	const float PositionMinValue = -134217728.0f;	// -2^(MaxBits-1)
	const float PositionMaxValue = 134217720.0f;	// Largest float smaller than 2^(MaxBits-1)-1
													// Using 134217727.0f would NOT work as it would be rounded up to 134217728.0f, which is
													// outside the range.

	const uint32 TangentZMaxBits = 16u;
	const float TangentZMinValue = -32768.0f;		// -2^(MaxBits-1)
	const float TangentZMaxValue = 32767.0f;		//  2^(MaxBits-1)-1

	struct FBatchHeader
	{
		uint32		DataOffset;
		uint32		NumElements;
		bool		bTangents;

		uint32		IndexBits;
		FIntVector	PositionBits;
		FIntVector	TangentZBits;

		uint32		IndexMin;
		FIntVector	PositionMin;
		FIntVector	TangentZMin;
	};

	// uint32 StartTime = FPlatformTime::Cycles();

	MorphData.Empty();
	NumTotalBatches = 0;

	BatchStartOffsetPerMorph.Empty(MorphTargets.Num());
	BatchesPerMorph.Empty(MorphTargets.Num());
	MaximumValuePerMorph.Empty(MorphTargets.Num());
	MinimumValuePerMorph.Empty(MorphTargets.Num());

	// Mark vertices that are in a section that doesn't recompute tangents as needing tangents
	const int32 RecomputeTangentsMode = GSkinCacheRecomputeTangents;
	TBitArray<> VertexNeedsTangents;
	VertexNeedsTangents.Init(false, NumVertices);
	for (const FSkelMeshRenderSection& RenderSection : RenderSections)
	{
		const bool bRecomputeTangents = RecomputeTangentsMode > 0 && (RenderSection.bRecomputeTangent || RecomputeTangentsMode == 1);
		if (!bRecomputeTangents)
		{
			for (uint32 i = 0; i < RenderSection.NumVertices; i++)
			{
				VertexNeedsTangents[RenderSection.BaseVertexIndex + i] = true;
			}
		}
	}

	// Populate the arrays to be filled in later in the render thread
	TArray<FBatchHeader> BatchHeaders;
	TArray<uint32> BitstreamData;
	for (int32 AnimIdx = 0; AnimIdx < MorphTargets.Num(); ++AnimIdx)
	{
		uint32 BatchStartOffset = NumTotalBatches;

		float MaximumValues[4] = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
		float MinimumValues[4] = { +FLT_MAX, +FLT_MAX, +FLT_MAX, +FLT_MAX };
		UMorphTarget* MorphTarget = MorphTargets[AnimIdx];
		int32 NumSrcDeltas = 0;
		const FMorphTargetDelta* MorphDeltas = MorphTarget->GetMorphTargetDelta(LODIndex, NumSrcDeltas);

		if (NumSrcDeltas == 0 || !MorphTarget->UsesBuiltinMorphTargetCompression())
		{
			MaximumValues[0] = 0.0f;
			MaximumValues[1] = 0.0f;
			MaximumValues[2] = 0.0f;
			MaximumValues[3] = 0.0f;

			MinimumValues[0] = 0.0f;
			MinimumValues[1] = 0.0f;
			MinimumValues[2] = 0.0f;
			MinimumValues[3] = 0.0f;
		}
		else
		{
			struct FQuantizedDelta
			{
				FIntVector	Position;
				FIntVector	TangentZ;
				uint32		Index;
			};
			TArray<FQuantizedDelta> QuantizedDeltas;
			QuantizedDeltas.Reserve(NumSrcDeltas);

			bool bVertexIndicesSorted = true;

			int32 PrevVertexIndex = -1;
			for (int32 DeltaIndex = 0; DeltaIndex < NumSrcDeltas; DeltaIndex++)
			{
				const auto& MorphDelta = MorphDeltas[DeltaIndex];
				const FVector3f TangentZDelta = (VertexNeedsTangents.IsValidIndex(MorphDelta.SourceIdx) && VertexNeedsTangents[MorphDelta.SourceIdx]) ? MorphDelta.TangentZDelta : FVector3f::ZeroVector;

				// when import, we do check threshold, and also when adding weight, we do have threshold for how smaller weight can fit in
				// so no reason to check here another threshold
				MaximumValues[0] = FMath::Max(MaximumValues[0], MorphDelta.PositionDelta.X);
				MaximumValues[1] = FMath::Max(MaximumValues[1], MorphDelta.PositionDelta.Y);
				MaximumValues[2] = FMath::Max(MaximumValues[2], MorphDelta.PositionDelta.Z);
				MaximumValues[3] = FMath::Max(MaximumValues[3], FMath::Max(TangentZDelta.X, FMath::Max(TangentZDelta.Y, TangentZDelta.Z)));

				MinimumValues[0] = FMath::Min(MinimumValues[0], MorphDelta.PositionDelta.X);
				MinimumValues[1] = FMath::Min(MinimumValues[1], MorphDelta.PositionDelta.Y);
				MinimumValues[2] = FMath::Min(MinimumValues[2], MorphDelta.PositionDelta.Z);
				MinimumValues[3] = FMath::Min(MinimumValues[3], FMath::Min(TangentZDelta.X, FMath::Min(TangentZDelta.Y, TangentZDelta.Z)));

				// Check if input is sorted. It usually is, but it might not be.
				if ((int32)MorphDelta.SourceIdx < PrevVertexIndex)
					bVertexIndicesSorted = false;
				PrevVertexIndex = (int32)MorphDelta.SourceIdx;

				// Quantize delta
				FQuantizedDelta QuantizedDelta;
				const FVector3f& PositionDelta = MorphDelta.PositionDelta;
				QuantizedDelta.Position.X = FMath::RoundToInt(FMath::Clamp(PositionDelta.X * RcpPositionPrecision, PositionMinValue, PositionMaxValue));
				QuantizedDelta.Position.Y = FMath::RoundToInt(FMath::Clamp(PositionDelta.Y * RcpPositionPrecision, PositionMinValue, PositionMaxValue));
				QuantizedDelta.Position.Z = FMath::RoundToInt(FMath::Clamp(PositionDelta.Z * RcpPositionPrecision, PositionMinValue, PositionMaxValue));
				QuantizedDelta.TangentZ.X = FMath::RoundToInt(FMath::Clamp(TangentZDelta.X * RcpTangentZPrecision, TangentZMinValue, TangentZMaxValue));
				QuantizedDelta.TangentZ.Y = FMath::RoundToInt(FMath::Clamp(TangentZDelta.Y * RcpTangentZPrecision, TangentZMinValue, TangentZMaxValue));
				QuantizedDelta.TangentZ.Z = FMath::RoundToInt(FMath::Clamp(TangentZDelta.Z * RcpTangentZPrecision, TangentZMinValue, TangentZMaxValue));
				QuantizedDelta.Index = MorphDelta.SourceIdx;

				if (QuantizedDelta.Position != FIntVector::ZeroValue || QuantizedDelta.TangentZ != FIntVector::ZeroValue)
				{
					// Only add delta if it is non-zero
					QuantizedDeltas.Add(QuantizedDelta);
				}
			}

			// Sort deltas if the source wasn't already sorted
			if (!bVertexIndicesSorted)
			{
				Algo::Sort(QuantizedDeltas, [](const FQuantizedDelta& A, const FQuantizedDelta& B) { return A.Index < B.Index; });
			}

			// Encode batch deltas
			const uint32 MorphNumBatches = (QuantizedDeltas.Num() + BatchSize - 1u) / BatchSize;
			for (uint32 BatchIndex = 0; BatchIndex < MorphNumBatches; BatchIndex++)
			{
				const uint32 BatchFirstElementIndex = BatchIndex * BatchSize;
				const uint32 NumElements = FMath::Min(BatchSize, QuantizedDeltas.Num() - BatchFirstElementIndex);

				// Calculate batch min/max bounds
				uint32 IndexMin = MAX_uint32;
				uint32 IndexMax = MIN_uint32;
				FIntVector PositionMin = FIntVector(MAX_int32);
				FIntVector PositionMax = FIntVector(MIN_int32);
				FIntVector TangentZMin = FIntVector(MAX_int32);
				FIntVector TangentZMax = FIntVector(MIN_int32);

				for (uint32 LocalElementIndex = 0; LocalElementIndex < NumElements; LocalElementIndex++)
				{
					const FQuantizedDelta& Delta = QuantizedDeltas[BatchFirstElementIndex + LocalElementIndex];

					// Trick: Deltas are sorted by index, so the index increase by at least one per delta.
					//		  Naively this would mean that a batch always spans at least 64 index values and
					//		  indices would have to use at least 6 bits per index.
					//		  If instead of storing the raw index, we store the index relative to its position in the batch,
					//		  then the spanned range becomes 63 smaller.
					//		  For a consecutive range this even gets us down to 0 bits per index!
					check(Delta.Index >= LocalElementIndex);
					const uint32 AdjustedIndex = Delta.Index - LocalElementIndex;
					IndexMin = FMath::Min(IndexMin, AdjustedIndex);
					IndexMax = FMath::Max(IndexMax, AdjustedIndex);

					PositionMin.X = FMath::Min(PositionMin.X, Delta.Position.X);
					PositionMin.Y = FMath::Min(PositionMin.Y, Delta.Position.Y);
					PositionMin.Z = FMath::Min(PositionMin.Z, Delta.Position.Z);

					PositionMax.X = FMath::Max(PositionMax.X, Delta.Position.X);
					PositionMax.Y = FMath::Max(PositionMax.Y, Delta.Position.Y);
					PositionMax.Z = FMath::Max(PositionMax.Z, Delta.Position.Z);

					TangentZMin.X = FMath::Min(TangentZMin.X, Delta.TangentZ.X);
					TangentZMin.Y = FMath::Min(TangentZMin.Y, Delta.TangentZ.Y);
					TangentZMin.Z = FMath::Min(TangentZMin.Z, Delta.TangentZ.Z);

					TangentZMax.X = FMath::Max(TangentZMax.X, Delta.TangentZ.X);
					TangentZMax.Y = FMath::Max(TangentZMax.Y, Delta.TangentZ.Y);
					TangentZMax.Z = FMath::Max(TangentZMax.Z, Delta.TangentZ.Z);
				}

				const uint32 IndexDelta = IndexMax - IndexMin;
				const FIntVector PositionDelta = PositionMax - PositionMin;
				const FIntVector TangentZDelta = TangentZMax - TangentZMin;
				const bool bBatchHasTangents = TangentZMin != FIntVector::ZeroValue || TangentZMax != FIntVector::ZeroValue;

				FBatchHeader BatchHeader;
				BatchHeader.DataOffset = BitstreamData.Num() * sizeof(uint32);
				BatchHeader.bTangents = bBatchHasTangents;
				BatchHeader.NumElements = NumElements;
				BatchHeader.IndexBits = FMath::CeilLogTwo(IndexDelta + 1);
				BatchHeader.PositionBits.X = FMath::CeilLogTwo(uint32(PositionDelta.X) + 1);
				BatchHeader.PositionBits.Y = FMath::CeilLogTwo(uint32(PositionDelta.Y) + 1);
				BatchHeader.PositionBits.Z = FMath::CeilLogTwo(uint32(PositionDelta.Z) + 1);
				BatchHeader.TangentZBits.X = FMath::CeilLogTwo(uint32(TangentZDelta.X) + 1);
				BatchHeader.TangentZBits.Y = FMath::CeilLogTwo(uint32(TangentZDelta.Y) + 1);
				BatchHeader.TangentZBits.Z = FMath::CeilLogTwo(uint32(TangentZDelta.Z) + 1);
				check(BatchHeader.IndexBits <= IndexMaxBits);
				check(BatchHeader.PositionBits.X <= PositionMaxBits);
				check(BatchHeader.PositionBits.Y <= PositionMaxBits);
				check(BatchHeader.PositionBits.Z <= PositionMaxBits);
				check(BatchHeader.TangentZBits.X <= TangentZMaxBits);
				check(BatchHeader.TangentZBits.Y <= TangentZMaxBits);
				check(BatchHeader.TangentZBits.Z <= TangentZMaxBits);
				BatchHeader.IndexMin = IndexMin;
				BatchHeader.PositionMin = PositionMin;
				BatchHeader.TangentZMin = TangentZMin;

				// Write quantized bits
				FDwordBitWriter BitWriter(BitstreamData);
				for (uint32 LocalElementIndex = 0; LocalElementIndex < NumElements; LocalElementIndex++)
				{
					const FQuantizedDelta& Delta = QuantizedDeltas[BatchFirstElementIndex + LocalElementIndex];
					const uint32 AdjustedIndex = Delta.Index - LocalElementIndex;
					BitWriter.PutBits(AdjustedIndex - IndexMin, BatchHeader.IndexBits);
					BitWriter.PutBits(uint32(Delta.Position.X - PositionMin.X), BatchHeader.PositionBits.X);
					BitWriter.PutBits(uint32(Delta.Position.Y - PositionMin.Y), BatchHeader.PositionBits.Y);
					BitWriter.PutBits(uint32(Delta.Position.Z - PositionMin.Z), BatchHeader.PositionBits.Z);
					if (bBatchHasTangents)
					{
						BitWriter.PutBits(uint32(Delta.TangentZ.X - TangentZMin.X), BatchHeader.TangentZBits.X);
						BitWriter.PutBits(uint32(Delta.TangentZ.Y - TangentZMin.Y), BatchHeader.TangentZBits.Y);
						BitWriter.PutBits(uint32(Delta.TangentZ.Z - TangentZMin.Z), BatchHeader.TangentZBits.Z);
					}
				}
				BitWriter.Flush();

				BatchHeaders.Add(BatchHeader);
			}
			NumTotalBatches += MorphNumBatches;
		}

		const uint32 MorphNumBatches = NumTotalBatches - BatchStartOffset;
		BatchStartOffsetPerMorph.Add(BatchStartOffset);
		BatchesPerMorph.Add(MorphNumBatches);
		MaximumValuePerMorph.Add(FVector4f(MaximumValues[0], MaximumValues[1], MaximumValues[2], MaximumValues[3]));
		MinimumValuePerMorph.Add(FVector4f(MinimumValues[0], MinimumValues[1], MinimumValues[2], MinimumValues[3]));
	}

	// Write packed batch headers
	for (const FBatchHeader& BatchHeader : BatchHeaders)
	{
		const uint32 DataOffset = BatchHeader.DataOffset + BatchHeaders.Num() * NumBatchHeaderDwords * sizeof(uint32);

		MorphData.Add(DataOffset);
		MorphData.Add(BatchHeader.IndexBits |
			(BatchHeader.PositionBits.X << 5) | (BatchHeader.PositionBits.Y << 10) | (BatchHeader.PositionBits.Z << 15) |
			(BatchHeader.bTangents ? (1u << 20) : 0u) |
			(BatchHeader.NumElements << 21));
		MorphData.Add(BatchHeader.IndexMin);
		MorphData.Add(BatchHeader.PositionMin.X);
		MorphData.Add(BatchHeader.PositionMin.Y);
		MorphData.Add(BatchHeader.PositionMin.Z);

		MorphData.Add(BatchHeader.TangentZBits.X | (BatchHeader.TangentZBits.Y << 5) | (BatchHeader.TangentZBits.Z << 10));
		MorphData.Add(BatchHeader.TangentZMin.X);
		MorphData.Add(BatchHeader.TangentZMin.Y);
		MorphData.Add(BatchHeader.TangentZMin.Z);
	}

	// Append bitstream data
	MorphData.Append(BitstreamData);
	
	if(MorphData.Num() > 0)
	{
		// Pad to make sure it is always safe to access the data with load4s.
		MorphData.Add(0u);
		MorphData.Add(0u);
		MorphData.Add(0u);
	}

	// UE_LOG(LogStaticMesh, Log, TEXT("Morph compression time: [%.2fs]"), FPlatformTime::ToMilliseconds(FPlatformTime::Cycles() - StartTime) / 1000.0f);

	ValidateVertexBuffers(true);
}

bool FMorphTargetVertexInfoBuffers::IsPlatformShaderSupported(EShaderPlatform ShaderPlatform)
{
	return IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM5);
}
