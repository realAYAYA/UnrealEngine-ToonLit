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

	virtual IDecoderPlatformResource* CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions)
	{ return nullptr; }
	virtual void ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy)
	{ }
};
