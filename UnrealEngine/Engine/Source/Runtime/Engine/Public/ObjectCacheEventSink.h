// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

struct ENGINE_API FObjectCacheEventSink
{
	// For parallel processing.  Calling BeginQueueNotifyEvents will prevent notify events from being processed, an instead placed in a list for later processing.
	// While other threads are actively updating the events, the main thread should process those events using this ProcessQueuedNotifyEvents
	// When parallel processing is complete, calling EndQueueNotifyEvents will flush all events, and return to normal in place processing.
	static void BeginQueueNotifyEvents();
	static void ProcessQueuedNotifyEvents();
	static void EndQueueNotifyEvents();

	static void NotifyUsedMaterialsChanged_Concurrent(const UPrimitiveComponent* PrimitiveComponent, const TArray<UMaterialInterface*>& UsedMaterials);
	static void NotifyRenderStateChanged_Concurrent(UActorComponent*);
	static void NotifyReferencedTextureChanged_Concurrent(UMaterialInterface*);
	static void NotifyStaticMeshChanged_Concurrent(UStaticMeshComponent*);
	static void NotifyMaterialDestroyed_Concurrent(UMaterialInterface*);
};

#endif // #if WITH_EDITOR