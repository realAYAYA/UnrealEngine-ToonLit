// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchFeatureLevel.h"

static_assert((int32)BuildPatchServices::EFeatureLevel::Latest == 18, "Please add support for the extra values to the two functions below.");

BUILDPATCHSERVICES_API const TCHAR* BuildPatchServices::FeatureLevelToString(const BuildPatchServices::EFeatureLevel& FeatureLevel)
{
#define CASE_ENUM_TO_STR(Value) case BuildPatchServices::EFeatureLevel::Value: return TEXT("EFeatureLevel::") TEXT(#Value)
	switch (FeatureLevel)
	{
		CASE_ENUM_TO_STR(Original);
		CASE_ENUM_TO_STR(CustomFields);
		CASE_ENUM_TO_STR(StartStoringVersion);
		CASE_ENUM_TO_STR(DataFileRenames);
		CASE_ENUM_TO_STR(StoresIfChunkOrFileData);
		CASE_ENUM_TO_STR(StoresDataGroupNumbers);
		CASE_ENUM_TO_STR(ChunkCompressionSupport);
		CASE_ENUM_TO_STR(StoresPrerequisitesInfo);
		CASE_ENUM_TO_STR(StoresChunkFileSizes);
		CASE_ENUM_TO_STR(StoredAsCompressedUClass);
		CASE_ENUM_TO_STR(StoresChunkDataShaHashes);
		CASE_ENUM_TO_STR(StoresPrerequisiteIds);
		CASE_ENUM_TO_STR(StoredAsBinaryData);
		CASE_ENUM_TO_STR(VariableSizeChunks);
		CASE_ENUM_TO_STR(UsesRuntimeGeneratedBuildId);
		CASE_ENUM_TO_STR(UsesBuildTimeGeneratedBuildId);
		default: return TEXT("Invalid");
	}
#undef CASE_ENUM_TO_STR
}

BUILDPATCHSERVICES_API bool BuildPatchServices::FeatureLevelFromString(const TCHAR* FeatureLevelString, BuildPatchServices::EFeatureLevel& FeatureLevel)
{
#define RETURN_IF_EQUAL(Value) if (FCString::Stricmp(FeatureLevelString, TEXT(#Value)) == 0) { FeatureLevel = BuildPatchServices::EFeatureLevel::Value; return true; }
	if (FCString::Strnicmp(FeatureLevelString, TEXT("EFeatureLevel::"), 15) == 0)
	{
		FeatureLevelString += 15;
	}
	RETURN_IF_EQUAL(Original);
	RETURN_IF_EQUAL(CustomFields);
	RETURN_IF_EQUAL(StartStoringVersion);
	RETURN_IF_EQUAL(DataFileRenames);
	RETURN_IF_EQUAL(StoresIfChunkOrFileData);
	RETURN_IF_EQUAL(StoresDataGroupNumbers);
	RETURN_IF_EQUAL(ChunkCompressionSupport);
	RETURN_IF_EQUAL(StoresPrerequisitesInfo);
	RETURN_IF_EQUAL(StoresChunkFileSizes);
	RETURN_IF_EQUAL(StoredAsCompressedUClass);
	RETURN_IF_EQUAL(StoresChunkDataShaHashes);
	RETURN_IF_EQUAL(StoresPrerequisiteIds);
	RETURN_IF_EQUAL(StoredAsBinaryData);
	RETURN_IF_EQUAL(VariableSizeChunks);
	RETURN_IF_EQUAL(UsesRuntimeGeneratedBuildId);
	RETURN_IF_EQUAL(UsesBuildTimeGeneratedBuildId);
	RETURN_IF_EQUAL(Latest);
	RETURN_IF_EQUAL(LatestNoChunks);
	RETURN_IF_EQUAL(LatestJson);
	RETURN_IF_EQUAL(FirstOptimisedDelta);
	RETURN_IF_EQUAL(StoresUniqueBuildId);
	return false;
#undef RETURN_IF_EQUAL
}
