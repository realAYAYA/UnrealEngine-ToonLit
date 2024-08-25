// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Engine/StreamableManager.h"
#include "MuR/ExtensionData.h"
#include "MuR/ExtensionDataStreamer.h"
#include "Templates/SharedPointerFwd.h"

class UCustomizableObjectSystemPrivate;
class UCustomizableObject;

/** An implementation of mu::ExtensionDataStreamer designed to work with the Customizable Object integration */
class CUSTOMIZABLEOBJECT_API FUnrealExtensionDataStreamer : public mu::ExtensionDataStreamer
{
public:
	FUnrealExtensionDataStreamer(const TWeakObjectPtr<UCustomizableObjectSystemPrivate>& InSystemPrivateWeak);
	virtual ~FUnrealExtensionDataStreamer();

	/** Not copyable because TFuture isn't copyable. This shouldn't be limiting in practise. */
	FUnrealExtensionDataStreamer(const FUnrealExtensionDataStreamer& Other) = delete;
	FUnrealExtensionDataStreamer& operator=(const FUnrealExtensionDataStreamer& Other) = delete;

	/**
	 * Note that *only* the functions from the mu::ExtensionDataStreamer interface may be called
	 * from other threads. All other functions may only be called from the Game thread.
	 */

	/**
	 * Set the Customizable Object that is currently running.
	 *
	 * StartLoad calls are assumed to come from the object passed in here.
	 */
	void SetActiveObject(UCustomizableObject* InObject);

	/**
	 * Clear the active object
	 * 
	 * Call this after a Customizable Object has finished running to help detect cases where
	 * SetActiveObject wasn't called before running a different CO.
	 */
	void ClearActiveObject();

	/** Returns true if there are any loads requested by StartLoad that haven't yet completed. */
	bool AreAnyLoadsPending() const;

	/** Cancels all loads requested by StartLoad that haven't yet completed. */
	void CancelPendingLoads();

	/** mu::ExtensionDataStreamer interface */
	virtual mu::ExtensionDataPtr CloneExtensionData(const mu::ExtensionDataPtrConst& Source) override;
	
	virtual TSharedRef<const mu::FExtensionDataLoadHandle> StartLoad(
		const mu::ExtensionDataPtrConst& Data,
		TArray<mu::ExtensionDataPtrConst>& OutUnloadedConstants) override;

private:
	/** Queued up on the Game thread to start the async load */
	static TSharedPtr<FStreamableHandle> StartLoadOnGameThread(
		const TWeakObjectPtr<UCustomizableObjectSystemPrivate>& SystemPrivateWeak,
		const TWeakObjectPtr<UCustomizableObject>& ObjectToLoadFor,
		const TSharedRef<mu::FExtensionDataLoadHandle>& LoadHandle);

	/** Called when the async load has finished loading the Extension Data */
	void NotifyLoadCompleted(UCustomizableObject* Object, const TSharedRef<mu::FExtensionDataLoadHandle>& LoadHandle);

	TWeakObjectPtr<UCustomizableObjectSystemPrivate> SystemPrivate; // Can not be a TStrongObjectPtr since then it will create a cycle b TSharedPtr and UObjects.

	/** Guards PendingLoads, ShouldCancelPtr and ActiveObject */
	FCriticalSection* Mutex = nullptr;

	TWeakObjectPtr<UCustomizableObject> ActiveObject;
	
	struct FPendingLoad
	{
		FPendingLoad(
			const TSharedRef<mu::FExtensionDataLoadHandle>& InLoadHandle,
			TFuture<TSharedPtr<FStreamableHandle>>&& InTaskFuture)
			: LoadHandle(InLoadHandle)
			, TaskFuture(MoveTemp(InTaskFuture))
		{
		}

		FPendingLoad(const FPendingLoad& Other) = delete;
		FPendingLoad& operator=(const FPendingLoad& Other) = delete;

		TSharedRef<mu::FExtensionDataLoadHandle> LoadHandle;
		TFuture<TSharedPtr<FStreamableHandle>> TaskFuture;
	};
	/** All outstanding loads that haven't called NotifyLoadCompleted yet */
	TArray<FPendingLoad> PendingLoads;

	/**
	 * This is shared by all outstanding loads and can be used to cancel them if their Game thread
	 * task hasn't run yet.
	 */
	TSharedPtr<bool> ShouldCancelPtr;
};
