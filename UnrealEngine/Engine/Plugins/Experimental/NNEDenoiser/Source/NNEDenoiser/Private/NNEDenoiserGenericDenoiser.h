// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/IntPoint.h"
#include "NNEDenoiserParameters.h"
#include "RenderGraphFwd.h"
#include "Templates/UniquePtr.h"

struct FRHIGPUMask;

namespace UE::NNEDenoiser::Private
{

class FHistory;
class IModelInstance;
class IInputProcess;
class IOutputProcess;

class FGenericDenoiser
{
public:
	FGenericDenoiser(TUniquePtr<IModelInstance> ModelInstance, TUniquePtr<IInputProcess> InputProcess, TUniquePtr<IOutputProcess> OutputProcess, FParameters DenoiserParameters);

	~FGenericDenoiser();

	// IPathTracingSpatialTemporalDenoiser interface
	const TCHAR* GetDebugName() const { return *DebugName; }

	TUniquePtr<FHistory> AddPasses(
		FRDGBuilder &GraphBuilder,
		FRDGTextureRef ColorTex,
		FRDGTextureRef AlbedoTex,
		FRDGTextureRef NormalTex,
		FRDGTextureRef OutputTex,
		FRDGTextureRef FlowTex,
		const FRHIGPUMask& GPUMask,
		FHistory* History);

private:
	inline static const FString DebugName = TEXT("FGenericDenoiser");

	TUniquePtr<IModelInstance> ModelInstance;
	TUniquePtr<IInputProcess> InputProcess;
	TUniquePtr<IOutputProcess> OutputProcess;
	FParameters DenoiserParameters;

	FIntPoint LastExtent = { -1, -1 };
};

} // namespace UE::NNEDenoiser::Private