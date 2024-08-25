// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/ConfigUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Tasks/Pipe.h"

#if PLATFORM_WRITES_ARE_SLOW
#include "Async/Async.h"
#include "Misc/QueuedThreadPool.h"
#endif

#define UE_HOTFIX_FOR_NEXT_BOOT_FILENAME TEXT("HotfixForNextBoot.txt")

namespace UE::ConfigUtilities
{

UE::Tasks::FPipe AsyncTaskPipe( TEXT("SaveHotfixForNextBootPipe") );

const TCHAR* ConvertValueFromHumanFriendlyValue( const TCHAR* Value )
{
	static const TCHAR* OnValue = TEXT("1");
	static const TCHAR* OffValue = TEXT("0");

	// allow human friendly names
	if (FCString::Stricmp(Value, TEXT("True")) == 0
		|| FCString::Stricmp(Value, TEXT("Yes")) == 0
		|| FCString::Stricmp(Value, TEXT("On")) == 0)
	{
		return OnValue;
	}
	else if (FCString::Stricmp(Value, TEXT("False")) == 0
		|| FCString::Stricmp(Value, TEXT("No")) == 0
		|| FCString::Stricmp(Value, TEXT("Off")) == 0)
	{
		return OffValue;
	}
	return Value;
}

// The dedicated server could have been deployed through cloud provider and allocated dynamically. 
// So can't really save the file for next boot, consider the runtime args for cvar instead
#if !UE_SERVER
void LoadCVarsFromFileForNextBoot(TMap<FString, FString>& OutCVars)
{
	if (!FPaths::HasProjectPersistentDownloadDir())
	{
		UE_LOG(LogConfig, Log, TEXT("No project persistent download dir available for boot hotfix"));
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	const FString FullPath = FPaths::ProjectPersistentDownloadDir() / UE_HOTFIX_FOR_NEXT_BOOT_FILENAME;

	if (!FileManager.FileExists(*FullPath))
	{
		UE_LOG(LogConfig, Log, TEXT("No local boot hotfix file found at: [%s]"), *FullPath);
		return;
	}

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FullPath))
	{
		UE_LOG(LogConfig, Error, TEXT("Failed to load local boot hotfix file: [%s]"), *FullPath);
		return;
	}

	// Delete it so that we don't worry about it when write.
	// Also if for some reason the switch don't work well when boot even before getting latest hotfix, 
	// the next boot will likely succeed by default like before without this file
	if (!FileManager.Delete(*FullPath, true/*RequireExists*/))
	{
		UE_LOG(LogConfig, Error, TEXT("Failed to delete local boot hotfix file [%s]"), *FullPath);
	}

	UE_LOG(LogConfig, Log, TEXT("Local boot hotfix file [%s] loaded and deleted"), *FullPath);

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString Key;
		FString Value;
		if (ensure(Line.Split(TEXT("="), &Key, &Value)))
		{
			OutCVars.FindOrAdd(Key) = Value;
		}
	}
}
#endif // !UE_SERVER

void SaveCVarForNextBoot(const TCHAR* Key, const TCHAR* Value)
{
#if !UE_SERVER
	if (!FPaths::HasProjectPersistentDownloadDir())
	{
		UE_LOG(LogConfig, Log, TEXT("No persistent download dir, ignoring CVar %s hotfix for next boot"), Key);
		return;
	}

	FString StrKey(Key);
	FString StrValue(Value);

#if PLATFORM_WRITES_ARE_SLOW
	AsyncPool(*GIOThreadPool, [StrKey, StrValue] {
		TMap<FString, FString> CVarsToSave;

		// Read from file, in case there are more than one cvar hotfix event in same run
		LoadCVarsFromFileForNextBoot(CVarsToSave);

		CVarsToSave.FindOrAdd(StrKey) = StrValue;

		FString ContentToSave;
		for (const TPair<FString, FString>& CVarPair : CVarsToSave)
		{
			ContentToSave.Append(FString::Format(TEXT("{0}={1}\r\n"), { CVarPair.Key, CVarPair.Value }));
		}

		const FString FullPath = FPaths::ProjectPersistentDownloadDir() / UE_HOTFIX_FOR_NEXT_BOOT_FILENAME;
		FFileHelper::SaveStringToFile(ContentToSave, *FullPath);

		UE_LOG(LogConfig, Log, TEXT("Local boot hotfix file [%s] saved with hotfixed CVar: %s=%s"), *FullPath, *StrKey, *StrValue);
		});
#else
	AsyncTaskPipe.Launch(UE_SOURCE_LOCATION, [StrKey, StrValue] {
		TMap<FString, FString> CVarsToSave;

		// Read from file, in case there are more than one cvar hotfix event in same run
		LoadCVarsFromFileForNextBoot(CVarsToSave);

		CVarsToSave.FindOrAdd(StrKey) = StrValue;

		FString ContentToSave;
		for (const TPair<FString, FString>& CVarPair : CVarsToSave)
		{
			ContentToSave.Append(FString::Format(TEXT("{0}={1}\r\n"), { CVarPair.Key, CVarPair.Value }));
		}

		const FString FullPath = FPaths::ProjectPersistentDownloadDir() / UE_HOTFIX_FOR_NEXT_BOOT_FILENAME;
		FFileHelper::SaveStringToFile(ContentToSave, *FullPath);

		UE_LOG(LogConfig, Log, TEXT("Local boot hotfix file [%s] saved with hotfixed CVar: %s=%s"), *FullPath, *StrKey, *StrValue);
	}, LowLevelTasks::ETaskPriority::BackgroundLow);
#endif // PLATFORM_WRITES_ARE_SLOW
#endif // !UE_SERVER
}

void ApplyCVarsFromBootHotfix()
{
#if !UE_SERVER
	TMap<FString, FString> CVarsToApply;
	LoadCVarsFromFileForNextBoot(CVarsToApply);

	for (const TPair<FString, FString>& CVarPair : CVarsToApply)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarPair.Key); 
		if (CVar && CVar->TestFlags(ECVF_SaveForNextBoot))
		{
			CVar->Set(*CVarPair.Value, ECVF_SetByHotfix);
		}
	}
#endif // !UE_SERVER
}

void OnSetCVarFromIniEntry(const TCHAR *IniFile, const TCHAR *Key, const TCHAR* Value, uint32 SetBy, bool bAllowCheating, bool bNoLogging)
{
	check(IniFile && Key && Value);
	check((SetBy & ECVF_FlagMask) == 0);

	Value = ConvertValueFromHumanFriendlyValue(Value);

	// we don't need to track cvar misses here (a lot will be not found early on in editor builds)
	bool bTrackFrequentCalls = false;
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Key, bTrackFrequentCalls);

	if(CVar)
	{
		bool bCheatFlag = CVar->TestFlags(EConsoleVariableFlags::ECVF_Cheat); 
		
		if(SetBy == ECVF_SetByScalability)
		{
			if(!CVar->TestFlags(EConsoleVariableFlags::ECVF_Scalability) && !CVar->TestFlags(EConsoleVariableFlags::ECVF_ScalabilityGroup))
			{
				ensureMsgf(false, TEXT("Scalability.ini can only set ECVF_Scalability console variables ('%s'='%s' is ignored)"), Key, Value);
				return;
			}
		}

		bool bAllowChange = !bCheatFlag || bAllowCheating;

		if(bAllowChange)
		{
#if !NO_LOGGING
			bool BoolValue = CVar->IsVariableBool() ? CVar->GetBool() : false;
			int32 IntValue = CVar->IsVariableInt() ? CVar->GetInt() : 0;
			float FloatValue = CVar->IsVariableFloat() ? CVar->GetFloat() : 0.0f;
			FString StringValue = CVar->IsVariableString() ? CVar->GetString() : FString();
			bool bFirstSet = (CVar->GetFlags() & ECVF_SetByMask) == ECVF_SetByConstructor;
#endif
			if (SetBy == ECVF_SetByMask)
			{
				CVar->SetWithCurrentPriority(Value);
			}
			else
			{
				CVar->Set(Value, (EConsoleVariableFlags)SetBy);
			}
#if !NO_LOGGING
			bool bChanged = bFirstSet;
			if(!bChanged)
			{
				if (CVar->IsVariableBool())
				{
					bChanged = BoolValue != CVar->GetBool();
				}
				else if (CVar->IsVariableInt())
				{
					bChanged = IntValue != CVar->GetInt();
				}
				else if (CVar->IsVariableFloat())
				{
					bChanged = FloatValue != CVar->GetFloat();
				}
				else if (CVar->IsVariableString())
				{
					bChanged = StringValue != CVar->GetString();
				}
			}
			UE_CLOG(!bNoLogging && bChanged, LogConfig, Log, TEXT("Set CVar [[%s:%s]]"), Key, Value);
#endif
		}
		else
		{
#if !DISABLE_CHEAT_CVARS
			if(bCheatFlag)
			{
				// We have one special cvar to test cheating and here we don't want to both the user of the engine
				if(FCString::Stricmp(Key, TEXT("con.DebugEarlyCheat")) != 0)
				{
					ensureMsgf(false, TEXT("The ini file '%s' tries to set the console variable '%s' marked with ECVF_Cheat, this is only allowed in consolevariables.ini"),
						IniFile, Key);
				}
			}
#endif // !DISABLE_CHEAT_CVARS
		}

		if (CVar->TestFlags(ECVF_SaveForNextBoot) && (SetBy == ECVF_SetByHotfix))
		{
			UE_LOG(LogConfig, Log, TEXT("Saving %s for boot hotfix"), Key);
			SaveCVarForNextBoot(Key, Value);
		}
	}
	else
	{
		// Create a dummy that is used when someone registers the variable later on.
		// this is important for variables created in external modules, such as the game module
		IConsoleManager::Get().RegisterConsoleVariable(Key, Value, TEXT("IAmNoRealVariable"),
			(uint32)ECVF_Unregistered | (uint32)ECVF_CreatedFromIni | SetBy);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogConfig, Log, TEXT("CVar [[%s:%s]] deferred - dummy variable created"), Key, Value);
#endif //!UE_BUILD_SHIPPING
	}
}


void ApplyCVarSettingsFromIni(const TCHAR* InSectionName, const TCHAR* InIniFilename, uint32 SetBy, bool bAllowCheating)
{
	FCoreDelegates::OnApplyCVarFromIni.Broadcast(InSectionName, InIniFilename, SetBy, bAllowCheating);

	UE_LOG(LogConfig,Log,TEXT("Applying CVar settings from Section [%s] File [%s]"),InSectionName,InIniFilename);

	if(const FConfigSection* Section = GConfig->GetSection(InSectionName, false, InIniFilename))
	{
		for(FConfigSectionMap::TConstIterator It(*Section); It; ++It)
		{
			const FString& KeyString = It.Key().GetPlainNameString(); 
			const FString& ValueString = It.Value().GetValue();

			OnSetCVarFromIniEntry(InIniFilename, *KeyString, *ValueString, SetBy, bAllowCheating, false);
		}
	}
}

void ForEachCVarInSectionFromIni(const TCHAR* InSectionName, const TCHAR* InIniFilename, TFunction<void(IConsoleVariable* CVar, const FString& KeyString, const FString& ValueString)> InEvaluationFunction)
{
	if (const FConfigSection* Section = GConfig->GetSection(InSectionName, false, InIniFilename))
	{
		for (FConfigSectionMap::TConstIterator It(*Section); It; ++It)
		{
			const FString& KeyString = It.Key().GetPlainNameString();
			const FString& ValueString = ConvertValueFromHumanFriendlyValue(*It.Value().GetValue());

			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*KeyString);
			if (CVar)
			{
				InEvaluationFunction(CVar, KeyString, ValueString);
			}
		}
	}
}





class FCVarIniHistoryHelper
{
private:
	struct FCVarIniHistory
	{
		FCVarIniHistory(const FString& InSectionName, const FString& InIniName, uint32 InSetBy, bool InbAllowCheating) :
			SectionName(InSectionName), FileName(InIniName), SetBy(InSetBy), bAllowCheating(InbAllowCheating)
		{}

		FString SectionName;
		FString FileName;
		uint32 SetBy;
		bool bAllowCheating;
	};
	TArray<FCVarIniHistory> CVarIniHistory;
	bool bRecurseCheck;

	void OnApplyCVarFromIniCallback(const TCHAR* SectionName, const TCHAR* IniFilename, uint32 SetBy, bool bAllowCheating)
	{
		if ( bRecurseCheck == true )
		{
			return;
		}
		CVarIniHistory.Add(FCVarIniHistory(SectionName, IniFilename, SetBy, bAllowCheating));
	}


public:
	FCVarIniHistoryHelper() : bRecurseCheck( false )
	{
		FCoreDelegates::OnApplyCVarFromIni.AddRaw(this, &FCVarIniHistoryHelper::OnApplyCVarFromIniCallback);
	}

	~FCVarIniHistoryHelper()
	{
		FCoreDelegates::OnApplyCVarFromIni.RemoveAll(this);
	}

	void ReapplyIniHistory()
	{
		for (const FCVarIniHistory& IniHistory : CVarIniHistory)
		{
			const FString& SectionName = IniHistory.SectionName;
			const FString& IniFilename = IniHistory.FileName;
			const uint32& SetBy = IniHistory.SetBy;
			if (const FConfigSection* Section = GConfig->GetSection(*SectionName, false, *IniFilename))
			{
				for (FConfigSectionMap::TConstIterator It(*Section); It; ++It)
				{
					const FString& KeyString = It.Key().GetPlainNameString();
					const FString& ValueString = It.Value().GetValue();


					IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*KeyString);

					if ( !CVar )
					{
						continue;
					}

					// if this cvar was last set by this config setting 
					// then we want to reapply any new changes
					if (!CVar->TestFlags((EConsoleVariableFlags)SetBy))
					{
						continue;
					}

					// convert to human friendly string
					const TCHAR* HumanFriendlyValue = ConvertValueFromHumanFriendlyValue(*ValueString);
					const FString CurrentValue = CVar->GetString();
					if (CurrentValue.Compare(HumanFriendlyValue, ESearchCase::CaseSensitive) == 0)
					{
						continue;
					}
					else
					{
						// this is more expensive then a CaseSensitive version and much less likely
						if (CurrentValue.Compare(HumanFriendlyValue, ESearchCase::IgnoreCase) == 0)
						{
							continue;
						}
					}

					if (CVar->TestFlags(EConsoleVariableFlags::ECVF_ReadOnly) )
					{
						UE_LOG(LogConfig, Warning, TEXT("Failed to change Readonly CVAR value %s %s -> %s Config %s %s"),
							*KeyString, *CurrentValue, HumanFriendlyValue, *IniFilename, *SectionName);

						continue;
					}

					UE_LOG(LogConfig, Display, TEXT("Applied changed CVAR value %s %s -> %s Config %s %s"), 
						*KeyString, *CurrentValue, HumanFriendlyValue, *IniFilename, *SectionName);

					OnSetCVarFromIniEntry(*IniFilename, *KeyString, *ValueString, SetBy, IniHistory.bAllowCheating, false);
				}
			}
		}
		bRecurseCheck=false;
	}
};
TUniquePtr<FCVarIniHistoryHelper> IniHistoryHelper;

#if !UE_BUILD_SHIPPING
class FConfigHistoryHelper
{
private:
	enum class HistoryType : int
	{
		Value,
		Section,
		SectionName,
		Count
	};
	friend const TCHAR* LexToString(HistoryType Type)
	{
		static const TCHAR* Strings[] = {
			TEXT("Value"),
			TEXT("Section"),
			TEXT("SectionName")
		};
		using UnderType = __underlying_type(HistoryType);
		static_assert(static_cast<UnderType>(HistoryType::Count) == UE_ARRAY_COUNT(Strings), "");
		return Strings[static_cast<UnderType>(Type)];
	}

	struct FConfigHistory
	{
		HistoryType Type = HistoryType::Value;

		FString FileName;
		FString SectionName;
		FString Key;
	};
	friend uint32 GetTypeHash(const FConfigHistory& CH)
	{
		uint32 Hash = ::GetTypeHash(CH.Type);
		Hash = HashCombine(Hash, GetTypeHash(CH.FileName));
		Hash = HashCombine(Hash, GetTypeHash(CH.SectionName));
		Hash = HashCombine(Hash, GetTypeHash(CH.Key));
		return Hash;
	}
	friend bool operator==(const FConfigHistory& A, const FConfigHistory& B)
	{
		return
			A.Type == B.Type &&
			A.FileName == B.FileName &&
			A.SectionName == B.SectionName &&
			A.Key == B.Key;
	}

	TSet<FConfigHistory> History;

	void OnConfigValueRead(const TCHAR* FileName, const TCHAR* SectionName, const TCHAR* Key)
	{
		History.Emplace(FConfigHistory{ HistoryType::Value, FileName, SectionName, Key });
	}

	void OnConfigSectionRead(const TCHAR* FileName, const TCHAR* SectionName)
	{
		History.Emplace(FConfigHistory{ HistoryType::Section, FileName, SectionName });
	}

	void OnConfigSectionNameRead(const TCHAR* FileName, const TCHAR* SectionName)
	{
		History.Emplace(FConfigHistory{ HistoryType::SectionName, FileName, SectionName });
	}

public:
	FConfigHistoryHelper()
	{
		FCoreDelegates::TSOnConfigValueRead().AddRaw(this, &FConfigHistoryHelper::OnConfigValueRead);
		FCoreDelegates::TSOnConfigSectionRead().AddRaw(this, &FConfigHistoryHelper::OnConfigSectionRead);
		FCoreDelegates::TSOnConfigSectionNameRead().AddRaw(this, &FConfigHistoryHelper::OnConfigSectionNameRead);
	}

	~FConfigHistoryHelper()
	{
		FCoreDelegates::TSOnConfigValueRead().RemoveAll(this);
		FCoreDelegates::TSOnConfigSectionRead().RemoveAll(this);
		FCoreDelegates::TSOnConfigSectionNameRead().RemoveAll(this);
	}

	void DumpHistory()
	{
		const FString SavePath = FPaths::ProjectLogDir() / TEXT("ConfigHistory.csv");

		FArchive* Writer = IFileManager::Get().CreateFileWriter(*SavePath, FILEWRITE_NoFail);

		auto WriteLine = [Writer](FString&& Line)
		{
			UE_LOG(LogConfig, Display, TEXT("%s"), *Line);
			FTCHARToUTF8 UTF8String(*(MoveTemp(Line) + LINE_TERMINATOR));
			Writer->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length());
		};

		UE_LOG(LogConfig, Display, TEXT("Dumping History of Config Reads to %s"), *SavePath);
		UE_LOG(LogConfig, Display, TEXT("Begin History of Config Reads"));
		UE_LOG(LogConfig, Display, TEXT("------------------------------------------------------"));
		WriteLine(FString::Printf(TEXT("Type, File, Section, Key")));
		for (const FConfigHistory& CH : History)
		{
			switch (CH.Type)
			{
			case HistoryType::Value:
				WriteLine(FString::Printf(TEXT("%s, %s, %s, %s"), LexToString(CH.Type), *CH.FileName, *CH.SectionName, *CH.Key));
				break;
			case HistoryType::Section:
			case HistoryType::SectionName:
				WriteLine(FString::Printf(TEXT("%s, %s, %s, None"), LexToString(CH.Type), *CH.FileName, *CH.SectionName));
				break;
			}
		}
		UE_LOG(LogConfig, Display, TEXT("------------------------------------------------------"));
		UE_LOG(LogConfig, Display, TEXT("End History of Config Reads"));

		delete Writer;
		Writer = nullptr;
	}
};
TUniquePtr<FConfigHistoryHelper> ConfigHistoryHelper;
#endif // !UE_BUILD_SHIPPING


void RecordApplyCVarSettingsFromIni()
{
	check(IniHistoryHelper == nullptr);
	IniHistoryHelper = MakeUnique<FCVarIniHistoryHelper>();
}

void ReapplyRecordedCVarSettingsFromIni()
{
	// first we need to reload the inis 
	for (const FString& Filename : GConfig->GetFilenames())
	{
		FConfigFile* ConfigFile = GConfig->FindConfigFile(Filename);
		if (ConfigFile->Num() > 0)
		{
			FName BaseName = ConfigFile->Name;
			// Must call LoadLocalIniFile (NOT LoadGlobalIniFile) to preserve original enginedir/sourcedir for plugins
			verify(FConfigCacheIni::LoadLocalIniFile(*ConfigFile, *BaseName.ToString(), true, nullptr, true));
		}
	}

	check(IniHistoryHelper != nullptr);
	IniHistoryHelper->ReapplyIniHistory();
}

void DeleteRecordedCVarSettingsFromIni()
{
	check(IniHistoryHelper != nullptr);
	IniHistoryHelper = nullptr;
}

void RecordConfigReadsFromIni()
{
#if !UE_BUILD_SHIPPING
	check(ConfigHistoryHelper == nullptr);
	ConfigHistoryHelper = MakeUnique<FConfigHistoryHelper>();
#endif
}

void DumpRecordedConfigReadsFromIni()
{
#if !UE_BUILD_SHIPPING
	check(ConfigHistoryHelper);
	ConfigHistoryHelper->DumpHistory();
#endif
}

void DeleteRecordedConfigReadsFromIni()
{
#if !UE_BUILD_SHIPPING
	check(ConfigHistoryHelper);
	ConfigHistoryHelper = nullptr;
#endif
}

}
