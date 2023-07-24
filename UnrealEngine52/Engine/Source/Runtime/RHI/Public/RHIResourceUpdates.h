// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIFwd.h"
#include "Containers/ArrayView.h"

struct FRHIShaderResourceViewUpdateInfo
{
	FRHIShaderResourceView* SRV;
	FRHIBuffer* Buffer;
	uint32 Stride;
	uint8 Format;
};

struct FRHIBufferUpdateInfo
{
	FRHIBuffer* DestBuffer;
	FRHIBuffer* SrcBuffer;
};

struct FRHIRayTracingGeometryUpdateInfo
{
	FRHIRayTracingGeometry* DestGeometry;
	FRHIRayTracingGeometry* SrcGeometry;
};

struct FRHIResourceUpdateInfo
{
	enum EUpdateType
	{
		/** Take over underlying resource from an intermediate buffer */
		UT_Buffer,
		/** Update an SRV to view on a different buffer */
		UT_BufferSRV,
		/** Update an SRV to view on a different buffer with a given format */
		UT_BufferFormatSRV,
		/** Take over underlying resource from an intermediate geometry */
		UT_RayTracingGeometry,
		/** Number of update types */
		UT_Num
	};

	EUpdateType Type;
	union
	{
		FRHIBufferUpdateInfo Buffer;
		FRHIShaderResourceViewUpdateInfo BufferSRV;
		FRHIRayTracingGeometryUpdateInfo RayTracingGeometry;
	};

	void ReleaseRefs();
};

struct RHI_API FRHIResourceUpdateBatcher
{
	TArrayView<FRHIResourceUpdateInfo> UpdateInfos;
	int32 NumBatched = 0;

	~FRHIResourceUpdateBatcher();

	void Flush();

	void QueueUpdateRequest(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer);
	void QueueUpdateRequest(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry);
	void QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer, uint32 Stride, uint8 Format);
	void QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer);

protected:
	FRHIResourceUpdateBatcher() = delete;
	FRHIResourceUpdateBatcher(TArrayView<FRHIResourceUpdateInfo> InUpdateInfos);

private:
	FRHIResourceUpdateInfo& GetNextUpdateInfo();
};

template <uint32 MaxNumUpdates>
struct TRHIResourceUpdateBatcher : public FRHIResourceUpdateBatcher
{
	FRHIResourceUpdateInfo UpdateInfoStorage[MaxNumUpdates];

	TRHIResourceUpdateBatcher() : FRHIResourceUpdateBatcher(UpdateInfoStorage)
	{}
};
