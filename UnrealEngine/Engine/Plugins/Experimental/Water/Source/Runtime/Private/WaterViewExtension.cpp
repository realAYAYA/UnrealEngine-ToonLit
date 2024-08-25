// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterViewExtension.h"
#include "WaterBodyComponent.h"
#include "WaterZoneActor.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "WaterMeshComponent.h"
#include "GerstnerWaterWaveSubsystem.h"
#include "WaterBodyManager.h"
#include "WaterSubsystem.h"
#include "Containers/DynamicRHIResourceArray.h"

static TAutoConsoleVariable<bool> CVarLocalTessellationFreeze(
	TEXT("r.Water.WaterMesh.LocalTessellation.Freeze"),
	false,
	TEXT("Pauses the local tessellation updates to allow the view to move forward without moving the sliding window.\n")
	TEXT("Can be used to view things outside the sliding window more closely."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarLocalTessellationUpdateMargin(
	TEXT("r.Water.WaterMesh.LocalTessellation.UpdateMargin"),
	15000.,
	TEXT("Controls the minimum distance between the view and the edge of the dynamic water mesh when local tessellation is enabled.\n")
	TEXT("If the view is less than UpdateMargin units away from the edge, it moves the sliding window forward."),
	ECVF_Default);

extern void OnCVarWaterInfoSceneProxiesValueChanged(IConsoleVariable*);
static TAutoConsoleVariable<int32> CVarWaterInfoRenderMethod(
	TEXT("r.Water.WaterInfo.RenderMethod"),
	1,
	TEXT("0: SceneCaptures, 1: Custom, 2: CustomRenderPasses"),
	FConsoleVariableDelegate::CreateStatic(OnCVarWaterInfoSceneProxiesValueChanged),
	ECVF_Default | ECVF_RenderThreadSafe);

// ----------------------------------------------------------------------------------

FWaterMeshGPUWork GWaterMeshGPUWork;

FWaterViewExtension::FWaterViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoReg, InWorld)
	, WaterGPUData(MakeShared<FWaterGPUResources, ESPMode::ThreadSafe>())
{
}

FWaterViewExtension::~FWaterViewExtension()
{
}

void FWaterViewExtension::Initialize()
{
	// Register the view extension to the Gerstner Wave subsystem so we can rebuild the water gpu data when waves change.
	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>())
	{
		GerstnerWaterWaveSubsystem->Register(this);
	}
}

void FWaterViewExtension::Deinitialize()
{
	ENQUEUE_RENDER_COMMAND(DeallocateWaterInstanceDataBuffer)
	(
		// Copy the shared ptr into a local copy for this lambda, this will increase the ref count and keep it alive on the renderthread until this lambda is executed
		[WaterGPUData=WaterGPUData](FRHICommandListImmediate& RHICmdList){}
	);

	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>())
	{
		GerstnerWaterWaveSubsystem->Unregister(this);
	}
}

void FWaterViewExtension::UpdateGPUBuffers()
{
	if (bRebuildGPUData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Water::RebuildWaterGPUData);

		const UWorld* WorldPtr = GetWorld();
		check(WorldPtr != nullptr);
		FWaterBodyManager* WaterBodyManager = UWaterSubsystem::GetWaterBodyManager(WorldPtr);
		check(WaterBodyManager);

		// Shrink the water manager storage to avoid over-preallocating in the WaterBodyDataBuffer.
		WaterBodyManager->Shrink();


		struct FWaterBodyData
		{
			float WaterZoneIndex;
			float WaveDataIndex;
			float NumWaves;
			float TargetWaveMaskDepth;
			float FixedVelocityXY; // Packed as two 16 bit floats. X is in the lower 16 bits.
			float FixedVelocityZ;
			float FixedZHeight;
			float FixedWaterDepth;
		};
		static_assert(sizeof(FWaterBodyData) == 2 * sizeof(FVector4f));

		struct FWaterZoneData
		{
			FVector2f Location;
			FVector2f Extent;
			FVector2f HeightExtent;
			float GroundZMin;

			float _Padding; // Unused

			FWaterZoneData(const FVector2f& InLocation, const FVector2f& InExtent, const FVector2f& InHeightExtent, float InGroundZMin)
				: Location(InLocation), Extent(InExtent), HeightExtent(InHeightExtent), GroundZMin(InGroundZMin) {}
		};
		static_assert(sizeof(FWaterZoneData) == 2 * sizeof(FVector4f));

		struct FGerstnerWaveData
		{
			FVector2f Direction;
			float WaveLength;
			float Amplitude;
			float Steepness;

			float _Padding[3]; // Unused

			FGerstnerWaveData(const FGerstnerWave& Wave)
				: Direction(FVector2D(Wave.Direction)), WaveLength(Wave.WaveLength), Amplitude(Wave.Amplitude), Steepness(Wave.Steepness) {}
		};
		static_assert(sizeof(FGerstnerWaveData) == 2 * sizeof(FVector4f));

		// Water Body Data Buffer layout:
		// -------------------------------------------------------------------------------
		// || WaterZoneIndex | WaveDataIndex | NumWaves | (Other members) ||   ...   ||
		// -------------------------------------------------------------------------------
		//
		// Water Aux Data Buffer layout:
		// -----------------------------------------------------------------------------
		// ||| WaterZone Data | ... || GerstnerWaveData | ... |||
		// -----------------------------------------------------------------------------
		//

		TArray<FWaterZoneData> WaterZoneData;

		{
			WaterBodyManager->ForEachWaterZone([&WaterZoneData](AWaterZone* WaterZone)
			{
				// #todo_water: LWC
				const FVector2f ZoneLocation = FVector2f(FVector2D(WaterZone->GetDynamicWaterInfoCenter()));

				const FVector2f ZoneExtent = FVector2f(FVector2D(WaterZone->GetDynamicWaterInfoExtent()));
				const FVector2f WaterHeightExtents = WaterZone->GetWaterHeightExtents();
				const float GroundZMin = WaterZone->GetGroundZMin();

				WaterZoneData.Emplace(ZoneLocation, ZoneExtent, WaterHeightExtents, GroundZMin);

				return true;
			});
		}


		TArray<FWaterBodyData> WaterBodyData;
		TArray<FGerstnerWaveData> WaveData;
		{
			const int32 NumWaterBodies =  WaterBodyManager->NumWaterBodies();
			// Pre-set up to the max water body index. Some entries may be empty and NumWaterBodies != MaxIndex
			WaterBodyData.SetNumZeroed(WaterBodyManager->MaxWaterBodyIndex());

			TMap<const UGerstnerWaterWaves*, int32> GerstnerWavesIndices;

			WaterBodyManager->ForEachWaterBodyComponent([&WaterBodyData, &WaveData, &GerstnerWavesIndices](UWaterBodyComponent* WaterBodyComponent)
			{
				const int32 WaterZoneIndex = WaterBodyComponent->GetWaterZone() ? WaterBodyComponent->GetWaterZone()->GetWaterZoneIndex() : -1;

				const FVector FixedVelocity = WaterBodyComponent->GetConstantVelocity();

				check(WaterBodyComponent->GetWaterBodyIndex() < WaterBodyData.Num());
				FWaterBodyData& WaterBodyDataEntry = WaterBodyData[WaterBodyComponent->GetWaterBodyIndex()];
				WaterBodyDataEntry.WaterZoneIndex = WaterZoneIndex;
				WaterBodyDataEntry.TargetWaveMaskDepth = WaterBodyComponent->TargetWaveMaskDepth;
				WaterBodyDataEntry.FixedVelocityXY = FMath::AsFloat(static_cast<uint32>(FFloat16(FixedVelocity.X).Encoded) | static_cast<uint32>(FFloat16(FixedVelocity.Y).Encoded) << 16u);
				WaterBodyDataEntry.FixedVelocityZ = static_cast<float>(FixedVelocity.Z);
				WaterBodyDataEntry.FixedZHeight = WaterBodyComponent->GetConstantSurfaceZ();
				WaterBodyDataEntry.FixedWaterDepth = WaterBodyComponent->GetConstantDepth();

				if (WaterBodyComponent->HasWaves())
				{
					const UWaterWavesBase* WaterWavesBase = WaterBodyComponent->GetWaterWaves();
					check(WaterWavesBase != nullptr);
					if (const UGerstnerWaterWaves* GerstnerWaves = Cast<const UGerstnerWaterWaves>(WaterWavesBase->GetWaterWaves()))
					{
						int32* WaveDataIndex = GerstnerWavesIndices.Find(GerstnerWaves);

						if (WaveDataIndex == nullptr)
						{
							// Where the data for this set of waves starts
							const int32 WaveDataBase = WaveData.Num();

							WaveDataIndex = &GerstnerWavesIndices.Add(GerstnerWaves, WaveDataBase);

							// Some max value
							constexpr int32 MaxWavesPerGerstnerWaves = 4096;

							const TArray<FGerstnerWave>& Waves = GerstnerWaves->GetGerstnerWaves();
							
							// Allocate for the waves in this water body
							const int32 NumWaves = FMath::Min(Waves.Num(), MaxWavesPerGerstnerWaves);
							WaveData.AddZeroed(NumWaves);

							for (int32 WaveIndex = 0; WaveIndex < NumWaves; WaveIndex++)
							{
								const uint32 WavesDataIndex = WaveDataBase + WaveIndex;
								WaveData[WavesDataIndex] = FGerstnerWaveData(Waves[WaveIndex]);
							}
						}

						const TArray<FGerstnerWave>& Waves = GerstnerWaves->GetGerstnerWaves();

						check(WaveDataIndex);

						WaterBodyDataEntry.WaveDataIndex = *WaveDataIndex;
						WaterBodyDataEntry.NumWaves = Waves.Num();
					}
				}
				return true;
			});
		}

		TResourceArray<FVector4f> WaterBodyDataBuffer;
		TResourceArray<FVector4f> WaterAuxDataBuffer;

		// The first element of the WaterDataBuffer contains the offsets to each of the sub-buffers.
		// X = WaterZoneDataOffset
		// Y = WaterWaveDataOffset
		// Z = Unused
		// W = Unused
		WaterAuxDataBuffer.AddZeroed();

		// Transform the individual arrays into the single buffer:
		{
			/** Copy a buffer of arbitrary PoD into a float4 resource array. Returns the starting offset of the source buffer in the dest buffer. */
			auto AppendDataToFloat4Buffer = []<typename T>(TResourceArray<FVector4f>& Dest, const TArray<T>& Source)
			{
				constexpr int32 NumFloat4PerElement = (sizeof(T) / sizeof(FVector4f));
				const int32 StartOffset = Dest.Num();
				Dest.AddUninitialized(Source.Num() * NumFloat4PerElement);
				FMemory::Memcpy(
					Dest.GetData() + StartOffset,
					Source.GetData(),
					Source.Num() * sizeof(T));
				return StartOffset;
			};

			AppendDataToFloat4Buffer(WaterBodyDataBuffer, WaterBodyData);

			const int32 ZoneDataOffset = AppendDataToFloat4Buffer(WaterAuxDataBuffer, WaterZoneData);
			const int32 WaveDataOffset = AppendDataToFloat4Buffer(WaterAuxDataBuffer, WaveData);

			// Store the offsets to each sub-buffer in the first entry.
			// If this layout ever changes, corresponding decode functions must be updated in GerstnerWaveFunctions.ush!
			FVector4f& OffsetData = WaterAuxDataBuffer[0];
			OffsetData.X = ZoneDataOffset;
			OffsetData.Y = WaveDataOffset;
			OffsetData.Z = 0.0f;
			OffsetData.W = 0.0f;
		}

		if (WaterBodyDataBuffer.Num() == 0)
		{
			WaterBodyDataBuffer.AddZeroed();
		}

		ENQUEUE_RENDER_COMMAND(AllocateWaterInstanceDataBuffer)
		(
			[WaterGPUData=WaterGPUData, WaterAuxDataBuffer, WaterBodyDataBuffer](FRHICommandListImmediate& RHICmdList) mutable
			{
				FRHIResourceCreateInfo AuxDataCreateInfo(TEXT("WaterAuxDataBuffer"), &WaterAuxDataBuffer);
				WaterGPUData->AuxDataBuffer = RHICmdList.CreateBuffer(WaterAuxDataBuffer.GetResourceDataSize(), BUF_VertexBuffer | BUF_ShaderResource | BUF_Static, sizeof(FVector4f), ERHIAccess::SRVMask, AuxDataCreateInfo);
				WaterGPUData->AuxDataSRV = RHICmdList.CreateShaderResourceView(WaterGPUData->AuxDataBuffer, sizeof(FVector4f), PF_A32B32G32R32F);

				FRHIResourceCreateInfo WaterBodyDataCreateInfo(TEXT("WaterBodyDataBuffer"), &WaterBodyDataBuffer);
				WaterGPUData->WaterBodyDataBuffer = RHICmdList.CreateBuffer(WaterBodyDataBuffer.GetResourceDataSize(), BUF_VertexBuffer | BUF_ShaderResource | BUF_Static, sizeof(FVector4f), ERHIAccess::SRVMask, WaterBodyDataCreateInfo);
				WaterGPUData->WaterBodyDataSRV = RHICmdList.CreateShaderResourceView(WaterGPUData->WaterBodyDataBuffer, sizeof(FVector4f), PF_A32B32G32R32F);
			}
		);

		bRebuildGPUData = false;
	}
}

void FWaterViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{

}

void FWaterViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	if (CVarLocalTessellationFreeze.GetValueOnGameThread())
	{
		return;
	}

	const FVector ViewLocation = InView.ViewLocation;

	const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
	check(WorldPtr.IsValid())

	// Prevent re-entrancy. 
	// Since the water info render will update the view extensions we could end up with a re-entrant case.
	static bool bUpdatingWaterInfo = false;
	if (bUpdatingWaterInfo)
	{
		return;
	}
	bUpdatingWaterInfo = true;
	ON_SCOPE_EXIT { bUpdatingWaterInfo = false; };

	const int32 WaterInfoRenderMethod = CVarWaterInfoRenderMethod.GetValueOnGameThread();
	FSceneInterface* Scene = WorldPtr.Get()->Scene;
	check(Scene != nullptr);

	for (const TPair<AWaterZone*, UE::WaterInfo::FRenderingContext>& Pair : WaterInfoContextsToRender)
	{
		AWaterZone* WaterZone = Pair.Key;
		check(WaterZone != nullptr);
		FWaterZoneInfo* WaterZoneInfo = WaterZoneInfos.Find(WaterZone);
		check(WaterZoneInfo != nullptr);

		if (WaterZone->IsLocalOnlyTessellationEnabled())
		{
			UWaterMeshComponent* WaterMesh= WaterZone->GetWaterMeshComponent();
			check(WaterMesh);

			const double TileSize = WaterMesh->GetTileSize();

			const FVector WaterMeshCenter(ViewLocation.GridSnap(TileSize));

			const FVector2D WaterInfoHalfExtent(WaterZone->GetDynamicWaterInfoExtent() / 2.0);
			const FBox2D WaterInfoBounds(FVector2D(WaterMeshCenter) - WaterInfoHalfExtent, FVector2D(WaterMeshCenter) + WaterInfoHalfExtent);

			WaterZone->SetLocalTessellationCenter(WaterMeshCenter);

			// Trigger the next update when the camera is <UpdateMargin> units away from the border of the current window.
			const FVector2D UpdateMargin(CVarLocalTessellationUpdateMargin.GetValueOnGameThread());

			// Keep a minimum of <1., 1.> bounds to avoid updating every frame if the update margin is larger than the zone.
			const FVector2D UpdateExtents = FVector2D::Max(FVector2D(1., 1.), WaterInfoHalfExtent - UpdateMargin);
			WaterZoneInfo->UpdateBounds.Emplace(FVector2D(WaterMeshCenter) - UpdateExtents, FVector2D(WaterMeshCenter) + UpdateExtents);

			const FVector2D WaterQuadTreeHalfExtent = WaterMesh->GetExtentInTiles() * TileSize;
			const FVector2D WaterQuadTreeCenter = WaterMesh->GetDynamicWaterMeshCenter();
			const FBox2D WaterQuadTreeBounds(WaterQuadTreeCenter - WaterQuadTreeHalfExtent, WaterQuadTreeCenter + WaterQuadTreeHalfExtent);

			// If the new water info bounds would be outside the water mesh, recenter and rebuild the water mesh
			if (!WaterQuadTreeBounds.IsInside(WaterInfoBounds.ExpandBy(WaterInfoBounds.GetExtent())))
			{
				WaterMesh->SetDynamicWaterMeshCenter(FVector2D(WaterMeshCenter));
			}

			// Mark GPU data dirty since we have a new WaterArea parameter and need to push this to water bodies.
			MarkGPUDataDirty();
		}
		else
		{
			WaterZoneInfo->UpdateBounds.Reset();
		}

		// Old method of rendering the water info texture; uses scene captures
		if (WaterInfoRenderMethod == 0)
		{
			const UE::WaterInfo::FRenderingContext& Context(Pair.Value);
			UE::WaterInfo::UpdateWaterInfoRendering(Scene, Context);
		}
		// Render the water info texture using custom render pass method
		else if (WaterInfoRenderMethod == 2)
		{
			const UE::WaterInfo::FRenderingContext& Context(Pair.Value);
			UE::WaterInfo::UpdateWaterInfoRendering_CustomRenderPass(Scene, InViewFamily, Context);
		}
	}

	// New method of rendering the water info texture; rendering is done in a separate pass when rendering the main view
	if (WaterInfoRenderMethod == 1)
	{
		UE::WaterInfo::UpdateWaterInfoRendering2(InView, WaterInfoContextsToRender);
	}

	WaterInfoContextsToRender.Empty();

	// Don't dirty the water info texture when we're rendering from a scene capture. Due to the frame delay after marking the texture as dirty, scene captures wouldn't have the right texture anyways.
	// #todo_water [roey]: Once we have no frame-delay for updating the texture and lesser performance impact, we can re-enable updates within scene captures.
	if (!InView.bIsSceneCapture && !InView.bIsSceneCaptureCube && !InView.bIsReflectionCapture && !InView.bIsPlanarReflection && !InView.bIsVirtualTexture)
	{
		// Check if the view location is no longer within the current update bounds of a water zone and if so, queue an update for it.
		for (AWaterZone* WaterZone : TActorRange<AWaterZone>(WorldPtr.Get()))
		{
			if (WaterZone->HasActorRegisteredAllComponents())
			{
				FWaterZoneInfo& WaterZoneInfo = WaterZoneInfos.FindChecked(WaterZone);
				if (WaterZone->IsLocalOnlyTessellationEnabled())
				{
					if (!WaterZoneInfo.UpdateBounds.IsSet() || !WaterZoneInfo.UpdateBounds->IsInside(FVector2D(ViewLocation)))
					{
						WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
					}
				}
			}
		}
	}

	// The logic in UpdateGPUBuffers() used to be done in SetupViewFamily(). However, SetupView() (which is responsible for water info rendering) potentially modifies the WaterZone but is called after SetupViewFamily().
	// This can lead to visual artifacts due to outdated data in the GPU buffers.
	UpdateGPUBuffers();
}

void FWaterViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FWaterViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (WaterGPUData->WaterBodyDataSRV && WaterGPUData->AuxDataSRV)
	{
		// TODO: Rename members on FSceneView in a separate CL. This will invalidate almost all shaders.
		InView.WaterDataBuffer = WaterGPUData->AuxDataSRV;
		InView.WaterIndirectionBuffer = WaterGPUData->WaterBodyDataSRV;
	}
}

void FWaterViewExtension::PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated)
{
	for (FWaterMeshGPUWork::FCallback& Callback : GWaterMeshGPUWork.Callbacks)
	{
		Callback.Function(GraphBuilder, bDepthBufferIsPopulated);
	}
}

void FWaterViewExtension::MarkWaterInfoTextureForRebuild(const UE::WaterInfo::FRenderingContext& RenderContext)
{
	WaterInfoContextsToRender.Emplace(RenderContext.ZoneToRender, RenderContext);
	MarkGPUDataDirty();
}

void FWaterViewExtension::MarkGPUDataDirty()
{
	bRebuildGPUData = true;
}

void FWaterViewExtension::AddWaterZone(AWaterZone* InWaterZone)
{
	check(!WaterZoneInfos.Contains(InWaterZone));
	WaterZoneInfos.Emplace(InWaterZone);
}

void FWaterViewExtension::RemoveWaterZone(AWaterZone* InWaterZone)
{
	WaterZoneInfos.FindAndRemoveChecked(InWaterZone);
}
