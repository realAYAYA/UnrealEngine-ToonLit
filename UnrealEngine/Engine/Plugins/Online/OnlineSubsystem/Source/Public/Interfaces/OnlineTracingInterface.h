// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

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
