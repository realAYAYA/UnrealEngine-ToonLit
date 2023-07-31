// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManifest.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "Modules/SimpleParse.h"

FModuleManifest::FModuleManifest()
{
}

FString FModuleManifest::GetFileName(const FString& DirectoryName, bool bIsGameFolder)
{
#if UE_BUILD_DEVELOPMENT
	return DirectoryName / ((FApp::GetBuildConfiguration() == EBuildConfiguration::DebugGame && bIsGameFolder)? TEXT(UBT_MODULE_MANIFEST_DEBUGGAME) : TEXT(UBT_MODULE_MANIFEST));
#else
	return DirectoryName / TEXT(UBT_MODULE_MANIFEST);
#endif
}

// Assumes GetTypeHash(AltKeyType) matches GetTypeHash(KeyType)
template<class KeyType, class ValueType, class AltKeyType, class AltValueType>
ValueType& FindOrAddHeterogeneous(TMap<KeyType, ValueType>& Map, const AltKeyType& Key, const AltValueType& Value) 
{
	checkSlow(GetTypeHash(KeyType(Key)) == GetTypeHash(Key));
	ValueType* Existing = Map.FindByHash(GetTypeHash(Key), Key);
	return Existing ? *Existing : Map.Emplace(KeyType(Key), AltValueType(Value));
}

bool FModuleManifest::TryRead(const FString& FileName, FModuleManifest& OutManifest)
{
	// Read the file to a string
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FileName))
	{
		return false;
	}

	const TCHAR* Ptr = *Text;
	if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT('{')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || FSimpleParse::MatchChar(Ptr, TEXT('}')))
	{
		return false;
	}

	for (;;)
	{
		TStringBuilder<64> Field;
		if (!FSimpleParse::ParseString(Ptr, Field))
		{
			return false;
		}

		if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT(':')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}

		if (Field.ToView() == TEXTVIEW("BuildId"))
		{
			if (!FSimpleParse::ParseString(Ptr, OutManifest.BuildId))
			{
				return false;
			}
		}
		else if (Field.ToView() == TEXTVIEW("Modules"))
		{
			if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT('{')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
			{
				return false;
			}

			if (!FSimpleParse::MatchChar(Ptr, TEXT('}')))
			{
				for (;;)
				{
					TStringBuilder<64> ModuleName;
					TStringBuilder<80> ModulePath;
					if (!FSimpleParse::ParseString(Ptr, ModuleName) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT(':')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::ParseString(Ptr, ModulePath) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
					{
						return false;
					}

					FindOrAddHeterogeneous(OutManifest.ModuleNameToFileName, ModuleName.ToView(), ModulePath.ToView());

					if (FSimpleParse::MatchChar(Ptr, TEXT('}')))
					{
						break;
					}

					if (!FSimpleParse::MatchChar(Ptr, TEXT(',')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
					{
						return false;
					}
				}
			}
		}
		else
		{
			return false;
		}

		if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}

		if (FSimpleParse::MatchChar(Ptr, TEXT('}')))
		{
			return true;
		}

		if (!FSimpleParse::MatchChar(Ptr, TEXT(',')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}
	}
}
