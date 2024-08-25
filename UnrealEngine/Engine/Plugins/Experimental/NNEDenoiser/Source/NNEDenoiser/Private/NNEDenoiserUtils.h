// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEDenoiserLog.h"
#include "NNEDenoiserModelIOMappingData.h"
#include "NNEDenoiserParameters.h"
#include "NNEDenoiserShadersDefaultCS.h"
#include "NNEDenoiserShadersOidnCS.h"
#include "NNEDenoiserTiling.h"
#include "NNETypes.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RHITypes.h"

namespace UE::NNEDenoiser::Private
{

	template<class IntType>
	bool IsTensorShapeValid(TConstArrayView<IntType> ShapeData, TConstArrayView<int32> RequiredShapeData, const FString& Label)
	{
		static_assert(std::is_integral_v<IntType>);

		if (ShapeData.Num() != RequiredShapeData.Num())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("%s has wrong rank (expected %d, got %d)!"), *Label, RequiredShapeData.Num(), ShapeData.Num())
			return false;
		}

		for (int32 I = 0; I < RequiredShapeData.Num(); I++)
		{
			if (RequiredShapeData[I] >= 0 && (int32)ShapeData[I] != RequiredShapeData[I])
			{
				UE_LOG(LogNNEDenoiser, Error, TEXT("%s does not have required shape!"), *Label)
				return false;
			}
		}

		return true;
	}

	template <typename PixelType>
	void CopyTextureFromGPUToCPU(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FIntPoint Size, TArray<PixelType>& DstArray)
	{
		SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.CopyTextureFromGPUToCPU", FColor::Magenta);

		uint32 SrcStride = 0;
		const PixelType* SrcBuffer = static_cast<PixelType*>(RHICmdList.LockTexture2D(SrcTexture, 0, RLM_ReadOnly, SrcStride, false));
		SrcStride /= sizeof(PixelType);
		PixelType* DstBuffer = DstArray.GetData();
		for (int32 Y = 0; Y < Size.Y; Y++, DstBuffer += Size.X, SrcBuffer += SrcStride)
		{
			FPlatformMemory::Memcpy(DstBuffer, SrcBuffer, Size.X * sizeof(PixelType));

		}
		RHICmdList.UnlockTexture2D(SrcTexture, 0, false);
	}

	template <typename PixelType>
	void CopyTextureFromCPUToGPU(FRHICommandListImmediate& RHICmdList, const TArray<PixelType>& SrcArray, FIntPoint Size, FRHITexture* DstTexture)
	{
		SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.CopyTextureFromCPUToGPU", FColor::Magenta);

		uint32 DestStride;
		FLinearColor* DstBuffer = static_cast<PixelType*>(RHICmdList.LockTexture2D(DstTexture, 0, RLM_WriteOnly, DestStride, false));
		DestStride /= sizeof(PixelType);
		const FLinearColor* SrcBuffer = SrcArray.GetData();
		for (int32 Y = 0; Y < Size.Y; Y++, SrcBuffer += Size.X, DstBuffer += DestStride)
		{
			FPlatformMemory::Memcpy(DstBuffer, SrcBuffer, Size.X * sizeof(PixelType));
		}
		RHICmdList.UnlockTexture2D(DstTexture, 0, false);
	}

	inline void CopyBufferFromGPUToCPU(FRHICommandListImmediate& RHICmdList, FRHIBuffer* SrcBuffer, int32 Count, TArray<uint8>& DstArray)
	{
		SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.CopyBufferFromGPUToCPU", FColor::Magenta);

		const uint8* Src = static_cast<uint8*>(RHICmdList.LockBuffer(SrcBuffer, 0, Count, RLM_ReadOnly));
		uint8* Dst = DstArray.GetData();
		
		FPlatformMemory::Memcpy(Dst, Src, Count);

		RHICmdList.UnlockBuffer(SrcBuffer);
	}

	inline void CopyBufferFromCPUToGPU(FRHICommandListImmediate& RHICmdList, const TArray<uint8>& SrcArray, int32 Count, FRHIBuffer* DstBuffer)
	{
		SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.CopyBufferFromCPUToGPU", FColor::Magenta);

		uint8* Dst = static_cast<uint8*>(RHICmdList.LockBuffer(DstBuffer, 0, Count, RLM_WriteOnly));
		const uint8* Src = SrcArray.GetData();
		
		FPlatformMemory::Memcpy(Dst, Src, Count);

		RHICmdList.UnlockBuffer(DstBuffer);
	}

	inline EPixelFormat GetBufferFormat(ENNETensorDataType TensorDataType)
	{
		switch (TensorDataType)
		{
			case ENNETensorDataType::Half: return EPixelFormat::PF_R16F;
			case ENNETensorDataType::Float: return EPixelFormat::PF_R32_FLOAT;
		}
		return EPixelFormat::PF_Unknown;
	}

	inline UE::NNEDenoiserShaders::Internal::ENNEDenoiserDataType GetDenoiserShaderDataType(ENNETensorDataType TensorDataType)
	{
		using UE::NNEDenoiserShaders::Internal::ENNEDenoiserDataType;

		switch (TensorDataType)
		{
			case ENNETensorDataType::Half: return ENNEDenoiserDataType::Half;
			case ENNETensorDataType::Float: return ENNEDenoiserDataType::Float;
		}
		return ENNEDenoiserDataType::None;
	}

	inline NNEDenoiserShaders::Internal::ENNEDenoiserInputKind GetInputKind(EResourceName TensorName)
	{
		using NNEDenoiserShaders::Internal::ENNEDenoiserInputKind;
		
		switch(TensorName)
		{
			case EResourceName::Color:	return ENNEDenoiserInputKind::Color;
			case EResourceName::Albedo:	return ENNEDenoiserInputKind::Albedo;
			case EResourceName::Normal:	return ENNEDenoiserInputKind::Normal;
			case EResourceName::Flow:	return ENNEDenoiserInputKind::Flow;
			case EResourceName::Output:	return ENNEDenoiserInputKind::Output;
		}

		// There should be a case for every resource name
		checkNoEntry();
		
		return ENNEDenoiserInputKind::Color;
	}

}