// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PackFactory.cpp: Factory for importing asset and feature packs
=============================================================================*/

#include "Factories/PackFactory.h"
#include "HAL/PlatformFileManager.h"
#include "Math/GuardedInt.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Compression.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/FeedbackContext.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/LinkerLoad.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "IPlatformFilePak.h"
#include "SourceCodeNavigation.h"
#include "Misc/HotReloadInterface.h"
#include "Misc/AES.h"
#include "GameProjectGenerationModule.h"
#include "Dialogs/SOutputLogDialog.h"
#include "Templates/UniquePtr.h"
#include "Logging/MessageLog.h"
#include "Misc/CoreDelegates.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPackFactory, Log, All);

UPackFactory::UPackFactory(const FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Since this factory can output multiple and any number of class it doesn't really have a 
	// SupportedClass per say, but one must be defined, so we just reference ourself
	SupportedClass = UPackFactory::StaticClass();

	Formats.Add(TEXT("upack;Asset Pack"));
	Formats.Add(TEXT("upack;Feature Pack"));

	bEditorImport = true;
}

namespace PackFactoryHelper
{
	// Utility function to copy a single pak entry out of the Source archive and in to the Destination archive using Buffer as temporary space
	bool BufferedCopyFile(FArchive& DestAr, FArchive& Source, const FPakEntry& Entry, TArray<uint8>& Buffer, const FPakFile& PakFile)
	{	
		// Align down
		const int64 BufferSize = Buffer.Num() & ~(FAES::AESBlockSize - 1);
		int64 RemainingSizeToCopy = Entry.Size;
		while (RemainingSizeToCopy > 0)
		{
			const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
			// If file is encrypted so we need to account for padding
			int64 SizeToRead = Entry.IsEncrypted() ? Align(SizeToCopy, FAES::AESBlockSize) : SizeToCopy;

			Source.Serialize(Buffer.GetData(), SizeToRead);
			if (Entry.IsEncrypted())
			{
				FAES::FAESKey Key;
				FPakPlatformFile::GetPakEncryptionKey(Key, PakFile.GetInfo().EncryptionKeyGuid);
				checkf(Key.IsValid(), TEXT("Trying to copy an encrypted file between pak files, but no decryption key is available"));
				FAES::DecryptData(Buffer.GetData(), IntCastChecked<uint32>(SizeToRead), Key);
			}
			DestAr.Serialize(Buffer.GetData(), SizeToCopy);
			RemainingSizeToCopy -= SizeToRead;
		}
		return true;
	}

	// Utility function to uncompress and copy a single pak entry out of the Source archive and in to the Destination archive using PersistentBuffer as temporary space
	bool UncompressCopyFile(FArchive& DestAr, FArchive& Source, const FPakEntry& Entry, TArray<uint8>& PersistentBuffer, const FPakFile& PakFile)
	{
		// Entry is untrusted data.
		if (Entry.UncompressedSize <= 0)
		{
			return false;
		}

		TOptional<FName> CompressionMethod = PakFile.GetInfo().TryGetCompressionMethod(Entry.CompressionMethodIndex);
		if (CompressionMethod.IsSet() == false)
		{
			return false;
		}

		FGuardedInt32 GuardedWorkingSize = FGuardedInt32(Entry.CompressionBlockSize);
		int32 MaxCompressionBlockSize = FCompression::GetMaximumCompressedSize(CompressionMethod.GetValue(), GuardedWorkingSize.Get(0));
		if (MaxCompressionBlockSize < 0)
		{
			return false;
		}

		GuardedWorkingSize += MaxCompressionBlockSize;
		int32 WorkingSize = GuardedWorkingSize.Get(0);
		if (WorkingSize <= 0)
		{
			return false;
		}

		// WorkingSize is now a sanitized int32 value.
		if (PersistentBuffer.Num() < WorkingSize)
		{
			PersistentBuffer.SetNumUninitialized(WorkingSize);
		}

		for (uint32 BlockIndex = 0, BlockIndexNum = Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
		{
			FGuardedInt64 CompressedBlockSize64 = FGuardedInt64(Entry.CompressionBlocks[BlockIndex].CompressedEnd) - Entry.CompressionBlocks[BlockIndex].CompressedStart;
			if (CompressedBlockSize64.ValidAndGreaterThan(0) == false ||
				IntFitsIn<int32>(CompressedBlockSize64.Get(0)) == false)
			{
				return false;
			}

			int32 CompressedBlockSize = static_cast<int32>(CompressedBlockSize64.Get(0));
			// CompressedBlockSize now sanitized


			FGuardedInt64 UncompressedBlockSize64 = FGuardedInt64(Entry.UncompressedSize) - FGuardedInt64(Entry.CompressionBlockSize) * BlockIndex;
			if (UncompressedBlockSize64.ValidAndGreaterThan(0) == false ||
				IntFitsIn<uint32>(UncompressedBlockSize64.Get(0)) == false)
			{
				return false;
			}

			// CompressionBlockSize is guaranteed to fit in int32 from earlier, so we know after the Min() we can fit in uint32
			uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(UncompressedBlockSize64.Get(0), Entry.CompressionBlockSize);

			if (Entry.Offset < 0)
			{
				return false;
			}

			FGuardedInt64 OffsetInPak = FGuardedInt64(Entry.CompressionBlocks[BlockIndex].CompressedStart) + (PakFile.GetInfo().HasRelativeCompressedChunkOffsets() ? Entry.Offset : 0);
			if (OffsetInPak.IsValid() == false)
			{
				return false;
			}

			Source.Seek(OffsetInPak.Get(0));
			int32 SizeToRead = Entry.IsEncrypted() ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
			if (SizeToRead > PersistentBuffer.Num())
			{
				// Shouldn't ever happen but I think this is possible if we are uncompressed and for some reason the block size isn't aligned to AESBlockSize.
				return false;
			}
			Source.Serialize(PersistentBuffer.GetData(), SizeToRead);

			if (Entry.IsEncrypted())
			{
				FAES::FAESKey Key;
				FPakPlatformFile::GetPakEncryptionKey(Key, PakFile.GetInfo().EncryptionKeyGuid);
				checkf(Key.IsValid(), TEXT("Trying to copy an encrypted file between pak files, but no decryption key is available"));
				FAES::DecryptData(PersistentBuffer.GetData(), SizeToRead, Key);
			}

			uint8* UncompressedBuffer = PersistentBuffer.GetData() + MaxCompressionBlockSize;
			if (!FCompression::UncompressMemory(CompressionMethod.GetValue(), UncompressedBuffer, UncompressedBlockSize, PersistentBuffer.GetData(), CompressedBlockSize))
			{
				return false;
			}
			DestAr.Serialize(UncompressedBuffer, UncompressedBlockSize);
		}

		return true;
	}

	// Utility function to extract a pak entry out of the memory reader containing the pak file and place in the destination archive.
	// Uses Buffer or PersistentCompressionBuffer depending on whether the entry is compressed or not.
	void ExtractFile(const FPakEntry& Entry, FBufferReader& PakReader, TArray<uint8>& Buffer, TArray<uint8>& PersistentCompressionBuffer, FArchive& DestAr, const FPakFile& PakFile)
	{
		// 0 is uncompressed
		if (Entry.CompressionMethodIndex == 0)
		{
			PackFactoryHelper::BufferedCopyFile(DestAr, PakReader, Entry, Buffer, PakFile);
		}
		else
		{
			PackFactoryHelper::UncompressCopyFile(DestAr, PakReader, Entry, PersistentCompressionBuffer, PakFile);
		}
	}

	// Utility function to extract a pak entry out of the memory reader containing the pak file and place in a string.
	// Uses Buffer or PersistentCompressionBuffer depending on whether the entry is compressed or not.
	void ExtractFileToString(const FPakEntry& Entry, FBufferReader& PakReader, TArray<uint8>& Buffer, TArray<uint8>& PersistentCompressionBuffer, FString& FileContents, const FPakFile& PakFile)
	{
		TArray<uint8> Contents;
		FMemoryWriter MemWriter(Contents);

		ExtractFile(Entry, PakReader, Buffer, PersistentCompressionBuffer, MemWriter, PakFile);

		// Add a line feed at the end because the FString archive read will consume the last byte
		Contents.Add('\n');

		// Insert the length of the string to the front of the memory chunk so we can use FString archive read
		const int32 StringLength = Contents.Num();
		Contents.InsertUninitialized(0,sizeof(int32));
		*(reinterpret_cast<int32*>(Contents.GetData())) = StringLength;

		FMemoryReader MemReader(Contents);
		MemReader << FileContents;
	}

	struct FPackConfigParameters
	{
		FPackConfigParameters()
			: bContainsSource(false)
			, bCompileSource(true)
		{
		}

		uint8 bContainsSource:1;
		uint8 bCompileSource:1;
		FString GameName;
		FString InstallMessage;
		TArray<FString> AdditionalFilesToAdd;
	};

	// Takes a string that represents the contents of a config file and sets up the supported config properties based on it
	// Currently we support Action and Axis Mappings and a GameName (for setting up redirects)
	void ProcessPackConfig(const FString& ConfigString, FPackConfigParameters& ConfigParameters)
	{
		FConfigFile PackConfig;
		PackConfig.ProcessInputFileContents(ConfigString, TEXT("Unknown (see PackFactoryHelper::ProcessPackConfig)"));

		// Input Settings
		static FArrayProperty* ActionMappingsProp = FindFieldChecked<FArrayProperty>(UInputSettings::StaticClass(), UInputSettings::GetActionMappingsPropertyName());
		static FArrayProperty* AxisMappingsProp = FindFieldChecked<FArrayProperty>(UInputSettings::StaticClass(), UInputSettings::GetAxisMappingsPropertyName());

		UInputSettings* InputSettingsCDO = GetMutableDefault<UInputSettings>();
		bool bCheckedOut = false;

		const FConfigSection* InputSettingsSection = PackConfig.FindSection("InputSettings");
		if (InputSettingsSection)
		{
			TArray<FInputActionKeyMapping> ActionMappingsToAdd;
			TArray<FInputAxisKeyMapping> AxisMappingsToAdd;

			for (auto SettingPair : *InputSettingsSection)
			{
				

				if (SettingPair.Key.ToString().Contains("ActionMappings"))
				{
					FInputActionKeyMapping ActionKeyMapping;
					ActionMappingsProp->Inner->ImportText_Direct(*SettingPair.Value.GetValue(), &ActionKeyMapping, nullptr, PPF_None);

					if (!InputSettingsCDO->DoesActionExist(ActionKeyMapping.ActionName))
					{
						ActionMappingsToAdd.Add(ActionKeyMapping);
					}
				}
				else if (SettingPair.Key.ToString().Contains("AxisMappings"))
				{
					FInputAxisKeyMapping AxisKeyMapping;
					AxisMappingsProp->Inner->ImportText_Direct(*SettingPair.Value.GetValue(), &AxisKeyMapping, nullptr, PPF_None);

					if (!InputSettingsCDO->DoesAxisExist(AxisKeyMapping.AxisName))
					{
						AxisMappingsToAdd.Add(AxisKeyMapping);
					}
				}
			}

			if (ActionMappingsToAdd.Num() > 0 || AxisMappingsToAdd.Num() > 0)
			{
				if (ISourceControlModule::Get().IsEnabled())
				{
					FText ErrorMessage;

					const FString InputSettingsFilename = FPaths::ConvertRelativePathToFull(InputSettingsCDO->GetDefaultConfigFilename());
					if (!SourceControlHelpers::CheckoutOrMarkForAdd(InputSettingsFilename, FText::FromString(InputSettingsFilename), NULL, ErrorMessage))
					{
						UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
					}
				}

				for (const FInputActionKeyMapping& ActionKeyMapping : ActionMappingsToAdd)
				{
					InputSettingsCDO->AddActionMapping(ActionKeyMapping);
				}
				for (const FInputAxisKeyMapping& AxisKeyMapping : AxisMappingsToAdd)
				{
					InputSettingsCDO->AddAxisMapping(AxisKeyMapping);
				}
					
				InputSettingsCDO->SaveKeyMappings();
				InputSettingsCDO->TryUpdateDefaultConfigFile();
			}
		}

		const FConfigSection* RedirectsSection = PackConfig.FindSection("Redirects");
		if (RedirectsSection)
		{	
			if (const FConfigValue* GameName = RedirectsSection->Find("GameName"))
			{
				ConfigParameters.GameName = GameName->GetValue();
			}
		}

		const FConfigSection* AdditionalFilesSection = PackConfig.FindSection("AdditionalFilesToAdd");
		if (AdditionalFilesSection)
		{
			for (auto FilePair : *AdditionalFilesSection)
			{
				if (FilePair.Key.ToString().Contains("Files"))
				{
					FString Filename = FPaths::GetCleanFilename(FilePair.Value.GetValue());
					FString Directory = FPaths::RootDir() / FPaths::GetPath(FilePair.Value.GetValue());
					FPaths::MakeStandardFilename(Directory);
					FPakFile::MakeDirectoryFromPath(Directory);

					if (Filename.Contains(TEXT("*")))
					{
						TArray<FString> FoundFiles;
						IFileManager::Get().FindFilesRecursive(FoundFiles, *Directory, *Filename, true, false);
						ConfigParameters.AdditionalFilesToAdd.Append(FoundFiles);
						if (!ConfigParameters.bContainsSource)
						{
							for (const FString& FoundFile : FoundFiles)
							{
								if (FoundFile.StartsWith(TEXT("Source/")) || FoundFile.Contains(TEXT("/Source/")))
								{
									ConfigParameters.bContainsSource = true;
									break;
								}
							}
						}
					}
					else
					{
						ConfigParameters.AdditionalFilesToAdd.Add(Directory / Filename);
						if (!ConfigParameters.bContainsSource && (ConfigParameters.AdditionalFilesToAdd.Last().StartsWith(TEXT("Source/")) || ConfigParameters.AdditionalFilesToAdd.Last().Contains(TEXT("/Source/"))))
						{
							ConfigParameters.bContainsSource = true;
						}
					}
				}
			}
		}

		const FConfigSection* FeaturePackSettingsSection = PackConfig.FindSection("FeaturePackSettings");
		if (FeaturePackSettingsSection)
		{
			if (const FConfigValue* CompileSource = FeaturePackSettingsSection->Find("CompileSource"))
			{
				ConfigParameters.bCompileSource = FCString::ToBool(*CompileSource->GetValue());
			}
			if (const FConfigValue* InstallMessage = FeaturePackSettingsSection->Find("InstallMessage"))
			{
				ConfigParameters.InstallMessage = InstallMessage->GetValue();
			}
		}
	}
}

UObject* UPackFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn
)
{ 
	FBufferReader PakReader((void*)Buffer, BufferEnd-Buffer, false);
	TRefCountPtr<FPakFile> PakFilePtr = new FPakFile(&PakReader);
	FPakFile& PakFile = *PakFilePtr;

	UObject* ReturnAsset = nullptr;

	if (PakFile.IsValid() && PakFile.HasFilenames())
	{
		static FString ContentFolder(TEXT("/Content/"));
		FString ContentDestinationRoot = FPaths::ProjectContentDir();

		const int32 ChopIndex = PakFile.GetMountPoint().Find(ContentFolder);
		if (ChopIndex != INDEX_NONE)
		{
			ContentDestinationRoot /= PakFile.GetMountPoint().RightChop(ChopIndex + ContentFolder.Len());
		}

		TArray<uint8> CopyBuffer;
		TArray<uint8> PersistentCompressionBuffer;
		CopyBuffer.AddUninitialized(8 * 1024 * 1024); // 8MB buffer for extracting
		int32 ErrorCount = 0;
		int32 FileCount = 0;

		FModuleContextInfo SourceModuleInfo;
		PackFactoryHelper::FPackConfigParameters ConfigParameters;

		TArray<FString> WrittenFiles;
		TArray<FString> WrittenSourceFiles;

		// Process the config files and identify if we have source files
		for (FPakFile::FPakEntryIterator It(PakFile); It; ++It, ++FileCount)
		{
			const FString* EntryFilename = It.TryGetFilename();
			check(EntryFilename);
			if (EntryFilename->StartsWith(TEXT("Config/")) || EntryFilename->Contains(TEXT("/Config/")))
			{
				const FPakEntry& Entry = It.Info();
				PakReader.Seek(Entry.Offset);
				FPakEntry EntryInfo;
				EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);

				if (EntryInfo.IndexDataEquals(Entry))
				{
					FString ConfigString;
					PackFactoryHelper::ExtractFileToString(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, ConfigString, PakFile);
					PackFactoryHelper::ProcessPackConfig(ConfigString, ConfigParameters);
				}
				else
				{
					UE_LOG(LogPackFactory, Error, TEXT("Index data mismatch for entry: \"%s\"."), **EntryFilename);
					ErrorCount++;
				}
			}
			else if (!ConfigParameters.bContainsSource && (EntryFilename ->StartsWith(TEXT("Source/")) || EntryFilename->Contains(TEXT("/Source/"))))
			{
				ConfigParameters.bContainsSource = true;
			}
		}

		bool bProjectHadSourceFiles = false;

		// If we have source files, set up the project files if necessary and the game name redirects for blueprints saved with class
		// references to the module name from the source template
		if (ConfigParameters.bContainsSource)
		{
			FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
			bProjectHadSourceFiles = GameProjectModule.Get().ProjectHasCodeFiles();

			if (!bProjectHadSourceFiles)
			{
				TArray<FString> StartupModuleNames;
				TArray<FString> CreatedFiles;
				FText OutFailReason;
				if ( GameProjectModule.Get().GenerateBasicSourceCode(CreatedFiles, OutFailReason) )
				{
					WrittenFiles.Append(CreatedFiles);
				}
				else
				{
					UE_LOG(LogPackFactory, Error, TEXT("Unable to create basic source code: '%s'"), *OutFailReason.ToString());
				}
			}

			for (const FModuleContextInfo& ModuleInfo : GameProjectModule.Get().GetCurrentProjectModules())
			{
				// Pick the module to insert the code in.  For now always pick the first Runtime module
				if (ModuleInfo.ModuleType == EHostType::Runtime)
				{
					SourceModuleInfo = ModuleInfo;

					// Setup the game name redirect
					if (!ConfigParameters.GameName.IsEmpty())
					{
						const FString EngineIniFilename = FPaths::ConvertRelativePathToFull(GetDefault<UEngine>()->GetDefaultConfigFilename());

						if (ISourceControlModule::Get().IsEnabled())
						{
							FText ErrorMessage;

							if (!SourceControlHelpers::CheckoutOrMarkForAdd(EngineIniFilename, FText::FromString(EngineIniFilename), NULL, ErrorMessage))
							{
								UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
							}
						}

						const FString RedirectsSection(TEXT("/Script/Engine.Engine"));
						const FString LongOldGameName = FString::Printf(TEXT("/Script/%s"), *ConfigParameters.GameName);
						const FString LongNewGameName = FString::Printf(TEXT("/Script/%s"), *ModuleInfo.ModuleName);
						
						FConfigCacheIni Config(EConfigCacheType::Temporary);
						FConfigFile& NewFile = Config.Add(EngineIniFilename, FConfigFile());
						FConfigCacheIni::LoadLocalIniFile(NewFile, TEXT("DefaultEngine"), false);

						NewFile.AddToSection(*RedirectsSection, TEXT("+ActiveGameNameRedirects"), FString::Printf(TEXT("(OldGameName=\"%s\",NewGameName=\"%s\")"), *LongOldGameName, *LongNewGameName));
						NewFile.AddToSection(*RedirectsSection, TEXT("+ActiveGameNameRedirects"), FString::Printf(TEXT("(OldGameName=\"%s\",NewGameName=\"%s\")"), *ConfigParameters.GameName, *LongNewGameName));

						NewFile.UpdateSections(*EngineIniFilename, *RedirectsSection);

						FConfigContext::ForceReloadIntoGConfig().Load(*RedirectsSection);

						FLinkerLoad::AddGameNameRedirect(*LongOldGameName, *LongNewGameName);
						FLinkerLoad::AddGameNameRedirect(*ConfigParameters.GameName, *LongNewGameName);
					}
					break;
				}
			}
		}

		// Process everything else and copy out to disk
		for (FPakFile::FPakEntryIterator It(PakFile); It; ++It, ++FileCount)
		{
			const FString* EntryFilename = It.TryGetFilename();
			check(EntryFilename);
			// config files already handled
			if (EntryFilename->StartsWith(TEXT("Config/")) || EntryFilename->Contains(TEXT("/Config/")))
			{
				continue;
			}

			// Media and manifest files don't get written out as part of the install
			if (EntryFilename->Contains(TEXT("manifest.json")) || EntryFilename->StartsWith(TEXT("Media/")) || EntryFilename->Contains(TEXT("/Media/")))
			{
				continue;
			}

			const FPakEntry& Entry = It.Info();
			PakReader.Seek(Entry.Offset);
			FPakEntry EntryInfo;
			EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);

			if (EntryInfo.IndexDataEquals(Entry))
			{
				if (EntryFilename->StartsWith(TEXT("Source/")) || EntryFilename->Contains(TEXT("/Source/")))
				{
					FString DestFilename = *EntryFilename;
					if (DestFilename.StartsWith(TEXT("Source/")))
					{
						DestFilename.RightChopInline(7, EAllowShrinking::No);
					}
					else 
					{
						const int32 SourceIndex = DestFilename.Find(TEXT("/Source/"));
						if (SourceIndex != INDEX_NONE)
						{
							DestFilename.RightChopInline(SourceIndex + 8, EAllowShrinking::No);
						}
					}

					DestFilename = SourceModuleInfo.ModuleSourcePath / DestFilename;
					UE_LOG(LogPackFactory, Log, TEXT("%s (%ld) -> %s"), **EntryFilename, Entry.Size, *DestFilename);

					FString SourceContents;
					PackFactoryHelper::ExtractFileToString(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, SourceContents, PakFile);

					FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));

					// Add the PCH for the project above the default pack include
					const FString StringToReplace = FString::Printf(TEXT("%s.h"),*ConfigParameters.GameName);
					const FString StringToReplaceWith = FString::Printf(TEXT("%s\"%s#include \"%s"),
						*GameProjectModule.Get().DetermineModuleIncludePath(SourceModuleInfo, DestFilename),
						LINE_TERMINATOR,
						*StringToReplace);

					if (FFileHelper::SaveStringToFile(SourceContents, *DestFilename))
					{
						WrittenFiles.Add(*DestFilename);
						WrittenSourceFiles.Add(*DestFilename);
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to write file \"%s\"."), *DestFilename);
						++ErrorCount;
					}
				}
				else
				{
					FString DestFilename = *EntryFilename;
					if (DestFilename.StartsWith(TEXT("Content/")))
					{
						DestFilename.RightChopInline(8, EAllowShrinking::No);
					}
					else
					{
						const int32 ContentIndex = DestFilename.Find(ContentFolder);
						if (ContentIndex != INDEX_NONE)
						{
							DestFilename.RightChopInline(ContentIndex + 9, EAllowShrinking::No);
						}
					}
					DestFilename = ContentDestinationRoot / DestFilename;
					UE_LOG(LogPackFactory, Log, TEXT("%s (%ld) -> %s"), **EntryFilename, Entry.Size, *DestFilename);

					TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));

					if (FileHandle)
					{
						PackFactoryHelper::ExtractFile(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, *FileHandle, PakFile);
						WrittenFiles.Add(*DestFilename);
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
						++ErrorCount;
					}
				}
			}
			else
			{
				UE_LOG(LogPackFactory, Error, TEXT("Index data mismatch for entry: \"%s\"."), **EntryFilename);
				ErrorCount++;
			}
		}

		UE_LOG(LogPackFactory, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);

		if (ConfigParameters.AdditionalFilesToAdd.Num() > 0)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			for (const FString& FileToCopy : ConfigParameters.AdditionalFilesToAdd)
			{
				if (FileToCopy.StartsWith(TEXT("Source/")) || FileToCopy.Contains(TEXT("/Source/")))
				{
					FString DestFilename = FileToCopy;
					if (DestFilename.StartsWith(TEXT("Source/")))
					{
						DestFilename.RightChopInline(7, EAllowShrinking::No);
					}
					else 
					{
						const int32 SourceIndex = DestFilename.Find(TEXT("/Source/"));
						if (SourceIndex != INDEX_NONE)
						{
							DestFilename.RightChopInline(SourceIndex + 8, EAllowShrinking::No);
						}
					}
					DestFilename = SourceModuleInfo.ModuleSourcePath / DestFilename;

					FString DestDirectory = FPaths::GetPath(DestFilename);

					if (PlatformFile.CreateDirectoryTree(*DestDirectory))
					{
						FString SourceContents;
						if (FFileHelper::LoadFileToString(SourceContents, *FileToCopy))
						{
							FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
							
							// Add the PCH for the project above the default pack include
							const FString StringToReplace = FString::Printf(TEXT("%s.h"),*ConfigParameters.GameName);
							const FString StringToReplaceWith = FString::Printf(TEXT("%s\"%s#include \"%s"),
								*GameProjectModule.Get().DetermineModuleIncludePath(SourceModuleInfo, DestFilename),
								LINE_TERMINATOR,
								*StringToReplace);

							SourceContents = SourceContents.Replace(*StringToReplace, *StringToReplaceWith, ESearchCase::CaseSensitive);

							if (FFileHelper::SaveStringToFile(SourceContents, *DestFilename))
							{
								WrittenFiles.Add(*DestFilename);
								WrittenSourceFiles.Add(*DestFilename);
							}
							else
							{
								UE_LOG(LogPackFactory, Error, TEXT("Unable to write file \"%s\"."), *DestFilename);
								++ErrorCount;
							}
						}
						else
						{
							UE_LOG(LogPackFactory, Error, TEXT("Unable to read file \"%s\"."), *FileToCopy);
						}
					}
				}
				else
				{
					FString DestFilename = FileToCopy;
					if (DestFilename.StartsWith(TEXT("Content/")))
					{
						DestFilename.RightChopInline(8, EAllowShrinking::No);
					}
					else
					{
						const int32 ContentIndex = DestFilename.Find(ContentFolder);
						if (ContentIndex != INDEX_NONE)
						{
							DestFilename.RightChopInline(ContentIndex + 9, EAllowShrinking::No);
						}
					}
					DestFilename = ContentDestinationRoot / DestFilename;

					FString DestDirectory = FPaths::GetPath(DestFilename);

					if (PlatformFile.CreateDirectoryTree(*DestDirectory))
					{
						if (PlatformFile.CopyFile(*DestFilename, *FileToCopy))
						{
							WrittenFiles.Add(DestFilename);
							UE_LOG(LogPackFactory, Log, TEXT("Copied \"%s\" to \"%s\""), *FileToCopy, *DestFilename);
						}
						else
						{
							UE_LOG(LogPackFactory, Error, TEXT("Unable to copy file \"%s\" to \"%s\"."), *FileToCopy, *DestFilename);
						}
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to create directory \"%s\"."), *FileToCopy, *DestFilename);
					}
				}
			}
		}

		if (WrittenFiles.Num() > 0)
		{
			// If we wrote out source files, kick off the hot reload process
			if (WrittenSourceFiles.Num() > 0)
			{
				// Update the game projects before we attempt to build
				FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
				FText FailReason, FailLog;
				if (!GameProjectModule.UpdateCodeProject(FailReason, FailLog))
				{
					SOutputLogDialog::Open(NSLOCTEXT("PackFactory", "CreateBinary", "Create binary"), FailReason, FailLog, FText::GetEmpty());
				}

				bool bCompileSource = ConfigParameters.bCompileSource;
#if WITH_LIVE_CODING
				if (bCompileSource)
				{
					ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
					if (LiveCoding != nullptr && LiveCoding->IsEnabledForSession())
					{
						if (bProjectHadSourceFiles)
						{
							if (!LiveCoding->Compile(ELiveCodingCompileFlags::WaitForCompletion, nullptr))
							{
								FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("PackFactory", "LiveCodingFailedToCompile", "Failed to compile sources, please close the editor and build from your IDE."));
							}
						}
						else
						{
							FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("PackFactory", "LiveCodingNoSources", "Project now includes sources, please close the editor and build from your IDE."));
						}

						// Don't allow hot-reload to try to compile
						bCompileSource = false;
					}
				}
#endif
				if (bCompileSource)
				{
					// Compile the new code, either using the in editor hot-reload (if an existing module), or as a brand new module (if no existing code)
					IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
					if (bProjectHadSourceFiles)
					{
						// We can only hot-reload via DoHotReloadFromEditor when we already had code in our project
						if (!HotReloadSupport.IsCurrentlyCompiling())
						{
							HotReloadSupport.DoHotReloadFromEditor(EHotReloadFlags::WaitForCompletion);
						}
					}
					else
					{
						// We didn't previously have source, so the UBT target name will be UnrealEditor, and attempts to recompile will end up building the wrong target. Now that we have source,
						// we need to change the UBT target to be the newly created editor module
						FPlatformMisc::SetUBTTargetName(*(FString(FApp::GetProjectName()) + TEXT("Editor")));

						if (!HotReloadSupport.RecompileModule(FApp::GetProjectName(), *GWarn, ERecompileModuleFlags::ReloadAfterRecompile | ERecompileModuleFlags::ForceCodeProject))
						{
							FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("PackFactory", "FailedToCompileNewGameModule", "Failed to compile newly created game module."));
						}
					}
				}

				// Ask about editing code where applicable
				if (FSlateApplication::Get().SupportsSourceAccess() )
				{
					// Code successfully added, notify the user and ask about opening the IDE now
					const FText Message = NSLOCTEXT("PackFactory", "CodeAdded", "Added source file(s). Would you like to edit the code now?");
					if ( FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes )
					{
						FSourceCodeNavigation::OpenSourceFiles(WrittenSourceFiles);
					}
				}
			}
			
			// Find an asset to return (It will be marked as dirty)
			for (const FString& Filename : WrittenFiles)
			{
				static const FString AssetExtension(TEXT(".uasset"));
				if (Filename.EndsWith(AssetExtension))
				{
					FString GameFileName = Filename;
					if (FPaths::MakePathRelativeTo(GameFileName, *FPaths::ProjectContentDir()))
					{
						int32 SlashIndex = INDEX_NONE;
						GameFileName = FString(TEXT("/Game/")) / GameFileName.LeftChop(AssetExtension.Len());
						if (GameFileName.FindLastChar(TEXT('/'), SlashIndex))
						{
							const FString AssetName = GameFileName.RightChop(SlashIndex + 1);
							ReturnAsset = LoadObject<UObject>(nullptr, *(GameFileName + TEXT(".") + AssetName));
							if (ReturnAsset)
							{
								break;
							}
						}
					}
				}
			}

			// If source control is enabled mark all the added files for checkout/add
			if (ISourceControlModule::Get().IsEnabled() && GetDefault<UEditorLoadingSavingSettings>()->bSCCAutoAddNewFiles)
			{
				for (const FString& Filename : WrittenFiles)
				{
					FText ErrorMessage;
					if (!SourceControlHelpers::CheckoutOrMarkForAdd(Filename, FText::FromString(Filename), NULL, ErrorMessage))
					{
						UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
					}
				}
			}
		}

		if (!ConfigParameters.InstallMessage.IsEmpty())
		{
			FMessageLog("AssetTools").Warning(FText::FromString(ConfigParameters.InstallMessage));
			FMessageLog("AssetTools").Open();
		}
	}
	else
	{
		if (!PakFile.IsValid())
		{
			UE_LOG(LogPackFactory, Warning, TEXT("Invalid pak file."));
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Pakfiles were loaded without Filenames, creation aborted."));
		}
	}

	return ReturnAsset;
}
