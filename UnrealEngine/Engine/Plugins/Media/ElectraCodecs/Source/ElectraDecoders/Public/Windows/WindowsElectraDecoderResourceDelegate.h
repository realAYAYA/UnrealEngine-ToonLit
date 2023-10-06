// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IElectraDecoderResourceDelegateBase.h"

class IElectraDecoderResourceDelegateWindows : public IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateWindows() = default;

	// DirectX video decoding needs to know the current D3D device and the version of DX being used.
	virtual bool GetD3DDevice(void **OutD3DDevice, int32* OutD3DVersionTimes1000) = 0;
};


using IElectraDecoderResourceDelegate = IElectraDecoderResourceDelegateWindows;
