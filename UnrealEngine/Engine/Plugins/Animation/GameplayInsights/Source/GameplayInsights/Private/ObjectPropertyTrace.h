// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"

#if WITH_ENGINE
#include "ObjectTrace.h"
#endif

#if WITH_ENGINE
#define OBJECT_PROPERTY_TRACE_ENABLED OBJECT_TRACE_ENABLED
#else
#define OBJECT_PROPERTY_TRACE_ENABLED 0
#endif

#if OBJECT_PROPERTY_TRACE_ENABLED

struct FObjectPropertyTrace
{
	/** Initialize object property tracing */
	static void Init();

	/** Shut down object property tracing */
	static void Destroy();

	/** Check whether object property tracing is enabled */
	static bool IsEnabled();

	/** Toggle registration for a UObject being traced by the system */
	static void ToggleObjectRegistration(const UObject* InObject);

	/** Register a UObject to be traced by the system */
	static void RegisterObject(const UObject* InObject);

	/** Unregister a UObject to be traced by the system */
	static void UnregisterObject(const UObject* InObject);

	/** Check whether an object is registered */
	static bool IsObjectRegistered(const UObject* InObject);
};

#endif
