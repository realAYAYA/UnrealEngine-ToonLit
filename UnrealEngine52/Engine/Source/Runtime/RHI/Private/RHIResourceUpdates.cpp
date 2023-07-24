// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIResourceUpdates.h"
#include "RHI.h"

FRHIResourceUpdateBatcher::FRHIResourceUpdateBatcher(TArrayView<FRHIResourceUpdateInfo> InUpdateInfos)
	: UpdateInfos(InUpdateInfos)
{}

FRHIResourceUpdateBatcher::~FRHIResourceUpdateBatcher()
{
	Flush();
}

void FRHIResourceUpdateBatcher::Flush()
{
	if (NumBatched > 0)
	{
		RHIUpdateRHIResources(&UpdateInfos[0], NumBatched, true);
		NumBatched = 0;
	}
}

void FRHIResourceUpdateBatcher::QueueUpdateRequest(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
	UpdateInfo.Type = FRHIResourceUpdateInfo::UT_Buffer;
	UpdateInfo.Buffer = { DestBuffer, SrcBuffer };
	DestBuffer->AddRef();
	if (SrcBuffer)
	{
		SrcBuffer->AddRef();
	}
}

void FRHIResourceUpdateBatcher::QueueUpdateRequest(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry)
{
	FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
	UpdateInfo.Type = FRHIResourceUpdateInfo::UT_RayTracingGeometry;
	UpdateInfo.RayTracingGeometry = { DestGeometry, SrcGeometry };
	DestGeometry->AddRef();
	if (SrcGeometry)
	{
		SrcGeometry->AddRef();
	}
}

void FRHIResourceUpdateBatcher::QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
{
	FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
	UpdateInfo.Type = FRHIResourceUpdateInfo::UT_BufferFormatSRV;
	UpdateInfo.BufferSRV = { SRV, Buffer, Stride, Format };
	SRV->AddRef();
	if (Buffer)
	{
		Buffer->AddRef();
	}
}

void FRHIResourceUpdateBatcher::QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer)
{
	FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
	UpdateInfo.Type = FRHIResourceUpdateInfo::UT_BufferSRV;
	UpdateInfo.BufferSRV = { SRV, Buffer };
	SRV->AddRef();
	if (Buffer)
	{
		Buffer->AddRef();
	}
}

FRHIResourceUpdateInfo& FRHIResourceUpdateBatcher::GetNextUpdateInfo()
{
	check(NumBatched <= UpdateInfos.Num());
	if (NumBatched >= UpdateInfos.Num())
	{
		Flush();
	}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6385) // Access is always in-bound due to the Flush above
#endif
	return UpdateInfos[NumBatched++];
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
