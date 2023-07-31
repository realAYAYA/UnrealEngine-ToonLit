// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetManager.h"

namespace Metasound
{
	namespace Frontend
	{
		namespace AssetTags
		{
			const FString ArrayDelim = TEXT(",");

			const FName AssetClassID = "AssetClassID";

#if WITH_EDITORONLY_DATA
			const FName IsPreset = "bIsPreset";
#endif // WITH_EDITORONLY_DATA

			const FName RegistryVersionMajor = "RegistryVersionMajor";
			const FName RegistryVersionMinor = "RegistryVersionMinor";

#if WITH_EDITORONLY_DATA
			const FName RegistryInputTypes = "RegistryInputTypes";
			const FName RegistryOutputTypes = "RegistryOutputTypes";
#endif // WITH_EDITORONLY_DATA
		} // namespace AssetTags

		IMetaSoundAssetManager* IMetaSoundAssetManager::Instance = nullptr;
	} // namespace Frontend
} // namespace Metasound
