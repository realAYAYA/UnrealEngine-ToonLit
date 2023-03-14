// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DEPRECATED LandscapeRenderMobile.h: This file will be removed
=============================================================================*/
#pragma once
#pragma message("LandscapeRenderMobile.h is deprecated. Please remove all usage of LandscapeRenderMobile.h")

#include "CoreMinimal.h"
#include "RenderResource.h"

#define LANDSCAPE_MAX_ES_LOD_COMP	2
#define LANDSCAPE_MAX_ES_LOD		6

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBufferMobile : public FVertexBuffer
{
	TArray<uint8> VertexData;
	int32 DataSize;
public:

	/** Constructor. */
	FLandscapeVertexBufferMobile(TArray<uint8> InVertexData)
	:	VertexData(InVertexData)
	,	DataSize(InVertexData.Num())
	{
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBufferMobile()
	{
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI() override
	{
	}

	UE_DEPRECATED(5.1, "FLandscapeVertexBufferMobile is now deprecated and will be removed.")
	static LANDSCAPE_API void UpdateMemoryStat(int32 Delta) 
	{
	}
};
