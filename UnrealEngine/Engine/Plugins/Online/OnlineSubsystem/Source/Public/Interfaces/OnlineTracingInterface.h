// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h" // IWYU pragma: keep
#include "Features/IModularFeature.h"

class FName;

class FOutputDevice;

/**
 * Online Tracing Interface
 */
class ONLINESUBSYSTEM_API IOnlineTracing
	: public IModularFeature
{
public:
	/**
	 * Starts a context
	 *
	 * @param ContextName user-specified context name.
	 */

	static void StartContext(FName ContextName);
	
	/**
	 * Ends a context
	 *
	 * @param ContextName user-specified context name.
	 */
	static void EndContext(FName ContextName);

	virtual bool GetUncompressedTracingLog(TArray<uint8>& OutLog) = 0;

	virtual FString GetFilename() = 0;

	static IOnlineTracing* GetTracingHelper();

protected:
	virtual void StartContextImpl(FName ContextName) = 0;

	virtual void EndContextImpl(FName ContextName) = 0;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Containers/Array.h"
#include "CoreMinimal.h"
#endif
