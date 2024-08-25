// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlobHandle.h"
#include "BlobType.h"
#include "../SharedBufferView.h"

/**
 * Describes a blob of data in the storage system
 */
struct HORDE_API FBlob
{
	/** Type of the blob. */
	FBlobType Type;

	/** Data for the blob. */
	FSharedBufferView Data;

	/** References to other blobs. */
	TArray<FBlobHandle> References;

	FBlob(const FBlobType& InType, FSharedBufferView InData, TArray<FBlobHandle> InReferences);
	~FBlob();
};
