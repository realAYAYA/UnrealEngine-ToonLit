// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "HAL/Platform.h"

/**
 * Class for utilizing memory in the stream cache budget for an unrelated, temporary audio-based feature.
 * Allows us to borrow from budgeted memory for audio features that would otherwise not fit in the overall memory budget.
 *
 * Usage:
 * - Create an instance of this class on an object or subsystem where want to track memory usage
 * - Memory usage will immediately be taken out of the Audio Stream Cache budget on construction of the object
 * - Update memory usage via this class as necessary.
 * - Deleting the instance will automatically reset the memory usage to 0
*/
class ENGINE_API FAudioStreamCacheMemoryHandle : public FNoncopyable
{
public:
	FAudioStreamCacheMemoryHandle(FName InFeatureName, uint64 InMemoryUseInBytes);
	
	~FAudioStreamCacheMemoryHandle();
	
	FORCEINLINE uint64 GetMemoryUseInBytes() const { return MemoryUseInBytes; } 
	FORCEINLINE FName GetFeatureName() const { return FeatureName; }

	void ResetMemoryUseInBytes(uint64 InMemoryUseInBytes);
	
private:
	const FName FeatureName;
	uint64 MemoryUseInBytes;
};