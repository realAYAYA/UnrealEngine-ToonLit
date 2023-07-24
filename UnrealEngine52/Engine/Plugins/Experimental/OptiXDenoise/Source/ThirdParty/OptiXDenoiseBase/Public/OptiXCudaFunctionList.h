// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cuda.h>

#ifndef OPTIXDENOISEBASE
#define OPTIXDENOISEBASE_API __declspec( dllimport )
#else
#define OPTIXDENOISEBASE_API __declspec( dllexport )
#endif

//#ifndef uint32_t
//	#define uint32_t unsigned int
//#endif
typedef unsigned int uint32_t;
typedef short int16_t;
//#ifndef int16_t
//	#define int16_t short
//#endif

typedef enum EOptiXImageFormat
{
	Undefined,
	CUDA_A32B32G32R32_F = 0x2204,
	INTERNAL_LAYER		= 0x2209
}EOptiXImageFormat;


typedef struct FOptiXImageData
{
	CUdeviceptr			Data;
	unsigned int		Width;
	unsigned int		Height;
	unsigned int		RowStrideInBytes;
	unsigned int		PixelStrideInBytes;
	EOptiXImageFormat	Format;
}FOptiXImageData;

class OPTIXDENOISEBASE_API FOptiXCudaFunctionInstance
{
public:
	virtual void InitializeCudaModule() = 0;
	virtual void ShutDown() = 0;

	virtual void CopySurfaceToCudaBuffer(CUsurfObject Surface, FOptiXImageData& Result, CUstream Stream, bool bIsNormalInUEViewSpace = false, float PreExposure = 1.0f) = 0;
	virtual void CopyCudaBufferToSurface(CUsurfObject Surface, const FOptiXImageData& Input, CUstream Stream) = 0;
	virtual void ConvertRGBA(unsigned char* result, const FOptiXImageData& Input, uint32_t inStrideXInBytes, CUstream Stream) = 0;
	virtual void ConvertFlow(FOptiXImageData& Output, const int16_t* flow, uint32_t outStrideXInBytes, CUstream Stream) = 0;
};


class OPTIXDENOISEBASE_API FOptiXCudaFunctionList
{
public:
	static FOptiXCudaFunctionList& Get();

	void ShutDown();
	void CopySurfaceToCudaBuffer(CUsurfObject Surface, FOptiXImageData& Result, CUstream Stream, bool bIsNormalInUEViewSpace = false, float PreExposure = 1.0f);
	void CopyCudaBufferToSurface(CUsurfObject Surface, const FOptiXImageData& Input, CUstream Stream);
	void ConvertRGBA(unsigned char* result, const FOptiXImageData& Input, uint32_t inStrideXInBytes, CUstream Stream);
	void ConvertFlow(FOptiXImageData& Output, const int16_t* flow, uint32_t outStrideXInBytes, CUstream Stream);
	void InitializeCudaModule();

	template<typename T>
	bool RegisterFunctionInstance() {
		return RegisterCUDAFunctionInstance(new T());
	}
private:
	bool RegisterCUDAFunctionInstance(FOptiXCudaFunctionInstance* InOptiXCudaFunctionInstance);
	FOptiXCudaFunctionInstance* GetCUDAFunctionInstance();

	FOptiXCudaFunctionInstance* OptiXCudaFunctionInstance = nullptr;
};