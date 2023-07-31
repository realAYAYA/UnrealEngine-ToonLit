// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ResourceArray.h"

/** Resource array to pass  */
class FNeuralTensorResourceArray : public FResourceArrayInterface
{
public:
	FNeuralTensorResourceArray(void* InData, const uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	FORCEINLINE virtual const void* GetResourceData() const override { return Data; }
	FORCEINLINE virtual uint32 GetResourceDataSize() const override { return Size; }
	FORCEINLINE virtual void Discard() override { }
	FORCEINLINE virtual bool IsStatic() const override { return false; }
	FORCEINLINE virtual bool GetAllowCPUAccess() const override { return false; }
	FORCEINLINE virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	void* Data;
	uint32 Size;
};
