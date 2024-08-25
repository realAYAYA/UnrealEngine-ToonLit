// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"

class UCustomizableObjectInstance;
class UTexture2D;
struct FCustomizableObjectInstanceDescriptor;


/** Stat which automatically gets updated to Insights when modified. */
#define DECLARE_BENCHMARK_STAT(Name, Type) \
	struct F##Name \
	{ \
	private: \
		Type Value = 0; \
		\
		mutable FCriticalSection Lock##Name; \
		\
	public: \
		Type GetValue() const \
		{ \
			FScopeLock ScopedLock(&Lock##Name); \
			return Value; \
		} \
		\
		F##Name& operator+=(const Type& Rhs) \
		{ \
			FScopeLock ScopedLock(&Lock##Name); \
			Value += Rhs; \
			return *this; \
		} \
		\
		F##Name& operator=(const Type& Rhs) \
		{ \
			FScopeLock ScopedLock(&Lock##Name); \
			Value = Rhs; \
		\
		SET_DWORD_STAT(STAT_Mutable##Name, Value); \
		\
		return *this; \
		} \
	}; \
	F##Name Name;

#define DECLARE_BENCHMARK_INSIGHTS(Name, Description) \
	DECLARE_DWORD_ACCUMULATOR_STAT(Description, STAT_Mutable##Name, STATGROUP_Mutable);


class FUpdateContextPrivate;
// Mutable stats
DECLARE_STATS_GROUP(TEXT("Mutable"), STATGROUP_Mutable, STATCAT_Advanced);

DECLARE_BENCHMARK_INSIGHTS(NumAllocatedSkeletalMeshes, TEXT("Num Allocated Mutable Skeletal Meshes"));
DECLARE_BENCHMARK_INSIGHTS(NumAllocatedTextures, TEXT("Num Allocated Mutable Textures"));
DECLARE_BENCHMARK_INSIGHTS(TextureGPUSize, TEXT("Size of the Mutable Texture mips that are resident on the GPU"));
DECLARE_BENCHMARK_INSIGHTS(NumInstances, TEXT("Num Instances"));
DECLARE_BENCHMARK_INSIGHTS(NumInstancesLOD0, TEXT("Num Instances at LOD 0"));
DECLARE_BENCHMARK_INSIGHTS(NumInstancesLOD1, TEXT("Num Instances at LOD 1"));
DECLARE_BENCHMARK_INSIGHTS(NumInstancesLOD2, TEXT("Num Instances at LOD 2 or more"));
DECLARE_BENCHMARK_INSIGHTS(NumPendingInstanceUpdates, TEXT("Num Pending Instance Updates"));
DECLARE_BENCHMARK_INSIGHTS(NumBuiltInstances, TEXT("Num Built Instances"))
DECLARE_BENCHMARK_INSIGHTS(InstanceBuildTimeAvrg, TEXT("Avrg Instance Build Time"));


extern TAutoConsoleVariable<bool> CVarEnableBenchmark;


/** Benchmarking system. Gathers stats and send it to Insights an Benchmarking Files. */
class FLogBenchmarkUtil
{
public:
	~FLogBenchmarkUtil();

	/** Get stats. */
	void GetInstancesStats(int32& OutNumInstances, int32& OutNumBuiltInstances, int32& OutNumInstancesLOD0, int32& OutNumInstancesLOD1, int32& OutNumInstancesLOD2, int32& OutNumAllocatedSkeletalMeshes) const;

	/** Add Mutable created Texture to track. */
	void AddTexture(UTexture2D& Texture);

	/** Update stats which can only be updated on the tick. */
	void UpdateStats();

	/** Gathers update stats when it has finished. */
	void FinishUpdateMesh(const TSharedRef<FUpdateContextPrivate>& Context);

	void FinishUpdateImage(const FString& CustomizableObjectPathName, const FString& InstancePathName, const FString& InstanceDescriptor, double TaskUpdateImageTime, const int64 TaskUpdateImageMemoryPeak, const int64 TaskUpdateImageRealMemoryPeak) const;
	
	DECLARE_BENCHMARK_STAT(NumAllocatedTextures, uint32);
	DECLARE_BENCHMARK_STAT(TextureGPUSize, uint64);

	DECLARE_BENCHMARK_STAT(NumAllocatedSkeletalMeshes, int32);

	DECLARE_BENCHMARK_STAT(NumInstances, int32);
	DECLARE_BENCHMARK_STAT(NumInstancesLOD0, int32);
	DECLARE_BENCHMARK_STAT(NumInstancesLOD1, int32);
	DECLARE_BENCHMARK_STAT(NumInstancesLOD2, int32);

	DECLARE_BENCHMARK_STAT(NumPendingInstanceUpdates, int32)
	DECLARE_BENCHMARK_STAT(NumBuiltInstances, int32)
	DECLARE_BENCHMARK_STAT(InstanceBuildTimeAvrg, double);

	TArray<TWeakObjectPtr<UTexture2D>> TextureTrackerArray;

private:
	double TotalUpdateTime = 0;
	uint32 NumUpdates = 0;

	TSharedPtr<FArchive> Archive;
};
