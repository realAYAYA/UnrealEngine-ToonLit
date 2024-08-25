// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/ConfigUtilities.h"
#include "Containers/StringView.h"
#include "Misc/DateTime.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Math/Vector4.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemStats.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Misc/RemoteConfigIni.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/ConfigManifest.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/StringBuilder.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Async/Async.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Logging/MessageLog.h"
#include <limits>

namespace
{
	const FString CurrentIniVersionStr = TEXT("CurrentIniVersion");
	const FString SectionsToSaveStr = TEXT("SectionsToSave");

	TMap<FString, FString> SectionRemap;
	TMap<FString, TMap<FString, FString>> KeyRemap;
}

DEFINE_LOG_CATEGORY(LogConfig);
#define LOCTEXT_NAMESPACE "ConfigCache"

/*-----------------------------------------------------------------------------
FConfigValue
-----------------------------------------------------------------------------*/

struct FConfigExpansion
{
	template<int N>
	FConfigExpansion(const TCHAR(&Var)[N], FString&& Val)
		: Variable(Var)
		, Value(MoveTemp(Val))
		, VariableLen(N - 1)
	{}

	template<int N>
	FConfigExpansion(const TCHAR(&Var)[N], const FString& Val)
		: Variable(Var)
		, Value(Val)
		, VariableLen(N - 1)
	{}

	const TCHAR* Variable;
	FString Value;
	int VariableLen;
};

static FString GetApplicationSettingsDirNormalized()
{
	FString Dir = FPlatformProcess::ApplicationSettingsDir();
	FPaths::NormalizeFilename(Dir);
	return Dir;
}

static const FConfigExpansion* MatchExpansions(const TCHAR* PotentialVariable)
{
	// Allocate replacement value strings once
	static const FConfigExpansion Expansions[] =
	{
		FConfigExpansion(TEXT("%GAME%"), FString(FApp::GetProjectName())),
		FConfigExpansion(TEXT("%GAMEDIR%"), FPaths::ProjectDir()),
		FConfigExpansion(TEXT("%ENGINEDIR%"), FPaths::EngineDir()),
		FConfigExpansion(TEXT("%ENGINEUSERDIR%"), FPaths::EngineUserDir()),
		FConfigExpansion(TEXT("%ENGINEVERSIONAGNOSTICUSERDIR%"), FPaths::EngineVersionAgnosticUserDir()),
		FConfigExpansion(TEXT("%APPSETTINGSDIR%"), GetApplicationSettingsDirNormalized()),
		FConfigExpansion(TEXT("%GAMESAVEDDIR%"), FPaths::ProjectSavedDir()),
	};

	for (const FConfigExpansion& Expansion : Expansions)
	{
		if (FCString::Strncmp(Expansion.Variable, PotentialVariable, Expansion.VariableLen) == 0)
		{
			return &Expansion;
		}
	}

	return nullptr;
}

static const FConfigExpansion* FindNextExpansion(const TCHAR* Str, const TCHAR*& OutMatch)
{
	for (const TCHAR* It = FCString::Strchr(Str, '%'); It; It = FCString::Strchr(It + 1, '%'))
	{
		if (const FConfigExpansion* Expansion = MatchExpansions(It))
		{
			OutMatch = It;
			return Expansion;
		}
	}

	return nullptr;
}

bool FConfigValue::ExpandValue(const FString& InCollapsedValue, FString& OutExpandedValue)
{
	struct FSubstring
	{
		const TCHAR* Begin;
		const TCHAR* End;

		int32 Len() const { return UE_PTRDIFF_TO_INT32(End - Begin); }
	};

	// Find substrings of input and expansions to concatenate to final output string
	TArray<FSubstring, TFixedAllocator<7>> Substrings;
	const TCHAR* It = *InCollapsedValue;
	while (true)
	{
		const TCHAR* Match;
		if (const FConfigExpansion* Expansion = FindNextExpansion(It, Match))
		{
			Substrings.Add({ It, Match });
			Substrings.Add({ *Expansion->Value, (*Expansion->Value) + Expansion->Value.Len() });

			It = Match + Expansion->VariableLen;
		}
		else if (Substrings.Num() == 0)
		{
			// No expansions matched, skip concatenation and return input string
			OutExpandedValue = InCollapsedValue;
			return false;
		}
		else
		{
			Substrings.Add({ It, *InCollapsedValue + InCollapsedValue.Len() });
			break;
		}
	}

	// Concat
	int32 OutLen = 0;
	for (const FSubstring& Substring : Substrings)
	{
		OutLen += Substring.Len();
	}

	OutExpandedValue.Reserve(OutLen);
	for (const FSubstring& Substring : Substrings)
	{
		OutExpandedValue.AppendChars(Substring.Begin, Substring.Len());
	}

	return true;
}

FString FConfigValue::ExpandValue(const FString& InCollapsedValue)
{
	FString OutExpandedValue;
	ExpandValue(InCollapsedValue, OutExpandedValue);
	return OutExpandedValue;
}

void FConfigValue::ExpandValueInternal()
{
	const TCHAR* Dummy;
	if (FindNextExpansion(*SavedValue, Dummy))
	{
		ExpandValue(SavedValue, /* out */ ExpandedValue);
	}
}

bool FConfigValue::CollapseValue(const FString& InExpandedValue, FString& OutCollapsedValue)
{
	int32 NumReplacements = 0;
	OutCollapsedValue = InExpandedValue;

	auto ExpandPathValueInline = [&](const FString& InPath, const TCHAR* InReplacement)
	{
		if (OutCollapsedValue.StartsWith(InPath, ESearchCase::CaseSensitive))
		{
			NumReplacements += OutCollapsedValue.ReplaceInline(*InPath, InReplacement, ESearchCase::CaseSensitive);
		}
		else if (FPaths::IsRelative(InPath))
		{
			const FString AbsolutePath = FPaths::ConvertRelativePathToFull(InPath);
			if (OutCollapsedValue.StartsWith(AbsolutePath, ESearchCase::CaseSensitive))
			{
				NumReplacements += OutCollapsedValue.ReplaceInline(*AbsolutePath, InReplacement, ESearchCase::CaseSensitive);
			}
		}
	};

	// Replace the game directory with %GAMEDIR%.
	ExpandPathValueInline(FPaths::ProjectDir(), TEXT("%GAMEDIR%"));

	// Replace the user's engine directory with %ENGINEUSERDIR%.
	ExpandPathValueInline(FPaths::EngineUserDir(), TEXT("%ENGINEUSERDIR%"));

	// Replace the user's engine agnostic directory with %ENGINEVERSIONAGNOSTICUSERDIR%.
	ExpandPathValueInline(FPaths::EngineVersionAgnosticUserDir(), TEXT("%ENGINEVERSIONAGNOSTICUSERDIR%"));

	// Replace the application settings directory with %APPSETTINGSDIR%.
	FString AppSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	FPaths::NormalizeFilename(AppSettingsDir);
	ExpandPathValueInline(AppSettingsDir, TEXT("%APPSETTINGSDIR%"));

	// Note: We deliberately don't replace the game name with %GAME% here, as the game name may exist in many places (including paths)

	return NumReplacements > 0;
}

FString FConfigValue::CollapseValue(const FString& InExpandedValue)
{
	FString CollapsedValue;
	CollapseValue(InExpandedValue, CollapsedValue);
	return CollapsedValue;
}

#if !UE_BUILD_SHIPPING
/**
* Checks if the section name is the expected name format (long package name or simple name)
*/
static void CheckLongSectionNames(const TCHAR* Section, const FConfigFile* File)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Guard against short names in ini files.
		if (FCString::Strnicmp(Section, TEXT("/Script/"), 8) == 0)
		{
			// Section is a long name
			if (File->FindSection(Section + 8))
			{
				UE_LOG(LogConfig, Fatal, TEXT("Short config section found while looking for %s"), Section);
			}
		}
		else
		{
			// Section is a short name
			FString LongName = FString(TEXT("/Script/")) + Section;
			if (File->FindSection(*LongName))
			{
				UE_LOG(LogConfig, Fatal, TEXT("Short config section used instead of long %s"), Section);
			}
		}
	}
}
#endif // UE_BUILD_SHIPPING

/*-----------------------------------------------------------------------------
	FConfigSection
-----------------------------------------------------------------------------*/


bool FConfigSection::HasQuotes( const FString& Test )
{
	if (Test.Len() < 2)
	{
		return false;
	}

	return Test.Left(1) == TEXT("\"") && Test.Right(1) == TEXT("\"");
}

bool FConfigSection::operator==( const FConfigSection& B ) const
{
	const FConfigSection&A = *this;
	if ( A.Pairs.Num() != B.Pairs.Num() )
	{
		return false;
	}

	FConfigSectionMap::TConstIterator AIter(A), BIter(B);
	while (AIter && BIter)
	{
		if (AIter.Key() != BIter.Key())
		{
			return false;
		}

		const FString& AIterValue = AIter.Value().GetValue();
		const FString& BIterValue = BIter.Value().GetValue();
		if ( FCString::Strcmp(*AIterValue,*BIterValue) &&
			(!HasQuotes(AIterValue) || FCString::Strcmp(*BIterValue,*AIterValue.Mid(1, AIterValue.Len() - 2))) &&
			(!HasQuotes(BIterValue) || FCString::Strcmp(*AIterValue,*BIterValue.Mid(1, BIterValue.Len() - 2))) )
		{
			return false;
		}

		++AIter, ++BIter;
	}
	return true;
}

bool FConfigSection::operator!=( const FConfigSection& Other ) const
{
	return ! (FConfigSection::operator==(Other));
}

namespace UE::ConfigCacheIni::Private
{

bool FAccessor::AreSectionsEqualForWriting(const FConfigSection& A, const FConfigSection& B)
{
	if (A.Pairs.Num() != B.Pairs.Num())
	{
		return false;
	}

	FConfigSectionMap::TConstIterator AIter(A), BIter(B);
	while (AIter && BIter)
	{
		if (AIter.Key() != BIter.Key())
		{
			return false;
		}

		const FString& AIterValue = AIter.Value().GetValueForWriting();
		const FString& BIterValue = BIter.Value().GetValueForWriting();
		if (FCString::Strcmp(*AIterValue, *BIterValue) &&
			(!FConfigSection::HasQuotes(AIterValue) || FCString::Strcmp(*BIterValue, *AIterValue.Mid(1, AIterValue.Len() - 2))) &&
			(!FConfigSection::HasQuotes(BIterValue) || FCString::Strcmp(*AIterValue, *BIterValue.Mid(1, BIterValue.Len() - 2))))
		{
			return false;
		}

		++AIter, ++BIter;
	}
	return true;
}

} // namespace UE::ConfigCacheIni::Private

FArchive& operator<<(FArchive& Ar, FConfigSection& ConfigSection)
{
	Ar << static_cast<FConfigSection::Super&>(ConfigSection);
	Ar << ConfigSection.ArrayOfStructKeys;
	return Ar;
}

// Pull out a property from a Struct property, StructKeyMatch should be in the form "MyProp=". This reduces
// memory allocations for each attempted match
static void ExtractPropertyValue(const FString& FullStructValue, const FString& StructKeyMatch, FString& Out)
{
	Out.Reset();

	int32 MatchLoc = FullStructValue.Find(StructKeyMatch);
	// we only look for matching StructKeys if the incoming Value had a key
	if (MatchLoc >= 0)
	{
		// skip to after the match string
		MatchLoc += StructKeyMatch.Len();

		const TCHAR* Start = &FullStructValue.GetCharArray()[MatchLoc];
		bool bInQuotes = false;
		// skip over an open quote
		if (*Start == '\"')
		{
			Start++;
			bInQuotes = true;
		}
		const TCHAR* Travel = Start;

		// look for end of token, using " if it started with one
		while (*Travel && ((bInQuotes && *Travel != '\"') || (!bInQuotes && (FChar::IsAlnum(*Travel) || *Travel == '_'))))
		{
			Travel++;
		}

		// pull out the token
		Out.AppendChars(Start, UE_PTRDIFF_TO_INT32(Travel - Start));
	}
}

void FConfigSection::HandleAddCommand(FName ValueName, FString&& Value, bool bAppendValueIfNotArrayOfStructsKeyUsed)
{
	if (!HandleArrayOfKeyedStructsCommand(ValueName, Forward<FString&&>(Value)))
	{
		if (bAppendValueIfNotArrayOfStructsKeyUsed)
		{
			Add(ValueName, FConfigValue(MoveTemp(Value)));
		}
		else
		{
			AddUnique(ValueName, FConfigValue(MoveTemp(Value)));
		}
	}
}

bool FConfigSection::HandleArrayOfKeyedStructsCommand(FName Key, FString&& Value)
{
	FString* StructKey = ArrayOfStructKeys.Find(Key);
	bool bHandledWithKey = false;
	if (StructKey)
	{
		// look at the incoming value for the StructKey
		FString StructKeyMatch = *StructKey + "=";

		// pull out the token that matches the StructKey (a property name) from the full struct property string
		FString StructKeyValueToMatch;
		ExtractPropertyValue(Value, StructKeyMatch, StructKeyValueToMatch);

		if (StructKeyValueToMatch.Len() > 0)
		{
			FString ExistingStructValueKey;
			// if we have a key for this array, then we look for it in the Value for each array entry
			for (FConfigSection::TIterator It(*this); It; ++It)
			{
				// only look at matching keys
				if (It.Key() == Key)
				{
					// now look for the matching ArrayOfStruct Key as the incoming KeyValue
					{
						const FString& ItValue = UE::ConfigCacheIni::Private::FAccessor::GetValueForWriting(It.Value()); // Don't report to AccessTracking
						ExtractPropertyValue(ItValue, StructKeyMatch, ExistingStructValueKey);
					}
					if (ExistingStructValueKey == StructKeyValueToMatch)
					{
						// we matched the key, so replace the existing value in place (so as not to reorder)
						It.Value() = Value;

						// mark that the key was found and the add has been processed
						bHandledWithKey = true;
						break;
					}
				}
			}
		}
	}

	return bHandledWithKey;
}

// Look through the file's per object config ArrayOfStruct keys and see if this section matches
static void FixupArrayOfStructKeysForSection(FConfigSection* Section, const FString& SectionName, const TMap<FString, TMap<FName, FString> >& PerObjectConfigKeys)
{
	for (TMap<FString, TMap<FName, FString> >::TConstIterator It(PerObjectConfigKeys); It; ++It)
	{
		if (SectionName.EndsWith(It.Key()))
		{
			for (TMap<FName, FString>::TConstIterator It2(It.Value()); It2; ++It2)
			{
				Section->ArrayOfStructKeys.Add(It2.Key(), It2.Value());
			}
		}
	}
}


/**
 * Check if an ini file exists, allowing a delegate to determine if it will handle loading it
 */
/*static*/ bool DoesConfigFileExistWrapper(const TCHAR* IniFile, const TSet<FString>* IniCacheSet)
{
	// will any delegates return contents via TSPreLoadConfigFileDelegate()?
	int32 ResponderCount = 0;
	FCoreDelegates::TSCountPreLoadConfigFileRespondersDelegate().Broadcast(IniFile, ResponderCount);

	if (ResponderCount > 0)
	{
		return true;
	}

	// So far, testing on cooked consoles, cooked desktop, and the Editor
	// works fine.
	// However, there was an issue where INIs wouldn't be found during cooking,
	// which would pass by silently. Realistically, in most cases this would never
	// have caused an issue, but using FPlatformProperties::RequiresCookedData
	// to prevent using the cache in that case ensures full consistency.
	if (IniCacheSet && FPlatformProperties::RequiresCookedData())
	{ 
		const FString IniFileString(IniFile);
		const bool bFileExistsCached = IniCacheSet->Contains(IniFileString);

		// This code can be uncommented if we expect there are INIs that are not being
		// found in the cache.
		/**
		const bool bFileExistsCachedTest = IFileManager::Get().FileSize(IniFile) >= 0;
		ensureMsgf(
			bFileExistsCached == bFileExistsCachedTest,
			TEXT("DoesConfigFileExistWrapper: InCache = %d, InFileSystem = %d, Name = %s, Configs = \n%s"),
			!!bFileExistsCached,
			!!bFileExistsCachedTest,
			IniFile,
			*FString::Join(*IniCacheSet, TEXT("\n"))
		);
		*/
		
		return bFileExistsCached;
	}

	// otherwise just look for the normal file to exist
	const bool bFileExistsCached = IFileManager::Get().FileSize(IniFile) >= 0;
	return bFileExistsCached;
}

/**
 * Load ini file, but allowing a delegate to handle the loading instead of the standard file load
 */
static bool LoadConfigFileWrapper(const TCHAR* IniFile, FString& Contents, bool bIsOverride = false)
{
	// We read the Base.ini and PluginBase.ini files many many times, so cache them
	static FString BaseIniContents;
	static FString PluginBaseIniContents;

	const TCHAR* LastSlash = FCString::Strrchr(IniFile, '/');
	if (LastSlash == nullptr)
	{
		LastSlash = FCString::Strrchr(IniFile, '\\');
	}

	bool bIsBaseIni = LastSlash != nullptr && FCString::Stricmp(LastSlash + 1, TEXT("Base.ini")) == 0;
	if (bIsBaseIni && BaseIniContents.Len() > 0)
	{
		Contents = BaseIniContents;
		return true;
	}

	bool bIsPluginBaseIni = LastSlash != nullptr && FCString::Stricmp(LastSlash + 1, TEXT("PluginBase.ini")) == 0;
	if (bIsPluginBaseIni && PluginBaseIniContents.Len() > 0)
	{
		Contents = PluginBaseIniContents;
		return true;
	}

	// let other systems load the file instead of the standard load below
	FCoreDelegates::TSPreLoadConfigFileDelegate().Broadcast(IniFile, Contents);

	// if this loaded any text, we are done, and we won't override the contents with standard ini file data
	if (Contents.Len())
	{
		return true;
	}

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	if (bIsOverride)
	{
		// Make sure we bypass the pak layer because if our override is likely under root the pak layer will
		// just resolve it even if it's an absolute path.

		return FFileHelper::LoadFileToString(Contents, &IPlatformFile::GetPlatformPhysical(), IniFile);
	}
#endif

	// note: we don't check if FileOperations are disabled because downloadable content calls this directly (which
	// needs file ops), and the other caller of this is already checking for disabled file ops
	// and don't read from the file, if the delegate got anything loaded
	bool bResult = FFileHelper::LoadFileToString(Contents, IniFile);
	if (bResult)
	{
		if (bIsBaseIni)
		{
			BaseIniContents = Contents;
		}
		else if (bIsPluginBaseIni)
		{
			PluginBaseIniContents = Contents;
		}
	}
	return bResult;
}

/**
 * Save an ini file, with delegates also saving the file (its safe to allow both to happen, even tho loading doesn't behave this way)
 */
static bool SaveConfigFileWrapper(const TCHAR* IniFile, const FString& Contents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SaveConfigFileWrapper);

	// let anyone that needs to save it, do so (counting how many did)
	int32 SavedCount = 0;
	FCoreDelegates::TSPreSaveConfigFileDelegate().Broadcast(IniFile, Contents, SavedCount);

	// save it even if a delegate did as well
	bool bLocalWriteSucceeded = false;

	if (FConfigFile::WriteTempFileThenMove())
	{
		const FString BaseFilename = FPaths::GetBaseFilename(IniFile);
		const FString TempFilename = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32));
		bLocalWriteSucceeded = FFileHelper::SaveStringToFile(Contents, *TempFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (bLocalWriteSucceeded)
		{
			if (!IFileManager::Get().Move(IniFile, *TempFilename))
			{
				IFileManager::Get().Delete(*TempFilename);
				bLocalWriteSucceeded = false;
			}
		}
	}
	else
	{
		bLocalWriteSucceeded = FFileHelper::SaveStringToFile(Contents, IniFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// success is based on a delegate or file write working (or both)
	return SavedCount > 0 || bLocalWriteSucceeded;
}

/*-----------------------------------------------------------------------------
	FConfigFile
-----------------------------------------------------------------------------*/
FConfigFile::FConfigFile()
: Dirty( false )
, NoSave( false )
, bHasPlatformName( false )
, bCanSaveAllSections( true )
, Name( NAME_None )
, SourceConfigFile(nullptr)
{
	FCoreDelegates::TSOnFConfigCreated().Broadcast(this);
}

FConfigFile::~FConfigFile()
{
	// this destructor can run at file scope, static shutdown

	if ( !GExitPurge )
	{
		FCoreDelegates::TSOnFConfigDeleted().Broadcast(this);
	}

	delete SourceConfigFile;
	SourceConfigFile = nullptr;
}

bool FConfigFile::operator==( const FConfigFile& Other ) const
{
	if ( Pairs.Num() != Other.Pairs.Num() )
		return 0;

	for ( TMap<FString,FConfigSection>::TConstIterator It(*this), OtherIt(Other); It && OtherIt; ++It, ++OtherIt)
	{
		if ( It.Key() != OtherIt.Key() )
			return 0;

		if ( It.Value() != OtherIt.Value() )
			return 0;
	}

	return 1;
}

bool FConfigFile::operator!=( const FConfigFile& Other ) const
{
	return ! (FConfigFile::operator==(Other));
}

FConfigSection* FConfigFile::FindOrAddSection(const FString& SectionName)
{
	return FindOrAddSectionInternal(SectionName);
}

FConfigSection* FConfigFile::FindOrAddSectionInternal(const FString& SectionName)
{
	FConfigSection* Section = FindInternal(SectionName);
	if (Section == nullptr)
	{
		Section = &Add(SectionName, FConfigSection());
	}
	return Section;
}

const FConfigSection* FConfigFile::FindOrAddConfigSection(const FString& SectionName)
{
	return FindOrAddSectionInternal(SectionName);
}

bool FConfigFile::Combine(const FString& Filename)
{
	FString FinalFileName = Filename;
	bool bFoundOverride = OverrideFileFromCommandline(FinalFileName);

	FString Text;
	if (LoadConfigFileWrapper(*FinalFileName, Text, bFoundOverride))
	{
		if (Text.StartsWith("#!"))
		{
			// this will import/"execute" another .ini file before this one - useful for subclassing platforms, like tvOS extending iOS
			// the text following the #! is a relative path to another .ini file
			FString TheLine;
			int32 LinesConsumed = 0;
			// skip over the #!
			const TCHAR* Ptr = *Text + 2;
			FParse::LineExtended(&Ptr, TheLine, LinesConsumed, false);
			TheLine = TheLine.TrimEnd();
		
			// now import the relative path'd file (TVOS would have #!../IOS) recursively
			Combine(FPaths::GetPath(Filename) / TheLine);
		}

		CombineFromBuffer(Text, Filename);
		return true;
	}
	else
	{
		checkf(!bFoundOverride, TEXT("Failed to Load config override %s"), *FinalFileName);
	}

	return false;
}

// Assumes GetTypeHash(AltKeyType) matches GetTypeHash(KeyType)
template<class KeyType, class ValueType, class AltKeyType>
ValueType& FindOrAddHeterogeneous(TMap<KeyType, ValueType>& Map, const AltKeyType& Key) 
{
	checkSlow(GetTypeHash(KeyType(Key)) == GetTypeHash(Key));
	ValueType* Existing = Map.FindByHash(GetTypeHash(Key), Key);
	return Existing ? *Existing : Map.Emplace(KeyType(Key));
}

namespace
{

// don't allow warning until all redirects are read in
bool GAllowConfigRemapWarning = false;

// either show an editor warning, or just write to log for non-editor
void LogOrEditorWarning(const FText& Msg, const FString& PartialKey, const FString& File)
{
	if (!GAllowConfigRemapWarning)
	{
		return;
	}
	
	if (GIsEditor)
	{
		static TSet<FString> AlreadyWarnedKeys;
		
		FString AbsPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*File);
		
		// make sure we haven't warned about this yet
		FString Key = PartialKey + AbsPath;
		if (AlreadyWarnedKeys.Contains(Key))
		{
			return;
		}
		AlreadyWarnedKeys.Add(Key);
		
		FMessageLog EditorErrors("EditorErrors");
		TSharedRef<FTokenizedMessage> Message = EditorErrors.Message(EMessageSeverity::Warning);
		if (File.EndsWith(TEXT(".ini")))
		{
			Message->AddToken(FURLToken::Create(FString::Printf(TEXT("file://%s"), *AbsPath), LOCTEXT("DeprecatedConfig_URLCLick", "Click to open file")));
		}
		Message->AddToken(FTextToken::Create(Msg));
		EditorErrors.Notify();
	}

	// always spit to log
	UE_LOG(LogConfig, Warning, TEXT("%s"), *Msg.ToString());
}

// warn about a section name that's deprecated
void WarnAboutSectionRemap(const FString& OldValue, const FString& NewValue, const FString& File)
{
	if (!GAllowConfigRemapWarning)
	{
		return;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("OldValue"), FText::FromString(OldValue));
	Arguments.Add(TEXT("NewValue"), FText::FromString(NewValue));
	Arguments.Add(TEXT("File"), FText::FromString(File));
	FText Msg = FText::Format(LOCTEXT("DeprecatedConfig", "Found a deprecated ini section name in {File}. Search for [{OldValue}] and replace with [{NewValue}]"), Arguments);
	
	FString Key = OldValue;
	if (!IsInGameThread())
	{
		AsyncTask( ENamedThreads::GameThread, [Msg, Key, File]() { LogOrEditorWarning(Msg, Key, File); });
	}
	else
	{
		LogOrEditorWarning(Msg, Key, File);
	}
}

// warn about a key that's deprecated
static void WarnAboutKeyRemap(const FString& OldValue, const FString& NewValue, const FString& Section, const FString& File)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("OldValue"), FText::FromString(OldValue));
	Arguments.Add(TEXT("NewValue"), FText::FromString(NewValue));
	Arguments.Add(TEXT("Section"), FText::FromString(Section));
	Arguments.Add(TEXT("File"), FText::FromString(File));
	FText Msg = FText::Format(LOCTEXT("DeprecatedConfigKey", "Found a deprecated ini key name in {File}. Search for [{OldValue}] and replace with [{NewValue}]"), Arguments);
	
	FString Key = OldValue+Section;
	if (!IsInGameThread())
	{
		AsyncTask( ENamedThreads::GameThread, [Msg, Key, File]() { LogOrEditorWarning(Msg, Key, File); });
	}
	else
	{
		LogOrEditorWarning(Msg, Key, File);
	}
}

}

void FConfigFile::CombineFromBuffer(const FString& Buffer, const FString& FileHint)
{
	static const FName ConfigFileClassName = TEXT("ConfigFile");
	const FName FileName = FName(*FileHint);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(FileName, ELLMTagSet::Assets);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(ConfigFileClassName, ELLMTagSet::AssetClasses);
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(FileName, ConfigFileClassName, FileName);

	const TCHAR* Ptr = *Buffer;
	FConfigSection* CurrentSection = nullptr;
	FString CurrentSectionName;
	TMap<FString, FString>* CurrentKeyRemap = nullptr;
	TStringBuilder<128> TheLine;
	FString ProcessedValue;
	bool Done = false;
	
	
	while( !Done )
	{
		// Advance past new line characters
		while( *Ptr=='\r' || *Ptr=='\n' )
		{
			Ptr++;
		}

		// read the next line
		int32 LinesConsumed = 0;
		FParse::LineExtended(&Ptr, /* reset */ TheLine, LinesConsumed, false);
		if (Ptr == nullptr || *Ptr == 0)
		{
			Done = true;
		}
		TCHAR* Start = const_cast<TCHAR*>(*TheLine);

		// Strip trailing spaces from the current line
		while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
		{
			Start[FCString::Strlen(Start)-1] = TEXT('\0');
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[FCString::Strlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[FCString::Strlen(Start)-1] = TEXT('\0');

			// If we don't have an existing section by this name, add one
			CurrentSectionName = Start;
			
			// lookup to see if there is an entry in the SectionName remap
			const FString* FoundRemap;
			if ((FoundRemap = SectionRemap.Find(CurrentSectionName)) != nullptr)
			{
				// show warning in editor
				WarnAboutSectionRemap(CurrentSectionName, *FoundRemap, FileHint);
				
				CurrentSectionName = *FoundRemap;
			}
			CurrentSection = FindOrAddSectionInternal(CurrentSectionName);

			// look to see if there is a set of key remaps for this section
			CurrentKeyRemap = KeyRemap.Find(CurrentSectionName);

			// make sure the CurrentSection has any of the special ArrayOfStructKeys added
			if (PerObjectConfigArrayOfStructKeys.Num() > 0)
			{
				FixupArrayOfStructKeysForSection(CurrentSection, CurrentSectionName, PerObjectConfigArrayOfStructKeys);
			}
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = FCString::Strstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the property name, advancing past the =
				*Value++ = TEXT('\0');

				// strip leading whitespace from the property name
				while ( *Start && FChar::IsWhitespace(*Start) )
				{						
					Start++;
				}

				// ~ is a packaging and should be skipped at runtime
				if (Start[0] == '~')
				{
					Start++;
				}

				// determine how this line will be merged
				TCHAR Cmd = Start[0];
				if ( Cmd=='+' || Cmd=='-' || Cmd=='.' || Cmd == '!' || Cmd == '@' || Cmd == '*' )
				{
					Start++;
				}
				else
				{
					Cmd = TEXT(' ');
				}

				// Strip trailing spaces from the property name.
				while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
				{
					Start[FCString::Strlen(Start)-1] = TEXT('\0');
				}

				const TCHAR* KeyName = Start;
				FConfigSection* OriginalCurrentSection = CurrentSection;
				// look up for key remap
				if (CurrentKeyRemap != nullptr)
				{
					const FString* FoundRemap;
					if ((FoundRemap = CurrentKeyRemap->Find(KeyName)) != nullptr)
					{
						WarnAboutKeyRemap(KeyName, *FoundRemap, CurrentSectionName, FileHint);

						// the Remap will not ever reallocate, so we can just point right into the FString
						KeyName = **FoundRemap;

						// look for a section:name remap
						int32 ColonLoc;
						if (FoundRemap->FindChar(':', ColonLoc))
						{
							// find or create a section for name before the :
							CurrentSection = FindOrAddSectionInternal(*FoundRemap->Mid(0, ColonLoc));
							// the name can still point right into the FString, but right after the :
							KeyName = **FoundRemap + ColonLoc + 1;
						}
					}
				}
				
				// Strip leading whitespace from the property value
				while ( *Value && FChar::IsWhitespace(*Value) )
				{
					Value++;
				}

				// strip trailing whitespace from the property value
				while( *Value && FChar::IsWhitespace(Value[FCString::Strlen(Value)-1]) )
				{
					Value[FCString::Strlen(Value)-1] = TEXT('\0');
				}

				ProcessedValue.Reset();

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					FParse::QuotedString(Value, ProcessedValue);
				}
				else
				{
					ProcessedValue = Value;
				}

				const FName Key(KeyName);
				if (Cmd == '+')
				{
					// Add if not already present.
					CurrentSection->HandleAddCommand(Key, MoveTemp(ProcessedValue), false);
				}
				else if( Cmd=='-' )
				{
					// Remove if present.
					CurrentSection->RemoveSingle(Key, ProcessedValue);
					CurrentSection->CompactStable();
				}
				else if ( Cmd=='.' )
				{
					CurrentSection->HandleAddCommand(Key, MoveTemp(ProcessedValue), true);
				}
				else if( Cmd=='!' )
				{
					CurrentSection->Remove(Key);
				}
				else if (Cmd == '@')
				{
					// track a key to show uniqueness for arrays of structs
					CurrentSection->ArrayOfStructKeys.Add(Key, MoveTemp(ProcessedValue));
				}
				else if (Cmd == '*')
				{
					// track a key to show uniqueness for arrays of structs
					TMap<FName, FString>& POCKeys = FindOrAddHeterogeneous(PerObjectConfigArrayOfStructKeys, CurrentSectionName);
					POCKeys.Add(Key, MoveTemp(ProcessedValue));
				}
				else
				{
					// First see if this can be processed as an array of keyed structs command
					if (!CurrentSection->HandleArrayOfKeyedStructsCommand(Key, MoveTemp(ProcessedValue)))
					{
						// Add if not present and replace if present.
						FConfigValue* ConfigValue = CurrentSection->Find(Key);
						if (!ConfigValue)
						{
							CurrentSection->Add(Key,
								FConfigValue(MoveTemp(ProcessedValue)));
						}
						else
						{
							*ConfigValue = MoveTemp(ProcessedValue);
						}
					}
				}
				
				// restore the current section, in case it was overridden
				CurrentSection = OriginalCurrentSection;

				// Mark as dirty so "Write" will actually save the changes.
				Dirty = true;
			}
		}
	}

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

/**
 * Process the contents of an .ini file that has been read into an FString
 * 
 * @param Contents Contents of the .ini file
 */
void FConfigFile::ProcessInputFileContents(FStringView Contents, const FString& FileHint)
{
	static const FName ConfigFileClassName = TEXT("ConfigFile");
	const FName FileName = FName(*FileHint);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(FileName, ELLMTagSet::Assets);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(ConfigFileClassName, ELLMTagSet::AssetClasses);
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(FileName, ConfigFileClassName, FileName);

	const TCHAR* Ptr = Contents.Len() > 0 ? Contents.GetData() : nullptr;
	FConfigSection* CurrentSection = nullptr;
	FString CurrentSectionName;
	TMap<FString, FString>* CurrentKeyRemap = nullptr;
	TStringBuilder<128> TheLine;
	bool Done = false;
	while( !Done && Ptr != nullptr )
	{
		// Advance past new line characters
		while( *Ptr=='\r' || *Ptr=='\n' )
		{
			Ptr++;
		}			
		// read the next line
		int32 LinesConsumed = 0;
		FParse::LineExtended(&Ptr, TheLine, LinesConsumed, false);
		if (Ptr == nullptr || *Ptr == 0)
		{
			Done = true;
		}
		TCHAR* Start = const_cast<TCHAR*>(*TheLine);

		// Strip trailing spaces from the current line
		while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
		{
			Start[FCString::Strlen(Start)-1] = TEXT('\0');
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[FCString::Strlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[FCString::Strlen(Start)-1] = TEXT('\0');

			// lookup to see if there is an entry in the SectionName remap
			CurrentSectionName = Start;
			const FString* FoundRemap;
			if ((FoundRemap = SectionRemap.Find(CurrentSectionName)) != nullptr)
			{
				WarnAboutSectionRemap(CurrentSectionName, *FoundRemap, FileHint);
				
				CurrentSectionName = *FoundRemap;
			}
			// look to see if there is a set of key remaps for this section
			CurrentKeyRemap = KeyRemap.Find(CurrentSectionName);

			// If we don't have an existing section by this name, add one
			CurrentSection = FindOrAddSectionInternal(CurrentSectionName);
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = FCString::Strstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the propertyname, advancing past the =
				*Value++ = TEXT('\0');

				// strip leading whitespace from the property name
				while ( *Start && FChar::IsWhitespace(*Start) )
					Start++;

				// Strip trailing spaces from the property name.
				while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
					Start[FCString::Strlen(Start)-1] = TEXT('\0');

				const TCHAR* KeyName = Start;
				// look up for key remap
				if (CurrentKeyRemap != nullptr)
				{
					const FString* FoundRemap;
					if ((FoundRemap = CurrentKeyRemap->Find(KeyName)) != nullptr)
					{
						WarnAboutKeyRemap(KeyName, *FoundRemap, CurrentSectionName, FileHint);
						
						// the Remap will not ever reallocate, so we can just point right into the FString
						KeyName = **FoundRemap;
					}
				}

				// Strip leading whitespace from the property value
				while ( *Value && FChar::IsWhitespace(*Value) )
					Value++;

				// strip trailing whitespace from the property value
				while( *Value && FChar::IsWhitespace(Value[FCString::Strlen(Value)-1]) )
					Value[FCString::Strlen(Value)-1] = TEXT('\0');

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					FString ProcessedValue;
					FParse::QuotedString(Value, ProcessedValue);

					// Add this pair to the current FConfigSection
					CurrentSection->Add(KeyName, FConfigValue(MoveTemp(ProcessedValue)));
				}
				else
				{
					// Add this pair to the current FConfigSection
					CurrentSection->Add(KeyName, FConfigValue(Value));
				}
			}
		}
	}

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

void FConfigFile::Read( const FString& Filename )
{
	// we can't read in a file if file IO is disabled
	if (GConfig == nullptr || !GConfig->AreFileOperationsDisabled())
	{
		Empty();
		FString Text;

		FString FinalFileName = Filename;
		bool bFoundOverride = OverrideFileFromCommandline(FinalFileName);
	
		if (LoadConfigFileWrapper(*FinalFileName, Text, bFoundOverride))
		{
			// process the contents of the string
			ProcessInputFileContents(Text, Filename);
		}
		else
		{
			checkf(!bFoundOverride, TEXT("Failed to Load config override %s"), *FinalFileName);
		}
	}
}

bool FConfigFile::ShouldExportQuotedString(const FString& PropertyValue)
{
	bool bEscapeNextChar = false;
	bool bIsWithinQuotes = false;

	// The value should be exported as quoted string if...
	const TCHAR* const DataPtr = *PropertyValue;
	for (const TCHAR* CharPtr = DataPtr; *CharPtr; ++CharPtr)
	{
		const TCHAR ThisChar = *CharPtr;
		const TCHAR NextChar = *(CharPtr + 1);

		const bool bIsFirstChar = CharPtr == DataPtr;
		const bool bIsLastChar = NextChar == 0;

		if (ThisChar == TEXT('"') && !bEscapeNextChar)
		{
			bIsWithinQuotes = !bIsWithinQuotes;
		}
		bEscapeNextChar = ThisChar == TEXT('\\') && bIsWithinQuotes && !bEscapeNextChar;

		// ... it begins or ends with a space (which is stripped on import)
		if (ThisChar == TEXT(' ') && (bIsFirstChar || bIsLastChar))
		{
			return true;
		}

		// ... it begins with a '"' (which would be treated as a quoted string)
		if (ThisChar == TEXT('"') && bIsFirstChar)
		{
			return true;
		}

		// ... it ends with a '\' (which would be treated as a line extension)
		if (ThisChar == TEXT('\\') && bIsLastChar)
		{
			return true;
		}

		// ... it contains unquoted '{' or '}' (which are stripped on import)
		if ((ThisChar == TEXT('{') || ThisChar == TEXT('}')) && !bIsWithinQuotes)
		{
			return true;
		}
		
		// ... it contains unquoted '//' (interpreted as a comment when importing)
		if ((ThisChar == TEXT('/') && NextChar == TEXT('/')) && !bIsWithinQuotes)
		{
			return true;
		}

		// ... it contains an unescaped new-line
		if (!bEscapeNextChar && (NextChar == TEXT('\r') || NextChar == TEXT('\n')))
		{
			return true;
		}
	}

	return false;
}

FString FConfigFile::GenerateExportedPropertyLine(const FString& PropertyName, const FString& PropertyValue)
{
	FString Out;
	AppendExportedPropertyLine(Out, PropertyName, PropertyValue);
	return Out;
}

void FConfigFile::AppendExportedPropertyLine(FString& Out, const FString& PropertyName, const FString& PropertyValue)
{
	// Append has been measured to be twice as fast as Appendf here
	Out.Append(PropertyName);

	Out.AppendChar(TEXT('='));

	if (FConfigFile::ShouldExportQuotedString(PropertyValue))
	{
		Out.AppendChar(TEXT('"'));
		Out.Append(PropertyValue.ReplaceCharWithEscapedChar());
		Out.AppendChar(TEXT('"'));
	}
	else
	{
		Out.Append(PropertyValue);
	}

	static const int32 LineTerminatorLen = FCString::Strlen(LINE_TERMINATOR);
	Out.Append(LINE_TERMINATOR, LineTerminatorLen);
}

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE

/** A collection of identifiers which will help us parse the commandline opions. */
namespace CommandlineOverrideSpecifiers
{
	// -ini:IniName:[Section1]:Key1=Value1,[Section2]:Key2=Value2
	const auto& IniFileOverrideIdentifier = TEXT("-iniFile=");
	const auto& IniSwitchIdentifier       = TEXT("-ini:");
	const auto& IniNameEndIdentifier      = TEXT(":[");
	const auto& SectionStartIdentifier    = TEXT("[");
	const auto& PropertyStartIdentifier   = TEXT("]:");
	const auto& PropertySeperator         = TEXT(",");
	const auto& CustomConfigIdentifier    = TEXT("-CustomConfig=");
}

#endif
bool FConfigFile::OverrideFileFromCommandline(FString& Filename)
{
#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	// look for this filename on the commandline in the format:
	//		-iniFile=<PatFile1>,<PatFile2>,<PatFile3>
	// for example:
	//		-iniFile=D:\FN-Main\FortniteGame\Config\Windows\WindowsDeviceProfiles.ini
	//       
	//		Description: 
	//          The FortniteGame\Config\Windows\WindowsDeviceProfiles.ini contained in the pak file will
	//          be replace with D:\FN-Main\FortniteGame\Config\Windows\WindowsDeviceProfiles.ini.

	//			Note: You will need the same base file path for this to work. If you
	//                want to override Engine/Config/BaseEngine.ini, you will need to place the override file 
	//                under the same folder structure. 
	//          Ex1: D:\<some_folder>\Engine\Config\BaseEngine.ini
	//			Ex2: D:\<some_folder>\FortniteGame\Config\Windows\WindowsEngine.ini
	FString StagedFilePaths;
	if(FParse::Value(FCommandLine::Get(), CommandlineOverrideSpecifiers::IniFileOverrideIdentifier, StagedFilePaths, false))
	{ 
		FString RelativePath = Filename;
		if (FPaths::IsUnderDirectory(RelativePath, FPaths::RootDir()))
		{
			FPaths::MakePathRelativeTo(RelativePath, *FPaths::RootDir());

			TArray<FString> Files;
			StagedFilePaths.ParseIntoArray(Files, TEXT(","), true);
			for (int32 Index = 0; Index < Files.Num(); Index++)
			{
				FString NormalizedOverride = Files[Index];
				FPaths::NormalizeFilename(NormalizedOverride);
				if (NormalizedOverride.EndsWith(RelativePath))
				{
					Filename = Files[Index];
					UE_LOG(LogConfig, Warning, TEXT("Loading override ini file: %s "), *Files[Index]);
					return true;
				}
			}
		}
	}
#endif

	return false;
}
/**
* Looks for any overrides on the commandline for this file
*
* @param File Config to possibly modify
* @param Filename Name of the .ini file to look for overrides
*/
void FConfigFile::OverrideFromCommandline(FConfigFile* File, const FString& Filename)
{
#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	FString Settings;
	// look for this filename on the commandline in the format:
	//		-ini:IniName:[Section1]:Key1=Value1,[Section2]:Key2=Value2
	// for example:
	//		-ini:Engine:[/Script/Engine.Engine]:bSmoothFrameRate=False,[TextureStreaming]:PoolSize=100
	//			(will update the cache after the final combined engine.ini)
	const TCHAR* CommandlineStream = FCommandLine::Get();
	while(FParse::Value(CommandlineStream, *FString::Printf(TEXT("%s%s"), CommandlineOverrideSpecifiers::IniSwitchIdentifier, *FPaths::GetBaseFilename(Filename)), Settings, false))
	{
		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(SettingPairs, CommandlineOverrideSpecifiers::PropertySeperator, true);
		for (int32 Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString SectionAndKey, Value;
			if (SettingPairs[Index].Split(TEXT("="), &SectionAndKey, &Value))
			{
				// now we need to split off the key from the rest of the section name
				int32 SectionNameEndIndex = SectionAndKey.Find(CommandlineOverrideSpecifiers::PropertyStartIdentifier, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				// check for malformed string
				if (SectionNameEndIndex == INDEX_NONE || SectionNameEndIndex == 0)
				{
					continue;
				}

				// Create the commandline override object
				FConfigCommandlineOverride& CommandlineOption = File->CommandlineOptions[File->CommandlineOptions.Emplace()];
				CommandlineOption.BaseFileName = *FPaths::GetBaseFilename(Filename);
				CommandlineOption.Section = SectionAndKey.Left(SectionNameEndIndex);
				
				// Remove commandline syntax from the section name.
				CommandlineOption.Section = CommandlineOption.Section.Replace(CommandlineOverrideSpecifiers::IniNameEndIdentifier, TEXT(""));
				CommandlineOption.Section = CommandlineOption.Section.Replace(CommandlineOverrideSpecifiers::PropertyStartIdentifier, TEXT(""));
				CommandlineOption.Section = CommandlineOption.Section.Replace(CommandlineOverrideSpecifiers::SectionStartIdentifier, TEXT(""));

				CommandlineOption.PropertyKey = SectionAndKey.Mid(SectionNameEndIndex + UE_ARRAY_COUNT(CommandlineOverrideSpecifiers::PropertyStartIdentifier) - 1);
				CommandlineOption.PropertyValue = Value;

				// now put it into this into the cache
				if (CommandlineOption.PropertyKey.StartsWith(TEXT("-")))
				{
					CommandlineOption.PropertyKey.RemoveFromStart(TEXT("-"));

					TArray<FString> ValueArray;
					File->GetArray(*CommandlineOption.Section, *CommandlineOption.PropertyKey, ValueArray);
					ValueArray.Remove(CommandlineOption.PropertyValue);
					File->SetArray(*CommandlineOption.Section, *CommandlineOption.PropertyKey, ValueArray);
				}
				else if (CommandlineOption.PropertyKey.StartsWith(TEXT("+")))
				{
					CommandlineOption.PropertyKey.RemoveFromStart(TEXT("+"));

					TArray<FString> ValueArray;
					File->GetArray(*CommandlineOption.Section, *CommandlineOption.PropertyKey, ValueArray);
					ValueArray.Add(CommandlineOption.PropertyValue);
					File->SetArray(*CommandlineOption.Section, *CommandlineOption.PropertyKey, ValueArray);
				}
				else
				{
					File->SetString(*CommandlineOption.Section, *CommandlineOption.PropertyKey, *CommandlineOption.PropertyValue);
				}	
			}
		}

		// Keep searching for more instances of -ini
		CommandlineStream = FCString::Stristr(CommandlineStream, CommandlineOverrideSpecifiers::IniSwitchIdentifier);
		check(CommandlineStream);
		CommandlineStream++;
	}
#endif
}


void FConfigFile::AddDynamicLayerToHierarchy(const FString& Filename)
{
	FString ConfigContent;
	if (!FFileHelper::LoadFileToString(ConfigContent, *Filename))
		return;

	if (SourceConfigFile)
	{
		SourceConfigFile->SourceIniHierarchy.AddDynamicLayer(Filename);
		SourceConfigFile->CombineFromBuffer(ConfigContent, Filename);
	}

	SourceIniHierarchy.AddDynamicLayer(Filename);
	CombineFromBuffer(ConfigContent, Filename);
}


namespace UE::ConfigCacheIni::Private
{

struct FImpl
{
/**
 * Check if the provided config section has a property which matches the one we are providing
 *
 * @param InSection			- The section which we are looking for a match in
 * @param InPropertyName	- The name of the property we are looking to match
 * @param InPropertyValue	- The value of the property which, if found, we are checking a match
 *
 * @return True if a property was found in the InSection which matched the Property Name and Value.
 */
static bool DoesConfigPropertyValueMatch(const FConfigSection* InSection, const FName& InPropertyName, const FString& InPropertyValue)
{
	bool bFoundAMatch = false;

	if (InSection)
	{
		const bool bIsInputStringValidFloat = FDefaultValueHelper::IsStringValidFloat(InPropertyValue);

		// Start Array check, if the property is in an array, we need to iterate over all properties.
		for (FConfigSection::TConstKeyIterator It(*InSection, InPropertyName); It && !bFoundAMatch; ++It)
		{
			const FString& PropertyValue = UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(It.Value());
			bFoundAMatch =
				PropertyValue.Len() == InPropertyValue.Len() &&
				PropertyValue == InPropertyValue;

			// if our properties don't match, run further checks
			if (!bFoundAMatch)
			{
				// Check that the mismatch isn't just a string comparison issue with floats
				if (bIsInputStringValidFloat && FDefaultValueHelper::IsStringValidFloat(PropertyValue))
				{
					bFoundAMatch = FCString::Atof(*PropertyValue) == FCString::Atof(*InPropertyValue);
				}
			}
		}
	}


	return bFoundAMatch;
}

}; // struct FImpl

} // namespace UE::ConfigCacheIni::Private

/**
 * Check if the provided property information was set as a commandline override
 *
 * @param InConfigFile		- The config file which we want to check had overridden values
 * @param InSectionName		- The name of the section which we are checking for a match
 * @param InPropertyName		- The name of the property which we are checking for a match
 * @param InPropertyValue	- The value of the property which we are checking for a match
 *
 * @return True if a commandline option was set that matches the input parameters
 */
bool PropertySetFromCommandlineOption(const FConfigFile* InConfigFile, const FString& InSectionName, const FName& InPropertyName, const FString& InPropertyValue)
{
	bool bFromCommandline = false;

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	for (const FConfigCommandlineOverride& CommandlineOverride : InConfigFile->CommandlineOptions)
	{
		if (CommandlineOverride.PropertyKey.Equals(InPropertyName.ToString(), ESearchCase::IgnoreCase) &&
			CommandlineOverride.PropertyValue.Equals(InPropertyValue, ESearchCase::IgnoreCase) &&
			CommandlineOverride.Section.Equals(InSectionName, ESearchCase::IgnoreCase) &&
			CommandlineOverride.BaseFileName.Equals(FPaths::GetBaseFilename(InConfigFile->Name.ToString()), ESearchCase::IgnoreCase))
		{
			bFromCommandline = true;
		}
	}
#endif // ALLOW_INI_OVERRIDE_FROM_COMMANDLINE

	return bFromCommandline;
}

bool FConfigFile::WriteTempFileThenMove()
{
#if PLATFORM_DESKTOP && WITH_EDITOR
	bool bWriteTempFileThenMove = !FApp::IsGame() && !FApp::IsUnattended();
#else // PLATFORM_DESKTOP
	bool bWriteTempFileThenMove = false;
#endif

	return bWriteTempFileThenMove;
}

bool FConfigFile::Write(const FString& Filename, bool bDoRemoteWrite/* = true*/, const FString& PrefixText/*=FString()*/)
{
	TMap<FString, FString> SectionTexts;
	TArray<FString> SectionOrder;
	if (!PrefixText.IsEmpty())
	{
		SectionTexts.Add(FString(), PrefixText);
	}
	return WriteInternal(Filename, bDoRemoteWrite, SectionTexts, SectionOrder);
}

void FConfigFile::WriteToString(FString& InOutText, const FString& SimulatedFilename /*= FString()*/, const FString& PrefixText /*= FString()*/)
{
	TMap<FString, FString> SectionTexts;
	TArray<FString> SectionOrder;
	if (!PrefixText.IsEmpty())
	{
		SectionTexts.Add(FString(), PrefixText);
	}

	int32 IniCombineThreshold = MAX_int32;
	bool bIsADefaultIniWrite = IsADefaultIniWrite(SimulatedFilename, IniCombineThreshold);

	WriteToStringInternal(InOutText, bIsADefaultIniWrite, IniCombineThreshold, SectionTexts, SectionOrder);
}

bool FConfigFile::IsADefaultIniWrite(const FString& Filename, int32& OutIniCombineThreshold) const
{
	bool bIsADefaultIniWrite = false;
	{
		// If we are writing to a default config file and this property is an array, we need to be careful to remove those from higher up the hierarchy
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		const FString AbsoluteGameGeneratedConfigDir = FPaths::ConvertRelativePathToFull(FPaths::GeneratedConfigDir());
		const FString AbsoluteGameAgnosticGeneratedConfigDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GameAgnosticSavedDir(), TEXT("Config")) + TEXT("/"));
		bIsADefaultIniWrite = !AbsoluteFilename.Contains(AbsoluteGameGeneratedConfigDir) && !AbsoluteFilename.Contains(AbsoluteGameAgnosticGeneratedConfigDir);
	}

	OutIniCombineThreshold = MAX_int32;
	if (bIsADefaultIniWrite)
	{
		// find the filename in ini hierarchy
		FString IniName = FPaths::GetCleanFilename(Filename);
		for (const auto& HierarchyFileIt : SourceIniHierarchy)
		{
			if (FPaths::GetCleanFilename(HierarchyFileIt.Value) == IniName)
			{
				OutIniCombineThreshold = HierarchyFileIt.Key;
				break;
			}
		}
	}

	return bIsADefaultIniWrite;
}

bool FConfigFile::WriteInternal(const FString& Filename, bool bDoRemoteWrite, TMap<FString, FString>& InOutSectionTexts, const TArray<FString>& InSectionOrder)
{
	if (!Dirty || NoSave || FParse::Param(FCommandLine::Get(), TEXT("nowrite")) ||
		(FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")) && !FParse::Param(FCommandLine::Get(), TEXT("MultiprocessSaveConfig"))) // Is can be useful to save configs with multiprocess if they are given INI overrides
		)
	{
		return true;
	}

	int32 IniCombineThreshold = MAX_int32;
	bool bIsADefaultIniWrite = IsADefaultIniWrite(Filename, IniCombineThreshold);	

	FString Text;
	WriteToStringInternal(Text, bIsADefaultIniWrite, IniCombineThreshold, InOutSectionTexts, InSectionOrder);

	if (bDoRemoteWrite)
	{
		// Write out the remote version (assuming it was loaded)
		FRemoteConfig::Get()->Write(*Filename, Text);
	}

	// don't write out non-default configs that are only whitespace
	if (!bIsADefaultIniWrite && Text.TrimStart().Len() == 0)
	{
		IFileManager::Get().Delete(*Filename);
		return true;
	}
	
	bool bResult = SaveConfigFileWrapper(*Filename, Text);

	// File is still dirty if it didn't save.
	Dirty = !bResult;

	// Return if the write was successful
	return bResult;
}

void FConfigFile::WriteToStringInternal(FString& InOutText, bool bIsADefaultIniWrite, int32 IniCombineThreshold, TMap<FString, FString>& InOutSectionTexts, const TArray<FString>& InSectionOrder)
{
	const int32 InitialInOutTextSize = InOutText.Len();

	// Estimate max size to reduce re-allocations (does not inspect actual properties for performance)
	int32 InitialEstimatedFinalTextSize = 0;
	int32 HighestPropertiesInSection = 0;
	for (TIterator SectionIterator(*this); SectionIterator; ++SectionIterator)
	{
		HighestPropertiesInSection = FMath::Max(HighestPropertiesInSection, SectionIterator.Value().Num());
		InitialEstimatedFinalTextSize += ((SectionIterator.Value().Num() + 1) * 90);
	}
	// Limit size estimate to avoid pre-allocating too much memory
	InitialEstimatedFinalTextSize = FMath::Min(InitialEstimatedFinalTextSize, 128 * 1024 * 1024);
	InOutText.Reserve(InitialInOutTextSize + InitialEstimatedFinalTextSize);

	TArray<FString> SectionOrder;
	SectionOrder.Reserve(InSectionOrder.Num() + this->Num());
	SectionOrder.Append(InSectionOrder);
	InOutSectionTexts.Reserve(InSectionOrder.Num() + this->Num());

	TArray<const FConfigValue*> CompletePropertyToWrite;
	FString PropertyNameString;
	TSet<FName> PropertiesAddedLookup;
	PropertiesAddedLookup.Reserve(HighestPropertiesInSection);
	int32 EstimatedFinalTextSize = 0;
	
	// no need to look up the section if it's a default ini, or if we are always saving all sections
	const FConfigSection* SectionsToSaveSection = (bIsADefaultIniWrite || bCanSaveAllSections) ? nullptr : FindSection(SectionsToSaveStr);
	TArray<FString> SectionsToSave;
	if (SectionsToSaveSection != nullptr)
	{
		// Do not report the read of SectionsToSave. Some ConfigFiles are reallocated without it, and we
		// log that the section disappeared. But this log is spurious since the only reason it was read was
		// for the internal save before the FConfigFile is made publicly available.
		TArray<const FConfigValue*, TInlineAllocator<10>> SectionsToSaveValues;
		SectionsToSaveSection->MultiFindPointer("Section", SectionsToSaveValues);
		SectionsToSave.Reserve(SectionsToSaveValues.Num());
		for (const FConfigValue* ConfigValue : SectionsToSaveValues)
		{
			SectionsToSave.Add(UE::ConfigCacheIni::Private::FAccessor::GetValueForWriting(*ConfigValue));
		}
	}
	
	for( TIterator SectionIterator(*this); SectionIterator; ++SectionIterator )
	{
		const FString& SectionName = SectionIterator.Key();
		const FConfigSection& Section = SectionIterator.Value();
		
		// null Sections array means to save everything, otherwise check if we can save this section
		bool bCanSaveThisSection = SectionsToSaveSection == nullptr || SectionsToSave.Contains(SectionName);
		if (!bCanSaveThisSection)
		{
			continue;
		}

		// If we have a config file to check against, have a look.
		const FConfigSection* SourceConfigSection = nullptr;
		if (SourceConfigFile)
		{
			// Check the sections which could match our desired section name
			SourceConfigSection = SourceConfigFile->FindSection(SectionName);

#if !UE_BUILD_SHIPPING
			if (!SourceConfigSection && FPlatformProperties::RequiresCookedData() == false && SectionName.StartsWith(TEXT("/Script/")))
			{
				// Guard against short names in ini files
				const FString ShortSectionName = SectionName.Replace(TEXT("/Script/"), TEXT("")); 
				if (SourceConfigFile->FindSection(ShortSectionName) != nullptr)
				{
					UE_LOG(LogConfig, Fatal, TEXT("Short config section found while looking for %s"), *SectionName);
				}
			}
#endif
		}

		InOutText.LeftInline(InitialInOutTextSize, EAllowShrinking::No);
		PropertiesAddedLookup.Reset();

		for( FConfigSection::TConstIterator It2(Section); It2; ++It2 )
		{
			const FName PropertyName = It2.Key();
			// Use GetSavedValueForWriting rather than GetSavedValue to avoid having this save operation mark the values as having been accessed for dependency tracking
			const FString& PropertyValue = UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(It2.Value());

			// Check if the we've already processed a property of this name. If it was part of an array we may have already written it out.
			if( !PropertiesAddedLookup.Contains( PropertyName ) )
			{
				// check whether the option we are attempting to write out, came from the commandline as a temporary override.
				const bool bOptionIsFromCommandline = PropertySetFromCommandlineOption(this, SectionName, PropertyName, PropertyValue);

				// We ALWAYS want to write CurrentIniVersion.
				const bool bIsCurrentIniVersion = (SectionName == CurrentIniVersionStr);

				// Check if the property matches the source configs. We do not wanna write it out if so.
				if ((bIsADefaultIniWrite || bIsCurrentIniVersion ||
					!UE::ConfigCacheIni::Private::FImpl::DoesConfigPropertyValueMatch(SourceConfigSection, PropertyName, PropertyValue))
					&& !bOptionIsFromCommandline)
				{
					// If this is the first property we are writing of this section, then print the section name
					if( InOutText.Len() == InitialInOutTextSize )
					{
						InOutText.Appendf(TEXT("[%s]" LINE_TERMINATOR_ANSI), *SectionName);

						// and if the section has any array of struct uniqueness keys, add them here
						for (auto It = Section.ArrayOfStructKeys.CreateConstIterator(); It; ++It)
						{
							InOutText.Appendf(TEXT("@%s=%s" LINE_TERMINATOR_ANSI), *It.Key().ToString(), *It.Value());
						}
					}

					// Write out our property, if it is an array we need to write out the entire array.
					CompletePropertyToWrite.Reset();
					Section.MultiFindPointer( PropertyName, CompletePropertyToWrite, true );

					if( bIsADefaultIniWrite )
					{
						ProcessPropertyAndWriteForDefaults(IniCombineThreshold, CompletePropertyToWrite, InOutText, SectionName, PropertyName.ToString());
					}
					else
					{
						PropertyNameString.Reset(FName::StringBufferSize);
						PropertyName.AppendString(PropertyNameString);
						for (const FConfigValue* ConfigValue : CompletePropertyToWrite)
						{
							// Use GetSavedValueForWriting rather than GetSavedValue to avoid marking these values used during save to disk as having been accessed for dependency tracking
							AppendExportedPropertyLine(InOutText, PropertyNameString, UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(*ConfigValue));
						}
					}

					PropertiesAddedLookup.Add( PropertyName );
				}
			}
		}

		// If we didn't decide to write any properties on this section, then we don't add the section
		// to the destination file
		if (InOutText.Len() > InitialInOutTextSize)
		{
			InOutSectionTexts.FindOrAdd(SectionName) = InOutText.RightChop(InitialInOutTextSize);

			// Add the Section to SectionOrder in case it's not already there
			SectionOrder.Add(SectionName);

			EstimatedFinalTextSize += InOutText.Len() - InitialInOutTextSize + 4;
		}
		else
		{
			InOutSectionTexts.Remove(SectionName);
		}
	}

	// Join all of the sections together
	InOutText.LeftInline(InitialInOutTextSize, EAllowShrinking::No);
	InOutText.Reserve(InitialInOutTextSize + EstimatedFinalTextSize);
	TSet<FString> SectionNamesLeftToWrite;
	SectionNamesLeftToWrite.Reserve(InOutSectionTexts.Num());
	for (TPair<FString,FString>& kvpair : InOutSectionTexts)
	{
		SectionNamesLeftToWrite.Add(kvpair.Key);
	}

	static const FString BlankLine(TEXT(LINE_TERMINATOR_ANSI LINE_TERMINATOR_ANSI));
	auto AddSectionToText = [&InOutText, &InOutSectionTexts, &SectionNamesLeftToWrite](const FString& SectionName)
	{
		FString* SectionText = InOutSectionTexts.Find(SectionName);
		if (!SectionText)
		{
			return;
		}
		if (SectionNamesLeftToWrite.Remove(SectionName) == 0)
		{
			// We already wrote this section
			return;
		}
		InOutText.Append(*SectionText);
		if (!InOutText.EndsWith(BlankLine, ESearchCase::CaseSensitive))
		{
			InOutText.Append(LINE_TERMINATOR);
		}
	};

	// First add the empty section
	AddSectionToText(FString());

	// Second add all the sections in SectionOrder; this includes any sections in *this that were not in InSectionOrder, because we added them during the loop
	for (FString& SectionName : SectionOrder)
	{
		AddSectionToText(SectionName);
	}

	// Third add any remaining sections that were passed in in InOutSectionTexts but were not specified in InSectionOrder and were not in *this
	if (SectionNamesLeftToWrite.Num() > 0)
	{
		TArray<FString> RemainingNames;
		RemainingNames.Reserve(SectionNamesLeftToWrite.Num());
		for (FString& SectionName : SectionNamesLeftToWrite)
		{
			RemainingNames.Add(SectionName);
		}
		RemainingNames.Sort();
		for (FString& SectionName : RemainingNames)
		{
			AddSectionToText(SectionName);
		}
	}
}

/** Adds any properties that exist in InSourceFile that this config file is missing */
void FConfigFile::AddMissingProperties( const FConfigFile& InSourceFile )
{
	for( TConstIterator SourceSectionIt( InSourceFile ); SourceSectionIt; ++SourceSectionIt )
	{
		const FString& SourceSectionName = SourceSectionIt.Key();
		const FConfigSection& SourceSection = SourceSectionIt.Value();

		{
			// If we don't already have this section, go ahead and add it now
			FConfigSection* DestSection = FindOrAddSectionInternal( SourceSectionName );
			DestSection->Reserve(SourceSection.Num());

			for( FConfigSection::TConstIterator SourcePropertyIt( SourceSection ); SourcePropertyIt; ++SourcePropertyIt )
			{
				const FName SourcePropertyName = SourcePropertyIt.Key();
				
				// If we don't already have this property, go ahead and add it now
				if( DestSection->Find( SourcePropertyName ) == nullptr )
				{
					TArray<const FConfigValue*, TInlineAllocator<32>> Results;
					SourceSection.MultiFindPointer(SourcePropertyName, Results, true);
					for (const FConfigValue* Result : Results)
					{
						DestSection->Add(SourcePropertyName, *Result);
						Dirty = true;
					}
				}
			}
		}
	}
}



void FConfigFile::Dump(FOutputDevice& Ar)
{
	Ar.Logf( TEXT("FConfigFile::Dump") );

	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		Ar.Logf( TEXT("[%s]"), *It.Key() );
		TArray<FName> KeyNames;

		FConfigSection& Section = It.Value();
		Section.GetKeys(KeyNames);
		for(TArray<FName>::TConstIterator KeyNameIt(KeyNames);KeyNameIt;++KeyNameIt)
		{
			const FName KeyName = *KeyNameIt;

			TArray<FConfigValue> Values;
			Section.MultiFind(KeyName,Values,true);

			if ( Values.Num() > 1 )
			{
				for ( int32 ValueIndex = 0; ValueIndex < Values.Num(); ValueIndex++ )
				{
					Ar.Logf(TEXT("	%s[%i]=%s"), *KeyName.ToString(), ValueIndex, *Values[ValueIndex].GetValue().ReplaceCharWithEscapedChar());
				}
			}
			else
			{
				Ar.Logf(TEXT("	%s=%s"), *KeyName.ToString(), *Values[0].GetValue().ReplaceCharWithEscapedChar());
			}
		}

		Ar.Log( LINE_TERMINATOR );
	}
}

bool FConfigFile::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value ) const
{
	const FConfigSection* Sec = FindSection( Section );
	if( Sec == nullptr )
	{
		return false;
	}
	const FConfigValue* PairString = Sec->Find( Key );
	if( PairString == nullptr )
	{
		return false;
	}
	Value = PairString->GetValue();
	return true;
}

bool FConfigFile::GetText( const TCHAR* Section, const TCHAR* Key, FText& Value ) const
{
	const FConfigSection* Sec = FindSection( Section );
	if( Sec == nullptr )
	{
		return false;
	}
	const FConfigValue* PairString = Sec->Find( Key );
	if( PairString == nullptr )
	{
		return false;
	}
	return FTextStringHelper::ReadFromBuffer( *PairString->GetValue(), Value, Section ) != nullptr;
}

bool FConfigFile::GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atoi(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atof(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetDouble(const TCHAR* Section, const TCHAR* Key, double& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atod(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetInt64( const TCHAR* Section, const TCHAR* Key, int64& Value ) const
{
	FString Text; 
	if( GetString( Section, Key, Text ) )
	{
		Value = FCString::Atoi64(*Text);
		return true;
	}
	return false;
}
bool FConfigFile::GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value ) const
{
	FString Text;
	if ( GetString(Section, Key, Text ))
	{
		Value = FCString::ToBool(*Text);
		return 1;
	}
	return 0;
}

int32 FConfigFile::GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value) const
{
	Value.Empty();
	const FConfigSection* Sec = FindSection(Section);
	if (Sec != nullptr)
	{
		Sec->MultiFind(Key, Value, true);
	}
#if !UE_BUILD_SHIPPING
	else
	{
		CheckLongSectionNames(Section, this);
	}
#endif

	return Value.Num();
}

bool FConfigFile::DoesSectionExist(const TCHAR* Section) const
{
	return FindSection(Section) != nullptr;
}

void FConfigFile::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value )
{
	FConfigSection* Sec = FindOrAddSectionInternal( Section );

	FConfigValue* ConfigValue = Sec->Find( Key );
	if( ConfigValue == nullptr )
	{
		Sec->Add(Key, FConfigValue(Value));
		Dirty = true;
	}
	// Use GetSavedValueForWriting rather than GetSavedValue to avoid reporting the value as having been accessed for dependency tracking
	else if( FCString::Strcmp(*UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(*ConfigValue),Value)!=0 )
	{
		Dirty = true;
		*ConfigValue = Value;
	}
}

void FConfigFile::SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value )
{
	FConfigSection* Sec = FindOrAddSectionInternal( Section );

	FString StrValue;
	FTextStringHelper::WriteToBuffer(StrValue, Value);

	FConfigValue* ConfigValue = Sec->Find( Key );
	if( ConfigValue == nullptr )
	{
		Sec->Add(Key, FConfigValue(MoveTemp(StrValue)));
		Dirty = true;
	}
	// Use GetSavedValueForWriting rather than GetSavedValue to avoid reporting the value as having been accessed for dependency tracking
	else if( FCString::Strcmp(*UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(*ConfigValue), *StrValue)!=0 )
	{
		Dirty = true;
		*ConfigValue = MoveTemp(StrValue);
	}
}

void FConfigFile::SetFloat(const TCHAR* Section, const TCHAR* Key, float Value)
{
	TCHAR Text[MAX_SPRINTF];
	FCString::Sprintf(Text, TEXT("%.*g"), std::numeric_limits<float>::max_digits10, Value);
	SetString(Section, Key, Text);
}

void FConfigFile::SetDouble(const TCHAR* Section, const TCHAR* Key, double Value)
{
	TCHAR Text[MAX_SPRINTF];
	FCString::Sprintf(Text, TEXT("%.*g"), std::numeric_limits<double>::max_digits10, Value);
	SetString(Section, Key, Text);
}

void FConfigFile::SetBool(const TCHAR* Section, const TCHAR* Key, bool Value)
{
	SetString(Section, Key, Value ? TEXT("True") : TEXT("False"));
}

void FConfigFile::SetInt64( const TCHAR* Section, const TCHAR* Key, int64 Value )
{
	TCHAR Text[MAX_SPRINTF];
	FCString::Sprintf( Text, TEXT("%lld"), Value );
	SetString( Section, Key, Text );
}


void FConfigFile::SetArray(const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value)
{
	FConfigSection* Sec = FindOrAddSectionInternal(Section);

	if (Sec->Remove(Key) > 0)
	{
		Dirty = true;
	}

	for (int32 i = 0; i < Value.Num(); i++)
	{
		Sec->Add(Key, FConfigValue(Value[i]));
		Dirty = true;
	}
}

bool FConfigFile::AddToSection(const TCHAR* SectionName, FName Key, const FString& Value)
{
	FConfigSection* Section = FindOrAddSectionInternal(SectionName);
	Section->Add(Key, FConfigValue(Value));
	Dirty = true;
	return true;
}

bool FConfigFile::AddUniqueToSection(const TCHAR* SectionName, FName Key, const FString& Value)
{
	FConfigSection* Section = FindOrAddSectionInternal(SectionName);
	if (Section->FindPair(Key, FConfigValue(Value)))
	{
		return false;
	}
	
	// just call Add since we already checked above if it exists (AddUnique can't return whether or not it existed)
	Section->Add(Key, FConfigValue(Value));
	Dirty = true;
	return true;
}

bool FConfigFile::RemoveKeyFromSection(const TCHAR* SectionName, FName Key)
{
	FConfigSection* Section = FindInternal(SectionName);
	// if it doesn't contain the key for any number of values
	if (Section == nullptr || !Section->Contains(Key))
	{
		return false;
	}

	Section->Remove(Key);
	Dirty = true;
	return true;
}

bool FConfigFile::RemoveFromSection(const TCHAR* SectionName, FName Key, const FString& Value)
{
	FConfigSection* Section = FindInternal(SectionName);
	// if it doesn't contain the pair, do nothing
	if (Section == nullptr || !Section->FindPair(Key, FConfigValue(Value)))
	{
		return false;
	}

	// remove any copies of the pair
	Section->Remove(Key, FConfigValue(Value));
	Dirty = true;
	return true;
}

void FConfigFile::SaveSourceToBackupFile()
{
	FString Text;

	FString BetweenRunsDir = (FPaths::ProjectIntermediateDir() / TEXT("Config/CoalescedSourceConfigs/"));
	FString Filename = FString::Printf( TEXT( "%s%s.ini" ), *BetweenRunsDir, *Name.ToString() );

	for( TMap<FString,FConfigSection>::TIterator SectionIterator(*SourceConfigFile); SectionIterator; ++SectionIterator )
	{
		const FString& SectionName = SectionIterator.Key();
		const FConfigSection& Section = SectionIterator.Value();

		Text += FString::Printf( TEXT("[%s]" LINE_TERMINATOR_ANSI), *SectionName);

		for( FConfigSection::TConstIterator PropertyIterator(Section); PropertyIterator; ++PropertyIterator )
		{
			const FName PropertyName = PropertyIterator.Key();
			// Use GetSavedValueForWriting rather than GetSavedValue to avoid having this save operation mark the values as having been accessed for dependency tracking
			const FString& PropertyValue = UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(PropertyIterator.Value());
			Text += FConfigFile::GenerateExportedPropertyLine(PropertyName.ToString(), PropertyValue);
		}
		Text += LINE_TERMINATOR;
	}

	if(!SaveConfigFileWrapper(*Filename, Text))
	{
		UE_LOG(LogConfig, Warning, TEXT("Failed to saved backup for config[%s]"), *Filename);
	}
}


void FConfigFile::ProcessSourceAndCheckAgainstBackup()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		FString BetweenRunsDir = (FPaths::ProjectIntermediateDir() / TEXT("Config/CoalescedSourceConfigs/"));
		FString BackupFilename = FString::Printf( TEXT( "%s%s.ini" ), *BetweenRunsDir, *Name.ToString() );

		FConfigFile BackupFile;
		ProcessIniContents(*BackupFilename, *BackupFilename, &BackupFile, false, false);

		for (TMap<FString,FConfigSection>::TIterator SectionIterator(*SourceConfigFile); SectionIterator; ++SectionIterator)
		{
			const FString& SectionName = SectionIterator.Key();
			const FConfigSection& SourceSection = SectionIterator.Value();
			const FConfigSection* BackupSection = BackupFile.FindSection( SectionName );
			
			if (BackupSection && !UE::ConfigCacheIni::Private::FAccessor::AreSectionsEqualForWriting(SourceSection, *BackupSection))
			{
				this->Remove( SectionName );
				this->Add( SectionName, SourceSection );
			}
		}

		SaveSourceToBackupFile();
	}
}

static TArray<FString> GetSourceProperties(const FConfigFileHierarchy& SourceIniHierarchy, int IniCombineThreshold, const FString& SectionName, const FString& PropertyName)
{
	// Build a config file out of this default configs hierarchy.
	FConfigCacheIni Hierarchy(EConfigCacheType::Temporary);

	int32 HighestFileIndex = 0;
	TArray<int32> ExistingEntries;
	SourceIniHierarchy.GetKeys(ExistingEntries);
	for (const int32& NextEntry : ExistingEntries)
	{
		HighestFileIndex = NextEntry > HighestFileIndex ? NextEntry : HighestFileIndex;
	}

	const FString& LastFileInHierarchy = SourceIniHierarchy.FindChecked(HighestFileIndex);
	FConfigFile& DefaultConfigFile = Hierarchy.Add(LastFileInHierarchy, FConfigFile());

	for (const auto& HierarchyFileIt : SourceIniHierarchy)
	{
		// Combine everything up to the level we're writing, but not including it.
		// Inclusion would result in a bad feedback loop where on subsequent writes 
		// we would be diffing against the same config we've just written to.
		if (HierarchyFileIt.Key < IniCombineThreshold)
		{
			DefaultConfigFile.Combine(HierarchyFileIt.Value);
		}
	}

	// Remove any array elements from the default configs hierearchy, we will add these in below
	// Note.	This compensates for an issue where strings in the hierarchy have a slightly different format
	//			to how the config system wishes to serialize them.
	TArray<FString> SourceArrayProperties;
	Hierarchy.GetArray(*SectionName, *PropertyName, SourceArrayProperties, *LastFileInHierarchy);

	return SourceArrayProperties;
}

void FConfigFile::ProcessPropertyAndWriteForDefaults(int IniCombineThreshold, const TArray<const FConfigValue*>& InCompletePropertyToProcess, FString& OutText, const FString& SectionName, const FString& PropertyName)
{
	// Only process against a hierarchy if this config file has one.
	if (SourceIniHierarchy.Num() > 0)
	{
		FString CleanedPropertyName = PropertyName;
		const bool bHadPlus = CleanedPropertyName.RemoveFromStart(TEXT("+"));
		const bool bHadBang = CleanedPropertyName.RemoveFromStart(TEXT("!"));

		const FString PropertyNameWithRemoveOp = TEXT("-") + CleanedPropertyName;

		// look for pointless !Clear entries that the config system wrote out when it noticed the user didn't have any entries
		if (bHadBang && InCompletePropertyToProcess.Num() == 1 && InCompletePropertyToProcess[0]->GetSavedValue() == TEXT("__ClearArray__"))
		{
			const TArray<FString> SourceArrayProperties = GetSourceProperties(SourceIniHierarchy, IniCombineThreshold, SectionName, CleanedPropertyName);
			for (const FString& NextElement : SourceArrayProperties)
			{
				OutText.Append(GenerateExportedPropertyLine(PropertyNameWithRemoveOp, NextElement));
			}

			// We don't need to write out the ! entry so just leave now.
			return;
		}

		// Handle array elements from the configs hierarchy.
		if (bHadPlus || InCompletePropertyToProcess.Num() > 1)
		{
			const TArray<FString> SourceArrayProperties = GetSourceProperties(SourceIniHierarchy, IniCombineThreshold, SectionName, CleanedPropertyName);
			for (const FString& NextElement : SourceArrayProperties)
			{
				OutText.Append(GenerateExportedPropertyLine(PropertyNameWithRemoveOp, NextElement));
			}
		}
	}

	// Write the properties out to a file.
	for (const FConfigValue* PropertyIt : InCompletePropertyToProcess)
	{
		OutText.Append(GenerateExportedPropertyLine(PropertyName, PropertyIt->GetSavedValue()));
	}
}


/*-----------------------------------------------------------------------------
	FConfigCacheIni
-----------------------------------------------------------------------------*/

namespace
{
	void OnConfigSectionsChanged(const FString& IniFilename, const TSet<FString>& SectionNames)
	{
		if (IniFilename == GEngineIni && SectionNames.Contains(TEXT("ConsoleVariables")))
		{
			UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("ConsoleVariables"), *GEngineIni, ECVF_SetByHotfix);
		}
	}
}

#if WITH_EDITOR
static TMap<FName, TFuture<void>>& GetPlatformConfigFutures()
{
	static TMap<FName, TFuture<void>> Futures;
	return Futures;
}
#endif

FConfigCacheIni::FConfigCacheIni(EConfigCacheType InType)
	: bAreFileOperationsDisabled(false)
	, bIsReadyForUse(false)
	, Type(InType)
{
}

FConfigCacheIni::FConfigCacheIni()
{
	EnsureRetrievingVTablePtrDuringCtor(TEXT("FConfigCacheIni()"));
}

FConfigCacheIni::~FConfigCacheIni()
{
	// this destructor can run at file scope, static shutdown
	Flush( 1 );
}


FConfigFile* FConfigCacheIni::FindConfigFile( const FString& Filename )
{
	// look for a known file, if there's no ini extension
	FConfigFile* Result = Filename.EndsWith(TEXT(".ini")) ? nullptr : KnownFiles.GetMutableFile(FName(*Filename));

	if (Result == nullptr)
	{
		Result = OtherFiles.FindRef(Filename);
	}
	return Result;
}

FConfigFile* FConfigCacheIni::Find(const FString& Filename)
{	
	// check for non-filenames
	if(Filename.Len() == 0)
	{
		return nullptr;
	}

	// Get the file if it exists
	FConfigFile* Result = FindConfigFile(Filename);

	// this is || filesize so we load up .int files if file IO is allowed
	if (!Result && !bAreFileOperationsDisabled)
	{
		// Before attempting to add another file, double check that this doesn't exist at a normalized path.
		const FString UnrealFileName = NormalizeConfigIniPath(Filename);
		Result = FindConfigFile(UnrealFileName);
		
		if (!Result)
		{
			Result = &Add(UnrealFileName, FConfigFile());
			UE_LOG(LogConfig, Verbose, TEXT("GConfig::Find is looking for file:  %s"), *UnrealFileName);
			if (DoesConfigFileExistWrapper(*UnrealFileName))
			{
				Result->Read(UnrealFileName);
				UE_LOG(LogConfig, Verbose, TEXT("GConfig::Find has loaded file:  %s"), *UnrealFileName);
			}
		}
		else
		{
			// We could normalize always normalize paths, but we don't want to always incur the penalty of that
			// when callers can cache the strings ahead of time.
			UE_LOG(LogConfig, Warning, TEXT("GConfig::Find attempting to access config with non-normalized path %s. Please use FConfigCacheIni::NormalizeConfigIniPath before accessing INI files through ConfigCache."), *Filename);
		}
	}

	return Result;
}

FConfigFile* FConfigCacheIni::Find(const FString& Filename, bool CreateIfNotFound)
{
	UE_LOG(LogConfig, Verbose, TEXT("GConfig::Find is ignoring deprecated parameter CreateIfNotFound for file:  %s"), *Filename);
	return Find(Filename);
}

FConfigFile* FConfigCacheIni::FindConfigFileWithBaseName(FName BaseName)
{
	if (FConfigFile* Result = KnownFiles.GetMutableFile(BaseName))
	{
		return Result;
	}

	for (TPair<FString,FConfigFile*>& CurrentFilePair : OtherFiles)
	{
		if (CurrentFilePair.Value->Name == BaseName)
		{
			return CurrentFilePair.Value;
		}
	}
	return nullptr;
}

FConfigFile& FConfigCacheIni::Add(const FString& Filename, const FConfigFile& File)
{
	FConfigFile*& Result = OtherFiles.FindOrAdd(Filename);
	if (Result)
	{
		delete Result;
	}
	Result = new FConfigFile(File);
	return *Result;
}

bool FConfigCacheIni::ContainsConfigFile(const FConfigFile* ConfigFile) const
{
	// Check the normal inis. Note that the FConfigFiles in the map
	// could have been reallocated if new inis were added to the map
	// since the point at which the caller received the ConfigFile pointer
	// they are testing. It is the caller's responsibility to not try to hold
	// on to the ConfigFile pointer during writes to this ConfigCacheIni
	for (const TPair<FString, FConfigFile*>& CurrentFilePair : OtherFiles)
	{
		if (ConfigFile == CurrentFilePair.Value)
		{
			return true;
		}
	}
	// Check the known inis
	for (const FKnownConfigFiles::FKnownConfigFile& KnownFile : KnownFiles.Files)
	{
		if (ConfigFile == &KnownFile.IniFile)
		{
			return true;
		}
	}

	return false;
}


TArray<FString> FConfigCacheIni::GetFilenames()
{
	TArray<FString> Result;
	OtherFiles.GetKeys(Result);

	for (const FConfigCacheIni::FKnownConfigFiles::FKnownConfigFile& File : KnownFiles.Files)
	{
		Result.Add(File.IniName.ToString());
	}

	return Result;
}


void FConfigCacheIni::Flush(bool bRemoveFromCache, const FString& Filename )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FConfigCacheIni::Flush);

	// never Flush temporary cache objects
	if (Type != EConfigCacheType::Temporary)
	{
		// write out the files if we can
		if (!bAreFileOperationsDisabled)
		{
			for (TPair<FString, FConfigFile*>& Pair : OtherFiles)
			{
				if (Filename.Len() == 0 || Pair.Key == Filename)
				{
					Pair.Value->Write(*Pair.Key);
				}
			}

			// now flush the known files (all or a single file)
			for (FConfigCacheIni::FKnownConfigFiles::FKnownConfigFile& File : KnownFiles.Files)
			{
				if (Filename.Len() == 0 || Filename == File.IniName.ToString())
				{
					File.IniFile.Write(*File.IniPath);
				}
			}
		}
	}

	if (bRemoveFromCache)
	{
		// we can't read it back in if file operations are disabled
		if (bAreFileOperationsDisabled)
		{
			UE_LOG(LogConfig, Warning, TEXT("Tried to flush the config cache and read it back in, but File Operations are disabled!!"));
			return;
		}

		if (Filename.Len() != 0)
		{
			Remove(Filename);
		}
		else
		{
			for (TPair<FString, FConfigFile*>& It : OtherFiles)
			{
				delete It.Value;
			}
			OtherFiles.Empty();
		}
	}
}

/**
 * Disables any file IO by the config cache system
 */
void FConfigCacheIni::DisableFileOperations()
{
	bAreFileOperationsDisabled = true;
}

/**
 * Re-enables file IO by the config cache system
 */
void FConfigCacheIni::EnableFileOperations()
{
	bAreFileOperationsDisabled = false;
}

/**
 * Returns whether or not file operations are disabled
 */
bool FConfigCacheIni::AreFileOperationsDisabled()
{
	return bAreFileOperationsDisabled;
}

/**
 * Parses apart an ini section that contains a list of 1-to-N mappings of names in the following format
 *	 [PerMapPackages]
 *	 .MapName1=Map1
 *	 .Package1=PackageA
 *	 .Package1=PackageB
 *	 .MapName2=Map2
 *	 .Package2=PackageC
 *	 .Package2=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example - the number suffix gets ignored but helps to keep ordering)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example - the number suffix gets ignored but helps to keep ordering)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const FString& Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	const FConfigSectionMap* ConfigSection = ConfigFile->FindSection(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FName>* WorkingList = nullptr;
	for( FConfigSectionMap::TConstIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key().ToString().StartsWith(KeyOne))
		{
			const FName KeyName(*It.Value().GetValue());

			// look for existing set in the map
			WorkingList = OutMap.Find(KeyName);

			// make a new one if it wasn't there
			if (WorkingList == nullptr)
			{
				WorkingList = &OutMap.Add(KeyName, TArray<FName>());
			}
		}
		// is the current key the N key?
		else if (It.Key().ToString().StartsWith(KeyN) && WorkingList != nullptr)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->Add(FName(*It.Value().GetValue()));
		}
		// if it's neither, then reset
		else
		{
			WorkingList = nullptr;
		}
	}
}

/**
 * Parses apart an ini section that contains a list of 1-to-N mappings of strings in the following format
 *	 [PerMapPackages]
 *	 .MapName1=Map1
 *	 .Package1=PackageA
 *	 .Package1=PackageB
 *	 .MapName2=Map2
 *	 .Package2=PackageC
 *	 .Package2=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example - the number suffix gets ignored but helps to keep ordering)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example - the number suffix gets ignored but helps to keep ordering)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const FString& Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	const FConfigSectionMap* ConfigSection = ConfigFile->FindSection(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FString>* WorkingList = nullptr;
	for( FConfigSectionMap::TConstIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key().ToString().StartsWith(KeyOne))
		{
			// look for existing set in the map
			WorkingList = OutMap.Find(It.Value().GetValue());

			// make a new one if it wasn't there
			if (WorkingList == nullptr)
			{
				WorkingList = &OutMap.Add(It.Value().GetValue(), TArray<FString>());
			}
		}
		// is the current key the N key?
		else if (It.Key().ToString().StartsWith(KeyN) && WorkingList != nullptr)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->Add(It.Value().GetValue());
		}
		// if it's neither, then reset
		else
		{
			WorkingList = nullptr;
		}
	}
}

void FConfigCacheIni::LoadFile( const FString& Filename, const FConfigFile* Fallback, const TCHAR* PlatformString )
{
	// if the file has some data in it, read it in
	if( !IsUsingLocalIniFile(*Filename, nullptr) || DoesConfigFileExistWrapper(*Filename) )
	{
		FConfigFile* Result = &Add(Filename, FConfigFile());
		bool bDoEmptyConfig = false;
		bool bDoCombine = false;
		ProcessIniContents(*Filename, *Filename, Result, bDoEmptyConfig, bDoCombine);
		UE_LOG(LogConfig, Verbose, TEXT( "GConfig::LoadFile has loaded file:  %s" ), *Filename);
	}
	else if( Fallback )
	{
		Add( *Filename, *Fallback );
		UE_LOG(LogConfig, Verbose, TEXT( "GConfig::LoadFile associated file:  %s" ), *Filename);
	}
	else
	{
		UE_LOG(LogConfig, Warning, TEXT( "FConfigCacheIni::LoadFile failed loading file as it was 0 size.  Filename was:  %s" ), *Filename);
	}
}


void FConfigCacheIni::SetFile( const FString& Filename, const FConfigFile* NewConfigFile )
{
	if (FConfigFile* FoundFile = KnownFiles.GetMutableFile(FName(*Filename, FNAME_Find)))
	{
		*FoundFile = *NewConfigFile;
	}
	else
	{
		Add(Filename, *NewConfigFile);
	}
}


void FConfigCacheIni::UnloadFile(const FString& Filename)
{
	FConfigFile* File = Find(Filename);
	if( File )
		Remove( Filename );
}

void FConfigCacheIni::Detach(const FString& Filename)
{
	FConfigFile* File = Find(Filename);
	if( File )
		File->NoSave = 1;
}

bool FConfigCacheIni::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find(Filename);
	if( !File )
	{
		return false;
	}
	const FConfigSection* Sec = File->FindSection( Section );
	if( !Sec )
	{
#if !UE_BUILD_SHIPPING
		CheckLongSectionNames( Section, File );
#endif
		return false;
	}
	const FConfigValue* ConfigValue = Sec->Find( Key );
	if( !ConfigValue )
	{
		return false;
	}
	Value = ConfigValue->GetValue();

	FCoreDelegates::TSOnConfigValueRead().Broadcast(*Filename, Section, Key);

	return true;
}

bool FConfigCacheIni::GetText( const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find(Filename);
	if( !File )
	{
		return false;
	}
	const FConfigSection* Sec = File->FindSection( Section );
	if( !Sec )
	{
#if !UE_BUILD_SHIPPING
		CheckLongSectionNames( Section, File );
#endif
		return false;
	}
	const FConfigValue* ConfigValue = Sec->Find( Key );
	if( !ConfigValue )
	{
		return false;
	}
	if (FTextStringHelper::ReadFromBuffer(*ConfigValue->GetValue(), Value, Section) == nullptr)
	{
		return false;
	}

	FCoreDelegates::TSOnConfigValueRead().Broadcast(*Filename, Section, Key);

	return true;
}

bool FConfigCacheIni::GetSection( const TCHAR* Section, TArray<FString>& Result, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	Result.Reset();
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return false;
	}
	const FConfigSection* Sec = File->FindSection( Section );
	if (!Sec)
	{
		return false;
	}
	Result.Reserve(Sec->Num());
	for (FConfigSection::TConstIterator It(*Sec); It; ++It)
	{
		Result.Add(FString::Printf(TEXT("%s=%s"), *It.Key().ToString(), *It.Value().GetValue()));
	}

	FCoreDelegates::TSOnConfigSectionRead().Broadcast(*Filename, Section);

	return true;
}

FConfigSection* FConfigCacheIni::GetSectionPrivate( const TCHAR* Section, const bool Force, const bool Const, const FString& Filename )
{
	FConfigSection* Sec = const_cast<FConfigSection*>(GetSection(Section, Force, Filename));
	
	// handle the non-const case
	if ((!Const || Force) && Sec != nullptr)
	{
		FConfigFile* File = Find(Filename);
		File->Dirty = true;
	}

	return Sec;
}

const FConfigSection* FConfigCacheIni::GetSection( const TCHAR* Section, const bool Force, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return nullptr;
	}
	const FConfigSection* Sec = File->FindSection( Section );
	if (!Sec && Force)
	{
		Sec = &File->Add(Section, FConfigSection());
		File->Dirty = true;
	}

	if (Sec)
	{
		FCoreDelegates::TSOnConfigSectionRead().Broadcast(*Filename, Section);
	}

	return Sec;
}

bool FConfigCacheIni::DoesSectionExist(const TCHAR* Section, const FString& Filename)
{
	bool bReturnVal = false;

	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find(Filename);

	bReturnVal = (File != nullptr && File->FindSection(Section) != nullptr);

	if (bReturnVal)
	{
		FCoreDelegates::TSOnConfigSectionNameRead().Broadcast(*Filename, Section);
	}

	return bReturnVal;
}

void FConfigCacheIni::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename )
{
	FConfigFile* File = Find(Filename);

	if (!File)
	{
		return;
	}

	File->SetString(Section, Key, Value);
}

void FConfigCacheIni::SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value, const FString& Filename )
{
	FConfigFile* File = Find(Filename);

	if ( !File )
	{
		return;
	}

	FConfigSection* Sec = File->FindOrAddSectionInternal( Section );

	FString StrValue;
	FTextStringHelper::WriteToBuffer(StrValue, Value);

	FConfigValue* ConfigValue = Sec->Find( Key );
	if( !ConfigValue )
	{
		Sec->Add(Key, FConfigValue(MoveTemp(StrValue)));
		File->Dirty = true;
	}
	// Use GetSavedValueForWriting rather than GetSavedValue to avoid reporting the value as having been accessed for dependency tracking
	else if( FCString::Strcmp(*UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(*ConfigValue), *StrValue)!=0 )
	{
		File->Dirty = true;
		*ConfigValue = MoveTemp(StrValue);
	}
}

bool FConfigCacheIni::RemoveKey( const TCHAR* Section, const TCHAR* Key, const FString& Filename )
{
	FConfigFile* File = Find(Filename);
	if( File )
	{
		if (File->RemoveKeyFromSection(Section, Key))
		{
			File->Dirty = 1;
			return true;
		}
	}
	return false;
}

bool FConfigCacheIni::EmptySection( const TCHAR* Section, const FString& Filename )
{
	FConfigFile* File = Find(Filename);
	if( File )
	{
		// remove the section name if there are no more properties for this section
		if(File->FindSection(Section) != nullptr)
		{
			File->Remove(Section);
			if (bAreFileOperationsDisabled == false)
			{
				if (File->Num())
				{
					File->Dirty = 1;
					Flush(0, Filename);
				}
				else
				{
					IFileManager::Get().Delete(*Filename);	
				}
			}
			return true;
		}
	}
	return false;
}

bool FConfigCacheIni::EmptySectionsMatchingString( const TCHAR* SectionString, const FString& Filename )
{
	bool bEmptied = false;
	FConfigFile* File = Find(Filename);
	if (File)
	{
		bool bSaveOpsDisabled = bAreFileOperationsDisabled;
		bAreFileOperationsDisabled = true;
		for (FConfigFile::TIterator It(*File); It; ++It)
		{
			if (It.Key().Contains(SectionString) )
			{
				bEmptied |= EmptySection(*(It.Key()), Filename);
			}
		}
		bAreFileOperationsDisabled = bSaveOpsDisabled;
	}
	return bEmptied;
}

FString FConfigCacheIni::GetConfigFilename(const TCHAR* BaseIniName)
{
	// Known ini files such as Engine, Game, etc.. are referred to as just the name with no extension within the config system.
	if (IsKnownConfigName(FName(BaseIniName, FNAME_Find)))
	{
		return FString(BaseIniName);
	}
	else
	{
		// Non-known ini files are looked up using their full path
		// This always uses the default platform as non-known files are not valid for other platforms
		return GetDestIniFilename(BaseIniName, nullptr, *FPaths::GeneratedConfigDir());
	}
}

/**
 * Retrieve a list of all of the config files stored in the cache
 *
 * @param ConfigFilenames Out array to receive the list of filenames
 */
void FConfigCacheIni::GetConfigFilenames(TArray<FString>& ConfigFilenames)
{
	ConfigFilenames = GetFilenames();
}

/**
 * Retrieve the names for all sections contained in the file specified by Filename
 *
 * @param	Filename			the file to retrieve section names from
 * @param	out_SectionNames	will receive the list of section names
 *
 * @return	true if the file specified was successfully found;
 */
bool FConfigCacheIni::GetSectionNames( const FString& Filename, TArray<FString>& out_SectionNames )
{
	bool bResult = false;

	FConfigFile* File = Find(Filename);
	if ( File != nullptr )
	{
		out_SectionNames.Empty(File->Num());
		for ( FConfigFile::TIterator It(*File); It; ++It )
		{
			out_SectionNames.Add(It.Key());

			FCoreDelegates::TSOnConfigSectionNameRead().Broadcast(*Filename, *It.Key());
		}
		bResult = true;
	}

	return bResult;
}

/**
 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
 *
 * @param	Filename			the file to retrieve section names from
 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
 * @param	MaxResults			the maximum number of section names to retrieve
 *
 * @return	true if the file specified was found and it contained at least 1 section for the specified class
 */
bool FConfigCacheIni::GetPerObjectConfigSections( const FString& Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, int32 MaxResults )
{
	bool bResult = false;

	MaxResults = FMath::Max(0, MaxResults);
	FConfigFile* File = Find(Filename);
	if ( File != nullptr )
	{
		out_SectionNames.Empty();
		for ( FConfigFile::TIterator It(*File); It && out_SectionNames.Num() < MaxResults; ++It )
		{
			const FString& SectionName = It.Key();
			
			// determine whether this section corresponds to a PerObjectConfig section
			int32 POCClassDelimiter = SectionName.Find(TEXT(" "));
			if ( POCClassDelimiter != INDEX_NONE )
			{
				// the section name contained a space, which for now we'll assume means that we've found a PerObjectConfig section
				// see if the remainder of the section name matches the class name we're searching for
				if ( SectionName.Mid(POCClassDelimiter + 1) == SearchClass )
				{
					// found a PerObjectConfig section for the class specified - add it to the list
					out_SectionNames.Insert(SectionName,0);
					bResult = true;

					FCoreDelegates::TSOnConfigSectionNameRead().Broadcast(*Filename, *SectionName);
				}
			}
		}
	}

	return bResult;
}

void FConfigCacheIni::Exit()
{
	Flush( 1 );

#if WITH_EDITOR
	for (auto& PlatformConfigFuture : GetPlatformConfigFutures())
	{
		PlatformConfigFuture.Value.Get();
	}
	GetPlatformConfigFutures().Empty();
#endif
}

void FConfigCacheIni::DumpFile(FOutputDevice& Ar, const FString& Filename, const FConfigFile& File)
{
	Ar.Logf(TEXT("FileName: %s"), *Filename);

	// sort the sections (and keys below) for easier diffing
	TArray<FString> SectionKeys;
	File.GetKeys(SectionKeys);
	SectionKeys.Sort();
	for (const FString& SectionKey : SectionKeys)
	{
		const FConfigSection& Sec = File[SectionKey];
		Ar.Logf(TEXT("   [%s]"), *SectionKey);

		TArray<FName> Keys;
		Sec.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (FName Key : Keys)
		{
			TArray<FConfigValue> Values;
			Sec.MultiFind(Key, Values, true);
			for (const FConfigValue& Value : Values)
			{
				Ar.Logf(TEXT("   %s=%s"), *Key.ToString(), *Value.GetValue());
			}
		}

		Ar.Log(LINE_TERMINATOR);
	}
}

void FConfigCacheIni::Dump(FOutputDevice& Ar, const TCHAR* BaseIniName)
{
	for (const FConfigCacheIni::FKnownConfigFiles::FKnownConfigFile& File : KnownFiles.Files)
	{
		if (BaseIniName == nullptr || File.IniName == BaseIniName)
		{
			DumpFile(Ar, File.IniName.ToString(), File.IniFile);
		}
	}

	// sort the non-known files for easier diffing
	TArray<FString> Keys;
	OtherFiles.GetKeys(Keys);
	Algo::Sort(Keys);
	for (const FString& Key : Keys)
	{
		if (BaseIniName == nullptr || FPaths::GetBaseFilename(Key) == BaseIniName)
		{
			DumpFile(Ar, Key, *OtherFiles[Key]);
		}
	}
}

// Derived functions.
FString FConfigCacheIni::GetStr( const TCHAR* Section, const TCHAR* Key, const FString& Filename )
{
	FString Result;
	GetString( Section, Key, Result, Filename );
	return Result;
}
bool FConfigCacheIni::GetInt
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	int32&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atoi(*Text);
		return true;
	}
	return false;
}
bool FConfigCacheIni::GetInt64
(
	const TCHAR* Section,
	const TCHAR* Key,
	int64& Value,
	const FString& Filename
)
{
	FString Text;
	if (GetString(Section, Key, Text, Filename))
	{
		Value = FCString::Atoi64(*Text);
		return true;
	}
	return false;
}
bool FConfigCacheIni::GetFloat
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	float&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atof(*Text);
		return true;
	}
	return false;
}
bool FConfigCacheIni::GetDouble
	(
	const TCHAR*		Section,
	const TCHAR*		Key,
	double&				Value,
	const FString&	Filename
	)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atod(*Text);
		return true;
	}
	return false;
}
bool FConfigCacheIni::GetBool
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	bool&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::ToBool(*Text);
		return true;
	}
	return false;
}
int32 FConfigCacheIni::GetArray
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	TArray<FString>&	out_Arr,
	const FString&	Filename
)
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	out_Arr.Empty();
	FConfigFile* File = Find(Filename);
	if ( File != nullptr )
	{
		File->GetArray(Section, Key, out_Arr);
	}

	if (out_Arr.Num())
	{
		FCoreDelegates::TSOnConfigValueRead().Broadcast(*Filename, Section, Key);
	}

	return out_Arr.Num();
}
/** Loads a "delimited" list of string
 * @param Section - Section of the ini file to load from
 * @param Key - The key in the section of the ini file to load
 * @param out_Arr - Array to load into
 * @param Delimiter - Break in the strings
 * @param Filename - Ini file to load from
 */
int32 FConfigCacheIni::GetSingleLineArray
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	TArray<FString>&	out_Arr,
	const FString&	Filename
)
{
	FString FullString;
	bool bValueExisted = GetString(Section, Key, FullString, Filename);
	const TCHAR* RawString = *FullString;

	//tokenize the string into out_Arr
	FString NextToken;
	while ( FParse::Token(RawString, NextToken, false) )
	{
		out_Arr.Add(MoveTemp(NextToken));
	}
	return bValueExisted;
}

bool FConfigCacheIni::GetColor
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FColor&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetVector2D(
	const TCHAR*   Section,
	const TCHAR*   Key,
	FVector2D&     Value,
	const FString& Filename)
{
	FString Text;
	if (GetString(Section, Key, Text, Filename))
	{
		return Value.InitFromString(Text);
	}
	return false;
}


bool FConfigCacheIni::GetVector
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FVector&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetVector4
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FVector4&			Value,
 const FString&	Filename
)
{
	FString Text;
	if(GetString(Section, Key, Text, Filename))
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetRotator
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FRotator&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

void FConfigCacheIni::SetInt
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	int32				Value,
	const FString&	Filename
)
{
	TCHAR Text[MAX_SPRINTF];
	FCString::Sprintf( Text, TEXT("%i"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetFloat
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	float				Value,
	const FString&	Filename
)
{
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return;
	}

	File->SetFloat(Section, Key, Value);
}
void FConfigCacheIni::SetDouble
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	double				Value,
	const FString&	Filename
)
{
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return;
	}

	File->SetDouble(Section, Key, Value);
}
void FConfigCacheIni::SetBool
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	bool				Value,
	const FString&	Filename
)
{
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return;
	}

	File->SetBool(Section, Key, Value);
}

void FConfigCacheIni::SetArray
(
	const TCHAR*			Section,
	const TCHAR*			Key,
	const TArray<FString>&	Value,
	const FString&		Filename
)
{
	FConfigFile* File = Find(Filename);
	if (!File)
	{
		return;
	}

	File->SetArray(Section, Key, Value);
}
/** Saves a "delimited" list of strings
 * @param Section - Section of the ini file to save to
 * @param Key - The key in the section of the ini file to save
 * @param In_Arr - Array to save from
 * @param Filename - Ini file to save to
 */
void FConfigCacheIni::SetSingleLineArray
(
	const TCHAR*			Section,
	const TCHAR*			Key,
	const TArray<FString>&	In_Arr,
	const FString&		Filename
)
{
	FString FullString;

	//append all strings to single string
	for (int32 i = 0; i < In_Arr.Num(); ++i)
	{
		FullString += In_Arr[i];
		FullString += TEXT(" ");
	}

	//save to ini file
	SetString(Section, Key, *FullString, Filename);
}

void FConfigCacheIni::SetColor
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FColor		Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetVector2D(
	const TCHAR*   Section,
	const TCHAR*   Key,
	FVector2D      Value,
	const FString& Filename)
{
	SetString(Section, Key, *Value.ToString(), Filename);
}

void FConfigCacheIni::SetVector
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FVector		 Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetVector4
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FVector4&	 Value,
 const FString&	Filename
)
{
	SetString(Section, Key, *Value.ToString(), Filename);
}

void FConfigCacheIni::SetRotator
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FRotator		Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}


bool FConfigCacheIni::AddToSection(const TCHAR* Section, FName Key, const FString& Value, const FString& Filename)
{
	if (FConfigFile* File = Find(*Filename))
	{
		return File->AddToSection(Section, Key, Value);
	}
	return false;
}

bool FConfigCacheIni::AddUniqueToSection(const TCHAR* Section, FName Key, const FString& Value, const FString& Filename)
{
	if (FConfigFile* File = Find(*Filename))
	{
		return File->AddUniqueToSection(Section, Key, Value);
	}
	return false;
}

bool FConfigCacheIni::RemoveKeyFromSection(const TCHAR* Section, FName Key, const FString& Filename)
{
	if (FConfigFile* File = Find(*Filename))
	{
		return File->RemoveKeyFromSection(Section, Key);
	}
	return false;
}

bool FConfigCacheIni::RemoveFromSection(const TCHAR* Section, FName Key, const FString& Value, const FString& Filename)
{
	if (FConfigFile* File = Find(*Filename))
	{
		return File->RemoveFromSection(Section, Key, Value);
	}
	return false;
}


/**
 * Archive for counting config file memory usage.
 */
class FArchiveCountConfigMem : public FArchive
{
public:
	FArchiveCountConfigMem()
	:	Num(0)
	,	Max(0)
	{
		ArIsCountingMemory = true;
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
protected:
	SIZE_T Num, Max;
};


/**
 * Tracks the amount of memory used by a single config or loc file
 */
struct FConfigFileMemoryData
{
	FString	ConfigFilename;
	SIZE_T		CurrentSize;
	SIZE_T		MaxSize;

	FConfigFileMemoryData( const FString& InFilename, SIZE_T InSize, SIZE_T InMax )
	: ConfigFilename(InFilename), CurrentSize(InSize), MaxSize(InMax)
	{}
};

/**
 * Tracks the memory data recorded for all loaded config files.
 */
struct FConfigMemoryData
{
	int32 NameIndent;
	int32 SizeIndent;
	int32 MaxSizeIndent;

	TArray<FConfigFileMemoryData> MemoryData;

	FConfigMemoryData()
	: NameIndent(0), SizeIndent(0), MaxSizeIndent(0)
	{}

	void AddConfigFile( const FString& ConfigFilename, FArchiveCountConfigMem& MemAr )
	{
		SIZE_T TotalMem = MemAr.GetNum();
		SIZE_T MaxMem = MemAr.GetMax();

		NameIndent = FMath::Max(NameIndent, ConfigFilename.Len());
		SizeIndent = FMath::Max(SizeIndent, FString::FromInt((int32)TotalMem).Len());
		MaxSizeIndent = FMath::Max(MaxSizeIndent, FString::FromInt((int32)MaxMem).Len());
		
		MemoryData.Emplace( ConfigFilename, TotalMem, MaxMem );
	}

	void SortBySize()
	{
		struct FCompareFConfigFileMemoryData
		{
			FORCEINLINE bool operator()( const FConfigFileMemoryData& A, const FConfigFileMemoryData& B ) const
			{
				return ( B.CurrentSize == A.CurrentSize ) ? ( B.MaxSize < A.MaxSize ) : ( B.CurrentSize < A.CurrentSize );
			}
		};
		MemoryData.Sort( FCompareFConfigFileMemoryData() );
	}
};

/**
 * Dumps memory stats for each file in the config cache to the specified archive.
 *
 * @param	Ar	the output device to dump the results to
 */
void FConfigCacheIni::ShowMemoryUsage( FOutputDevice& Ar )
{
	FConfigMemoryData ConfigCacheMemoryData;

	for (TPair<FString, FConfigFile*>& Pair : OtherFiles)
	{
		FString Filename = Pair.Key;
		FConfigFile* ConfigFile = Pair.Value;

		FArchiveCountConfigMem MemAr;

		// count the bytes used for storing the filename
		MemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		MemAr << *ConfigFile;
		
		ConfigCacheMemoryData.AddConfigFile(Filename, MemAr);
	}
	{
		FArchiveCountConfigMem MemAr;
		MemAr << KnownFiles;
		ConfigCacheMemoryData.AddConfigFile(TEXT("KnownFiles"), MemAr);
	}

	// add a little extra spacing between the columns
	ConfigCacheMemoryData.SizeIndent += 10;
	ConfigCacheMemoryData.MaxSizeIndent += 10;

	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	OtherFiles.CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();

	Ar.Log(TEXT("Config cache memory usage:"));
	// print out the header
	Ar.Logf(TEXT("%*s %*s %*s"), ConfigCacheMemoryData.NameIndent, TEXT("FileName"), ConfigCacheMemoryData.SizeIndent, TEXT("NumBytes"), ConfigCacheMemoryData.MaxSizeIndent, TEXT("MaxBytes"));

	ConfigCacheMemoryData.SortBySize();
	for ( int32 Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData[Index];
			Ar.Logf(TEXT("%*s %*u %*u"), 
			ConfigCacheMemoryData.NameIndent, *ConfigFileMemoryData.ConfigFilename,
			ConfigCacheMemoryData.SizeIndent, (uint32)ConfigFileMemoryData.CurrentSize,
			ConfigCacheMemoryData.MaxSizeIndent, (uint32)ConfigFileMemoryData.MaxSize);

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

	Ar.Logf(TEXT("%*s %*u %*u"), 
		ConfigCacheMemoryData.NameIndent, TEXT("Total"),
		ConfigCacheMemoryData.SizeIndent, (uint32)TotalMemoryUsage,
		ConfigCacheMemoryData.MaxSizeIndent, (uint32)MaxMemoryUsage);
}



SIZE_T FConfigCacheIni::GetMaxMemoryUsage()
{
	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	OtherFiles.CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();


	FConfigMemoryData ConfigCacheMemoryData;

	for (TPair<FString, FConfigFile*>& Pair : OtherFiles)
	{
		FString Filename = Pair.Key;
		FConfigFile* ConfigFile = Pair.Value;

		FArchiveCountConfigMem FileMemAr;

		// count the bytes used for storing the filename
		FileMemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		FileMemAr << *ConfigFile;

		ConfigCacheMemoryData.AddConfigFile(Filename, FileMemAr);
	}
	{
		FArchiveCountConfigMem FileMemAr;
		FileMemAr << KnownFiles;
		ConfigCacheMemoryData.AddConfigFile(TEXT("KnownFiles"), FileMemAr);
	}

	for ( int32 Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData[Index];

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

	return MaxMemoryUsage;
}

bool FConfigCacheIni::ForEachEntry(const FKeyValueSink& Visitor, const TCHAR* Section, const FString& Filename)
{
	FConfigFile* File = Find(Filename);
	if(!File)
	{
		return false;
	}

	const FConfigSection* Sec = File->FindSection(Section);
	if(!Sec)
	{
		return false;
	}

	for(FConfigSectionMap::TConstIterator It(*Sec); It; ++It)
	{
		Visitor.Execute(*It.Key().GetPlainNameString(), *It.Value().GetValue());
	}

	return true;
}

FString FConfigCacheIni::GetDestIniFilename(const TCHAR* BaseIniName, const TCHAR* PlatformName, const TCHAR* GeneratedConfigDir)
{
	// figure out what to look for on the commandline for an override
	FString CommandLineSwitch = FString::Printf(TEXT("%sINI="), BaseIniName);
	
	// if it's not found on the commandline, then generate it
	FString IniFilename;
	if (FParse::Value(FCommandLine::Get(), *CommandLineSwitch, IniFilename) == false)
	{
		FString Name(PlatformName ? PlatformName : ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));

		// if the BaseIniName doesn't contain the config dir, put it all together
		if (FCString::Stristr(BaseIniName, GeneratedConfigDir) != nullptr)
		{
			IniFilename = BaseIniName;
		}
		else
		{
			IniFilename = FString::Printf(TEXT("%s%s/%s.ini"), GeneratedConfigDir, *Name, BaseIniName);
		}
	}

	// standardize it!
	FPaths::MakeStandardFilename(IniFilename);
	return IniFilename;
}

void FConfigCacheIni::SaveCurrentStateForBootstrap(const TCHAR* Filename)
{
	TArray<uint8> FileContent;
	{
		// Use FMemoryWriter because FileManager::CreateFileWriter doesn't serialize FName as string and is not overridable
		FMemoryWriter MemoryWriter(FileContent, true);
		SerializeStateForBootstrap_Impl(MemoryWriter);
	}

	FFileHelper::SaveArrayToFile(FileContent, Filename);
}

void FConfigCacheIni::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		int Num;
		Ar << Num;
		for (int Index = 0; Index < Num; Index++)
		{
			FString Filename;
			FConfigFile* File = new FConfigFile;
			Ar << Filename;
			Ar << *File;
			OtherFiles.Add(Filename, File);
		}
	}
	else
	{
		int Num = OtherFiles.Num();
		Ar << Num;
		for (TPair<FString, FConfigFile*>& It : OtherFiles)
		{
			Ar << It.Key;
			Ar << *It.Value;
		}
	}
	Ar << KnownFiles;
	Ar << bAreFileOperationsDisabled;
	Ar << bIsReadyForUse;
	Ar << Type;
}

void FConfigCacheIni::SerializeStateForBootstrap_Impl(FArchive& Ar)
{
	// This implementation is meant to stay private and be used for 
	// bootstrapping another processes' config cache with a serialized state.
	// It doesn't include any versioning as it is used with the
	// the same binary executable for both the parent and 
	// children processes. It also takes care of saving/restoring
	// global ini variables.
	Serialize(Ar);
	Ar << GEditorIni;
	Ar << GEditorKeyBindingsIni;
	Ar << GEditorLayoutIni;
	Ar << GEditorSettingsIni;
	Ar << GEditorPerProjectIni;
	Ar << GCompatIni;
	Ar << GLightmassIni;
	Ar << GScalabilityIni;
	Ar << GHardwareIni;
	Ar << GInputIni;
	Ar << GGameIni;
	Ar << GGameUserSettingsIni;
	Ar << GRuntimeOptionsIni;
	Ar << GEngineIni;
}


bool FConfigCacheIni::InitializeKnownConfigFiles(FConfigContext& Context)
{
	// check for scalability platform override.
	FConfigContext* ScalabilityPlatformOverrideContext = nullptr;
#if !UE_BUILD_SHIPPING && WITH_EDITOR
	if (Context.ConfigSystem == GConfig)
	{
		FString ScalabilityPlatformOverrideCommandLine;
		FParse::Value(FCommandLine::Get(), TEXT("ScalabilityIniPlatformOverride="), ScalabilityPlatformOverrideCommandLine);
		if (!ScalabilityPlatformOverrideCommandLine.IsEmpty())
		{
			ScalabilityPlatformOverrideContext = new FConfigContext(FConfigContext::ReadIntoConfigSystem(Context.ConfigSystem, ScalabilityPlatformOverrideCommandLine));
		}
	}
#endif

	bool bEngineConfigCreated = false;
	for (uint8 KnownIndex = 0; KnownIndex < (uint8)EKnownIniFile::NumKnownFiles; KnownIndex++)
	{
		FConfigCacheIni::FKnownConfigFiles::FKnownConfigFile& KnownFile = Context.ConfigSystem->KnownFiles.Files[KnownIndex];

		// allow for scalability to come from another platform (made above)
		FConfigContext& ContextToUse = (KnownIndex == (uint8)EKnownIniFile::Scalability && ScalabilityPlatformOverrideContext) ? *ScalabilityPlatformOverrideContext : Context;

		// and load it, saving the dest path to IniPath
		bool bConfigCreated = ContextToUse.Load(*KnownFile.IniName.ToString(), KnownFile.IniPath);
		
		// we want to return if the Engine config was successfully created (to not remove any functionality from old code)
		if (KnownIndex == (uint8)EKnownIniFile::Engine)
		{
			bEngineConfigCreated = bConfigCreated;
		}
	}

	// Gconfig set itself ready for use later on
	if (Context.ConfigSystem != GConfig)
	{
		Context.ConfigSystem->bIsReadyForUse = true;
	}

	return bEngineConfigCreated;
}

bool FConfigCacheIni::IsKnownConfigName(FName ConfigName)
{
	return KnownFiles.GetFile(ConfigName) != nullptr;
}

const FConfigFile* FConfigCacheIni::FKnownConfigFiles::GetFile(FName Name)
{
	// sharing logic
	return GetMutableFile(Name);
}

FConfigFile* FConfigCacheIni::FKnownConfigFiles::GetMutableFile(FName Name)
{
	// walk the list of files looking for matching FName (a TMap was a bit slower)
	for (FKnownConfigFile& File : Files)
	{
		if (File.IniName == Name)
		{
			return &File.IniFile;
		}
	}

	return nullptr;
}
const FString& FConfigCacheIni::FKnownConfigFiles::GetFilename(FName Name)
{
	for (FKnownConfigFile& File : Files)
	{
		if (File.IniName == Name)
		{
			return File.IniPath;
		}
	}

	static FString Empty;
	return Empty;
}

FConfigCacheIni::FKnownConfigFiles::FKnownConfigFiles()
{
	// set the FNames associated with each file

	// 	Files[(uint8)EKnownIniFile::Engine].IniName = FName("Engine");
	#define SET_KNOWN_NAME(Ini) Files[(uint8)EKnownIniFile::Ini].IniName = FName(#Ini);
		ENUMERATE_KNOWN_INI_FILES(SET_KNOWN_NAME);
	#undef SET_KNOWN_NAME
}

FArchive& operator<<(FArchive& Ar, FConfigCacheIni::FKnownConfigFiles& Names)
{
	for (FConfigCacheIni::FKnownConfigFiles::FKnownConfigFile& File : Names.Files)
	{
		Ar << File.IniPath << File.IniFile;
	}

	return Ar;
}

#if PRELOAD_BINARY_CONFIG

#include "Misc/PreLoadFile.h"
static FPreLoadFile GPreLoadConfigBin(TEXT("{PROJECT}Config/BinaryConfig.ini"));

bool FConfigCacheIni::CreateGConfigFromSaved(const TCHAR* Filename)
{
	SCOPED_BOOT_TIMING("FConfigCacheIni::CreateGConfigFromSaved");
	// get the already loaded file
	int64 Size;
	void* PreloadedData = GPreLoadConfigBin.TakeOwnershipOfLoadedData(&Size);
	if (PreloadedData == nullptr)
	{
		return false;
	}

	UE_LOG(LogInit, Display, TEXT("Loading binary GConfig..."));

	// serialize right out of the preloaded data
	FLargeMemoryReader MemoryReader((uint8*)PreloadedData, Size);
	FKnownConfigFiles Names;
	GConfig = new FConfigCacheIni(EConfigCacheType::Temporary);

	// make an object that we can use to pass to delegates for any extra binary data they want to write
//	FCoreDelegates::FExtraBinaryConfigData ExtraData(*GConfig, false);

	GConfig->Serialize(MemoryReader);

	// Forced to be disk backed so that GameUserSettings does get written out.
	GConfig->Type = EConfigCacheType::DiskBacked;
	MemoryReader << Names;// << ExtraData.Data;

	// now let the delegates pull their data out, after GConfig is set up
//	FCoreDelegates::TSAccessExtraBinaryConfigData().Broadcast(ExtraData);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ConfigReadyForUseBroadcast);
		FCoreDelegates::TSConfigReadyForUse().Broadcast();
	}

	FMemory::Free(PreloadedData);
	return true;
}

#endif

static void LoadRemainingConfigFiles(FConfigContext& Context)
{
	SCOPED_BOOT_TIMING("LoadRemainingConfigFiles");

#if PLATFORM_DESKTOP
	// load some desktop only .ini files
	Context.Load(TEXT("Compat"), GCompatIni);
	Context.Load(TEXT("Lightmass"), GLightmassIni);
#endif

#if WITH_EDITOR
	// load some editor specific .ini files

	Context.Load(TEXT("Editor"), GEditorIni);

	// Upgrade editor user settings before loading the editor per project user settings
	FConfigManifest::MigrateEditorUserSettings();
	Context.Load(TEXT("EditorPerProjectUserSettings"), GEditorPerProjectIni);

	// Project agnostic editor ini files, so save them to a shared location (Engine, not Project)
	Context.GeneratedConfigDir = FPaths::EngineEditorSettingsDir();
	Context.Load(TEXT("EditorSettings"), GEditorSettingsIni);
	Context.Load(TEXT("EditorKeyBindings"), GEditorKeyBindingsIni);
	Context.Load(TEXT("EditorLayout"), GEditorLayoutIni);

#endif

	if (FParse::Param(FCommandLine::Get(), TEXT("dumpconfig")))
	{
		GConfig->Dump(*GLog);
	}
}

static void InitializeConfigRemap()
{
	// read in the single remap file
	FConfigFile RemapFile;
	FConfigContext Context = FConfigContext::ReadSingleIntoLocalFile(RemapFile);
	
	// read in engine and project ini files (these are not hierarchical, so it has to be done in two passes)
	for (int Pass = 0; Pass < 2; Pass++)
	{
		// if there isn't an active project, then skip the project pass
		if (Pass == 1 && FPaths::ProjectDir() == FPaths::EngineDir())
		{
			continue;
		}
		
		Context.Load(*FPaths::Combine(Pass == 0 ? FPaths::EngineDir() : FPaths::ProjectDir(), TEXT("Config/ConfigRedirects.ini")));

		for (const TPair<FString, FConfigSection>& Section : AsConst(RemapFile))
		{
			if (Section.Key == TEXT("SectionNameRemap"))
			{
				for (const TPair<FName, FConfigValue>& Line : Section.Value)
				{
					SectionRemap.Add(Line.Key.ToString(), Line.Value.GetSavedValue());
				}
			}
			else
			{
				TMap<FString, FString>& KeyRemaps = KeyRemap.FindOrAdd(Section.Key);
				for (const TPair<FName, FConfigValue>& Line : Section.Value)
				{
					KeyRemaps.Add(Line.Key.ToString(), Line.Value.GetSavedValue());
				}
			}
		}
	}
	
	GAllowConfigRemapWarning = true;
}

void FConfigCacheIni::InitializeConfigSystem()
{
	// assign the G***Ini strings for the known ini's
	#define ASSIGN_GLOBAL_INI_STRING(IniName) G##IniName##Ini = FString(#IniName);
		// GEngineIni = FString("Engine")
		ENUMERATE_KNOWN_INI_FILES(ASSIGN_GLOBAL_INI_STRING);
	#undef ASSIGN_GLOBAL_INI_STRING

	
	InitializeConfigRemap();
	
#if PLATFORM_SUPPORTS_BINARYCONFIG && PRELOAD_BINARY_CONFIG
	// attempt to load from staged binary config data
	if (!FParse::Param(FCommandLine::Get(), TEXT("textconfig")) &&
		FConfigCacheIni::CreateGConfigFromSaved(nullptr))
	{
		FConfigContext Context = FConfigContext::ForceReloadIntoGConfig();
		// Force reload GameUserSettings because they may be saved to disk on consoles/similar platforms
		// So the safest thing to do is to re-read the file after binary configs load.
		Context.Load(TEXT("GameUserSettings"), GGameUserSettingsIni);

#if WITH_EDITOR
		// a cooked editor (cooked cooker more likely) can be initialized from binary config for speed, 
		// but we still need the other files as well
		LoadRemainingConfigFiles(Context);
#endif
		return;
	}
#endif

	// Bootstrap the Ini config cache
	FString IniBootstrapFilename;
	if (FParse::Value( FCommandLine::Get(), TEXT("IniBootstrap="), IniBootstrapFilename))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IniBootstrap);
		TArray<uint8> FileContent;
		if (FFileHelper::LoadFileToArray(FileContent, *IniBootstrapFilename, FILEREAD_Silent))
		{
			FMemoryReader MemoryReader(FileContent, true);
			GConfig = new FConfigCacheIni(EConfigCacheType::Temporary);
			GConfig->SerializeStateForBootstrap_Impl(MemoryReader);
			GConfig->bIsReadyForUse = true;
			TRACE_CPUPROFILER_EVENT_SCOPE(ConfigReadyForUseBroadcast);
			FCoreDelegates::TSConfigReadyForUse().Broadcast();
			return;
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Unable to bootstrap from archive %s, will fallback on normal initialization\n"), *IniBootstrapFilename);
		}
	}

	// Perform any upgrade we need before we load any configuration files
	FConfigManifest::UpgradeFromPreviousVersions();

	// create GConfig
	GConfig = new FConfigCacheIni(EConfigCacheType::DiskBacked);

	// create a context object that we will use for all of the main ini files
	FConfigContext Context = FConfigContext::ReadIntoGConfig();

	// load in the default ini files
	bool bEngineConfigCreated = InitializeKnownConfigFiles(Context);

	// verify if needed
	const bool bIsGamelessExe = !FApp::HasProjectName();
	if ( !bIsGamelessExe )
	{
		// Now check and see if our game is correct if this is a game agnostic binary
		if (GIsGameAgnosticExe && !bEngineConfigCreated)
		{
			const FText AbsolutePath = FText::FromString( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::GetPath(GEngineIni)) );
			//@todo this is too early to localize
			const FText Message = FText::Format( NSLOCTEXT("Core", "FirstCmdArgMustBeGameName", "'{0}' must exist and contain a DefaultEngine.ini."), AbsolutePath );
			if (!GIsBuildMachine)
			{
				FMessageDialog::Open(EAppMsgType::Ok, Message);
			}
			FApp::SetProjectName(TEXT("")); // this disables part of the crash reporter to avoid writing log files to a bogus directory
			if (!GIsBuildMachine)
			{
				exit(1);
			}
			UE_LOG(LogInit, Fatal,TEXT("%s"), *Message.ToString());
		}
	}

	// load editor, etc config files
	LoadRemainingConfigFiles(Context);

	FCoreDelegates::TSOnConfigSectionsChanged().AddStatic(OnConfigSectionsChanged);

	// now we can make use of GConfig
	GConfig->bIsReadyForUse = true;

#if WITH_EDITOR
	// this needs to be called after setting bIsReadyForUse, because it uses ProjectDir, and bIsReadyForUse can reset the 
	// ProjectDir array while the async threads are using it and crash
	AsyncInitializeConfigForPlatforms();
#endif

	TRACE_CPUPROFILER_EVENT_SCOPE(ConfigReadyForUseBroadcast);
	FCoreDelegates::TSConfigReadyForUse().Broadcast();
}

const FString& FConfigCacheIni::GetCustomConfigString()
{
	static FString CustomConfigString;
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;

		// Set to compiled in value, then possibly override
		bool bCustomConfigOverrideApplied = false;
		CustomConfigString = TEXT(CUSTOM_CONFIG);

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
		if (FParse::Value(FCommandLine::Get(), CommandlineOverrideSpecifiers::CustomConfigIdentifier, CustomConfigString))
		{
			bCustomConfigOverrideApplied = true;
			UE_LOG(LogConfig, Log, TEXT("Overriding CustomConfig from %s to %s using -customconfig cmd line param"), TEXT(CUSTOM_CONFIG), *CustomConfigString);
		}
#endif

#ifdef UE_USE_COMMAND_LINE_PARAM_FOR_CUSTOM_CONFIG
		FString CustomName = PREPROCESSOR_TO_STRING(UE_USE_COMMAND_LINE_PARAM_FOR_CUSTOM_CONFIG);
		if (!bCustomConfigOverrideApplied && FParse::Param(FCommandLine::Get(), *CustomName))
		{
			bCustomConfigOverrideApplied = true;
			CustomConfigString = CustomName;
			UE_LOG(LogConfig, Log, TEXT("Overriding CustomConfig from %s to %s using a custom cmd line param"), TEXT(CUSTOM_CONFIG), *CustomConfigString);
		}
#endif

		if (!bCustomConfigOverrideApplied && !CustomConfigString.IsEmpty())
		{
			UE_LOG(LogConfig, Log, TEXT("Using compiled CustomConfig %s"), *CustomConfigString);
		}
	}
	return CustomConfigString;
}

bool FConfigCacheIni::LoadGlobalIniFile(FString& OutFinalIniFilename, const TCHAR* BaseIniName, const TCHAR* Platform, bool bForceReload, bool bRequireDefaultIni, bool bAllowGeneratedIniWhenCooked, bool bAllowRemoteConfig, const TCHAR* GeneratedConfigDir, FConfigCacheIni* ConfigSystem)
{
	FConfigContext Context = FConfigContext::ReadIntoConfigSystem(ConfigSystem, Platform);
	if (GeneratedConfigDir != nullptr)
	{
		Context.GeneratedConfigDir = GeneratedConfigDir;
	}
	Context.bForceReload = bForceReload;
	Context.bAllowGeneratedIniWhenCooked = bAllowGeneratedIniWhenCooked;
	Context.bAllowRemoteConfig = bAllowRemoteConfig;
	return Context.Load(BaseIniName, OutFinalIniFilename);
}

bool FConfigCacheIni::LoadLocalIniFile(FConfigFile & ConfigFile, const TCHAR * IniName, bool bIsBaseIniName, const TCHAR * Platform, bool bForceReload)
{
	FConfigContext Context = bIsBaseIniName ? FConfigContext::ReadIntoLocalFile(ConfigFile, Platform) : FConfigContext::ReadSingleIntoLocalFile(ConfigFile, Platform);
	Context.bForceReload = bForceReload;
	return Context.Load(IniName);
}

bool FConfigCacheIni::LoadExternalIniFile(FConfigFile & ConfigFile, const TCHAR * IniName, const TCHAR * EngineConfigDir, const TCHAR * SourceConfigDir, bool bIsBaseIniName, const TCHAR * Platform, bool bForceReload, bool bWriteDestIni, bool bAllowGeneratedIniWhenCooked, const TCHAR * GeneratedConfigDir)
{
	LLM_SCOPE(ELLMTag::ConfigSystem);

	// 	could also set Context.bIsHierarchicalConfig instead of the ?: operator
	FConfigContext Context = bIsBaseIniName ? FConfigContext::ReadIntoLocalFile(ConfigFile, Platform) : FConfigContext::ReadSingleIntoLocalFile(ConfigFile, Platform);
	Context.EngineConfigDir = EngineConfigDir;
	Context.ProjectConfigDir = SourceConfigDir;
	Context.bForceReload = bForceReload;
	Context.bAllowGeneratedIniWhenCooked = bAllowGeneratedIniWhenCooked;
	Context.GeneratedConfigDir = GeneratedConfigDir;
	Context.bWriteDestIni = bWriteDestIni;
	return Context.Load(IniName);
}

FConfigFile* FConfigCacheIni::FindPlatformConfig(const TCHAR* IniName, const TCHAR* Platform)
{
	if (Platform != nullptr && FCString::Stricmp(Platform, ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())) != 0)
	{
#if ALLOW_OTHER_PLATFORM_CONFIG
		return FConfigCacheIni::ForPlatform(Platform)->FindConfigFile(IniName);
#else
		return nullptr;
#endif
	}

	if (GConfig != nullptr)
	{
		return GConfig->FindConfigFile(IniName);
	}

	return nullptr;
}

FConfigFile* FConfigCacheIni::FindOrLoadPlatformConfig(FConfigFile& LocalFile, const TCHAR* IniName, const TCHAR* Platform)
{
	FConfigFile* File = FindPlatformConfig(IniName, Platform);
	if (File == nullptr)
	{
#if ALLOW_OTHER_PLATFORM_CONFIG
		// Check if this ini file corresponds to a plugin
		FConfigPluginDirs* PluginDirs = nullptr;
		{
			FScopeLock Lock(&FConfigContext::ConfigToPluginDirsLock);
			TUniquePtr<FConfigPluginDirs>* PluginDirsPtr = FConfigContext::ConfigToPluginDirs.Find(IniName);
			if (PluginDirsPtr != nullptr)
			{
				// we never remove the item so the raw pointer will not become invalid
				PluginDirs = PluginDirsPtr->Get();
			}
		}
		if (PluginDirs != nullptr)
		{
			// If so, read using the plugin hierarchy
			FConfigContext Context = FConfigContext::ReadIntoPluginFile(LocalFile, *PluginDirs->PluginPath, PluginDirs->PluginExtensionBaseDirs, Platform);
			Context.Load(IniName);
		}
		else
#endif
		{
			FConfigContext Context = FConfigContext::ReadIntoLocalFile(LocalFile, Platform);
			Context.Load(IniName);
		}
		File = &LocalFile;
	}

	return File;
}

void FConfigCacheIni::LoadConsoleVariablesFromINI()
{
#if !DISABLE_CHEAT_CVARS
	{
		const TCHAR* StartupSectionName = TEXT("Startup");
		FString PlatformName = FPlatformProperties::IniPlatformName();
		FString StartupPlatformSectionName = FString::Printf(TEXT("Startup_%s"), *PlatformName);
		FString ConsoleVariablesPath = FPaths::EngineDir() + TEXT("Config/ConsoleVariables.ini");

		// First we read from "../../../Engine/Config/ConsoleVariables.ini" [Startup] section if it exists
		// This is the only ini file where we allow cheat commands (this is why it's not there for UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE::ConfigUtilities::ApplyCVarSettingsFromIni(StartupSectionName, *ConsoleVariablesPath, ECVF_SetByConsoleVariablesIni, true);
		UE::ConfigUtilities::ApplyCVarSettingsFromIni(*StartupPlatformSectionName, *ConsoleVariablesPath, ECVF_SetByConsoleVariablesIni, true);

		#if !UE_BUILD_SHIPPING
		{
			FString OverrideConsoleVariablesPath;
			FParse::Value(FCommandLine::Get(), TEXT("-cvarsini="), OverrideConsoleVariablesPath);

			if (!OverrideConsoleVariablesPath.IsEmpty())
			{
				ensureMsgf(FPaths::FileExists(OverrideConsoleVariablesPath), TEXT("-cvarsini's file %s doesn't exist"), *OverrideConsoleVariablesPath);
				UE::ConfigUtilities::ApplyCVarSettingsFromIni(StartupSectionName, *OverrideConsoleVariablesPath, ECVF_SetByConsoleVariablesIni, true);
				UE::ConfigUtilities::ApplyCVarSettingsFromIni(*StartupPlatformSectionName, *OverrideConsoleVariablesPath, ECVF_SetByConsoleVariablesIni, true);
			}

		}
		#endif
	}
#endif // !DISABLE_CHEAT_CVARS

	// We also apply from Engine.ini [ConsoleVariables] section
	UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("ConsoleVariables"), *GEngineIni, ECVF_SetBySystemSettingsIni);

#if WITH_EDITOR
	// We also apply from DefaultEditor.ini [ConsoleVariables] section
	UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("ConsoleVariables"), *GEditorIni, ECVF_SetBySystemSettingsIni);
#endif	//WITH_EDITOR

	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

FString FConfigCacheIni::NormalizeConfigIniPath(const FString& NonNormalizedPath)
{
	// CreateStandardFilename may not actually do anything in certain cases (e.g., if we detect a network drive, non-root drive, etc.)
	// At a minimum, we will remove double slashes to try and fix some errors.
	return FPaths::CreateStandardFilename(FPaths::RemoveDuplicateSlashes(NonNormalizedPath));
}

FArchive& operator<<(FArchive& Ar, FConfigFile& ConfigFile)
{
	bool bHasSourceConfigFile = ConfigFile.SourceConfigFile != nullptr;
	bool bDirty = ConfigFile.Dirty;
	bool bNoSave = ConfigFile.NoSave;
	bool bHasPlatformName = ConfigFile.bHasPlatformName;

	Ar << static_cast<FConfigFile::Super&>(ConfigFile);
	Ar << bDirty;
	Ar << bNoSave;
	Ar << bHasPlatformName;

	Ar << ConfigFile.Name;
	Ar << ConfigFile.SourceIniHierarchy;
	Ar << ConfigFile.SourceEngineConfigDir;
	Ar << bHasSourceConfigFile;
	if (bHasSourceConfigFile)
	{
		// Handle missing instance for the loading case
		if (ConfigFile.SourceConfigFile == nullptr)
		{
			ConfigFile.SourceConfigFile = new FConfigFile();
		}

		Ar << *ConfigFile.SourceConfigFile;
	}
	Ar << ConfigFile.SourceProjectConfigDir;
	Ar << ConfigFile.PlatformName;
	Ar << ConfigFile.PerObjectConfigArrayOfStructKeys;

	if (Ar.IsLoading())
	{
		ConfigFile.Dirty = bDirty;
		ConfigFile.NoSave = bNoSave;
		ConfigFile.bHasPlatformName = bHasPlatformName;
	}

	return Ar;
}

void FConfigFile::UpdateSections(const TCHAR* DiskFilename, const TCHAR* IniRootName/*=nullptr*/, const TCHAR* OverridePlatform/*=nullptr*/)
{
	// Since we don't want any modifications to other sections, we manually process the file, not read into sections, etc
	// Keep track of existing SectionTexts and orders so that we can preserve the order of the sections in Write to reduce the diff we make to the file on disk
	FString DiskFile;
	FString NewFile;
	TStringBuilder<128> SectionText;
	TMap<FString, FString> SectionTexts;
	TArray<FString> SectionOrder;
	FString SectionName;

	auto AddSectionText = [this, &SectionTexts, &SectionOrder, &SectionName, &SectionText]()
	{
		if (SectionText.Len() == 0)
		{
			// No text in the section, not even a section header, e.g. because this is the prefix section and there was no prefix.
			// Skip the section - add it to SectionTexts or SectionOrder
		}
		else
		{
			if (Contains(SectionName))
			{
				// Do not add the section to SectionTexts, so that Write will skip writing it at all if it is empty
				// But do add it to SectionOrder, so that Write will write it to the right location if it is non-empty
			}
			else
			{
				// Check for duplicate sections in the file-on-disk; handle these by combining them.  This will modify the file, but will guarantee that we don't lose data
				FString* ExistingSectionText = SectionTexts.Find(SectionName);
				if (ExistingSectionText)
				{
					ExistingSectionText->Append(SectionText.ToString(), SectionText.Len());
				}
				else
				{
					SectionTexts.Add(SectionName, FString(SectionText.ToString()));
				}
			}

			SectionOrder.Add(SectionName);
		}

		// Clear the name and text for the next section
		SectionName.Reset();
		SectionText.Reset();
	};

	SectionName = FString(); // The lines we read before we encounter a section header should be preserved as prefix lines; we implement this by storing them in an empty SectionName
	if (LoadConfigFileWrapper(DiskFilename, DiskFile))
	{
		// walk each line
		const TCHAR* Ptr = DiskFile.Len() > 0 ? *DiskFile : nullptr;
		bool bDone = Ptr ? false : true;
		bool bIsSkippingSection = true;
		while (!bDone)
		{
			// read the next line
			FString TheLine;
			if (FParse::Line(&Ptr, TheLine, true) == false)
			{
				bDone = true;
			}
			else
			{
				// Strip any trailing whitespace to match config parsing.
				TheLine.TrimEndInline();

				// is this line a section? (must be at least [x])
				if (TheLine.Len() > 3 && TheLine[0] == '[' && TheLine[TheLine.Len() - 1] == ']')
				{
					// Add the old section we just finished reading
					AddSectionText();

					// Set SectionName to the name of new section we are about to read
					SectionName = TheLine.Mid(1, TheLine.Len() - 2);
				}

				SectionText.Append(TheLine);
				SectionText.Append(LINE_TERMINATOR);
			}
		}
	}

	// Add the last section we read
	AddSectionText();

	// load the hierarchy up to right before this file
	if (IniRootName != nullptr)
	{
		// Get a collection of the source hierarchy properties
		if (SourceConfigFile)
		{
			delete SourceConfigFile;
		}
		SourceConfigFile = new FConfigFile();

		// now when Write it called below, it will diff against this SourceConfigFile
		FConfigContext BaseContext = FConfigContext::ReadUpToBeforeFile(*SourceConfigFile, OverridePlatform, DiskFilename);
		BaseContext.Load(IniRootName);

		SourceIniHierarchy = SourceConfigFile->SourceIniHierarchy;
	}

	WriteInternal(DiskFilename, true, SectionTexts, SectionOrder);
}


/**
 * Functionality to assist with updating a config file with one property value change.
 */
class FSinglePropertyConfigHelper
{
public:
	

	/**
	 * We need certain information for the helper to be useful.
	 *
	 * @Param InIniFilename - The disk location of the file we wish to edit.
	 * @Param InSectionName - The section the property belongs to.
	 * @Param InPropertyName - The name of the property that has been edited.
	 * @Param InPropertyValue - The new value of the property that has been edited, or unset to remove the property.
	 */
	FSinglePropertyConfigHelper(const FString& InIniFilename, const FString& InSectionName, const FString& InPropertyName, const TOptional<FString>& InPropertyValue)
		: IniFilename(InIniFilename)
		, SectionName(InSectionName)
		, PropertyName(InPropertyName)
		, PropertyValue(InPropertyValue)
	{
		// Split the file into the necessary parts.
		PopulateFileContentHelper();
	}

	/**
	 * Perform the action of updating the config file with the new property value.
	 */
	bool UpdateConfigFile()
	{
		UpdatePropertyInSection();
		// Rebuild the file with the updated section.

		FString NewFile = IniFileMakeup.BeforeSection + IniFileMakeup.Section + IniFileMakeup.AfterSection;
		if (!NewFile.EndsWith(TEXT(LINE_TERMINATOR_ANSI LINE_TERMINATOR_ANSI)))
		{
			NewFile.AppendChars(LINE_TERMINATOR, TCString<TCHAR>::Strlen(LINE_TERMINATOR));
		}
		return SaveConfigFileWrapper(*IniFilename, NewFile);
	}


private:

	/**
	 * Clear any trailing whitespace from the end of the output.
	 */
	void ClearTrailingWhitespace(FString& InStr)
	{
		const FString Endl(LINE_TERMINATOR);
		while (InStr.EndsWith(LINE_TERMINATOR, ESearchCase::CaseSensitive))
		{
			InStr.LeftChopInline(Endl.Len(), EAllowShrinking::No);
		}
	}
	
	/**
	 * Update the section with the new value for the property.
	 */
	void UpdatePropertyInSection()
	{
		FString UpdatedSection;
		if (IniFileMakeup.Section.IsEmpty())
		{
			if (PropertyValue.IsSet())
			{
				const FString DecoratedSectionName = FString::Printf(TEXT("[%s]"), *SectionName);

				ClearTrailingWhitespace(IniFileMakeup.BeforeSection);
				UpdatedSection += LINE_TERMINATOR;
				UpdatedSection += LINE_TERMINATOR;
				UpdatedSection += DecoratedSectionName;
				AppendPropertyLine(UpdatedSection);
			}
		}
		else
		{
			FString SectionLine;
			const TCHAR* Ptr = *IniFileMakeup.Section;
			bool bUpdatedPropertyOnPass = false;
			while (Ptr != nullptr && FParse::Line(&Ptr, SectionLine, true))
			{
				if (SectionLine.StartsWith(FString::Printf(TEXT("%s="), *PropertyName)))
				{
					if (PropertyValue.IsSet())
					{
						UpdatedSection += FConfigFile::GenerateExportedPropertyLine(PropertyName, PropertyValue.GetValue());
					}
					bUpdatedPropertyOnPass = true;
				}
				else
				{
					UpdatedSection += SectionLine;
					UpdatedSection += LINE_TERMINATOR;
				}
			}

			// If the property wasnt found in the text of the existing section content,
			// append it to the end of the section.
			if (!bUpdatedPropertyOnPass && PropertyValue.IsSet())
			{
				AppendPropertyLine(UpdatedSection);
			}
			else
			{
				UpdatedSection += LINE_TERMINATOR;
			}
		}

		IniFileMakeup.Section = UpdatedSection;
	}
	
	/**
	 * Split the file up into parts:
	 * -> Before the section we wish to edit, which will remain unaltered,
	 * ->-> The section we wish to edit, we only seek to edit the single property,
	 * ->->-> After the section we wish to edit, which will remain unaltered.
	 */
	void PopulateFileContentHelper()
	{
		FString UnprocessedFileContents;
		if (LoadConfigFileWrapper(*IniFilename, UnprocessedFileContents))
		{
			// Find the section in the file text.
			const FString DecoratedSectionName = FString::Printf(TEXT("[%s]"), *SectionName);

			const int32 DecoratedSectionNameStartIndex = UnprocessedFileContents.Find(DecoratedSectionName);
			if (DecoratedSectionNameStartIndex != INDEX_NONE)
			{
				// If we found the section, cache off the file text before the section.
				IniFileMakeup.BeforeSection = UnprocessedFileContents.Left(DecoratedSectionNameStartIndex);
				UnprocessedFileContents.RemoveAt(0, IniFileMakeup.BeforeSection.Len());

				// For the rest of the file, split it into the section we are editing and the rest of the file after.
				const TCHAR* Ptr = UnprocessedFileContents.Len() > 0 ? *UnprocessedFileContents : nullptr;
				FString NextUnprocessedLine;
				bool bReachedNextSection = false;
				while (Ptr != nullptr && FParse::Line(&Ptr, NextUnprocessedLine, true))
				{
					bReachedNextSection |= (NextUnprocessedLine.StartsWith(TEXT("[")) && NextUnprocessedLine != DecoratedSectionName);
					if (bReachedNextSection)
					{
						IniFileMakeup.AfterSection += NextUnprocessedLine;
						IniFileMakeup.AfterSection += LINE_TERMINATOR;
					}
					else
					{
						IniFileMakeup.Section += NextUnprocessedLine;
						IniFileMakeup.Section += LINE_TERMINATOR;
					}
				}
			}
			else
			{
				IniFileMakeup.BeforeSection = UnprocessedFileContents;
			}
		}
	}
	
	/**
	 * Append the property entry to the section
	 */
	void AppendPropertyLine(FString& PreText)
	{
		check(PropertyValue.IsSet());
		
		// Make sure we dont leave much whitespace, and append the property name/value entry
		ClearTrailingWhitespace(PreText);
		PreText += LINE_TERMINATOR;
		PreText += FConfigFile::GenerateExportedPropertyLine(PropertyName, PropertyValue.GetValue());
		PreText += LINE_TERMINATOR;
	}


private:
	// The disk location of the ini file we seek to edit
	FString IniFilename;

	// The section in the config file
	FString SectionName;

	// The name of the property that has been changed
	FString PropertyName;

	// The new value, in string format, of the property that has been changed
	// This will be unset if the property has been removed from the config
	TOptional<FString> PropertyValue;

	// Helper struct that holds the makeup of the ini file.
	struct IniFileContent
	{
		// The section we wish to edit
		FString Section;

		// The file contents before the section we are editing
		FString BeforeSection;

		// The file contents after the section we are editing
		FString AfterSection;
	} IniFileMakeup; // Instance of the helper to maintain file structure.
};


bool FConfigFile::UpdateSinglePropertyInSection(const TCHAR* DiskFilename, const TCHAR* PropertyName, const TCHAR* SectionName)
{
	TOptional<FString> PropertyValue;
	if (const FConfigSection* LocalSection = this->FindSection(SectionName))
	{
		if (const FConfigValue* ConfigValue = LocalSection->Find(PropertyName))
		{
			// Use GetSavedValueForWriting rather than GetSavedValue to avoid having this save operation mark the value as having been accessed for dependency tracking
			PropertyValue = UE::ConfigCacheIni::Private::FAccessor::GetSavedValueForWriting(*ConfigValue);
		}
	}

	FSinglePropertyConfigHelper SinglePropertyConfigHelper(DiskFilename, SectionName, PropertyName, PropertyValue);
	return SinglePropertyConfigHelper.UpdateConfigFile();
}



#if ALLOW_OTHER_PLATFORM_CONFIG
// these are knowingly leaked
static TMap<FName, FConfigCacheIni*> GConfigForPlatform;
static FCriticalSection GConfigForPlatformLock;

#if WITH_EDITOR
void FConfigCacheIni::AsyncInitializeConfigForPlatforms()
{
	// make sure any (non-const static) paths the worker threads will use are already initialized
	FPaths::ProjectDir();
	FPlatformMisc::GeneratedConfigDir(); // also inits FPaths::ProjectSavedDir
	FConfigContext::EnsureRequiredGlobalPathsHaveBeenInitialized();
	FPlatformProcess::ApplicationSettingsDir();

	// pre-create all platforms so that the loop below doesn't reallocate anything in the map
	const TMap<FName, FDataDrivenPlatformInfo>& AllPlatformInfos = FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos();
	for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
	{
		GetPlatformConfigFutures().Emplace(Pair.Key);
		GConfigForPlatform.Add(Pair.Key, new FConfigCacheIni(EConfigCacheType::Temporary));
	}

	for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
	{
		FName PlatformName = Pair.Key;
		GetPlatformConfigFutures()[PlatformName] = Async(EAsyncExecution::ThreadPool, [PlatformName]
		{
			double Start = FPlatformTime::Seconds();

			FConfigCacheIni* NewConfig = GConfigForPlatform.FindChecked(PlatformName);
			FConfigContext Context = FConfigContext::ReadIntoConfigSystem(NewConfig, PlatformName.ToString());
			InitializeKnownConfigFiles(Context);
	
			UE_LOG(LogConfig, Display, TEXT("Loading %s ini files took %.2f seconds"), *PlatformName.ToString(), FPlatformTime::Seconds() - Start);
		});
	}
}

#endif
#endif


FConfigCacheIni* FConfigCacheIni::ForPlatform(FName PlatformName)
{
#if ALLOW_OTHER_PLATFORM_CONFIG
	check(GConfig != nullptr && GConfig->bIsReadyForUse);

	// use GConfig when no platform is specified
	if (PlatformName == NAME_None)
	{
		return GConfig;
	}

#if WITH_EDITOR
	// they are likely already loaded, but just block to make sure
	{
		if (auto* PlatformCache = GetPlatformConfigFutures().Find(PlatformName))
		{
			PlatformCache->Get();
		}
		else
		{
			return GConfig;
		}
	}
#endif

	// protect against other threads clearing the array, or two threads trying to read in a missing platform at the same time
	FScopeLock Lock(&GConfigForPlatformLock);
	FConfigCacheIni* PlatformConfig = GConfigForPlatform.FindRef(PlatformName);

	// read any missing platform configs now, on demand (this will happen when WITH_EDITOR is 0)
	if (PlatformConfig == nullptr)
	{
		double Start = FPlatformTime::Seconds();
		
		PlatformConfig = GConfigForPlatform.Add(PlatformName, new FConfigCacheIni(EConfigCacheType::Temporary));
		FConfigContext Context = FConfigContext::ReadIntoConfigSystem(PlatformConfig, PlatformName.ToString());
		InitializeKnownConfigFiles(Context);

		UE_LOG(LogConfig, Display, TEXT("Read in platform %s ini files took %.2f seconds"), *PlatformName.ToString(), FPlatformTime::Seconds() - Start);
	}

	return GConfigForPlatform.FindRef(PlatformName);

#else
	UE_LOG(LogConfig, Error, TEXT("FConfigCacheIni::ForPlatform cannot be called when not in a developer tool"));
	return nullptr;
#endif
}

void FConfigCacheIni::ClearOtherPlatformConfigs()
{
#if ALLOW_OTHER_PLATFORM_CONFIG
	// this will read in on next call to ForPlatform()
	FScopeLock Lock(&GConfigForPlatformLock);
	GConfigForPlatform.Empty();
#endif
}









////////////////////////////////////////
//
// Deprecated function wrappers
//
////////////////////////////////////////

void ApplyCVarSettingsFromIni(const TCHAR* InSectionBaseName, const TCHAR* InIniFilename, uint32 SetBy, bool bAllowCheating)
{
	UE::ConfigUtilities::ApplyCVarSettingsFromIni(InSectionBaseName, InIniFilename, SetBy, bAllowCheating);
}

void ForEachCVarInSectionFromIni(const TCHAR* InSectionName, const TCHAR* InIniFilename, TFunction<void(IConsoleVariable* CVar, const FString& KeyString, const FString& ValueString)> InEvaluationFunction)
{
	UE::ConfigUtilities::ForEachCVarInSectionFromIni(InSectionName, InIniFilename, InEvaluationFunction);
}

void RecordApplyCVarSettingsFromIni()
{
	UE::ConfigUtilities::RecordApplyCVarSettingsFromIni();
}

void ReapplyRecordedCVarSettingsFromIni()
{
	UE::ConfigUtilities::ReapplyRecordedCVarSettingsFromIni();
}

void DeleteRecordedCVarSettingsFromIni()
{
	UE::ConfigUtilities::DeleteRecordedCVarSettingsFromIni();
}

void RecordConfigReadsFromIni()
{
	UE::ConfigUtilities::RecordConfigReadsFromIni();
}

void DumpRecordedConfigReadsFromIni()
{
	UE::ConfigUtilities::DumpRecordedConfigReadsFromIni();
}

void DeleteRecordedConfigReadsFromIni()
{
	UE::ConfigUtilities::DeleteRecordedConfigReadsFromIni();
}

const TCHAR* ConvertValueFromHumanFriendlyValue(const TCHAR* Value)
{
	return UE::ConfigUtilities::ConvertValueFromHumanFriendlyValue(Value);
}

#undef LOCTEXT_NAMESPACE
