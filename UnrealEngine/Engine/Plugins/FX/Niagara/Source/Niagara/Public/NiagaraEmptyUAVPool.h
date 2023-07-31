// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

// Scoped access for UAVs this is not required when running inside the main dispatch loop
// but is required for external usage that is outside, i.e. if you are doing some custom dispatch setup
struct FNiagaraEmptyUAVPoolScopedAccess
{
public:
	explicit FNiagaraEmptyUAVPoolScopedAccess(class FNiagaraEmptyUAVPool* EmptyUAVPool);
	FNiagaraEmptyUAVPoolScopedAccess(const FNiagaraEmptyUAVPoolScopedAccess&) = delete;
	FNiagaraEmptyUAVPoolScopedAccess(FNiagaraEmptyUAVPoolScopedAccess&&) = delete;
	~FNiagaraEmptyUAVPoolScopedAccess();

	FNiagaraEmptyUAVPoolScopedAccess& operator=(const FNiagaraEmptyUAVPoolScopedAccess&) = delete;
	FNiagaraEmptyUAVPoolScopedAccess& operator=(FNiagaraEmptyUAVPoolScopedAccess&&) = delete;

private:
	class FNiagaraEmptyUAVPool* EmptyUAVPool;
};

// Type of empty UAV to get
enum class ENiagaraEmptyUAVType
{
	Buffer,
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCube,
	Num
};

// Empty UAV pool used for ensuring we bind a buffer when one does not exist
class NIAGARA_API FNiagaraEmptyUAVPool
{
	friend struct FNiagaraEmptyUAVPoolScopedAccess;

public:
	/**
	Grab a temporary empty RW buffer from the pool.
	Note: When doing this outside of Niagara you must be within a FNiagaraUAVPoolAccessScope.
	*/
	FRHIUnorderedAccessView* GetEmptyUAVFromPool(FRHICommandList& RHICmdList, EPixelFormat Format, ENiagaraEmptyUAVType Type);

	/**
	Returns all used UAVs back to the pool
	*/
	void ResetEmptyUAVPools();

protected:
	struct FEmptyUAV
	{
		~FEmptyUAV();

		FBufferRHIRef Buffer;
		FTextureRHIRef Texture;
		FUnorderedAccessViewRHIRef UAV;
	};

	struct FEmptyUAVPool
	{
		~FEmptyUAVPool();

		int32 NextFreeIndex = 0;
		TArray<FEmptyUAV> UAVs;
	};

	uint32 UAVAccessCounter = 0;
	TMap<EPixelFormat, FEmptyUAVPool> UAVPools[(int)ENiagaraEmptyUAVType::Num];
};
