// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "IElectraDecoderResourceDelegateBase.h"

class IElectraDecoderResourceDelegateAndroid : public IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateAndroid() = default;

	class IDecoderPlatformResourceAndroid : public IDecoderPlatformResource
	{
	public:
		class ISurfaceRequestCallback : public TSharedFromThis<ISurfaceRequestCallback, ESPMode::ThreadSafe>
		{
		public:
			virtual ~ISurfaceRequestCallback() = default;
			enum class ESurfaceType
			{
				NoSurface,		// No decoding surface. Must use CPU side buffer.
				Surface,
				SurfaceView,
				Error
			};
			virtual void OnNewSurface(ESurfaceType InSurfaceType, void* InSurface) = 0;
		};
		// The decoder calls this to request a surface onto which to decode.
		virtual void RequestSurface(TWeakPtr<ISurfaceRequestCallback, ESPMode::ThreadSafe> InRequestCallback) = 0;

		enum class ESurfaceChangeResult
		{
			NoChange,
			NewSurface,
			Error
		};
		/*
			Called by the decoder prior to decoding an access unit if the initial surface returned by RequestSurface()
			is a SurfaceView. If the SurfaceView has changed since it was initially returned the new SurfaceView must
			be set to `OutNewSurfaceView` and `NewSurface` returned.
			If there is no change returning `NoChange` is sufficient. In case of any error return `Error` to stop decoding.
		*/
		virtual ESurfaceChangeResult VerifySurfaceView(void*& OutNewSurfaceView, void* InCurrentSurfaceView) = 0;

		class IBufferReleaseCallback : public TSharedFromThis<IBufferReleaseCallback, ESPMode::ThreadSafe>
		{
		public:
			virtual ~IBufferReleaseCallback() = default;
			virtual void OnReleaseBuffer(int32 InBufferIndex, TOptional<int32> InBufferValidCount, TOptional<bool> InDoRender, TOptional<int64> InRenderAt) = 0;
		};
		// The decoder calls this to set the buffer release callback.
		virtual void SetBufferReleaseCallback(TWeakPtr<IBufferReleaseCallback, ESPMode::ThreadSafe> InBufferReleaseCallback) = 0;
	};
};

class IElectraDecoderVideoOutputCopyResources
{
public:
	virtual ~IElectraDecoderVideoOutputCopyResources() = default;
	virtual void SetBufferIndex(int32 InIndex) = 0;
	virtual void SetValidCount(int32 InValidCount) = 0;
	virtual bool ShouldReleaseBufferImmediately() = 0;
};


using IElectraDecoderResourceDelegate = IElectraDecoderResourceDelegateAndroid;
