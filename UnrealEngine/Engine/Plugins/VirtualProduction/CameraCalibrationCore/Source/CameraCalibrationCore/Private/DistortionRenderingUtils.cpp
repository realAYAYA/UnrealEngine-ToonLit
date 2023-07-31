// Copyright Epic Games, Inc. All Rights Reserved.


#include "DistortionRenderingUtils.h"

#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

class FUndistortImagePointsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUndistortImagePointsCS);

	SHADER_USE_PARAMETER_STRUCT(FUndistortImagePointsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, DistortionMap)
		SHADER_PARAMETER_SAMPLER(SamplerState, DistortionMapSampler)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, InputPoints)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector2D>, UndistortedPoints)
	END_SHADER_PARAMETER_STRUCT()

public:
	// Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// --------------------------------------------------------------------------------------------------------------------
IMPLEMENT_GLOBAL_SHADER(FUndistortImagePointsCS, "/Plugin/CameraCalibrationCore/Private/UndistortImagePoints.usf", "UndistortImagePointsCS", SF_Compute);

namespace DistortionRenderingUtils
{
	void UndistortImagePoints(UTextureRenderTarget2D* DistortionMap, TArray<FVector2D> ImagePoints, TArray<FVector2D>& OutUndistortedPoints)
	{
		if ((DistortionMap == nullptr) || (ImagePoints.Num() < 1))
		{
			return;
		}

		const int32 NumPoints = ImagePoints.Num();
		const FTextureRenderTargetResource* const DistortionMapResource = DistortionMap->GameThread_GetRenderTargetResource();

		ENQUEUE_RENDER_COMMAND(DistortionRenderingUtils_UndistortImagePoints)(
			[DistortionMapResource, NumPoints, Points = MoveTemp(ImagePoints), &OutUndistortedPoints](FRHICommandListImmediate& RHICmdList)
			{
				// Set up parameters
				FUndistortImagePointsCS::FParameters Parameters;
				Parameters.DistortionMap = DistortionMapResource->TextureRHI;
				Parameters.DistortionMapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				TResourceArray<FVector2f> InputPointsResourceArray;
				InputPointsResourceArray.Reserve(NumPoints);
				for (int32 Index = 0; Index < NumPoints; Index++)
				{
					InputPointsResourceArray.Add(FVector2f(Points[Index]));	// LWC_TODO: Precision loss
				}

				TResourceArray<FVector2f> EmptyBuffer;
				EmptyBuffer.AddZeroed(NumPoints);

				const uint32 BufferSize = sizeof(FVector2f) * NumPoints;

				// Create an SRV for the input buffer of image points
				FRHIResourceCreateInfo CreateInfo(TEXT("ImagePointsInitialData"), &InputPointsResourceArray);
				FBufferRHIRef InputPointsBuffer = RHICreateStructuredBuffer(sizeof(FVector2f), BufferSize, BUF_Static | BUF_ShaderResource, CreateInfo);
				FShaderResourceViewRHIRef InputPointsSRV = RHICreateShaderResourceView(InputPointsBuffer);
				Parameters.InputPoints = InputPointsSRV;

				// Create a RWBuffer to use as a UAV for the output buffer of undistorted points
				FRWBuffer UndistortedPointsBuffer;
				UndistortedPointsBuffer.Initialize(TEXT("UndistortedPointsBuffer"), sizeof(FVector2f), NumPoints, PF_G32R32F, ERHIAccess::UAVCompute, BUF_SourceCopy | BUF_UnorderedAccess, &EmptyBuffer);
				Parameters.UndistortedPoints = UndistortedPointsBuffer.UAV;

				// Dispatch compute shader
				TShaderMapRef<FUndistortImagePointsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters, FIntVector(NumPoints, 1, 1));

				// Copy the undistorted points buffer to a staging buffer to read it back on the CPU
				RHICmdList.Transition(FRHITransitionInfo(UndistortedPointsBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));
				FStagingBufferRHIRef DestinationStagingBuffer = RHICreateStagingBuffer();
				RHICmdList.CopyToStagingBuffer(UndistortedPointsBuffer.Buffer, DestinationStagingBuffer, 0, BufferSize);

				// Wait to ensure that the staging buffer is ready to read
				RHICmdList.SubmitCommandsAndFlushGPU();
				RHICmdList.BlockUntilGPUIdle();

				// Copy data out of the staging buffer
				FVector2f* UndistortedPointData = static_cast<FVector2f*>(DestinationStagingBuffer->Lock(0, BufferSize));

				if (UndistortedPointData)
				{
					for (int32 Index = 0; Index < NumPoints; ++Index)
					{
						OutUndistortedPoints[Index] = FVector2D(UndistortedPointData[Index]);
					}
				}

				DestinationStagingBuffer->Unlock();
			});

		// Ensure that all rendering commands have been issued so that OutUndistortedPoints() is valid and has correct data 
		FlushRenderingCommands();
	}
}