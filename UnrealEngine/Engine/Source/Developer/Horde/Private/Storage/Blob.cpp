// Copyright Epic Games, Inc. All Rights Reserved.

#include "Storage/Blob.h"
#include "Storage/BlobHandle.h"

FBlob::FBlob(const FBlobType& InType, FSharedBufferView InData, TArray<FBlobHandle> InReferences)
	: Type(InType)
	, Data(MoveTemp(InData))
	, References(MoveTemp(InReferences))
{ }

FBlob::~FBlob()
{ }
