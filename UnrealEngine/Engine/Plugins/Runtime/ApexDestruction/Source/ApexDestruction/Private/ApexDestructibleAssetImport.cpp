// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApexDestructibleAssetImport.cpp:

	Creation of an APEX NxDestructibleAsset from a binary buffer.
	SkeletalMesh creation from an APEX NxDestructibleAsset.

	SkeletalMesh creation largely based on FbxSkeletalMeshImport.cpp

=============================================================================*/

#include "ApexDestructibleAssetImport.h"
#include "Logging/LogMacros.h"

#if WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogApexDestructibleAssetImport, Log, All);

#endif // WITH_EDITOR
