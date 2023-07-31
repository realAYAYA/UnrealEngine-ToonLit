// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sampling/MeshMapBaker.h"
#include "Sampling/MeshBakerCommon.h"
#include "Sampling/MeshMapBakerQueue.h"
#include "Image/ImageOccupancyMap.h"
#include "Image/ImageTile.h"
#include "Selections/MeshConnectedComponents.h"
#include "ProfilingDebugging/ScopedTimers.h"

using namespace UE::Geometry;

//
// FMeshMapBaker
//

static constexpr float BoxFilterRadius = 0.5f;
static constexpr float BCFilterRadius = 0.769f;

FBoxFilter FMeshMapBaker::BoxFilter(BoxFilterRadius);
FBSplineFilter FMeshMapBaker::BSplineFilter(BCFilterRadius);
FMitchellNetravaliFilter FMeshMapBaker::MitchellNetravaliFilter(BCFilterRadius);


void FMeshMapBaker::InitBake()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::InitBake);
	
	// Retrieve evaluation contexts and cache:
	// - index lists of accumulation modes (BakeAccumulateLists)
	// - evaluator to bake result offsets (BakeOffsets)
	// - buffer size per sample (BakeSampleBufferSize)
	const int32 NumBakers = Bakers.Num();
	BakeContexts.SetNum(NumBakers);
	BakeOffsets.SetNumUninitialized(NumBakers + 1);
	BakeAccumulateLists.SetNum(static_cast<int32>(FMeshMapEvaluator::EAccumulateMode::Last));
	BakeSampleBufferSize = 0;
	int32 Offset = 0;
	for (int32 Idx = 0; Idx < NumBakers; ++Idx)
	{
		Bakers[Idx]->Setup(*this, BakeContexts[Idx]);
		checkSlow(BakeContexts[Idx].Evaluate != nullptr && BakeContexts[Idx].EvaluateDefault != nullptr);
		checkSlow(BakeContexts[Idx].DataLayout.Num() > 0);
		const int32 NumData = BakeContexts[Idx].DataLayout.Num();
		for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
		{
			BakeSampleBufferSize += static_cast<int32>(BakeContexts[Idx].DataLayout[DataIdx]);
		}
		BakeOffsets[Idx] = Offset;
		Offset += NumData;
		BakeAccumulateLists[static_cast<int32>(BakeContexts[Idx].AccumulateMode)].Add(Idx);
	}
	BakeOffsets[NumBakers] = Offset;

	// Initialize our BakeResults list and cache offsets into the sample buffer
	// per bake result
	const int32 NumResults = Offset;
	BakeResults.SetNum(NumResults);
	BakeSampleOffsets.SetNumUninitialized(NumResults + 1);
	int32 SampleOffset = 0;
	for (int32 Idx = 0; Idx < NumBakers; ++Idx)
	{
		const int32 NumData = BakeContexts[Idx].DataLayout.Num();
		for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
		{
			const int32 ResultIdx = BakeOffsets[Idx] + DataIdx;
			BakeResults[ResultIdx] = MakeUnique<TImageBuilder<FVector4f>>();
			BakeResults[ResultIdx]->SetDimensions(Dimensions);

			BakeSampleOffsets[ResultIdx] = SampleOffset;
			const int32 NumFloats = static_cast<int32>(BakeContexts[Idx].DataLayout[DataIdx]);
			SampleOffset += NumFloats;
		}
	}
	BakeSampleOffsets[NumResults] = SampleOffset;

	InitBakeDefaults();

	for (int32 Idx = 0; Idx < NumResults; ++Idx)
	{
		BakeResults[Idx]->Clear(BakeDefaultColors[Idx]);
	}

	InitFilter();

	// Compute UV charts if null or invalid.
	if (!TargetMeshUVCharts || !ensure(TargetMeshUVCharts->Num() == TargetMesh->TriangleCount()))
	{
		ComputeUVCharts(*TargetMesh, TargetMeshUVChartsLocal);
		TargetMeshUVCharts = &TargetMeshUVChartsLocal;
	}
}

void FMeshMapBaker::InitBakeDefaults()
{
	// Cache default float buffer and colors for each bake result.
	checkSlow(BakeSampleBufferSize > 0);
	BakeDefaults.SetNumUninitialized(BakeSampleBufferSize);
	float* Buffer = BakeDefaults.GetData();
	float* BufferPtr = Buffer;

	const int32 NumBakers = Bakers.Num();
	for (int32 Idx = 0; Idx < NumBakers; ++Idx)
	{
		BakeContexts[Idx].EvaluateDefault(BufferPtr, BakeContexts[Idx].EvalData);
	}
	checkSlow((BufferPtr - Buffer) == BakeSampleBufferSize);

	BufferPtr = Buffer;
	const int32 NumBakeResults = BakeResults.Num();
	BakeDefaultColors.SetNumUninitialized(NumBakeResults);
	for (int32 Idx = 0; Idx < NumBakers; ++Idx)
	{
		const FMeshMapEvaluator::FEvaluationContext& Context = BakeContexts[Idx];
		const int32 NumData = Context.DataLayout.Num();
		for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
		{
			const int32 ResultIdx = BakeOffsets[Idx] + DataIdx;
			Context.EvaluateColor(DataIdx, BufferPtr, BakeDefaultColors[ResultIdx], Context.EvalData);
		}
	}
	checkSlow((BufferPtr - Buffer) == BakeSampleBufferSize);
}

void FMeshMapBaker::Bake()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake);

	BakeAnalytics.Reset();
	FScopedDurationTimer TotalBakeTimer(BakeAnalytics.TotalBakeDuration);
	
	if (Bakers.IsEmpty() || !TargetMesh)
	{
		return;
	}

	InitBake();

	const FDynamicMesh3* Mesh = TargetMesh;
	const FDynamicMeshUVOverlay* UVOverlay = GetTargetMeshUVs();
	const FDynamicMeshNormalOverlay* NormalOverlay = GetTargetMeshNormals();

	{
		// Generate UV space mesh
		TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_CreateUVMesh);
		
		FlatMesh = FDynamicMesh3(EMeshComponents::FaceGroups);
		for (const int32 TriId : Mesh->TriangleIndicesItr())
		{
			if (UVOverlay->IsSetTriangle(TriId))
			{
				FVector2f A, B, C;
				UVOverlay->GetTriElements(TriId, A, B, C);
				const int32 VertA = FlatMesh.AppendVertex(FVector3d(A.X, A.Y, 0));
				const int32 VertB = FlatMesh.AppendVertex(FVector3d(B.X, B.Y, 0));
				const int32 VertC = FlatMesh.AppendVertex(FVector3d(C.X, C.Y, 0));
				/*int32 NewTriID =*/ FlatMesh.AppendTriangle(VertA, VertB, VertC, TriId);
			}
		}
	}

	ECorrespondenceStrategy UseStrategy = this->CorrespondenceStrategy;
	bool bIsIdentity = true;
	int NumDetailMeshes = 0;
	auto CheckIdentity = [this, Mesh, &bIsIdentity, &NumDetailMeshes](const void* DetailMesh)
	{
		// When the mesh pointers differ, loosely compare the meshes as a sanity check.
		// TODO: Expose additional comparison metrics on the detail sampler when the mesh pointers differ.
		bIsIdentity = bIsIdentity && (DetailMesh == Mesh || Mesh->TriangleCount() == DetailSampler->GetTriangleCount(DetailMesh));
		++NumDetailMeshes;
	};
	DetailSampler->ProcessMeshes(CheckIdentity);
	if (UseStrategy == ECorrespondenceStrategy::Identity && !ensure(bIsIdentity && NumDetailMeshes == 1))
	{
		// Identity strategy requires there to be only one mesh that is the same
		// as the target mesh. 
		UseStrategy = ECorrespondenceStrategy::NearestPoint;
	}

	// Computes the correspondence sample assuming the SampleInfo is valid
	// Returns true if the correspondence is valid and false otherwise
	auto ComputeCorrespondenceSample
		= [Mesh, NormalOverlay, UseStrategy, this](const FMeshUVSampleInfo& SampleInfo, FMeshMapEvaluator::FCorrespondenceSample& ValueOut)
	{
		NormalOverlay->GetTriBaryInterpolate<double>(SampleInfo.TriangleIndex, &SampleInfo.BaryCoords.X, &ValueOut.BaseNormal.X);
		Normalize(ValueOut.BaseNormal);

		ValueOut.BaseSample = SampleInfo;
		ValueOut.DetailMesh = nullptr;
		ValueOut.DetailTriID = FDynamicMesh3::InvalidID;

		if (UseStrategy == ECorrespondenceStrategy::Identity && DetailSampler->SupportsIdentityCorrespondence())
		{
			ValueOut.DetailMesh = Mesh;
			ValueOut.DetailTriID = SampleInfo.TriangleIndex;
			ValueOut.DetailBaryCoords = SampleInfo.BaryCoords;
		}
		else if (UseStrategy == ECorrespondenceStrategy::NearestPoint && DetailSampler->SupportsNearestPointCorrespondence())
		{
			ValueOut.DetailMesh = GetDetailMeshTrianglePoint_Nearest(DetailSampler, SampleInfo.SurfacePoint,
				ValueOut.DetailTriID, ValueOut.DetailBaryCoords);
		}
		else if (UseStrategy == ECorrespondenceStrategy::Custom && DetailSampler->SupportsCustomCorrespondence())
		{
			ValueOut.DetailMesh = DetailSampler->ComputeCustomCorrespondence(SampleInfo, ValueOut);
		}
		else	// Fall back to raycast strategy
		{
			checkSlow(DetailSampler->SupportsRaycastCorrespondence());
			
			const double SampleThickness = this->GetProjectionDistance();		// could modulate w/ a map here...

			// Find detail mesh triangle point
			const FVector3d RayDir = ValueOut.BaseNormal;
			ValueOut.DetailMesh = GetDetailMeshTrianglePoint_Raycast(DetailSampler, SampleInfo.SurfacePoint, RayDir,
				ValueOut.DetailTriID, ValueOut.DetailBaryCoords, SampleThickness,
				(UseStrategy == ECorrespondenceStrategy::RaycastStandardThenNearest));
		}

		return DetailSampler->IsValidCorrespondence(ValueOut);
	};

	// This computes a FMeshUVSampleInfo to pass to the ComputeCorrespondenceSample function, which will find the
	// correspondence between the target surface and detail surface.
	MeshUVSampler.Initialize(Mesh, UVOverlay, EMeshSurfaceSamplerQueryType::TriangleAndUV);

	// Create a temporary output float buffer for the full image dimensions.
	const FImageTile FullImageTile(FVector2i(0,0), FVector2i(Dimensions.GetWidth(), Dimensions.GetHeight()));
	FMeshMapTileBuffer FullImageTileBuffer(FullImageTile, BakeSampleBufferSize);

	// Tile the image
	FImageTiling Tiles(Dimensions, TileSize, TileSize);
	const int32 NumTiles = Tiles.Num();
	TArray<TArray64<TTuple<int64, int64>>> GutterTexelsPerTile;
	GutterTexelsPerTile.SetNum(NumTiles);

	// WriteToOutputBuffer transfers local tile data (TileBuffer) to the image output buffer (FullImageTileBuffer).
	auto WriteToOutputBuffer = [this, &FullImageTileBuffer] (FMeshMapTileBuffer& TileBufferIn, const FImageTile& TargetTile, const TArray<int32>& EvaluatorIds, auto&& Op, auto&& WeightOp)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_WriteToOutputBuffer);
		
		const int TargetTileWidth = TargetTile.GetWidth();
		const int TargetTileHeight = TargetTile.GetHeight();
		for (FVector2i TileCoords(0,0); TileCoords.Y < TargetTileHeight; ++TileCoords.Y)
		{
			for (TileCoords.X = 0; TileCoords.X < TargetTileWidth; ++TileCoords.X)
			{
				if (CancelF())
				{
					return; // WriteToOutputBuffer
				}

				const FVector2i ImageCoords = TargetTile.GetSourceCoords(TileCoords);

				const int64 ImageLinearIdx = Dimensions.GetIndex(ImageCoords);
				float& ImagePixelWeight = FullImageTileBuffer.GetPixelWeight(ImageLinearIdx);
				float* ImagePixelBuffer = FullImageTileBuffer.GetPixel(ImageLinearIdx);

				const FImageTile& BufferTile = TileBufferIn.GetTile();

				const int64 TilePixelLinearIdx = BufferTile.GetIndexFromSourceCoords(ImageCoords);
				const float& TilePixelWeight = TileBufferIn.GetPixelWeight(TilePixelLinearIdx);
				float* TilePixelBuffer = TileBufferIn.GetPixel(TilePixelLinearIdx);

				WeightOp(TilePixelWeight, ImagePixelWeight);
				for( int32 Idx : EvaluatorIds )
				{
					const FMeshMapEvaluator::FEvaluationContext& Context = BakeContexts[Idx];
					const int32 NumData = Context.DataLayout.Num();
					const int32 ResultOffset = BakeOffsets[Idx];
					for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
					{
						const int32 ResultIdx = ResultOffset + DataIdx;
						const int32 Offset = BakeSampleOffsets[ResultIdx];
						float* BufferPtr = &TilePixelBuffer[Offset];
						float* ImageBufferPtr = &ImagePixelBuffer[Offset];

						const int32 NumFloats = static_cast<int32>(Context.DataLayout[DataIdx]);
						for (int32 FloatIdx = 0; FloatIdx < NumFloats; ++FloatIdx)
						{
							Op(BufferPtr[FloatIdx], ImageBufferPtr[FloatIdx]);
						}
					}
				}
			}
		}
	};

	auto WriteToOutputBufferQueued = [this, &WriteToOutputBuffer](FMeshMapBakerQueue& Queue)
	{
		constexpr auto AddFn = [](const float& In, float& Out)
		{
			Out += In;
		};

		if (Queue.AcquireProcessLock())
		{
			void* OutputData = Queue.Process();
			while (OutputData)
			{
				FMeshMapTileBuffer* TileBufferPtr = static_cast<FMeshMapTileBuffer*>(OutputData);
				WriteToOutputBuffer(*TileBufferPtr, TileBufferPtr->GetTile(), EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Add), AddFn, AddFn);
				delete TileBufferPtr;
				OutputData = Queue.Process();
			}
			Queue.ReleaseProcessLock();
		}
	};

	FMeshMapBakerQueue OutputQueue(NumTiles);
	ParallelFor(NumTiles, [this, &Tiles, &GutterTexelsPerTile, &OutputQueue, &WriteToOutputBuffer, &WriteToOutputBufferQueued, &ComputeCorrespondenceSample](int32 TileIdx)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_EvalTile);

		if (CancelF())
		{
			return; // ParallelFor
		}
		
		// Generate unpadded and padded tiles.
		const FImageTile Tile = Tiles.GetTile(TileIdx);	// Image area to sample
		const FImageTile PaddedTile = Tiles.GetTile(TileIdx, TilePadding); // Filtered image area

		FImageOccupancyMap OccupancyMap;
		OccupancyMap.GutterSize = GutterSize;
		OccupancyMap.Initialize(Dimensions, PaddedTile, SamplesPerPixel);
		OccupancyMap.ComputeFromUVSpaceMesh(FlatMesh, [this](int32 TriangleID) { return FlatMesh.GetTriangleGroup(TriangleID); }, TargetMeshUVCharts);
		GutterTexelsPerTile[TileIdx] = OccupancyMap.GutterTexels;

		const int64 NumTilePixels = Tile.Num();
		for (int64 TilePixelIdx = 0; TilePixelIdx < NumTilePixels; ++TilePixelIdx)
		{
			const FVector2i SourceCoords = Tile.GetSourceCoords(TilePixelIdx);
			const int64 OccupancyMapIdx = OccupancyMap.Tile.GetIndexFromSourceCoords(SourceCoords);
			BakeAnalytics.NumSamplePixels += OccupancyMap.TexelInteriorSamples[OccupancyMapIdx];; 
		}

		FMeshMapTileBuffer* TileBuffer = new FMeshMapTileBuffer(PaddedTile, BakeSampleBufferSize);

		{
			// Evaluate valid/interior samples
			TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_EvalTileSamples);
			
			const int TileWidth = Tile.GetWidth();
			const int TileHeight = Tile.GetHeight();
			const int32 NumSamples = OccupancyMap.PixelSampler.Num();
			for (FVector2i TileCoords(0,0); TileCoords.Y < TileHeight; ++TileCoords.Y)
			{
				for (TileCoords.X = 0; TileCoords.X < TileWidth; ++TileCoords.X)
				{
					if (CancelF())
					{
						delete TileBuffer;
						return; // ParallelFor
					}

					const FVector2i ImageCoords = Tile.GetSourceCoords(TileCoords);
					const int64 OccupancyMapLinearIdx = OccupancyMap.Tile.GetIndexFromSourceCoords(ImageCoords);
					if (OccupancyMap.TexelNumSamples(OccupancyMapLinearIdx) == 0)
					{
						continue;
					}

					// Iterate over all the samples in the pixel
					for (int32 SampleIdx = 0; SampleIdx < NumSamples; ++SampleIdx)
					{
						const int64 LinearIdx = OccupancyMapLinearIdx * NumSamples + SampleIdx;
						if (OccupancyMap.IsInterior(LinearIdx))
						{
							const FVector2d UVPosition = (FVector2d)OccupancyMap.TexelQueryUV[LinearIdx];
							const int32 UVTriangleID = OccupancyMap.TexelQueryTriangle[LinearIdx];

							// Compute the per-sample correspondence data 
							// Note: Since we check LinearIdx is an interior sample above we know we'll get a valid
							// SampleInfo because interior samples all have valid UVTriangleIDs.
							FMeshUVSampleInfo SampleInfo;
							if (MeshUVSampler.QuerySampleInfo(UVTriangleID, UVPosition, SampleInfo))
							{
								FMeshMapEvaluator::FCorrespondenceSample Sample;
								bool bSampleValid = ComputeCorrespondenceSample(SampleInfo, Sample);
								if (bSampleValid)
								{
									BakeSample(*TileBuffer, Sample, UVPosition, ImageCoords, OccupancyMap);
								}
								InteriorSampleCallback(bSampleValid, Sample, UVPosition, ImageCoords);
							}
						}
					}
				}
			}
		}

		constexpr auto NoopFn = [](const float& In, float& Out)
		{
		};

		constexpr auto OverwriteFn = [](const float& In, float& Out)
		{
			Out = In;
		};

		// Transfer 'Overwrite' float data to image tile buffer
		WriteToOutputBuffer(*TileBuffer, Tile, EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Overwrite), OverwriteFn, NoopFn);

		// Accumulate 'Add' float data to image tile buffer
		OutputQueue.Post(TileIdx, TileBuffer);
		WriteToOutputBufferQueued(OutputQueue);
	}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	if (CancelF())
	{
		// If cancelled, delete any outstanding tile buffers in the queue.
		while (!OutputQueue.IsDone())
		{
			void* Data = OutputQueue.Process</*bFlush*/ true>();
			if (Data)
			{
				const FMeshMapTileBuffer* TileBuffer = static_cast<FMeshMapTileBuffer*>(Data);
				delete TileBuffer;
			}
		}

		return;
	}

	{
		// The queue only acquires the process lock if the next item in the queue
		// is ready. This could mean that there are potential leftovers in the queue
		// after the parallel for. Write them out now.
		WriteToOutputBufferQueued(OutputQueue);
	}
	
	if (CancelF())
	{
		return;
	}

	{
		FScopedDurationTimer WriteToImageTimer(BakeAnalytics.WriteToImageDuration);
		
		// Normalize and convert ImageTileBuffer data to color data.
		ParallelFor(NumTiles, [this, &Tiles, &FullImageTileBuffer](int32 TileIdx)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_WriteToImageBuffer);
		
			const FImageTile Tile = Tiles.GetTile(TileIdx);
			const int TileWidth = Tile.GetWidth();
			const int TileHeight = Tile.GetHeight();
			for (FVector2i TileCoords(0,0); TileCoords.Y < TileHeight; ++TileCoords.Y)
			{
				for (TileCoords.X = 0; TileCoords.X < TileWidth; ++TileCoords.X)
				{
					if (CancelF())
					{
						return; // ParallelFor
					}

					const FVector2i ImageCoords = Tile.GetSourceCoords(TileCoords);

					const int64 ImageLinearIdx = Dimensions.GetIndex(ImageCoords);
					const float& PixelWeight = FullImageTileBuffer.GetPixelWeight(ImageLinearIdx);
					float* PixelBuffer = FullImageTileBuffer.GetPixel(ImageLinearIdx);

					auto WriteToPixel = [this, &PixelBuffer, &ImageLinearIdx](const TArray<int32>& EvaluatorIds, float OneOverWeight)
					{
						for (const int32 Idx : EvaluatorIds)
						{
							const FMeshMapEvaluator::FEvaluationContext& Context = BakeContexts[Idx];
							const int32 NumData = Context.DataLayout.Num();
							const int32 ResultOffset = BakeOffsets[Idx];
							for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
							{
								const int32 ResultIdx = ResultOffset + DataIdx;
								const int32 Offset = BakeSampleOffsets[ResultIdx];
								float* BufferPtr = &PixelBuffer[Offset];

								// Apply weight to raw float data.
								const int32 NumFloats = static_cast<int32>(Context.DataLayout[DataIdx]);
								for (int32 FloatIdx = 0; FloatIdx < NumFloats; ++FloatIdx)
								{
									BufferPtr[FloatIdx] *= OneOverWeight;
								}

								// Convert float data to color.
								FVector4f& Pixel = BakeResults[ResultIdx]->GetPixel(ImageLinearIdx);
								Context.EvaluateColor(DataIdx, BufferPtr, Pixel, Context.EvalData);
							}
						}
					};
				
					if (PixelWeight > 0.0)
					{
						WriteToPixel(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Add), 1.0f / PixelWeight);
					}
					WriteToPixel(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Overwrite), 1.0f);
				}
			}
		}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	}

	if (CancelF())
	{
		return;
	}

	PostWriteToImageCallback(BakeResults);

	if (CancelF())
	{
		return;
	}

	// Gutter Texel processing
	if (bGutterEnabled)
	{
		FScopedDurationTimer WriteToGutterTimer(BakeAnalytics.WriteToGutterDuration);
		
		const int32 NumResults = BakeResults.Num();
		ParallelFor(NumTiles, [this, &NumResults, &GutterTexelsPerTile](int32 TileIdx)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMapBaker::Bake_WriteGutterPixels);

			if (CancelF())
			{
				return; // ParallelFor
			}

			const int64 NumGutter = GutterTexelsPerTile[TileIdx].Num();
			for (int64 GutterIdx = 0; GutterIdx < NumGutter; ++GutterIdx)
			{
				int64 GutterPixelTo;
				int64 GutterPixelFrom;
				Tie(GutterPixelTo, GutterPixelFrom) = GutterTexelsPerTile[TileIdx][GutterIdx];
				for (int32 Idx = 0; Idx < NumResults; Idx++)
				{
					BakeResults[Idx]->CopyPixel(GutterPixelFrom, GutterPixelTo);
				}
			}

			BakeAnalytics.NumGutterPixels += NumGutter;
		}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	}
}

// Precondition: Must be passed a valid Sample
void FMeshMapBaker::BakeSample(
	FMeshMapTileBuffer& TileBuffer,
	const FMeshMapEvaluator::FCorrespondenceSample& Sample,
	const FVector2d& UVPosition,
	const FVector2i& ImageCoords,
	const FImageOccupancyMap& OccupancyMap)
{
	// Evaluate each baker into stack allocated float buffer
	float* Buffer = static_cast<float*>(FMemory_Alloca(sizeof(float) * BakeSampleBufferSize));
	float* BufferPtr = Buffer;
	const int32 NumEvaluators = Bakers.Num();
	for (int32 Idx = 0; Idx < NumEvaluators; ++Idx)
	{
		BakeContexts[Idx].Evaluate(BufferPtr, Sample, BakeContexts[Idx].EvalData);
	}
	checkSlow((BufferPtr - Buffer) == BakeSampleBufferSize);

	const FImageTile& Tile = TileBuffer.GetTile();

	const int64 OccupancyMapSampleIdx = OccupancyMap.Tile.GetIndexFromSourceCoords(ImageCoords);
	const int32 SampleUVChart = OccupancyMap.TexelQueryUVChart[OccupancyMapSampleIdx];

	auto AddFn = [this, &ImageCoords, &UVPosition, &Tile, &TileBuffer, &OccupancyMap, SampleUVChart](const TArray<int32>& EvaluatorIds, const float* SourceBuffer, float Weight) -> void
	{
		const FVector2i BoxFilterStart(
			FMath::Clamp(ImageCoords.X - FilterKernelSize, 0, Dimensions.GetWidth()),
			FMath::Clamp(ImageCoords.Y - FilterKernelSize, 0, Dimensions.GetHeight())
			);
		const FVector2i BoxFilterEnd(
			FMath::Clamp(ImageCoords.X + FilterKernelSize + 1, 0, Dimensions.GetWidth()),
			FMath::Clamp(ImageCoords.Y + FilterKernelSize + 1, 0, Dimensions.GetHeight())
			);
		const FImageTile BoxFilterTile(BoxFilterStart, BoxFilterEnd);

		for (int64 FilterIdx = 0; FilterIdx < BoxFilterTile.Num(); FilterIdx++)
		{
			const FVector2i SourceCoords = BoxFilterTile.GetSourceCoords(FilterIdx);
			const int64 OccupancyMapFilterIdx = OccupancyMap.Tile.GetIndexFromSourceCoords(SourceCoords);
			const int32 BufferTilePixelUVChart = OccupancyMap.TexelQueryUVChart[OccupancyMapFilterIdx];

			// Get the weight and value buffers for this pixel
			const int64 BufferTilePixelLinearIdx = Tile.GetIndexFromSourceCoords(SourceCoords);
			float* PixelBuffer = TileBuffer.GetPixel(BufferTilePixelLinearIdx);
			float& PixelWeight = TileBuffer.GetPixelWeight(BufferTilePixelLinearIdx);

			// Compute the filter weight based on the UV distance from the pixel center to the sample position
			// Note: There will be no contribution if the sample and pixel are on different UV charts
			float FilterWeight = Weight * static_cast<float>(SampleUVChart == BufferTilePixelUVChart);
			{
				FVector2d TexelDistance = Dimensions.GetTexelUV(SourceCoords) - UVPosition;
				TexelDistance.X *= Dimensions.GetWidth();
				TexelDistance.Y *= Dimensions.GetHeight();
				FilterWeight *= TextureFilterEval(TexelDistance);
			}

			// Update the weight of this pixel
			PixelWeight += FilterWeight;

			// Update the value of this pixel for each evaluator
			for (const int32 Idx : EvaluatorIds)
			{
				const FMeshMapEvaluator::FEvaluationContext& Context = BakeContexts[Idx];
				const int32 NumData = Context.DataLayout.Num();
				const int32 ResultOffset = BakeOffsets[Idx];
				for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
				{
					const int32 ResultIdx = ResultOffset + DataIdx;
					const int32 Offset = BakeSampleOffsets[ResultIdx];

					const int32 NumFloats = static_cast<int32>(Context.DataLayout[DataIdx]);
					for (int32 FloatIdx = Offset; FloatIdx < Offset + NumFloats; ++FloatIdx)
					{
						PixelBuffer[FloatIdx] += SourceBuffer[FloatIdx] * FilterWeight;
					}
				}
			}
		}
	};

	auto OverwriteFn = [this, &ImageCoords, &Tile, &TileBuffer](const TArray<int32>& EvaluatorIds, const float* SourceBuffer) -> void
	{
		const int64 BufferTilePixelLinearIdx = Tile.GetIndexFromSourceCoords(ImageCoords); 
		float* PixelBuffer = TileBuffer.GetPixel(BufferTilePixelLinearIdx);
		
		for (const int32 Idx : EvaluatorIds)
		{
			const FMeshMapEvaluator::FEvaluationContext& Context = BakeContexts[Idx];
			const int32 NumData = Context.DataLayout.Num();
			const int32 ResultOffset = BakeOffsets[Idx];
			for (int32 DataIdx = 0; DataIdx < NumData; ++DataIdx)
			{
				const int32 ResultIdx = ResultOffset + DataIdx;
				const int32 Offset = BakeSampleOffsets[ResultIdx];
				
				const int32 NumFloats = static_cast<int32>(Context.DataLayout[DataIdx]);
				for (int32 FloatIdx = Offset; FloatIdx < Offset + NumFloats; ++FloatIdx)
				{
					PixelBuffer[FloatIdx] = SourceBuffer[FloatIdx];
				}
			}
		}
	};

	if (SampleFilterF)
	{
		const float SampleMaskWeight = FMath::Clamp(SampleFilterF(ImageCoords, UVPosition, Sample.BaseSample.TriangleIndex), 0.0f, 1.0f);
		AddFn(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Add), Buffer, SampleMaskWeight);
		AddFn(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Add), BakeDefaults.GetData(), 1.0f - SampleMaskWeight);
		OverwriteFn(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Overwrite), (SampleMaskWeight == 0) ? BakeDefaults.GetData() : Buffer);
	}
	else
	{
		AddFn(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Add), Buffer, 1.0f);
		OverwriteFn(EvaluatorIdsForMode(FMeshMapEvaluator::EAccumulateMode::Overwrite), Buffer);
	}
}

int32 FMeshMapBaker::AddEvaluator(const TSharedPtr<FMeshMapEvaluator, ESPMode::ThreadSafe>& Eval)
{
	return Bakers.Add(Eval);
}

FMeshMapEvaluator* FMeshMapBaker::GetEvaluator(const int32 EvalIdx) const
{
	return Bakers[EvalIdx].Get();
}

void FMeshMapBaker::Reset()
{
	Bakers.Empty();
	BakeResults.Empty();
}

int32 FMeshMapBaker::NumEvaluators() const
{
	return Bakers.Num();
}

const TArrayView<TUniquePtr<TImageBuilder<FVector4f>>> FMeshMapBaker::GetBakeResults(const int32 EvalIdx)
{
	const int32 ResultIdx = BakeOffsets[EvalIdx];
	const int32 NumResults = BakeOffsets[EvalIdx + 1] - ResultIdx;
	return TArrayView<TUniquePtr<TImageBuilder<FVector4f>>>(&BakeResults[ResultIdx], NumResults);
}

void FMeshMapBaker::SetDimensions(const FImageDimensions DimensionsIn)
{
	Dimensions = DimensionsIn;
}

void FMeshMapBaker::SetGutterEnabled(const bool bEnabled)
{
	bGutterEnabled = bEnabled;
}

void FMeshMapBaker::SetGutterSize(const int32 GutterSizeIn)
{
	// GutterSize must be >= 1 since it is tied to MaxDistance for the
	// OccupancyMap spatial search.
	GutterSize = GutterSizeIn >= 1 ? GutterSizeIn : 1;
}

void FMeshMapBaker::SetSamplesPerPixel(const int32 SamplesPerPixelIn)
{
	SamplesPerPixel = SamplesPerPixelIn;
}

void FMeshMapBaker::SetFilter(const EBakeFilterType FilterTypeIn)
{
	FilterType = FilterTypeIn;
}

void FMeshMapBaker::SetTileSize(const int TileSizeIn)
{
	TileSize = TileSizeIn;
}

void FMeshMapBaker::InitFilter()
{
	FilterKernelSize = TilePadding;
	switch(FilterType)
	{
	case EBakeFilterType::None:
		FilterKernelSize = 0;
		TextureFilterEval = &EvaluateFilter<EBakeFilterType::None>;
		break;
	case EBakeFilterType::Box:
		TextureFilterEval = &EvaluateFilter<EBakeFilterType::Box>;
		break;
	case EBakeFilterType::BSpline:
		TextureFilterEval = &EvaluateFilter<EBakeFilterType::BSpline>;
		break;
	case EBakeFilterType::MitchellNetravali:
		TextureFilterEval = &EvaluateFilter<EBakeFilterType::MitchellNetravali>;
		break;
	}
}

template<FMeshMapBaker::EBakeFilterType BakeFilterType>
float FMeshMapBaker::EvaluateFilter(const FVector2d& Dist)
{
	float Result = 0.0f;
	if constexpr(BakeFilterType == EBakeFilterType::None)
	{
		Result = 1.0f;
	}
	else if constexpr(BakeFilterType == EBakeFilterType::Box)
	{
		Result = BoxFilter.GetWeight(Dist);
	}
	else if constexpr(BakeFilterType == EBakeFilterType::BSpline)
	{
		Result = BSplineFilter.GetWeight(Dist);
	}
	else if constexpr(BakeFilterType == EBakeFilterType::MitchellNetravali)
	{
		Result = MitchellNetravaliFilter.GetWeight(Dist);
	}
	return Result;
}


void FMeshMapBaker::ComputeUVCharts(const FDynamicMesh3& Mesh, TArray<int32>& MeshUVCharts)
{
	MeshUVCharts.SetNumZeroed(Mesh.TriangleCount());
	if (const FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes() ? Mesh.Attributes()->PrimaryUV() : nullptr)
	{
		FMeshConnectedComponents UVComponents(&Mesh);
		UVComponents.FindConnectedTriangles([UVOverlay](int32 Triangle0, int32 Triangle1) {
			return UVOverlay ? UVOverlay->AreTrianglesConnected(Triangle0, Triangle1) : false;
		});
		const int32 NumComponents = UVComponents.Num();
		for (int32 ComponentId = 0; ComponentId < NumComponents; ++ComponentId)
		{
			const FMeshConnectedComponents::FComponent& UVComp = UVComponents.GetComponent(ComponentId);
			for (const int32 TriId : UVComp.Indices)
			{
				MeshUVCharts[TriId] = ComponentId;
			}
		}
	}
}



