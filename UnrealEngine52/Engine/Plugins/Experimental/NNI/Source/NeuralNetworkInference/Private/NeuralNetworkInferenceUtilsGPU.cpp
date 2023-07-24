// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkInferenceUtilsGPU.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralTensorResourceArray.h"

#ifdef PLATFORM_WIN64
	#include "ID3D12DynamicRHI.h"
#endif

/* FNeuralNetworkInferenceUtilsGPU public functions
 *****************************************************************************/

void FNeuralNetworkInferenceUtilsGPU::CreateAndLoadSRVBuffer(TSharedPtr<FReadBuffer>& OutReadBuffer, const TArray<uint32>& InArrayData, const TCHAR* InDebugName)
{
	if (OutReadBuffer.IsValid())
	{
		OutReadBuffer->Release();
	}
	OutReadBuffer = MakeShared<FReadBuffer>();
	TSharedPtr<FNeuralTensorResourceArray> TensorResourceArray = MakeShared<FNeuralTensorResourceArray>((void*)InArrayData.GetData(), sizeof(uint32) * InArrayData.Num());
	OutReadBuffer->Initialize(InDebugName, sizeof(uint32), InArrayData.Num(), PF_R32_UINT, BUF_ShaderResource | BUF_Static, TensorResourceArray.Get());
}

bool FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(const FRDGBuilder* const InGraphBuilder)
{
	// Sanity checks
	if (!IsInRenderingThread())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("GPUSanityChecks(): IsInRenderingThread() should be true."));
		return false;
	}
	else if (!InGraphBuilder)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("GPUSanityChecks(): InOutGraphBuilder cannot be nullptr."));
		return false;
	}
	return true;
}

bool FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(const FRDGBuilder* const InGraphBuilder, const bool bInIsLoaded)
{
	// Sanity checks
	if (!bInIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("GPUSanityChecks(): bIsLoaded should be true."));
		return false;
	}
	return GPUSanityChecks(InGraphBuilder);
}


#ifdef PLATFORM_WIN64

bool FNeuralNetworkInferenceUtilsGPU::IsD3D12RHI()
{
	return RHIGetInterfaceType() == ERHIInterfaceType::D3D12;
}

ID3D12Resource* FNeuralNetworkInferenceUtilsGPU::GetD3D12Resource(FRHIBuffer* Buffer)
{
	return GetID3D12DynamicRHI()->RHIGetResource(Buffer);
}

#endif