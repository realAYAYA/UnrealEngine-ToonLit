// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cuda.h>
#include "OptiXCudaFUnctionList.h"
#include <Mutex>

FOptiXCudaFunctionList& FOptiXCudaFunctionList::Get()
{
	static FOptiXCudaFunctionList OptiXCudaFunctionList;
	return OptiXCudaFunctionList;
}

void FOptiXCudaFunctionList::InitializeCudaModule()
{
	OptiXCudaFunctionInstance->InitializeCudaModule();
}

inline bool FOptiXCudaFunctionList::RegisterCUDAFunctionInstance(FOptiXCudaFunctionInstance* InOptiXCudaFunctionInstance) 
{ 
	bool RegisterStatus = false;

	if (!OptiXCudaFunctionInstance)
	{
		OptiXCudaFunctionInstance = InOptiXCudaFunctionInstance;
		RegisterStatus = true;
	}

	return RegisterStatus;
}

inline FOptiXCudaFunctionInstance* FOptiXCudaFunctionList::GetCUDAFunctionInstance() 
{ 
	return OptiXCudaFunctionInstance; 
}

void FOptiXCudaFunctionList::ShutDown()
{
	OptiXCudaFunctionInstance->ShutDown();
}

void FOptiXCudaFunctionList::CopySurfaceToCudaBuffer(CUsurfObject Surface, FOptiXImageData& Result, CUstream Stream, bool bIsNormalInUEViewSpace, float PreExposure)
{
	OptiXCudaFunctionInstance->CopySurfaceToCudaBuffer(Surface, Result, Stream, bIsNormalInUEViewSpace, PreExposure);
}

void FOptiXCudaFunctionList::CopyCudaBufferToSurface(CUsurfObject Surface, const FOptiXImageData& Input, CUstream Stream)
{
	OptiXCudaFunctionInstance->CopyCudaBufferToSurface(Surface, Input, Stream);
}

void FOptiXCudaFunctionList::ConvertRGBA(unsigned char* result, const FOptiXImageData& Input, uint32_t inStrideXInBytes, CUstream Stream)
{
	OptiXCudaFunctionInstance->ConvertRGBA(result, Input, inStrideXInBytes, Stream);
}

void FOptiXCudaFunctionList::ConvertFlow(FOptiXImageData& Output, const int16_t* flow, uint32_t outStrideXInBytes, CUstream Stream)
{
	OptiXCudaFunctionInstance->ConvertFlow(Output, flow, outStrideXInBytes, Stream);
}

