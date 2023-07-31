// Copyright Epic Games, Inc. All Rights Reserved.

#include "IScriptGeneratorPluginInterface.h"
#include "Exceptions.h"
#include "UnrealHeaderTool.h"
#include "Algo/FindSortedStringCaseInsensitive.h"

extern FString GManifestFilename; // This is a bit of a hack...

EBuildModuleType::Type EBuildModuleType::Parse(const TCHAR* Value)
{
	static const TCHAR* AlphabetizedTypes[] = {
		TEXT("EngineDeveloper"),
		TEXT("EngineEditor"),
		TEXT("EngineRuntime"),
		TEXT("EngineThirdParty"),
		TEXT("EngineUncooked"),
		TEXT("GameDeveloper"),
		TEXT("GameEditor"),
		TEXT("GameRuntime"),
		TEXT("GameThirdParty"),
		TEXT("GameUncooked"),
		TEXT("Program")
	};

	int32 TypeIndex = Algo::FindSortedStringCaseInsensitive(Value, AlphabetizedTypes);
	if (TypeIndex < 0)
	{
		FUHTMessage(GManifestFilename).Throwf(TEXT("Unrecognized EBuildModuleType name: %s"), Value);
	}

	static EBuildModuleType::Type AlphabetizedValues[] = {
		EngineDeveloper,
		EngineEditor,
		EngineRuntime,
		EngineThirdParty,
		EngineUncooked,
		GameDeveloper,
		GameEditor,
		GameRuntime,
		GameThirdParty,
		GameUncooked,
		Program
	};

	return AlphabetizedValues[TypeIndex];
}

EPackageOverrideType::Type EPackageOverrideType::Parse(const TCHAR* Value)
{
	static const TCHAR* AlphabetizedTypes[] = {
		TEXT("EditorOnly"),
		TEXT("EngineDeveloper"),
		TEXT("EngineUncookedOnly"),
		TEXT("GameDeveloper"),
		TEXT("GameUncookedOnly"),
		TEXT("None"),
	};

	int32 TypeIndex = Algo::FindSortedStringCaseInsensitive(Value, AlphabetizedTypes);
	if (TypeIndex < 0)
	{
		FUHTMessage(GManifestFilename).Throwf(TEXT("Unrecognized EPackageOverrideType name: %s"), Value);
	}

	static EPackageOverrideType::Type AlphabetizedValues[] = {
		EditorOnly,
		EngineDeveloper,
		EngineUncookedOnly,
		GameDeveloper,
		GameUncookedOnly,
		None
	};

	return AlphabetizedValues[TypeIndex];
}
