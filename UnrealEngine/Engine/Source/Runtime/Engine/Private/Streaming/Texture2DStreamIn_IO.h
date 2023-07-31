// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.h: Stream in helper for 2D textures using texture streaming files.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DStreamIn.h"

class IBulkDataIORequest;

// Base StreamIn framework exposing MipData
class FTexture2DStreamIn_IO : public FTexture2DStreamIn
{
public:

	FTexture2DStreamIn_IO(UTexture2D* InTexture, bool InPrioritizedIORequest);
	~FTexture2DStreamIn_IO();

protected:

	// ****************************
	// ********* Helpers **********
	// ****************************

	// Set the IO requests for streaming the mips.
	void SetIORequests(const FContext& Context);
	// Cancel / destroy each requests created in SetIORequests()
	void ClearIORequests(const FContext& Context);
	// Report IO errors if any.
	void ReportIOError(const FContext& Context);
	// Set the IO callback used for streaming the mips.
	void SetAsyncFileCallback();
	// Cancel all IO requests.
	void CancelIORequests();

	// Start an async task to cancel pending IO requests.
	void Abort() override;

private:

	// Poll if any of the mips currently have an active IO request
	bool HasPendingIORequests();

	class FCancelIORequestsTask : public FNonAbandonableTask
	{
	public:
		FCancelIORequestsTask(FTexture2DStreamIn_IO* InPendingUpdate) : PendingUpdate(InPendingUpdate) {}
		void DoWork();
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FCancelIORequestsTask_Texture, STATGROUP_ThreadPoolAsyncTasks);
		}
	protected:
		TRefCountPtr<FTexture2DStreamIn_IO> PendingUpdate;
	};

	typedef FAutoDeleteAsyncTask<FCancelIORequestsTask> FAsyncCancelIORequestsTask;
	friend class FCancelIORequestsTask;


	// Request for loading into each mip.
	TArray<IBulkDataIORequest*, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > IORequests;

	// Whether an IO error was detected (when files do not exists).
	bool bFailedOnIOError = false;

	// Whether the IO request should be created with an higher priority for quicker response time.
	bool bPrioritizedIORequest = false;

	FBulkDataIORequestCallBack AsyncFileCallBack;
};

