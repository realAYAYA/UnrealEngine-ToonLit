// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocProfilerEx.cpp: Extended memory profiling support.
=============================================================================*/

#include "MallocProfilerEx.h"
#include "UObject/UObjectGlobals.h"
#include "RHI.h"

#if USE_MALLOC_PROFILER

#include "ProfilingDebugging/MallocProfiler.h"
#include "HAL/MemoryMisc.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "UObject/Package.h"

// These functions are here because FMallocProfiler is in the Core
// project, and therefore can't access most of the classes needed by these functions.

/**
 * Constructor, initializing all member variables and potentially loading symbols.
 *
 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
 */
FMallocProfilerEx::FMallocProfilerEx( FMalloc* InMalloc )
	: FMallocProfiler( InMalloc )
{
	// Add callbacks for garbage collection
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic( FMallocProfiler::SnapshotMemoryGCStart );
	FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic( FMallocProfiler::SnapshotMemoryGCEnd );
}

/** 
 * Writes names of currently loaded levels. 
 * Only to be called from within the mutex / scope lock of the FMallocProfiler.
 */
void FMallocProfilerEx::WriteLoadedLevels( UWorld* InWorld )
{
	uint16 NumLoadedLevels = 0;
	int64 NumLoadedLevelsPosition = BufferedFileWriter.Tell();
	BufferedFileWriter << NumLoadedLevels;

	if (InWorld)
	{
		// Write the name of the map.
		const FString MapName = InWorld->GetCurrentLevel()->GetOutermost()->GetName();
		int32 MapNameIndex = GetNameTableIndex( MapName );
		NumLoadedLevels ++;

		BufferedFileWriter << MapNameIndex;

		// Write out all of the fully loaded levels.
		for (ULevelStreaming* LevelStreaming : InWorld->GetStreamingLevels())
		{
			if ((LevelStreaming != nullptr)
				&& (LevelStreaming->GetWorldAssetPackageFName() != NAME_None)
				&& (LevelStreaming->GetWorldAssetPackageFName() != InWorld->GetOutermost()->GetFName())
				&& (LevelStreaming->GetLoadedLevel() != nullptr))
			{
				NumLoadedLevels++;

				int32 LevelPackageIndex = GetNameTableIndex(LevelStreaming->GetWorldAssetPackageFName());

				BufferedFileWriter << LevelPackageIndex;
			}
		}

		// Patch up the count.
		if (NumLoadedLevels > 0)
		{
			int64 EndPosition = BufferedFileWriter.Tell();
			BufferedFileWriter.Seek(NumLoadedLevelsPosition);
			BufferedFileWriter << NumLoadedLevels;
			BufferedFileWriter.Seek(EndPosition);
		}
	}
}

/** 
 * Gather texture memory stats. 
 */
void FMallocProfilerEx::GetTexturePoolSize( FGenericMemoryStats& out_Stats )
{
	FTextureMemoryStats Stats;

	if( GIsRHIInitialized )
	{
		RHIGetTextureMemoryStats(Stats);
	}

	const TCHAR* NAME_TextureAllocatedMemorySize = TEXT("Texture Allocated Memory Size");
	out_Stats.Add( NAME_TextureAllocatedMemorySize, Stats.AllocatedMemorySize );
}

#endif // USE_MALLOC_PROFILER
