// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FRHIBuffer;
struct FReadBuffer;

#ifdef PLATFORM_WIN64
	struct ID3D12Resource;
#endif

class FNeuralNetworkInferenceUtilsGPU
{
public:
	/**
	 * If OutReadBufferis nullptr, it creates a new FReadBuffer pointer into OutReadBuffer and copies the data from InArrayData.
	 * @param InDebugName Input name for FReadBuffer::Initialize().
	 */
	static void CreateAndLoadSRVBuffer(TSharedPtr<FReadBuffer>& OutReadBuffer, const TArray<uint32>& InArrayData, const TCHAR* InDebugName);

	/**
	 * Sanity checks when running the forward operators or their related GPU functions.
	 */
	static bool GPUSanityChecks(const FRDGBuilder* const InGraphBuilder);
	static bool GPUSanityChecks(const FRDGBuilder* const InGraphBuilder, const bool bInIsLoaded);


#ifdef PLATFORM_WIN64
	static bool IsD3D12RHI();

	static ID3D12Resource* GetD3D12Resource(FRHIBuffer* Buffer);
#endif
};
