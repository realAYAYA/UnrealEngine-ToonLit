// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoder.h"
#include "VideoDecoderCommon.h"
#include "VideoDecoderAllocationTypes.h"

DEFINE_LOG_CATEGORY(LogVideoDecoder);

namespace AVEncoder
{

struct FVideoDecoder::FPlatformDecoderAllocInterface
{
	FVideoDecoderMethodsWindows	AllocMethods = {};
	void*						ClientData = nullptr;
};


FVideoDecoder::~FVideoDecoder()
{
	// This must have been released via ReleaseDecoderAllocationInterface() !
	check(PlatformDecoderAllocInterface == nullptr);
}

bool FVideoDecoder::CreateDecoderAllocationInterface()
{
	if (!PlatformDecoderAllocInterface)
	{
		check(PlatformDecoderAllocInterface == nullptr);
		FPlatformDecoderAllocInterface* Interface = new FPlatformDecoderAllocInterface {};

// TODO: Platform specific!!
		Interface->AllocMethods.MagicCookie = 0x57696e58;		// 'WinX'
		Interface->AllocMethods.This = nullptr;					// This will become the client's 'this' pointer
		Interface->AllocMethods.GetD3DDevice = nullptr;			// Method to get the current D3D device from the client.
		Interface->AllocMethods.AllocateFrameBuffer = nullptr;	// Method to allocate frame buffer memory from the client.

		// Call into the client to set up the allocation interface.
		CreateDecoderAllocationInterfaceFN(&Interface->AllocMethods, &Interface->ClientData);

		// Check for success by testing the returned client data pointer.
		if (Interface->ClientData)
		{
			PlatformDecoderAllocInterface = Interface;
			return true;
		}
		else
		{
			delete Interface;
			return false;
		}
	}
	return true;
}

void FVideoDecoder::ReleaseDecoderAllocationInterface()
{
	if (PlatformDecoderAllocInterface)
	{
		void* NoReturnValue = nullptr;
		ReleaseDecoderAllocationInterfaceFN(PlatformDecoderAllocInterface->ClientData, &NoReturnValue);
		delete PlatformDecoderAllocInterface;
		PlatformDecoderAllocInterface = nullptr;
	}

}

void* FVideoDecoder::GetAllocationInterfaceMethods()
{
	return PlatformDecoderAllocInterface ? &PlatformDecoderAllocInterface->AllocMethods : nullptr;
}


EFrameBufferAllocReturn FVideoDecoder::AllocateOutputFrameBuffer(FVideoDecoderAllocFrameBufferResult* OutBuffer, const FVideoDecoderAllocFrameBufferParams* InAllocParams)
{
	check(InAllocParams);
	check(OutBuffer);
	if (PlatformDecoderAllocInterface && PlatformDecoderAllocInterface->AllocMethods.AllocateFrameBuffer.IsBound())
	{
		return PlatformDecoderAllocInterface->AllocMethods.AllocateFrameBuffer.Execute(PlatformDecoderAllocInterface->AllocMethods.This, InAllocParams, OutBuffer);
	}
	return EFrameBufferAllocReturn::CODEC_Failure;
}


} /* namespace AVEncoder */
