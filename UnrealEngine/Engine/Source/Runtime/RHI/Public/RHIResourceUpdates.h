// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIFwd.h"
#include "Containers/ArrayView.h"
#include "RHIResources.h"

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
		/** Take over underlying resource from an intermediate geometry */
		UT_RayTracingGeometry,
		/** Number of update types */
		UT_Num
	};

	EUpdateType Type;
	union
	{
		FRHIBufferUpdateInfo Buffer;
		FRHIRayTracingGeometryUpdateInfo RayTracingGeometry;
	};

	void ReleaseRefs();
};

struct FRHIResourceUpdateBatcher
{
	TArrayView<FRHIResourceUpdateInfo> UpdateInfos;
	int32 NumBatched = 0;

	RHI_API ~FRHIResourceUpdateBatcher();

	RHI_API void Flush();

	RHI_API void QueueUpdateRequest(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer);
	RHI_API void QueueUpdateRequest(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry);

protected:
	FRHIResourceUpdateBatcher() = delete;
	RHI_API FRHIResourceUpdateBatcher(TArrayView<FRHIResourceUpdateInfo> InUpdateInfos);

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
