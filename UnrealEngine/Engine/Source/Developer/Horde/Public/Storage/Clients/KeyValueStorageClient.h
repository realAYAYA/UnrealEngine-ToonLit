// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Storage/StorageClient.h"
#include "Storage/BlobHandle.h"

/*
 * Base class for storage clients that wrap a diirect key/value type store without any merging/splitting.
 */
class HORDE_API FKeyValueStorageClient : public FStorageClient
{
public:
	/** Read a single blob from the underlying store. */
	virtual FBlob ReadBlob(const FBlobLocator& Locator) const = 0;

	/** Write a single blob to the underlying store. */
	virtual FBlobHandle WriteBlob(const FUtf8StringView& BasePath, const FBlobType& Type, const TArrayView<FMemoryView>& Data, const TArrayView<FBlobHandle>& Imports) = 0;

	// Implementation of FStorageClient
	virtual FBlobHandle CreateHandle(const FBlobLocator& Locator) const override;
	virtual TUniquePtr<FBlobWriter> CreateWriter(FUtf8String BasePath = FUtf8String()) override;

private:
	class FHandleData;
};
