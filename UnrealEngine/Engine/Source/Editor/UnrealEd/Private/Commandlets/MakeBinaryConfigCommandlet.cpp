// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/MakeBinaryConfigCommandlet.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreGlobals.h"
#include "HAL/PlatformCrt.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

UMakeBinaryConfigCommandlet::UMakeBinaryConfigCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UMakeBinaryConfigCommandlet::Main(const FString& Params)
{
	FString OutputFile;
	if (!FParse::Value(FCommandLine::Get(), TEXT("OutputFile="), OutputFile))
	{
		UE_LOG(LogTemp, Fatal, TEXT("OutputFile= parameter required"));
		return -1;
	}

	FString StagedPluginsFile;
	if (!FParse::Value(FCommandLine::Get(), TEXT("StagedPluginsFile="), StagedPluginsFile))
	{
		UE_LOG(LogTemp, Fatal, TEXT("StagedPluginsFile= parameter required"));
		return -1;
	}

	// only expecting one targetplatform
	const TArray<ITargetPlatform*>& Platforms = GetTargetPlatformManagerRef().GetActiveTargetPlatforms();
	check(Platforms.Num() == 1);
	FString PlatformName = Platforms[0]->IniPlatformName();

	FConfigCacheIni Config(EConfigCacheType::Temporary);
	FConfigContext Context = FConfigContext::ReadIntoConfigSystem(&Config, PlatformName);
	Config.InitializeKnownConfigFiles(Context);

	// removing for now, because this causes issues with some plugins not getting ini files merged in
//	IPluginManager::Get().IntegratePluginsIntoConfig(Config, *FinalConfigFilenames.EngineIni, *PlatformName, *StagedPluginsFile);

	// pull out deny list entries

	TArray<FString> KeyDenyListStrings;
	TArray<FString> SectionsDenyList;
	GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("IniKeyDenylist"), KeyDenyListStrings, GGameIni);
	GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("IniSectionDenylist"), SectionsDenyList, GGameIni);
	TArray<FName> KeysDenyList;
	for (FString Key : KeyDenyListStrings)
	{
		KeysDenyList.Add(FName(*Key));
	}

	for (const FString& Filename : Config.GetFilenames())
	{
		FConfigFile* File = Config.FindConfigFile(Filename);

		delete File->SourceConfigFile;
		File->SourceConfigFile = nullptr;

		for (FString Section : SectionsDenyList)
		{
			File->Remove(Section);
		}

		// now go over any remaining sections and remove keys
		for (const TPair<FString, FConfigSection>& SectionPair : AsConst(*File))
		{
			for (FName Key : KeysDenyList)
			{
				File->RemoveKeyFromSection(*SectionPair.Key, Key);
			}
		}
	}

	// check the deny list removed itself
	KeyDenyListStrings.Empty();
	Config.GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("IniKeyDenylist"), KeyDenyListStrings, GGameIni);
	check(KeyDenyListStrings.Num() == 0);

	// allow delegates to modify the config data with some tagged binary data
	FCoreDelegates::FExtraBinaryConfigData ExtraData(Config, true);
	FCoreDelegates::TSAccessExtraBinaryConfigData().Broadcast(ExtraData);

	// write it all out!
	TArray<uint8> FileContent;
	{
		// Use FMemoryWriter because FileManager::CreateFileWriter doesn't serialize FName as string and is not overridable
		FMemoryWriter MemoryWriter(FileContent, true);

		Config.Serialize(MemoryWriter);
		MemoryWriter << ExtraData.Data;
	}

	if (!FFileHelper::SaveArrayToFile(FileContent, *OutputFile))
	{
		UE_LOG(LogTemp, Fatal, TEXT("Failed to create Config.bin file"));
		return -1;
	}

	return 0;
}
