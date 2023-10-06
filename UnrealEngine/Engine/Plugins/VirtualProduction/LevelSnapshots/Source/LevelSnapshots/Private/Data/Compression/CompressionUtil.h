// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FWorldSnapshotData;

namespace UE::LevelSnapshots
{
	/** Compresses the data when saving */
	void Compress(FArchive& Ar, FWorldSnapshotData& Data);

	/** Decompresses the data when loading */
	void Decompress(FArchive& Ar, FWorldSnapshotData& Data);
}
