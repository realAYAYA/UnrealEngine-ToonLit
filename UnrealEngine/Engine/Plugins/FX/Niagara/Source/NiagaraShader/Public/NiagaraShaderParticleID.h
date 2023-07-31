// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"

extern NIAGARASHADER_API void NiagaraInitGPUFreeIDList(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FRHIUnorderedAccessView* NewBufferUAV, uint32 NewBufferNumElements, FRHIShaderResourceView* ExistingBufferSRV, uint32 ExistingBufferNumElements);
extern NIAGARASHADER_API void NiagaraComputeGPUFreeIDs(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FRHIShaderResourceView* IDToIndexTableSRV, uint32 NumIDs, FRHIUnorderedAccessView* FreeIDUAV, FRHIUnorderedAccessView* FreeIDListSizesUAV, uint32 FreeIDListIndex);
extern NIAGARASHADER_API void NiagaraFillGPUIntBuffer(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FRHIUnorderedAccessView* BufferUAV, uint32 NumElements, int32 FillValue);
