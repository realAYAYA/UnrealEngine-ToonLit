// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

typedef TBitArray<SceneRenderingBitArrayAllocator> FSceneBitArray;
typedef TConstSetBitIterator<SceneRenderingBitArrayAllocator> FSceneSetBitIterator;
typedef TConstDualSetBitIterator<SceneRenderingBitArrayAllocator,SceneRenderingBitArrayAllocator> FSceneDualSetBitIterator;

// Forward declarations.
class FScene;

class FOcclusionQueryHelpers
{
public:

	enum
	{
		MaxBufferedOcclusionFrames = 4
	};

	// get the system-wide number of frames of buffered occlusion queries.
	static int32 GetNumBufferedFrames(ERHIFeatureLevel::Type FeatureLevel);

	// get the index of the oldest query based on the current frame and number of buffered frames.
	static uint32 GetQueryLookupIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}

	// get the index of the query to overwrite for new queries.
	static uint32 GetQueryIssueIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}
};
