// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cuda.h>
#include "OptiXCudaFunctionList.h"
#include "OptiXDenoiseBase.h"
#include "Misc/FileHelper.h"

#ifndef CUDA_CHECK
#define CUDA_CHECK(Call)																		\
	do																							\
	{																							\
		auto Error = Call;																		\
		if (Error != 0)																			\
		{																						\
			UE_LOG(LogOptiXDenoise, Error, TEXT("Cuda call (%s) failed with code [%d] (%s: %d"), ANSI_TO_TCHAR(#Call), Error, ANSI_TO_TCHAR(__FILE__), (__LINE__));	\
		}																						\
	} while (false);																			\

#endif

class FOptiXDenoiserFunctionInstance : public FOptiXCudaFunctionInstance
{
public:
	FOptiXDenoiserFunctionInstance()
	{
		
	}

	virtual ~FOptiXDenoiserFunctionInstance()
	{
		ShutDown();
	}

	void InitializeCudaModule()
	{

		if (OptixWrapperModule)
		{
			return;
		}

		FString OptiXDenoiserFunctionInstancePTX;
		{
			PTXFilePathDirectory = IPluginManager::Get().FindPlugin(TEXT("OptixDenoise"))->GetBaseDir();

			FString OptiXDenoiserFunctionInstancePTXFileName = PTXFilePathDirectory + 
				TEXT("/Source/ThirdParty/OptiXDenoiseBase/lib/OptiXDenoiserFunctionList.ptx");

			uint32 ReadFlags = 0;
			if (!FFileHelper::LoadFileToString(OptiXDenoiserFunctionInstancePTX, *OptiXDenoiserFunctionInstancePTXFileName, FFileHelper::EHashOptions::None, ReadFlags))
			{
				UE_LOG(LogOptiXDenoise, Error, TEXT("Failed to load PTX file : %s"), *OptiXDenoiserFunctionInstancePTXFileName);
			}
		}

		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		CUDA_CHECK(FCUDAModule::CUDA().cuModuleLoadData(&OptixWrapperModule, TCHAR_TO_ANSI(*OptiXDenoiserFunctionInstancePTX)));

		CUDA_CHECK(FCUDAModule::CUDA().cuModuleGetFunction(&CopySurfaceToCudaBufferKernel, OptixWrapperModule, "_Z21k_CopySurfaceToBuffery17FFloatWriteAccessbf"));//k_CopySurfaceToBuffer
		CUDA_CHECK(FCUDAModule::CUDA().cuModuleGetFunction(&CopyCudaBufferToSurfaceKernel, OptixWrapperModule, "_Z21k_CopyBufferToSurfacey16FFloatReadAccess"));//k_CopyBufferToSurface
		CUDA_CHECK(FCUDAModule::CUDA().cuModuleGetFunction(&ConvertRGBAKernel, OptixWrapperModule, "_Z13k_ConvertRGBAPh16FFloatReadAccessi"));//k_ConvertRGBA
		CUDA_CHECK(FCUDAModule::CUDA().cuModuleGetFunction(&ConvertFlowKernel, OptixWrapperModule, "_Z13k_ConvertFlow17FFloatWriteAccessPKsi"));//k_ConvertFlow

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
	}

	void ShutDown()
	{
		if (OptixWrapperModule)
		{
			FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());
			CUDA_CHECK(FCUDAModule::CUDA().cuModuleUnload(OptixWrapperModule));
			FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
			OptixWrapperModule = nullptr;
		}
	}

public:

	//--------------------------------------------------------------------------------
	// Delcare read write header
	struct FFloatReadAccess
	{
		inline FFloatReadAccess(const FOptiXImageData& InImage)
			: Image(InImage)
			, PixelStrideInBytes(InImage.PixelStrideInBytes)
		{
			check(PixelStrideInBytes);
		}
		FOptiXImageData Image;
		unsigned int PixelStrideInBytes;
	};

	struct FFloatWriteAccess
	{
		inline FFloatWriteAccess(const FOptiXImageData& InImage)
			: Image(InImage)
			, PixelStrideInBytes(InImage.PixelStrideInBytes){}
		FOptiXImageData Image;
		unsigned int PixelStrideInBytes;
	};

	void CopySurfaceToCudaBuffer(TSurfaceObject Surface, FOptiXImageData& Result, CUstream Stream, bool bIsNormalInUEViewSpace = false, float PreExposure = 1.0f)
	{
		/*
			dim3 block( 32, 32, 1 );
			dim3 grid = dim3( divUp( Result.width, block.x ), divUp( Result.height, block.y ), 1 );

			k_CopySurfaceToBuffer<<<grid, block, 0, Stream>>>(Surface, floatWrAccess(Result), IsNormalInUEViewSpace);
		*/
		check(Surface);
		check(Result.Data);

		FFloatWriteAccess WriteAccess(Result);
		
		uint32 BlockSize = 32;
		FIntVector Grid = FIntVector(
			(Result.Width + BlockSize - 1) / BlockSize,
			(Result.Height + BlockSize - 1) / BlockSize,
			1);

		CUDA_CHECK(FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext()));
		
		void* Args[] = { &Surface, &WriteAccess, &bIsNormalInUEViewSpace, &PreExposure};

		CUDA_CHECK(FCUDAModule::CUDA().cuLaunchKernel(CopySurfaceToCudaBufferKernel,
			Grid.X, Grid.Y, Grid.Z,
			BlockSize, BlockSize, 1,
			0,
			Stream, Args, NULL));

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);

	}
	void CopyCudaBufferToSurface(TSurfaceObject Surface, const FOptiXImageData& Input, CUstream Stream)
	{
		/*
			dim3 block(32, 32, 1);
			dim3 grid = dim3(divUp(Input.width, block.x), divUp(Input.height, block.y), 1);

			k_CopyBufferToSurface<<<grid, block, 0, Stream >>>(Surface, floatRdAccess(Input));
		*/
		FFloatReadAccess ReadAccess(Input);
		
		uint32 BlockSize = 32;
		FIntVector Grid = FIntVector(
			(Input.Width + BlockSize - 1) / BlockSize,
			(Input.Height + BlockSize - 1) / BlockSize,
			1);

		CUDA_CHECK(FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext()));

		void* Args[] = { &Surface, &ReadAccess };

		CUDA_CHECK(FCUDAModule::CUDA().cuLaunchKernel(CopyCudaBufferToSurfaceKernel,
			Grid.X, Grid.Y, Grid.Z,
			BlockSize, BlockSize, 1,
			0,
			Stream, Args, NULL));

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
	}

	void ConvertRGBA(unsigned char* result, const FOptiXImageData& Input, uint32_t inStrideXInBytes, CUstream Stream)
	{
		FFloatReadAccess ReadAccess(Input);
		void* Args[] = { &result, &ReadAccess, &inStrideXInBytes };

		uint32 BlockSize = 32;
		FIntVector Grid = FIntVector(
			(Input.Width + BlockSize - 1) / BlockSize,
			(Input.Height + BlockSize - 1) / BlockSize,
			1);

		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		CUDA_CHECK(FCUDAModule::CUDA().cuLaunchKernel(ConvertRGBAKernel,
			Grid.X, Grid.Y, Grid.Z,
			BlockSize, BlockSize, 1,
			0,
			Stream, Args, NULL));

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
	}

	void ConvertFlow(FOptiXImageData& Output, const int16_t* flow, uint32_t outStrideXInBytes, CUstream Stream)
	{
		FFloatWriteAccess Result(Output);
		void* Args[] = { &Result, &flow, &outStrideXInBytes };

		uint32 BlockSize = 32;
		FIntVector Grid = FIntVector(
			(Output.Width + BlockSize - 1) / BlockSize,
			(Output.Height + BlockSize - 1) / BlockSize,
			1);

		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		CUDA_CHECK(FCUDAModule::CUDA().cuLaunchKernel(ConvertFlowKernel,
			Grid.X, Grid.Y, Grid.Z,
			BlockSize, BlockSize, 1,
			0,
			Stream, Args, NULL));

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
	}

private:

	FString PTXFilePathDirectory;
	CUmodule OptixWrapperModule = nullptr;

	CUfunction CopySurfaceToCudaBufferKernel = nullptr;
	CUfunction CopyCudaBufferToSurfaceKernel = nullptr;
	CUfunction ConvertRGBAKernel = nullptr;
	CUfunction ConvertFlowKernel = nullptr;
};