// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>

/**
 * Experimental type for signaling cancellation of asynchronouse I/O operations.
 */
struct FIoCancellationToken
{
	FIoCancellationToken() = default;
	FIoCancellationToken(const FIoCancellationToken&) = delete;
	FIoCancellationToken(FIoCancellationToken&&) = delete;

	FIoCancellationToken& operator=(const FIoCancellationToken&) = delete;
	FIoCancellationToken& operator=(FIoCancellationToken&&) = delete;

	/** Returns true if the operation should cancel or not. */
	bool IsCancelled() const
	{ 
		return bCancellationRequested;
	}

	/** Signal the asynchronouse operation to cancel. */
	void Cancel()
	{
		bCancellationRequested = true;
	}

private:
	std::atomic_bool bCancellationRequested{false};
};
