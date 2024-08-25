// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"


class IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateBase() = default;

	/**
	 * Interface class for a platform specific resource allocated when a decoder is created
	 * and released when it is destroyed.
	 */
	class IDecoderPlatformResource
	{
	public:
		virtual ~IDecoderPlatformResource() = default;
	};

	enum class EDecoderPlatformResourceType
	{
		Video
	};

	/**
	* Interface class to abstract async job synchronization primitives on platforms implementing them
	*/
	class IAsyncConsecutiveTaskSync
	{
	public:
		virtual ~IAsyncConsecutiveTaskSync() = default;
	protected:
		IAsyncConsecutiveTaskSync() = default;
	};

	virtual TSharedPtr<IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe> CreateAsyncConsecutiveTaskSync()
	{ return nullptr; }
	virtual bool RunCodeAsync(TFunction<void()>&& CodeToRun, IAsyncConsecutiveTaskSync* TaskSync = nullptr)
	{ return false; }

	virtual IDecoderPlatformResource* CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions)
	{ return nullptr; }
	virtual void ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy)
	{ }
};
