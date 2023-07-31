// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IBuildPatchServicesModule.h"

namespace BuildPatchTool
{
	enum class EReturnCode : int32;

	class IToolMode
	{
	public:
		TCHAR const * const EqualsStr = TEXT("=");
		TCHAR const * const QuoteStr = TEXT("\"");

	public:
		virtual EReturnCode Execute() = 0;

		/**
		 * Helper for normalizing a URI referring to a file. We make sure not to cause issues with URIs that separate a protocol and authority with ://
		 * and windows UNC paths that begin with \\. Windows UNC paths still work in UE4 code with //.
		 *
		 * @param UriFile       The file URI to be Normalized.
		 */
		void NormalizeUriFile(FString& UriFile)
		{
			// Replace all slashes.
			UriFile.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
		}

		/**
		 * Helper for normalizing a URI referring to a path. We make sure not to cause issues with URIs that separate a protocol and authority with ://
		 * and windows UNC paths that begin with \\. Windows UNC paths still work in UE4 code with //.
		 *
		 * @param UriPath       The path URI to be Normalized.
		 */
		void NormalizeUriPath(FString& UriPath)
		{
			// Fix all slashes.
			NormalizeUriFile(UriPath);
			// Remove trailing slashes, unless it is for a windows based drive (e.g. "C:/")
			int32 TrailingSlashIdx = UriPath.Len() - 1;
			while (UriPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive) && !UriPath.EndsWith(TEXT(":/"), ESearchCase::CaseSensitive))
			{
				// Overwrite trailing slash with terminator, and trim size.
				UriPath.GetCharArray()[TrailingSlashIdx--] = 0;
				UriPath.GetCharArray().SetNum(UriPath.GetCharArray().Num() - 1, false);
			}
		}

		/**
		 * Helper for parsing a switch from an array of switches, usually produced using FCommandLine::Parse(..)
		 *
		 * @param InSwitch      The switch name, ending with =. E.g. option=, foo=. It would usually be a compile time const.
		 * @param Value         Receives the value from the switch.
		 * @param Switches      The array of switches to search through.
		 * @return true if the switch was found.
		 */
		template <typename TValueType>
		bool ParseSwitch(const TCHAR* InSwitch, TValueType& Value, const TArray<FString>& Switches)
		{
			// Debug check requirements for InSwitch
			checkSlow(InSwitch != nullptr);
			checkSlow(InSwitch[FCString::Strlen(InSwitch)-1] == TEXT('='));

			for (const FString& Switch : Switches)
			{
				if (Switch.StartsWith(InSwitch))
				{
					FString StringValue;
					Switch.Split(EqualsStr, nullptr, &StringValue);
					return ParseValue(StringValue, Value);
				}
			}
			return false;
		}

		/**
		 * Helper for parsing an array of multiple same name switches from the full array of switches, usually produced using FCommandLine::Parse(..)
		 *
		 * @param InSwitch      The switch name, ending with =. E.g. option=, foo=. It would usually be a compile time const.
		 * @param Values        Receives the values from the switches.
		 * @param Switches      The array of switches to search through.
		 * @return true if at least one match was found.
		 */
		template <typename TValueType>
		bool ParseSwitches(const TCHAR* InSwitch, TArray<TValueType>& Values, const TArray<FString>& Switches)
		{
			// Debug check requirements for InSwitch
			checkSlow(InSwitch != nullptr);
			checkSlow(InSwitch[FCString::Strlen(InSwitch)-1] == TEXT('='));

			bool bFoundValue = false;
			for (const FString& Switch : Switches)
			{
				if (Switch.StartsWith(InSwitch))
				{
					FString StringValue;
					TValueType Value;
					Switch.Split(EqualsStr, nullptr, &StringValue);
					if (ParseValue(StringValue, Value))
					{
						Values.Emplace(MoveTemp(Value));
						bFoundValue = true;
					}
				}
			}
			return bFoundValue;
		}

		bool ParseOption(const TCHAR* InSwitch, const TArray<FString>& Switches)
		{
			return Switches.Contains(InSwitch);
		}

		bool ParseValue(const FString& ValueIn, FString& ValueOut)
		{
			ValueOut = ValueIn.TrimQuotes();
			return true;
		}

		bool ParseValue(const FString& ValueIn, uint64& ValueOut)
		{
			if (FCString::IsNumeric(*ValueIn) && !ValueIn.Contains(TEXT("-"), ESearchCase::CaseSensitive))
			{
				ValueOut = FCString::Strtoui64(*ValueIn, nullptr, 10);
				return true;
			}
			return false;
		}

		bool ParseValue(const FString& ValueIn, uint32& ValueOut)
		{
			if (FCString::IsNumeric(*ValueIn) && !ValueIn.Contains(TEXT("-"), ESearchCase::CaseSensitive))
			{
				ValueOut = (uint32)FCString::Strtoi(*ValueIn, nullptr, 10);
				return true;
			}
			return false;
		}

		template<typename ContainerType>
		bool ParseValue(const FString& ValueIn, ContainerType& ValueOut)
		{
			TArray<FString> TempOut;
			ValueOut.Empty();
			FString CleanValueIn = ValueIn.TrimQuotes();
			CleanValueIn.RemoveSpacesInline();
			if (CleanValueIn.ParseIntoArray(TempOut, TEXT(","), false) != 0)
			{
				ValueOut.Append(MoveTemp(TempOut));
			}
			else
			{
				// Add a single empty element. This is what is intended if switch is present on the command
				// line and contains no commas.
				ValueOut.Add(TEXT(""));
			}

			return true;
		}
	};

	typedef TSharedRef<IToolMode> IToolModeRef;
	typedef TSharedPtr<IToolMode> IToolModePtr;

	class FToolModeFactory
	{
	public:
		static IToolModeRef Create(IBuildPatchServicesModule& BpsInterface);
	};
}
