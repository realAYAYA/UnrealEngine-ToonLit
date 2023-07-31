// Copyright Epic Games, Inc. All Rights Reserved.

#include "GerstnerWaterWaveViewExtension.h"
#include "GerstnerWaterWaveSubsystem.h"
#include "WaterBodyActor.h"
#include "GerstnerWaterWaves.h"
#include "Engine/Engine.h"
#include "WaterBodyManager.h"
#include "WaterSubsystem.h"

FGerstnerWaterWaveViewExtension::FGerstnerWaterWaveViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld) : FWorldSceneViewExtension(AutoReg, InWorld), WaveGPUData(MakeShared<FWaveGPUResources, ESPMode::ThreadSafe>())
{
}

FGerstnerWaterWaveViewExtension::~FGerstnerWaterWaveViewExtension()
{
}

void FGerstnerWaterWaveViewExtension::Initialize()
{
	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>())
	{
		GerstnerWaterWaveSubsystem->Register(this);
	}
}

void FGerstnerWaterWaveViewExtension::Deinitialize()
{
	ENQUEUE_RENDER_COMMAND(DeallocateWaterInstanceDataBuffer)
	(
		// Copy the shared ptr into a local copy for this lambda, this will increase the ref count and keep it alive on the renderthread until this lambda is executed
		[WaveGPUData=WaveGPUData](FRHICommandListImmediate& RHICmdList){}
	);

	// It's possible for GEngine to be null when UWaterSubsystem deinitializes
	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine ? GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>() : nullptr)
		{
			GerstnerWaterWaveSubsystem->Unregister(this);
		}
	}

void FGerstnerWaterWaveViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	if (bRebuildGPUData)
	{
		TResourceArray<FVector4f> WaterIndirectionBuffer;
		TResourceArray<FVector4f> WaterDataBuffer;

		const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
		check(WorldPtr.IsValid())

		FWaterBodyManager::ForEachWaterBodyComponent(WorldPtr.Get(), [&WaterIndirectionBuffer, &WaterDataBuffer](UWaterBodyComponent* WaterBodyComponent)
		{
		// Some max value
			constexpr int32 MaxWavesPerWaterBody = 4096;
			constexpr int32 NumFloat4PerWave = 2;

			WaterIndirectionBuffer.AddZeroed();

			if (WaterBodyComponent && WaterBodyComponent->HasWaves())
			{
				const UWaterWavesBase* WaterWavesBase = WaterBodyComponent->GetWaterWaves();
				check(WaterWavesBase != nullptr);
				if (const UGerstnerWaterWaves* GerstnerWaves = Cast<const UGerstnerWaterWaves>(WaterWavesBase->GetWaterWaves()))
				{
					const TArray<FGerstnerWave>& Waves = GerstnerWaves->GetGerstnerWaves();
					
					// Where the data for this water body starts (including header)
					const int32 DataBaseIndex = WaterDataBuffer.Num();
					// Allocate for the waves in this water body
					const int32 NumWaves = FMath::Min(Waves.Num(), MaxWavesPerWaterBody);
					WaterDataBuffer.AddZeroed(NumWaves * NumFloat4PerWave);

					// The header is a vector4 and contains generic per-water body information
					// X: Index to the wave data
					// Y: Num waves
					// Z: TargetWaveMaskDepth
					// W: Unused
					FVector4f& Header = WaterIndirectionBuffer.Last();
					Header.X = DataBaseIndex;
					Header.Y = NumWaves;
					Header.Z = WaterBodyComponent->TargetWaveMaskDepth;
					Header.W = 0.0f;

					for (int32 i = 0; i < NumWaves; i++)
					{
						const FGerstnerWave& Wave = Waves[i];

						const int32 WaveIndex = DataBaseIndex + (i * NumFloat4PerWave);

						WaterDataBuffer[WaveIndex] = FVector4f(Wave.Direction.X, Wave.Direction.Y, Wave.WaveLength, Wave.Amplitude);
						WaterDataBuffer[WaveIndex + 1] = FVector4f(Wave.Steepness, 0.0f, 0.0f, 0.0f);
				}
			}
		}
			return true;
		});

		if (WaterIndirectionBuffer.Num() == 0)
		{
			WaterIndirectionBuffer.AddZeroed();
		}
		
		if (WaterDataBuffer.Num() == 0)
		{
			WaterDataBuffer.AddZeroed();
		}

		ENQUEUE_RENDER_COMMAND(AllocateWaterInstanceDataBuffer)
		(
			[WaveGPUData=WaveGPUData, WaterDataBuffer, WaterIndirectionBuffer](FRHICommandListImmediate& RHICmdList) mutable
			{
				FRHIResourceCreateInfo CreateInfoData(TEXT("WaterDataBuffer"), &WaterDataBuffer);
				WaveGPUData->DataBuffer = RHICreateBuffer(WaterDataBuffer.GetResourceDataSize(), BUF_VertexBuffer | BUF_ShaderResource | BUF_Static, sizeof(FVector4f), ERHIAccess::SRVMask, CreateInfoData);
				WaveGPUData->DataSRV = RHICreateShaderResourceView(WaveGPUData->DataBuffer, sizeof(FVector4f), PF_A32B32G32R32F);

				FRHIResourceCreateInfo CreateInfoIndirection(TEXT("WaterIndirectionBuffer"), &WaterIndirectionBuffer);
				WaveGPUData->IndirectionBuffer = RHICreateBuffer(WaterIndirectionBuffer.GetResourceDataSize(), BUF_VertexBuffer | BUF_ShaderResource | BUF_Static, sizeof(FVector4f), ERHIAccess::SRVMask, CreateInfoIndirection);
				WaveGPUData->IndirectionSRV = RHICreateShaderResourceView(WaveGPUData->IndirectionBuffer, sizeof(FVector4f), PF_A32B32G32R32F);
			}
		);

		bRebuildGPUData = false;
	}
}

void FGerstnerWaterWaveViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (WaveGPUData->DataSRV && WaveGPUData->IndirectionSRV)
	{
		InView.WaterDataBuffer = WaveGPUData->DataSRV;
		InView.WaterIndirectionBuffer = WaveGPUData->IndirectionSRV;
	}
}
