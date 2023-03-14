// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPrivate_Platform.h"
#include "MediaVideoDecoderOutputPC.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/LowLevelMemTracker.h"

THIRD_PARTY_INCLUDES_START
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/


class FElectraPlayerVideoDecoderOutputPC : public FVideoDecoderOutputPC
{
public:
	FElectraPlayerVideoDecoderOutputPC();

	// Hardware decode to buffer (Win7/DX12)
	void InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, Electra::FParamDict* InParamDict);

	// Hardware decode to shared DX11 texture (Win8+)
	void InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<IMFSample>& MFSample, const FIntPoint& OutputDim, Electra::FParamDict* InParamDict);

	// Software decode (into texture if DX11 device specified - available only Win8+)
	void SetSWDecodeTargetBufferSize(uint32 InTargetBufferSize);
	void PreInitForDecode(FIntPoint OutputDim, const TFunction<void(int32 /*ApiReturnValue*/, const FString& /*Message*/, uint16 /*Code*/, UEMediaError /*Error*/)>& PostError);
	void ProcessDecodeOutput(FIntPoint OutputDim, Electra::FParamDict* InParamDict);

	void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer) override;

	void ShutdownPoolable() override;

	virtual EOutputType GetOutputType() const override;

	virtual TRefCountPtr<IMFSample> GetMFSample() const override;

	virtual const TArray<uint8>& GetBuffer() const override;

	virtual uint32 GetStride() const override;

	virtual TRefCountPtr<ID3D11Texture2D> GetTexture() const override;

	virtual TRefCountPtr<ID3D11Device> GetDevice() const override;

	virtual FIntPoint GetDim() const override;

private:
	// Decoder output type
	EOutputType OutputType;

	// Output texture (with device that created it) for SW decoder output Win8+
	TRefCountPtr<ID3D11Texture2D> Texture;
	TRefCountPtr<ID3D11Texture2D> SharedTexture;
	TRefCountPtr<ID3D11Device> D3D11Device;

	// CPU-side buffer
	TOptional<uint32> TargetBufferAllocSize;
	TArray<uint8> Buffer;
	uint32 Stride;

	// WMF sample (owned by this class if SW decoder is used)
	TRefCountPtr<IMFSample> MFSample;

	// Dimension of any internally allocated buffer - stored explicitly to cover various special cases for DX
	FIntPoint SampleDim;

	// We hold a weak reference to the video renderer. During destruction the video renderer could be destroyed while samples are still out there..
	TWeakPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> OwningRenderer;
};
