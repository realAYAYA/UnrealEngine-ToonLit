// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextFromSourceCommandlet.h"
#include "HAL/FileManager.h"
#include "Misc/AsciiSet.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ExpressionParserTypes.h"
#include "Algo/Transform.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Internationalization/TextNamespaceUtil.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromSourceCommandlet, Log, All);

//////////////////////////////////////////////////////////////////////////
//GatherTextFromSourceCommandlet

#define LOC_DEFINE_REGION

UGatherTextFromSourceCommandlet::UGatherTextFromSourceCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::DefineString(TEXT("#define "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::UndefString(TEXT("#undef "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IfString(TEXT("#if "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IfDefString(TEXT("#ifdef "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::ElIfString(TEXT("#elif "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::ElseString(TEXT("#else"));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::EndIfString(TEXT("#endif"));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::DefinedString(TEXT("defined "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IniNamespaceString(TEXT("["));
const FString UGatherTextFromSourceCommandlet::FMacroDescriptor::TextMacroString(TEXT("TEXT"));
const FString UGatherTextFromSourceCommandlet::ChangelistName(TEXT("Update Localization"));

bool UGatherTextFromSourceCommandlet::ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const
{
	const FString* GatherType = ParamVals.Find(UGatherTextCommandletBase::GatherTypeParam);
	// If the param is not specified, it is assumed that both source and assets are to be gathered 
	return !GatherType || *GatherType == TEXT("Source") || *GatherType == TEXT("All");
}

int32 UGatherTextFromSourceCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
	
	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;
	
	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// GatheredSourceBasePath
	FString GatheredSourceBasePath;
	GetPathFromConfig(*SectionName, TEXT("GatheredSourceBasePath"), GatheredSourceBasePath, GatherTextConfigPath);
	if (GatheredSourceBasePath.IsEmpty())
	{
		GatheredSourceBasePath = UGatherTextCommandletBase::GetProjectBasePath();
	}

	// SearchDirectoryPaths
	TArray<FString> SearchDirectoryPaths;
	GetPathArrayFromConfig(*SectionName, TEXT("SearchDirectoryPaths"), SearchDirectoryPaths, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			SearchDirectoryPaths.Append(IncludePaths);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use SearchDirectoryPaths."), *SectionName);
		}
	}

	if (SearchDirectoryPaths.Num() == 0)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("No search directory paths in section %s."), *SectionName);
		return 0;
	}

	// ExcludePathFilters
	TArray<FString> ExcludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	// FileNameFilters
	TArray<FString> FileNameFilters;
	GetStringArrayFromConfig(*SectionName, TEXT("FileNameFilters"), FileNameFilters, GatherTextConfigPath);

	// SourceFileSearchFilters (DEPRECATED)
	{
		TArray<FString> SourceFileSearchFilters;
		GetStringArrayFromConfig(*SectionName, TEXT("SourceFileSearchFilters"), SourceFileSearchFilters, GatherTextConfigPath);
		if (SourceFileSearchFilters.Num())
		{
			FileNameFilters.Append(SourceFileSearchFilters);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("SourceFileSearchFilters detected in section %s. SourceFileSearchFilters is deprecated, please use FileNameFilters."), *SectionName);
		}
	}

	if (FileNameFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("No source filters in section %s"), *SectionName);
		return 0;
	}

	//Ensure all filters are unique.
	TArray<FString> UniqueSourceFileSearchFilters;
	for (const FString& SourceFileSearchFilter : FileNameFilters)
	{
		UniqueSourceFileSearchFilters.AddUnique(SourceFileSearchFilter);
	}

	// Build the final set of include/exclude paths to scan.
	TArray<FString> IncludePathFilters;
	Algo::Transform(SearchDirectoryPaths, IncludePathFilters, [](const FString& SearchDirectoryPath)
	{
		const TCHAR LastChar = SearchDirectoryPath.Len() > 0 ? SearchDirectoryPath[SearchDirectoryPath.Len() - 1] : 0;
		return (LastChar == TEXT('*') || LastChar == TEXT('?'))
			? SearchDirectoryPath								// Already a wildcard
			: FPaths::Combine(SearchDirectoryPath, TEXT("*"));	// Add a catch-all wildcard
	});

	FGatherTextDelegates::GetAdditionalGatherPaths.Broadcast(GatherManifestHelper->GetTargetName(), IncludePathFilters, ExcludePathFilters);

	// Search in the root folder for each of the wildcard filters specified and build a list of files
	TArray<FString> FilesToProcess;
	{
		TArray<FString> RootSourceFiles;
		TSet<FString, FLocKeySetFuncs> ProcessedSearchDirectoryPaths;
		for (const FString& IncludePathFilter : IncludePathFilters)
		{
			constexpr FAsciiSet Wildcards("*?");

			FString SearchDirectoryPath = IncludePathFilter;
			if (const TCHAR* FirstWildcard = FAsciiSet::FindFirstOrEnd(*SearchDirectoryPath, Wildcards); *FirstWildcard != 0)
			{
				// Trim the wildcard from this search path
				SearchDirectoryPath = SearchDirectoryPath.Left(UE_PTRDIFF_TO_INT32(FirstWildcard - *SearchDirectoryPath));
				SearchDirectoryPath = FPaths::GetPath(MoveTemp(SearchDirectoryPath));
			}

			bool bAlreadyProcessed = false;
			ProcessedSearchDirectoryPaths.Add(SearchDirectoryPath, &bAlreadyProcessed);
			if (bAlreadyProcessed)
			{
				continue;
			}

			for (const FString& UniqueSourceFileSearchFilter : UniqueSourceFileSearchFilters)
			{
				IFileManager::Get().FindFilesRecursive(RootSourceFiles, *SearchDirectoryPath, *UniqueSourceFileSearchFilter, true, false, false);

				for (FString& RootSourceFile : RootSourceFiles)
				{
					if (FPaths::IsRelative(RootSourceFile))
					{
						RootSourceFile = FPaths::ConvertRelativePathToFull(MoveTemp(RootSourceFile));
					}
				}

				FilesToProcess.Append(MoveTemp(RootSourceFiles));
				RootSourceFiles.Reset();
			}
		}
	}

	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePathFilters, ExcludePathFilters);
	FilesToProcess.RemoveAll([&FuzzyPathMatcher](const FString& FoundFile)
	{
		// Filter out assets whose package file paths do not pass the "fuzzy path" filters.
		if (FuzzyPathMatcher.TestPath(FoundFile) != FFuzzyPathMatcher::EPathMatch::Included)
		{
			return true;
		}

		return false;
	});
	
	// Return if no source files were found
	if( FilesToProcess.Num() == 0 )
	{
		FString SpecifiedDirectoriesString;
		for (const FString& IncludePath : IncludePathFilters)
		{
			SpecifiedDirectoriesString.Append(FString(SpecifiedDirectoriesString.IsEmpty() ? TEXT("") : TEXT("\n")) + FString::Printf(TEXT("+ %s"), *IncludePath));
		}
		for (const FString& ExcludePath : ExcludePathFilters)
		{
			SpecifiedDirectoriesString.Append(FString(SpecifiedDirectoriesString.IsEmpty() ? TEXT("") : TEXT("\n")) + FString::Printf(TEXT("- %s"), *ExcludePath));
		}

		FString SourceFileSearchFiltersString;
		for (const FString& Filter : UniqueSourceFileSearchFilters)
		{
			SourceFileSearchFiltersString += FString(SourceFileSearchFiltersString.IsEmpty() ? TEXT("") : TEXT(", ")) + Filter;
		}

		UE_LOG(LogGatherTextFromSourceCommandlet, Display, TEXT("The GatherTextFromSource commandlet couldn't find any source files matching (%s) in the specified directories:\n%s"), *SourceFileSearchFiltersString, *SpecifiedDirectoriesString);
		return 0;
	}

	// Add any manifest dependencies if they were provided
	TArray<FString> ManifestDependenciesList;
	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);
	
	for (const FString& ManifestDependency : ManifestDependenciesList)
	{
		FText OutError;
		if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("The GatherTextFromSource commandlet couldn't load the specified manifest dependency: '%s'. %s"), *ManifestDependency, *OutError.ToString());
			return -1;
		}
	}

	// Get the loc macros and their syntax
	TArray<FParsableDescriptor*> Parsables;

	Parsables.Add(new FDefineDescriptor());

	Parsables.Add(new FUndefDescriptor());

	Parsables.Add(new FIfDescriptor());

	Parsables.Add(new FIfDefDescriptor());

	Parsables.Add(new FElIfDescriptor());

	Parsables.Add(new FElseDescriptor());

	Parsables.Add(new FEndIfDescriptor());

	Parsables.Add(new FUICommandMacroDescriptor());

	Parsables.Add(new FUICommandExtMacroDescriptor());

	Parsables.Add(new FMetasoundParamMacroDescriptor());
	
	// New Localization System with Namespace as literal argument.
	Parsables.Add(new FStringMacroDescriptor( FString(TEXT("NSLOCTEXT")),
		FStringMacroDescriptor::FMacroArg(FStringMacroDescriptor::MAS_Namespace, true),
		FStringMacroDescriptor::FMacroArg(FStringMacroDescriptor::MAS_Identifier, true),
		FStringMacroDescriptor::FMacroArg(FStringMacroDescriptor::MAS_SourceText, true)));
	
	// New Localization System with Namespace as preprocessor define.
	Parsables.Add(new FStringMacroDescriptor( FString(TEXT("LOCTEXT")),
		FStringMacroDescriptor::FMacroArg(FStringMacroDescriptor::MAS_Identifier, true),
		FStringMacroDescriptor::FMacroArg(FStringMacroDescriptor::MAS_SourceText, true)));

	Parsables.Add(new FStringTableMacroDescriptor());

	Parsables.Add(new FStringTableFromFileMacroDescriptor(TEXT("LOCTABLE_FROMFILE_ENGINE"), FPaths::EngineContentDir()));

	Parsables.Add(new FStringTableFromFileMacroDescriptor(TEXT("LOCTABLE_FROMFILE_GAME"), FPaths::ProjectContentDir()));

	Parsables.Add(new FStringTableEntryMacroDescriptor());

	Parsables.Add(new FStringTableEntryMetaDataMacroDescriptor());

	Parsables.Add(new FStructuredLogMacroDescriptor(TEXT("UE_LOGFMT_LOC"), FStructuredLogMacroDescriptor::EFlags::None));
	Parsables.Add(new FStructuredLogMacroDescriptor(TEXT("UE_LOGFMT_LOC_EX"), FStructuredLogMacroDescriptor::EFlags::None));
	Parsables.Add(new FStructuredLogMacroDescriptor(TEXT("UE_LOGFMT_NSLOC"), FStructuredLogMacroDescriptor::EFlags::Namespace));
	Parsables.Add(new FStructuredLogMacroDescriptor(TEXT("UE_LOGFMT_NSLOC_EX"), FStructuredLogMacroDescriptor::EFlags::Namespace));

	Parsables.Add(new FIniNamespaceDescriptor());

	// Init a parse context to track the state of the file parsing 
	FSourceFileParseContext ParseCtxt(this);

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ParseCtxt.ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ParseCtxt.ShouldGatherFromEditorOnlyData = false;
	}

	// Parse all source files for macros and add entries to SourceParsedEntries
	for ( FString& SourceFile : FilesToProcess)
	{
		ParseCtxt.Filename = SourceFile;
		ParseCtxt.FileTypes = ParseCtxt.Filename.EndsWith(TEXT(".ini")) ? EGatherTextSourceFileTypes::Ini : EGatherTextSourceFileTypes::Cpp;
		FPaths::MakePathRelativeTo(ParseCtxt.Filename, *GatheredSourceBasePath);
		ParseCtxt.LineNumber = 0;
		ParseCtxt.FilePlatformName = GetSplitPlatformNameFromPath(ParseCtxt.Filename);
		ParseCtxt.LineText.Reset();
		ParseCtxt.Namespace.Reset();
		ParseCtxt.RawStringLiteralClosingDelim.Reset();
		ParseCtxt.ExcludedRegion = false;
		ParseCtxt.WithinBlockComment = false;
		ParseCtxt.WithinLineComment = false;
		ParseCtxt.WithinStringLiteral = false;
		ParseCtxt.WithinNamespaceDefineLineNumber = INDEX_NONE;
		ParseCtxt.WithinStartingLine = nullptr;
		ParseCtxt.TextLines.Reset();
		ParseCtxt.FlushMacroStack();

		FString SourceFileText;
		if (!FFileHelper::LoadFileToString(SourceFileText, *SourceFile))
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("%s: GatherTextSource failed to open file"), *ParseCtxt.Filename);
		}
		else
		{
			if (!ParseSourceText(SourceFileText, Parsables, ParseCtxt))
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: GatherTextSource error(s) parsing source file"), *ParseCtxt.Filename);
			}
			else
			{
				if (ParseCtxt.WithinNamespaceDefineLineNumber != INDEX_NONE)
				{
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Missing '#undef LOCTEXT_NAMESPACE' for '#define LOCTEXT_NAMESPACE'"), *ParseCtxt.Filename, ParseCtxt.WithinNamespaceDefineLineNumber);
				}
			}
		}
	}
	
	// Process any parsed string tables
	for (const auto& ParsedStringTablePair : ParseCtxt.ParsedStringTables)
	{
		if (ParsedStringTablePair.Value.SourceLocation.Line == INDEX_NONE)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("String table with ID '%s' had %d entries parsed for it, but the table was never registered. Skipping for gather."), *ParsedStringTablePair.Key.ToString(), ParsedStringTablePair.Value.TableEntries.Num());
		}
		else
		{
			for (const auto& ParsedStringTableEntryPair : ParsedStringTablePair.Value.TableEntries)
			{
				if (!ParsedStringTableEntryPair.Value.bIsEditorOnly || ParseCtxt.ShouldGatherFromEditorOnlyData)
				{
					FManifestContext SourceContext;
					SourceContext.Key = ParsedStringTableEntryPair.Key;
					SourceContext.SourceLocation = ParsedStringTableEntryPair.Value.SourceLocation.ToString();
					SourceContext.PlatformName = ParsedStringTableEntryPair.Value.PlatformName;

					const FParsedStringTableEntryMetaDataMap* ParsedMetaDataMap = ParsedStringTablePair.Value.MetaDataEntries.Find(ParsedStringTableEntryPair.Key);
					if (ParsedMetaDataMap && ParsedMetaDataMap->Num() > 0)
					{
						SourceContext.InfoMetadataObj = MakeShareable(new FLocMetadataObject());
						for (const auto& ParsedMetaDataPair : *ParsedMetaDataMap)
						{
							if (!ParsedMetaDataPair.Value.bIsEditorOnly || ParseCtxt.ShouldGatherFromEditorOnlyData)
							{
								SourceContext.InfoMetadataObj->SetStringField(ParsedMetaDataPair.Key.ToString(), ParsedMetaDataPair.Value.MetaData);
							}
						}
					}

					GatherManifestHelper->AddSourceText(ParsedStringTablePair.Value.TableNamespace, FLocItem(ParsedStringTableEntryPair.Value.SourceString), SourceContext);
				}
			}
		}
	}

	// Clear parsables list safely
	for (int32 i=0; i<Parsables.Num(); i++)
	{
		delete Parsables[i];
	}

	return 0;
}

FString UGatherTextFromSourceCommandlet::UnescapeLiteralCharacterEscapeSequences(const FString& InString)
{
	// We need to un-escape any octal, hex, or universal character sequences that exist in this string to mimic what happens when the string is processed by the compiler
	enum class EParseState : uint8
	{
		Idle,		// Not currently parsing a sequence
		InOct,		// Within an octal sequence (\012)
		InHex,		// Within an hexadecimal sequence (\xBEEF)
		InUTF16,	// Within a UTF-16 sequence (\u1234)
		InUTF32,	// Within a UTF-32 sequence (\U12345678)
	};

	FString RetString;
	RetString.Reserve(InString.Len());

	EParseState ParseState = EParseState::Idle;
	FString EscapedLiteralCharacter;
	for (const TCHAR* CharPtr = *InString; ; ++CharPtr)
	{
		const TCHAR CurChar = *CharPtr;

		if (ParseState == EParseState::Idle && CurChar == 0)
		{
			// End of string
			break;
		}

		switch (ParseState)
		{
		case EParseState::Idle:
			{
				const TCHAR NextChar = *(CharPtr + 1);
				if (CurChar == TEXT('\\') && NextChar)
				{
					if (FChar::IsOctDigit(NextChar))
					{
						ParseState = EParseState::InOct;
					}
					else if (NextChar == TEXT('x'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InHex;
					}
					else if (NextChar == TEXT('u'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InUTF16;
					}
					else if (NextChar == TEXT('U'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InUTF32;
					}
				}
				
				if (ParseState == EParseState::Idle)
				{
					RetString.AppendChar(CurChar);
				}
				else
				{
					EscapedLiteralCharacter.Reset();
				}
			}
			break;

		case EParseState::InOct:
			{
				if (FChar::IsOctDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// Octal sequences can only be up-to 3 digits long
					check(EscapedLiteralCharacter.Len() <= 3);
					if (EscapedLiteralCharacter.Len() == 3)
					{
						RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 8));
						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					if (EscapedLiteralCharacter.Len() > 0)
					{
						RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 8));
					}
					ParseState = EParseState::Idle;
					--CharPtr; // Walk backwards as we need to consider whether the current character is the start of a new escape sequence
				}
			}
			break;

		case EParseState::InHex:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);
				}
				else
				{
					if (EscapedLiteralCharacter.Len() > 0)
					{
						RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16));
					}
					ParseState = EParseState::Idle;
					--CharPtr; // Walk backwards as we need to consider whether the current character is the start of a new escape sequence
				}
			}
			break;

		case EParseState::InUTF16:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// UTF-16 sequences can only be up-to 4 digits long
					check(EscapedLiteralCharacter.Len() <= 4);
					if (EscapedLiteralCharacter.Len() == 4)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}

						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					if (EscapedLiteralCharacter.Len() > 0)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}
					}

					ParseState = EParseState::Idle;
					--CharPtr; // Walk backwards as we need to consider whether the current character is the start of a new escape sequence
				}
			}
			break;

		case EParseState::InUTF32:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// UTF-32 sequences can only be up-to 8 digits long
					check(EscapedLiteralCharacter.Len() <= 8);
					if (EscapedLiteralCharacter.Len() == 8)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoui64(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}

						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					if (EscapedLiteralCharacter.Len() > 0)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoui64(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}
					}

					ParseState = EParseState::Idle;
					--CharPtr; // Walk backwards as we need to consider whether the current character is the start of a new escape sequence
				}
			}
			break;

		default:
			break;
		}
	}

	return RetString.ReplaceEscapedCharWithChar();
}

FString UGatherTextFromSourceCommandlet::RemoveStringFromTextMacro(const FString& TextMacro, const FString& IdentForLogging, bool& Error)
{
	FString Text;
	Error = true;

	// need to strip text literal out of TextMacro ( format should be TEXT("stringvalue") ) 
	if (!TextMacro.StartsWith(FMacroDescriptor::TextMacroString))
	{
		Error = false;
		Text = TextMacro.TrimQuotes();
	}
	else
	{
		int32 OpenQuoteIdx = TextMacro.Find(TEXT("\""), ESearchCase::CaseSensitive);
		if (0 > OpenQuoteIdx || TextMacro.Len() - 1 == OpenQuoteIdx)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing quotes in %s"), *FLocTextHelper::SanitizeLogOutput(IdentForLogging));
		}
		else
		{
			int32 CloseQuoteIdx = TextMacro.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenQuoteIdx+1);
			if (0 > CloseQuoteIdx)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing quotes in %s"), *FLocTextHelper::SanitizeLogOutput(IdentForLogging));
			}
			else
			{
				Text = TextMacro.Mid(OpenQuoteIdx + 1, CloseQuoteIdx - OpenQuoteIdx - 1);
				Error = false;
			}
		}
	}

	if (!Error)
	{
		Text = UnescapeLiteralCharacterEscapeSequences(Text);
	}

	return Text;
}

FString UGatherTextFromSourceCommandlet::StripCommentsFromToken(const FString& InToken, FSourceFileParseContext& Context)
{
	check(!Context.WithinLineComment);
	check(!Context.WithinStringLiteral);

	// Remove both block and inline comments from the given token
	FString StrippedToken;
	StrippedToken.Reserve(InToken.Len());

	TCHAR WithinQuote = 0;
	bool bIgnoreNextQuote = false;
	for (const TCHAR* Char = *InToken; *Char; ++Char)
	{
		if (WithinQuote != 0)
		{
			StrippedToken += *Char;

			if (!bIgnoreNextQuote)
			{
				if (*Char == TEXT('\\'))
				{
					bIgnoreNextQuote = true;
					continue;
				}

				if (*Char == WithinQuote)
				{
					// Found an unescaped closing quote - we are no longer within quotes
					WithinQuote = 0;
				}
			}

			bIgnoreNextQuote = false;
		}
		else
		{
			if (*Char == TEXT('/'))
			{
				const TCHAR* NextChar = Char + 1;

				if (*NextChar == TEXT('/'))
				{
					// Found an inline quote - this strips the remainder of the string so just break out of the loop
					break;
				}

				if (*NextChar == TEXT('*'))
				{
					// Found a block comment - skip all characters until we find the closing quote
					Context.WithinBlockComment = true;
					++Char; // Skip over the opening slash, and the for loop will skip over the *
					continue;
				}
			}

			if (Context.WithinBlockComment)
			{
				if (*Char == TEXT('*'))
				{
					const TCHAR* NextChar = Char + 1;

					if (*NextChar == TEXT('/'))
					{
						// Found the end of a block comment
						Context.WithinBlockComment = false;
						++Char; // Skip over the opening *, and the for loop will skip over the slash
						continue;
					}
				}

				// Skip over all characters while within a block comment
				continue;
			}

			StrippedToken += *Char;

			if (*Char == TEXT('"') || *Char == TEXT('\''))
			{
				// We found an opening quote - keep track of it until we find a matching closing quote
				WithinQuote = *Char;
			}
		}
	}

	return StrippedToken.TrimStartAndEnd();
}

bool UGatherTextFromSourceCommandlet::ParseSourceText(const FString& Text, const TArray<FParsableDescriptor*>& Parsables, FSourceFileParseContext& ParseCtxt)
{
	// Cache array of parsables and tokens valid for this filetype
	TArray< FParsableDescriptor*> ParsablesForFile;
	TArray<FString> ParsableTokensForFile;
	for (FParsableDescriptor* Parsable : Parsables)
	{
		if (Parsable->MatchesFileTypes(ParseCtxt.FileTypes))
		{
			ParsablesForFile.Add(Parsable);
			ParsableTokensForFile.Add(Parsable->GetToken());
		}
	}
	check(ParsablesForFile.Num() == ParsableTokensForFile.Num());

	// Anything to parse for this filetype?
	if (ParsablesForFile.Num() == 0)
	{
		return true;
	}

	// Create array of ints, one for each parsable we're looking for.
	TArray<int32> ParsableMatchCountersForFile;
	ParsableMatchCountersForFile.AddZeroed(ParsablesForFile.Num());

	// Use the file extension to work out what comments look like for this file
	// We default to C++-style comments
	const TCHAR* LineComment = TEXT("//");
	const TCHAR* BlockCommentStart = TEXT("/*");
	const TCHAR* BlockCommentEnd = TEXT("*/");
	if (EnumHasAnyFlags(ParseCtxt.FileTypes, EGatherTextSourceFileTypes::Ini))
	{
		LineComment = TEXT(";");
		BlockCommentStart = nullptr;
		BlockCommentEnd = nullptr;
	}
	const int32 LineCommentLen = LineComment ? FCString::Strlen(LineComment) : 0;
	const int32 BlockCommentStartLen = BlockCommentStart ? FCString::Strlen(BlockCommentStart) : 0;
	const int32 BlockCommentEndLen = BlockCommentEnd ? FCString::Strlen(BlockCommentEnd) : 0;
	checkf((BlockCommentStartLen == 0 && BlockCommentEndLen == 0) || (BlockCommentStartLen > 0 && BlockCommentEndLen > 0), TEXT("Block comments require both a start and an end marker!"));

	// Split the file into lines
	TArray<FString>& TextLines = ParseCtxt.TextLines;
	Text.ParseIntoArrayLines(TextLines, false);

	// Move through the text lines looking for the tokens that denote the items in the Parsables list 
	for (int32 LineIdx = 0; LineIdx < TextLines.Num(); LineIdx++)
	{
		// Remove spaces at the end of the line.
		TextLines[LineIdx].TrimEndInline();
		const FString& Line = TextLines[LineIdx];
		if ( Line.IsEmpty() )
			continue;

		// Use these pending vars to defer parsing a token hit until longer tokens can't hit too
		int32 PendingParseIdx = INDEX_NONE;
		const TCHAR* ParsePoint = NULL;
		for (int32& Element : ParsableMatchCountersForFile)
		{
			Element = 0;
		}
		ParseCtxt.LineNumber = LineIdx + 1;
		ParseCtxt.LineText = Line;
		ParseCtxt.WithinLineComment = false;
		ParseCtxt.EndParsingCurrentLine = false;

		const TCHAR* Cursor = *Line;
		while (*Cursor && !ParseCtxt.EndParsingCurrentLine)
		{
			// Check if we're starting comments or string literals

			if (!ParseCtxt.WithinLineComment && !ParseCtxt.WithinBlockComment && !ParseCtxt.WithinStringLiteral)
			{
				// Detect that the line starts with a Line comment.
				if (LineCommentLen > 0 && FCString::Strncmp(Cursor, LineComment, LineCommentLen) == 0)
				{
					ParseCtxt.WithinLineComment = true;
					ParseCtxt.WithinStartingLine = *Line;
					ParseCtxt.EndParsingCurrentLine = true;
					Cursor += LineCommentLen;
					continue;
				}
				else if (BlockCommentStartLen > 0 && FCString::Strncmp(Cursor, BlockCommentStart, BlockCommentStartLen) == 0)
				{
					ParseCtxt.WithinBlockComment = true;
					ParseCtxt.WithinStartingLine = *Line;
					Cursor += BlockCommentStartLen;
					continue;
				}
			}

			// We are not in a comment (line or block) and we are not parsing a string.
			if (!ParseCtxt.WithinLineComment && !ParseCtxt.WithinBlockComment && !ParseCtxt.WithinStringLiteral)
			{
				if (*Cursor == TEXT('\"'))
				{
					if (Cursor == *Line)
					{
						ParseCtxt.WithinStringLiteral = true;
						ParseCtxt.WithinStartingLine = *Line;
						++Cursor;
						continue;
					}
					else if (Cursor > *Line)
					{
						const TCHAR* const ReverseCursor = Cursor - 1;
						if (EnumHasAnyFlags(ParseCtxt.FileTypes, EGatherTextSourceFileTypes::Cpp) && *ReverseCursor == TEXT('R'))
						{
							// Potentially a C++11 raw string literal, so walk forwards and validate that this looks legit
							// While doing this we can parse out its optional user defined delimiter so we can find when the string closes
							//   eg) For 'R"Delim(string)Delim"', ')Delim' would be the closing delimiter.
							//   eg) For 'R"(string)"', ')' would be the closing delimiter.
							ParseCtxt.RawStringLiteralClosingDelim = TEXT(")");
							{
								bool bIsValid = true;

								const TCHAR* ForwardCursor = Cursor + 1;
								for (;;)
								{
									const TCHAR DelimChar = *ForwardCursor++;
									if (DelimChar == TEXT('('))
									{
										break;
									}
									// We also permit '_' to support the use of _JSON as a delimiter for the raw strings 
									if (DelimChar == 0 || !(FChar::IsAlnum(DelimChar) || DelimChar == TCHAR('_')))
									{
										bIsValid = false;
										break;
									}
									ParseCtxt.RawStringLiteralClosingDelim += DelimChar;
								}

								if (bIsValid)
								{
									ParseCtxt.WithinStringLiteral = true;
									ParseCtxt.WithinStartingLine = *Line;
									Cursor = ForwardCursor;
									continue;
								}
								else
								{
									ParseCtxt.RawStringLiteralClosingDelim.Reset();
									// Fall through to the quoted string parsing below
								}
							}
						}
						
						if (*ReverseCursor != TEXT('\\') && *ReverseCursor != TEXT('\''))
						{
							ParseCtxt.WithinStringLiteral = true;
							ParseCtxt.WithinStartingLine = *Line;
							++Cursor;
							continue;
						}
						else 
						{
							bool IsEscaped = false;
							{
								//if the backslash or single quote is itself escaped then the quote is good
								const TCHAR* EscapeCursor = ReverseCursor;
								while (EscapeCursor > *Line && *(--EscapeCursor) == TEXT('\\'))
								{
									IsEscaped = !IsEscaped;
								}
							}

							if (IsEscaped)
							{
								ParseCtxt.WithinStringLiteral = true;
								ParseCtxt.WithinStartingLine = *Line;
								++Cursor;
								continue;
							}
							else
							{
								//   check for '"'
								const TCHAR* const ForwardCursor = Cursor + 1;
								if (*ReverseCursor == TEXT('\'') && *ForwardCursor != TEXT('\''))
								{
									ParseCtxt.WithinStringLiteral = true;
									ParseCtxt.WithinStartingLine = *Line;
									++Cursor;
									continue;
								}
							}
						}
					}
				}
			}
			else if (ParseCtxt.WithinStringLiteral)
			{
				if (*Cursor == TEXT('\"'))
				{
					if (Cursor == *Line && ParseCtxt.RawStringLiteralClosingDelim.IsEmpty())
					{
						ParseCtxt.WithinStringLiteral = false;
						++Cursor;
						continue;
					}
					else if (Cursor > *Line)
					{
						// Is this ending a C++11 raw string literal?
						if (!ParseCtxt.RawStringLiteralClosingDelim.IsEmpty())
						{
							const TCHAR* EndDelimCursor = Cursor - ParseCtxt.RawStringLiteralClosingDelim.Len();
							if (EndDelimCursor >= *Line && FCString::Strncmp(EndDelimCursor, *ParseCtxt.RawStringLiteralClosingDelim, ParseCtxt.RawStringLiteralClosingDelim.Len()) == 0)
							{
								ParseCtxt.RawStringLiteralClosingDelim.Reset();
								ParseCtxt.WithinStringLiteral = false;
							}
							++Cursor;
							continue;
						}

						const TCHAR* const ReverseCursor = Cursor - 1;
						if (*ReverseCursor != TEXT('\\') && *ReverseCursor != TEXT('\''))
						{
							ParseCtxt.WithinStringLiteral = false;
							++Cursor;
							continue;
						}
						else
						{
							bool IsEscaped = false;
							{
								//if the backslash or single quote is itself escaped then the quote is good
								const TCHAR* EscapeCursor = ReverseCursor;
								while (EscapeCursor > *Line && *(--EscapeCursor) == TEXT('\\'))
								{
									IsEscaped = !IsEscaped;
								}
							}

							if (IsEscaped)
							{
								ParseCtxt.WithinStringLiteral = false;
								++Cursor;
								continue;
							}
							else
							{
								//   check for '"'
								const TCHAR* const ForwardCursor = Cursor + 1;
								if (*ReverseCursor == TEXT('\'') && *ForwardCursor != TEXT('\''))
								{
									ParseCtxt.WithinStringLiteral = false;
									++Cursor;
									continue;
								}
							}
						}
					}
				}
			}

			// Check if we're ending comments
			if (ParseCtxt.WithinBlockComment && BlockCommentEndLen > 0 && FCString::Strncmp(Cursor, BlockCommentEnd, BlockCommentEndLen) == 0)
			{
				ParseCtxt.WithinBlockComment = false;
				Cursor += BlockCommentEndLen;
				continue;
			}

			// Go through all the Parserables to find matches
			for (int32 ParIdx = 0; ParIdx < ParsablesForFile.Num(); ++ParIdx)
			{
				FParsableDescriptor* Parsable = ParsablesForFile[ParIdx];
				const FString& Token = ParsableTokensForFile[ParIdx];

				if (Token.Len() == ParsableMatchCountersForFile[ParIdx])
				{
					// already seen this entire token and are looking for longer matches - skip it
					continue;
				}

				if (*Cursor == Token[ParsableMatchCountersForFile[ParIdx]])
				{
					// Char at cursor matches the next char in the parsable's identifying token
					if (Token.Len() == ++(ParsableMatchCountersForFile[ParIdx]))
					{
						// don't immediately parse - this parsable has seen its entire token but a longer one could be about to hit too
						const TCHAR* TokenStart = Cursor + 1 - Token.Len();
						if (0 > PendingParseIdx || ParsePoint >= TokenStart)
						{
							PendingParseIdx = ParIdx;
							ParsePoint = TokenStart;
						}
					}
				}
				else
				{
					// Char at cursor doesn't match the next char in the parsable's identifying token
					// Reset the counter to start of the token
					ParsableMatchCountersForFile[ParIdx] = 0;
				}
			}

			// Now check PendingParse and only run it if there are no better candidates
			if (PendingParseIdx != INDEX_NONE)
			{
				FParsableDescriptor* PendingParsable = ParsablesForFile[PendingParseIdx];

				bool MustDefer = false; // pending will be deferred if another parsable has a equal and greater number of matched chars
				if (!PendingParsable->OverridesLongerTokens())
				{
					for (int32 ParIdx = 0; ParIdx < ParsablesForFile.Num(); ++ParIdx)
					{
						if (PendingParseIdx != ParIdx && ParsableMatchCountersForFile[ParIdx] >= ParsableTokensForFile[PendingParseIdx].Len())
						{
							// a longer token is matching so defer
							MustDefer = true;
						}
					}
				}

				if (!MustDefer)
				{
					// Do the parse now
					// TODO: Would be nice if TryParse returned what it consumed, and operated on const TCHAR*
					PendingParsable->TryParse(FString(ParsePoint), ParseCtxt);
					for (int32& Element : ParsableMatchCountersForFile)
					{
						Element = 0;
					}
					PendingParseIdx = INDEX_NONE;
					ParsePoint = NULL;
				}
			}

			// Advance cursor
			++Cursor;
		}

		// Handle a string literal that went beyond a single line
		if (ParseCtxt.WithinStringLiteral)
		{
			if (EnumHasAnyFlags(ParseCtxt.FileTypes, EGatherTextSourceFileTypes::Ini))
			{
				// INI files don't support multi-line literals; always terminate them after ending a line
				ParseCtxt.WithinStringLiteral = false;
			}
			else if (Cursor > *Line && ParseCtxt.RawStringLiteralClosingDelim.IsEmpty())
			{
				// C++ only allows multi-line literals if they're escaped with a trailing slash or within a C++11 raw string literal
				ParseCtxt.WithinStringLiteral = *(Cursor - 1) == TEXT('\\');
			}

			UE_CLOG(!ParseCtxt.WithinStringLiteral, LogGatherTextFromSourceCommandlet, Warning, TEXT("A string literal was not correctly terminated. File %s at line %d, starting line: %s"), *ParseCtxt.Filename, ParseCtxt.LineNumber, ParseCtxt.WithinStartingLine);
		}
	}

	// Handle a string C++11 raw string literal that was never closed as this is likely a false positive that needs to be fixed in the parser
	if (ParseCtxt.WithinStringLiteral && !ParseCtxt.RawStringLiteralClosingDelim.IsEmpty())
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%s): A C++11 raw string literal was not correctly terminated."), *ParseCtxt.Filename, ParseCtxt.WithinStartingLine);
	}

	return true;
}

int32 UGatherTextFromSourceCommandlet::FMacroArgumentGatherer::GetNumberOfArguments() const 
{
	return Args.Num();
}


bool UGatherTextFromSourceCommandlet::FMacroArgumentGatherer::EndArgument()
{
	if (CurrentArgument.IsEmpty() || IsInDoubleQuotes())
	{
		return false;
	}
	Args.Push(CurrentArgument);
	CurrentArgument.Empty();
	return true;
}

bool UGatherTextFromSourceCommandlet::FMacroArgumentGatherer::Gather(const TCHAR* Arg, int32 Count)
{
	if(Arg == nullptr || Count == 0)
	{
		return false;
	}
	FString NewArgument = FString(Count, Arg);
	NewArgument.TrimEndInline();

	int32 CurrentArgLen = CurrentArgument.Len();
	if (CurrentArgLen == 0)
	{
		CurrentArgument = NewArgument;
		return true;
	}
	
	// If the last argument is a string that ends with " and the new argument too we append it
	// This is to support this and remove duplicate double quotes.
	// MYMACRO("Very long string\n"
	//         "Next part of the very long string\n");
	// And
	// MYMACRO("Very long string\n" \
	//         "Next part of the very long string\n");
	if (NewArgument[0] == '\"' && CurrentArgument[CurrentArgLen - 1] == '\"')
	{
		Arg++;
		CurrentArgLen--;
		CurrentArgument.RemoveAt(CurrentArgLen);
		Count--;
	}

	CurrentArgument.Append(Arg, Count);
	CurrentArgument.TrimEndInline();
	return true;
}
	

void UGatherTextFromSourceCommandlet::FMacroArgumentGatherer::ExtractArguments(TArray<FString>& Arguments)
{
	Arguments = Args;
	Args.Empty();
}

bool UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddManifestText( const FString& Token, const FString& InNamespace, const FString& SourceText, const FManifestContext& Context )
{
	const bool bIsEditorOnly = EvaluateEditorOnlyDefineState() == EEditorOnlyDefineState::Defined;

	if (!bIsEditorOnly || ShouldGatherFromEditorOnlyData)
	{
		const FString EntryDescription = FString::Printf(TEXT("%s macro"), *Token);
		return OwnerCommandlet->GatherManifestHelper->AddSourceText(InNamespace, FLocItem(SourceText), Context, &EntryDescription);
	}

	return false;
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::PushMacroBlock( const FString& InBlockCtx )
{
	MacroBlockStack.Push(InBlockCtx);
	CachedEditorOnlyDefineState.Reset();
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::PopMacroBlock()
{
	if (MacroBlockStack.Num() > 0)
	{
		MacroBlockStack.Pop(EAllowShrinking::No);
		CachedEditorOnlyDefineState.Reset();
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::FlushMacroStack()
{
	MacroBlockStack.Reset();
	CachedEditorOnlyDefineState.Reset();
}

UGatherTextFromSourceCommandlet::EEditorOnlyDefineState UGatherTextFromSourceCommandlet::FSourceFileParseContext::EvaluateEditorOnlyDefineState() const
{
	if (CachedEditorOnlyDefineState.IsSet())
	{
		return CachedEditorOnlyDefineState.GetValue();
	}

	static const FString WithEditorString = TEXT("WITH_EDITOR");
	static const FString WithEditorOnlyDataString = TEXT("WITH_EDITORONLY_DATA");

	CachedEditorOnlyDefineState = EEditorOnlyDefineState::Undefined;
	for (const FString& BlockCtx : MacroBlockStack)
	{
		if (BlockCtx.Equals(WithEditorString, ESearchCase::CaseSensitive) || BlockCtx.Equals(WithEditorOnlyDataString, ESearchCase::CaseSensitive))
		{
			CachedEditorOnlyDefineState = EEditorOnlyDefineState::Defined;
			break;
		}
	}

	return CachedEditorOnlyDefineState.GetValue();
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::SetDefine(const FString& InDefineCtx)
{
	static const FString LocDefRegionString = TEXT("LOC_DEFINE_REGION");
	static const FString LocNamespaceString = TEXT("LOCTEXT_NAMESPACE");

	if (InDefineCtx.Equals(LocDefRegionString, ESearchCase::CaseSensitive))
	{
		// #define LOC_DEFINE_REGION
		if (ExcludedRegion)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Found a '#define LOC_DEFINE_REGION' within another '#define LOC_DEFINE_REGION'"), *Filename, LineNumber);
		}
		else
		{
			ExcludedRegion = true;
		}
		return;
	}
	else if (!ExcludedRegion)
	{
		if (InDefineCtx.StartsWith(LocNamespaceString, ESearchCase::CaseSensitive) && InDefineCtx.IsValidIndex(LocNamespaceString.Len()) && (FText::IsWhitespace(InDefineCtx[LocNamespaceString.Len()]) || InDefineCtx[LocNamespaceString.Len()] == TEXT('"')))
		{
			// #define LOCTEXT_NAMESPACE <namespace>
			if (WithinNamespaceDefineLineNumber != INDEX_NONE)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Found a '#define LOCTEXT_NAMESPACE' within another '#define LOCTEXT_NAMESPACE'"), *Filename, LineNumber);
			}
			else
			{
				FString RemainingText = InDefineCtx.RightChop(LocNamespaceString.Len()).TrimStart();

				bool RemoveStringError;
				const FString DefineDesc = FString::Printf(TEXT("%s define at %s:%d"), *RemainingText, *Filename, LineNumber);
				FString NewNamespace = RemoveStringFromTextMacro(RemainingText, DefineDesc, RemoveStringError);

				if (!RemoveStringError)
				{
					Namespace = MoveTemp(NewNamespace);
					WithinNamespaceDefineLineNumber = LineNumber;
				}
			}
			return;
		}
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::RemoveDefine(const FString& InDefineCtx)
{
	static const FString LocDefRegionString = TEXT("LOC_DEFINE_REGION");
	static const FString LocNamespaceString = TEXT("LOCTEXT_NAMESPACE");

	if (InDefineCtx.Equals(LocDefRegionString, ESearchCase::CaseSensitive))
	{
		// #undef LOC_DEFINE_REGION
		if (!ExcludedRegion)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Found an '#undef LOC_DEFINE_REGION' without a corresponding '#define LOC_DEFINE_REGION'"), *Filename, LineNumber);
		}
		else
		{
			ExcludedRegion = false;
		}
		return;
	}
	else if (!ExcludedRegion)
	{
		if (InDefineCtx.Equals(LocNamespaceString, ESearchCase::CaseSensitive))
		{
			// #undef LOCTEXT_NAMESPACE
			if (WithinNamespaceDefineLineNumber == INDEX_NONE)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Found an '#undef LOCTEXT_NAMESPACE' without a corresponding '#define LOCTEXT_NAMESPACE'"), *Filename, LineNumber);
			}
			else
			{
				Namespace.Empty();
				WithinNamespaceDefineLineNumber = INDEX_NONE;
			}
			return;
		}
	}
}

bool UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableImpl(const FName InTableId, const FString& InTableNamespace)
{
	// String table entries may be parsed before the string table itself (due to code ordering), so only warn about duplication here if we've already got a source location for the string table (as adding entries doesn't set that)
	FParsedStringTable& ParsedStringTable = ParsedStringTables.FindOrAdd(InTableId);
	if (ParsedStringTable.SourceLocation.Line != INDEX_NONE)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: String table with ID \"%s\" was already parsed at \"%s\". Ignoring additional definition."), *FSourceLocation(Filename, LineNumber).ToString(), *InTableId.ToString(), *ParsedStringTable.SourceLocation.ToString());
		return false;
	}

	ParsedStringTable.TableNamespace = InTableNamespace;
	ParsedStringTable.SourceLocation = FSourceLocation(Filename, LineNumber);
	return true;
}

bool UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableEntryImpl(const FName InTableId, const FString& InKey, const FString& InSourceString, const FSourceLocation& InSourceLocation, const FName InPlatformName)
{
	const bool bIsEditorOnly = EvaluateEditorOnlyDefineState() == EEditorOnlyDefineState::Defined;

	// String table entries may be parsed before the string table itself (due to code ordering), so we may need to add our string table below
	FParsedStringTable& ParsedStringTable = ParsedStringTables.FindOrAdd(InTableId);

	FParsedStringTableEntry* ExistingEntry = ParsedStringTable.TableEntries.Find(InKey);
	if (ExistingEntry)
	{
		if (ExistingEntry->SourceString.Equals(InSourceString, ESearchCase::CaseSensitive))
		{
			ExistingEntry->bIsEditorOnly &= bIsEditorOnly;
			return true;
		}
		else
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: String table entry with ID \"%s\" and key \"%s\" was already parsed at \"%s\". Ignoring additional definition."), *FSourceLocation(Filename, LineNumber).ToString(), *InTableId.ToString(), *InKey, *ExistingEntry->SourceLocation.ToString());
			return false;
		}
	}
	else
	{
		FParsedStringTableEntry& ParsedStringTableEntry = ParsedStringTable.TableEntries.Add(InKey);
		ParsedStringTableEntry.SourceString = InSourceString;
		ParsedStringTableEntry.SourceLocation = InSourceLocation;
		ParsedStringTableEntry.PlatformName = InPlatformName;
		ParsedStringTableEntry.bIsEditorOnly = bIsEditorOnly;
		return true;
	}
}

bool UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableEntryMetaDataImpl(const FName InTableId, const FString& InKey, const FName InMetaDataId, const FString& InMetaData, const FSourceLocation& InSourceLocation)
{
	const bool bIsEditorOnly = EvaluateEditorOnlyDefineState() == EEditorOnlyDefineState::Defined;

	// String table meta-data may be parsed before the string table itself (due to code ordering), so we may need to add our string table below
	FParsedStringTable& ParsedStringTable = ParsedStringTables.FindOrAdd(InTableId);
	FParsedStringTableEntryMetaDataMap& MetaDataMap = ParsedStringTable.MetaDataEntries.FindOrAdd(InKey);

	FParsedStringTableEntryMetaData* ExistingMetaData = MetaDataMap.Find(InMetaDataId);
	if (ExistingMetaData)
	{
		if (ExistingMetaData->MetaData.Equals(InMetaData, ESearchCase::CaseSensitive))
		{
			ExistingMetaData->bIsEditorOnly &= bIsEditorOnly;
			return true;
		}
		else
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: String table entry meta-data with ID \"%s\" and key \"%s\" was already parsed at \"%s\". Ignoring additional definition."), *FSourceLocation(Filename, LineNumber).ToString(), *InTableId.ToString(), *InKey, *ExistingMetaData->SourceLocation.ToString());
			return false;
		}
	}
	else
	{
		FParsedStringTableEntryMetaData& ParsedMetaData = MetaDataMap.Add(InMetaDataId);
		ParsedMetaData.MetaData = InMetaData;
		ParsedMetaData.SourceLocation = InSourceLocation;
		ParsedMetaData.bIsEditorOnly = bIsEditorOnly;
		return true;
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTable(const FName InTableId, const FString& InTableNamespace)
{
	AddStringTableImpl(InTableId, InTableNamespace);
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableFromFile(const FName InTableId, const FString& InTableNamespace, const FString& InTableFilename, const FString& InRootPath)
{
	if (AddStringTableImpl(InTableId, InTableNamespace))
	{
		const FString FullImportPath = InRootPath / InTableFilename;

		FStringTableRef TmpStringTable = FStringTable::NewStringTable();
		if (TmpStringTable->ImportStrings(FullImportPath))
		{
			const FSourceLocation SourceLocation = FSourceLocation(InTableFilename, INDEX_NONE);
			const FName TablePlatformName = OwnerCommandlet->GetSplitPlatformNameFromPath(InTableFilename);

			TmpStringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString)
			{
				AddStringTableEntryImpl(InTableId, InKey, InSourceString, SourceLocation, TablePlatformName);

				TmpStringTable->EnumerateMetaData(InKey, [&](const FName InMetaDataId, const FString& InMetaData)
				{
					AddStringTableEntryMetaDataImpl(InTableId, InKey, InMetaDataId, InMetaData, SourceLocation);
					return true; // continue enumeration
				});

				return true; // continue enumeration
			});
		}
		else
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: String table with ID \"%s\" failed to import strings from \"%s\"."), *FSourceLocation(Filename, LineNumber).ToString(), *InTableId.ToString(), *FullImportPath);
		}
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableEntry(const FName InTableId, const FString& InKey, const FString& InSourceString)
{
	AddStringTableEntryImpl(InTableId, InKey, InSourceString, FSourceLocation(Filename, LineNumber), FilePlatformName);
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddStringTableEntryMetaData(const FName InTableId, const FString& InKey, const FName InMetaDataId, const FString& InMetaData)
{
	AddStringTableEntryMetaDataImpl(InTableId, InKey, InMetaDataId, InMetaData, FSourceLocation(Filename, LineNumber));
}

void UGatherTextFromSourceCommandlet::FDefineDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #define <defname>
	//  or
	// #define <defname> <value>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).TrimStart();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.SetDefine(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FUndefDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #undef <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).TrimStart();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.RemoveDefine(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #if <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).TrimStart();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		// Handle "#if defined <defname>"
		if (RemainingText.StartsWith(DefinedString, ESearchCase::CaseSensitive))
		{
			RemainingText.RightChopInline(DefinedString.Len(), EAllowShrinking::No);
			RemainingText.TrimStartInline();
		}

		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FIfDefDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #ifdef <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).TrimStart();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FElIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #elif <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).TrimStart();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		// Handle "#elif defined <defname>"
		if (RemainingText.StartsWith(DefinedString, ESearchCase::CaseSensitive))
		{
			RemainingText.RightChopInline(DefinedString.Len(), EAllowShrinking::No);
			RemainingText.TrimStartInline();
		}

		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FElseDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #else

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.PushMacroBlock(FString());
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FEndIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #endif

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.EndParsingCurrentLine = true;
	}
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::ParseArgumentString(const FString& Text, const int32 CursorOffset, int32& BracketStack, const FSourceFileParseContext& Context, FMacroArgumentGatherer& ArgsGatherer) const
{
	bool bEscapeNextChar = false;

	const TCHAR* ArgStart = *Text + CursorOffset;
	const TCHAR* Cursor = ArgStart;
	for (; 0 < BracketStack; ++Cursor)
	{
		// First: check is we are at end of line.
		if ('\0' == *Cursor)
		{
			if (Cursor - ArgStart > 0)
			{
				// Here, we found the end of the line, but we don't know if it's the end of the argument.
				if (!ArgsGatherer.Gather(ArgStart, UE_PTRDIFF_TO_INT32(Cursor - ArgStart)))
				{
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
					return false;
				}
			}
			break;
		}
		// Skip this character
		else if (bEscapeNextChar)
		{
			bEscapeNextChar = false;
		}
		else if ((ArgsGatherer.IsInDoubleQuotes() || ArgsGatherer.IsInSingleQuotes()) && !bEscapeNextChar && '\\' == *Cursor)
		{
			bEscapeNextChar = true;

			// If we hit the escape character, we must verify if we are at end of line
			if ('\0' == *(Cursor + 1))
			{
				if (Cursor - ArgStart - 1 > 0)
				{
					if (!ArgsGatherer.Gather(ArgStart, UE_PTRDIFF_TO_INT32(Cursor - ArgStart) - 1))
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
						return false;
					}
				}
				break;
			}
		}
		// We are closing "
		else if (ArgsGatherer.IsInDoubleQuotes())
		{
			if ('\"' == *Cursor)
			{
				ArgsGatherer.CloseDoubleQuotes();
			}
		}
		// We are closing a '
		else if (ArgsGatherer.IsInSingleQuotes())
		{
			if ('\'' == *Cursor)
			{
				ArgsGatherer.CloseSingleQuotes();
			}
		}
		// We are opening a "
		else if ('\"' == *Cursor)
		{
			ArgsGatherer.OpenDoubleQuotes();
		}
		// We are opening a '
		else if ('\'' == *Cursor)
		{
			ArgsGatherer.OpenSingleQuotes();
		}
		// We found an opening bracket '(', increment the stack
		else if ('(' == *Cursor)
		{
			++BracketStack;
		}
		// We found the closing bracket ')' decrement the stack
		else if (')' == *Cursor)
		{
			--BracketStack;

			if (0 > BracketStack)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Unexpected bracket ')' in %s macro while parsing. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return false;
			}
		}		
		else if (',' == *Cursor)
		{
			if (1 == BracketStack)
			{
				if (Cursor - ArgStart > 0)
				{
					// We have a single bracket open and we found a ',' this is the end of the argument. If Bracket stack is > 1 it means that we are in a function call in one of the parameters.
					if (!ArgsGatherer.Gather(ArgStart, UE_PTRDIFF_TO_INT32(Cursor - ArgStart)))
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
						return false;
					}
					if (!ArgsGatherer.EndArgument())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
						return false;
					}

					ArgStart = Cursor + 1;
				}
				else
				{
					// The ',' character is the first thing in the line. We need to close out the previous argument 
					// E.g MYMACRO(Param1, "Param2"
					//	    , "Param3")
					if (!ArgsGatherer.EndArgument())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
						return false;
					}
					ArgStart = Cursor + 1;
				}
			}
		}
		else if ('\\' == *Cursor)
		{
			ensure(!ArgsGatherer.IsInDoubleQuotes());
			// We hit a escape character outside a double quote, if the next character is end of line, we are done for this line and the next one must start with a double quote
			if ('\0' == *(Cursor + 1))
			{
				if (Cursor - ArgStart - 1 > 0)
				{
					if (!ArgsGatherer.Gather(ArgStart, UE_PTRDIFF_TO_INT32(Cursor - ArgStart) - 1))
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
						return false;
					}
				}
				break;
			}
		}

		// We just closed the last ')' this is the end of all args for this macro 
		if (0 == BracketStack)
		{
			// If the arg is empty it means we found a closing braket after a ',' or at the begining of a line			
			if (Cursor - ArgStart > 0)
			{
				if (!ArgsGatherer.Gather(ArgStart, UE_PTRDIFF_TO_INT32(Cursor - ArgStart)))
				{
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
					return false;
				}
			}
			if (!ArgsGatherer.EndArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Parsing Arguments failed in %s macro. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return false;
			}
			break;
		}
	}
	return true;
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::ParseArgsFromMacro(const FString& Text, TArray<FString>& Args, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// NAME(param0, param1, param2, etc)

	// Step over the token name and any whitespace after it
	FString RemainingText = Text.RightChop(GetToken().Len());

	//RemainingText could be empty if the bracket is at the begining of the next line
	RemainingText.TrimStartInline();

	// Find the Opening bracket.
	const int32 OpenBracketIdx = RemainingText.Find(TEXT("("), ESearchCase::CaseSensitive);

	// If we find a bracket it must be the first character of the remaining text
	if (OpenBracketIdx > 0)
	{
		// We stepped over the whitespace when building RemainingText, so if the bracket isn't the first character in the text then it means we only partially matched a longer token and shouldn't parse it
		return false;
	}

	Args.Empty();

	FMacroArgumentGatherer ArgumentGatherer;
	int32 BracketStack = OpenBracketIdx == INDEX_NONE ? 0:1;

	// If we found a bracket, we can start parsing argument on this line
	if (BracketStack > 0)
	{
		// Parse the argument that are on the same line as the macro.
		if (!ParseArgumentString(RemainingText, OpenBracketIdx + 1, BracketStack, Context, ArgumentGatherer))
		{
			return false;
		}
	}

	// We didn't find the end bracket, we must continue.
	if (BracketStack > 0)
	{
		if (!ParseArgsFromNextLines(ArgumentGatherer, BracketStack, Context))
		{
			return false;
		}
	}

	ArgumentGatherer.ExtractArguments(Args);

	return 0 < Args.Num() ? true : false;
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::ParseArgsFromNextLines(FMacroArgumentGatherer& ArgsGatherer, int32& BracketStack, FSourceFileParseContext& Context) const
{
	// Loop until we have all arguments and the closing bracket.
	for (int32 i = Context.LineNumber; ArgsGatherer.GetNumberOfArguments() < GetMinNumberOfArgument() || BracketStack > 0; i++)
	{
		if(i >= Context.TextLines.Num())
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): We reached end of file while parsing specified %s macro for arguments. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			return false;
		}

		FString LineText = StripCommentsFromToken(Context.TextLines[i], Context);
		// Ignore empty lines.
		if (LineText.IsEmpty())
		{
			continue;
		}

		int32 ParsingOffset = 0;

		// We are not in an opening bracket yet, look at the beginning of the line.
		if (BracketStack == 0)
		{
			int32 OpenBracketIdx = LineText.Find(TEXT("("), ESearchCase::CaseSensitive);
			// We did not find the opening bracket on the first line and on the second non empty line, we give up
			if (OpenBracketIdx == INDEX_NONE)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Opening bracket '(' not found while parsing specified %s macro for arguments. %s"), *Context.Filename, Context.LineNumber, *GetToken(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return false;
			}
			BracketStack = 1;
			// Start parsing right after the bracket
			ParsingOffset = OpenBracketIdx + 1;
		}
		
		if (!ParseArgumentString(LineText, ParsingOffset, BracketStack, Context, ArgsGatherer))
		{
			return false;
		}	
	}
	return true;
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::PrepareArgument(FString& Argument, bool IsAutoText, const FString& IdentForLogging, bool& OutHasQuotes)
{
	bool Error = false;
	if (!IsAutoText)
	{
		Argument = RemoveStringFromTextMacro(Argument, IdentForLogging, Error);
		OutHasQuotes = Error ? false : true;
	}
	else
	{
		Argument = Argument.TrimEnd().TrimQuotes(&OutHasQuotes);
		Argument = UnescapeLiteralCharacterEscapeSequences(Argument);
	}
	return Error ? false : true;
}

void UGatherTextFromSourceCommandlet::FUICommandMacroDescriptor::TryParseArgs(const FString& Text, FSourceFileParseContext& Context, const TArray<FString>& Arguments, const int32 ArgIndexOffset) const
{
	FString Identifier = Arguments[ArgIndexOffset];
	Identifier.TrimStartInline(); // Remove whitespaces at the start of the line.

	// Identifier may optionally be in quotes, as it's sometimes a string literal (in UE_COMMAND_EXT), and sometimes stringified by the macro (in UI_COMMAND)
	// Because this is optional, we don't care if this processing fails
	bool HasQuotes = false;
	PrepareArgument(Identifier, true, FString(), HasQuotes);

	FString SourceLocation = FSourceLocation(Context.Filename, Context.LineNumber).ToString();
	if (Identifier.IsEmpty())
	{
		//The command doesn't have an identifier so we can't gather it
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has an empty identifier and cannot be gathered."), *SourceLocation , *GetToken());
		return;
	}

	FString SourceText = Arguments[ArgIndexOffset + 1];
	SourceText.TrimStartInline();

	static const FString UICommandRootNamespace = TEXT("UICommands");
	FString Namespace = Context.WithinNamespaceDefineLineNumber != INDEX_NONE && !Context.Namespace.IsEmpty() ? FString::Printf(TEXT("%s.%s"), *UICommandRootNamespace, *Context.Namespace) : UICommandRootNamespace;

	// parse DefaultLangString argument - this arg will be in quotes without TEXT macro
	FString MacroDesc = FString::Printf(TEXT("%s(%d): \"FriendlyName\" argument in %s macro."), *Context.Filename, Context.LineNumber, *GetToken());
	if (PrepareArgument(SourceText, true, MacroDesc, HasQuotes))
	{
		if (HasQuotes && !Identifier.IsEmpty() && !SourceText.IsEmpty())
		{
			// First create the command entry
			FManifestContext CommandContext;
			CommandContext.Key = Identifier;
			CommandContext.SourceLocation = SourceLocation;
			CommandContext.PlatformName = Context.FilePlatformName;

			Context.AddManifestText(GetToken(), Namespace, SourceText, CommandContext);

			// parse DefaultLangTooltipString argument - this arg will be in quotes without TEXT macro
			FString TooltipSourceText = Arguments[ArgIndexOffset + 2];
			TooltipSourceText.TrimStartInline();
			MacroDesc = FString::Printf(TEXT("%s(%d): \"InDescription\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
			if (PrepareArgument(TooltipSourceText, true, MacroDesc, HasQuotes))
			{
				if (HasQuotes && !TooltipSourceText.IsEmpty())
				{
					// Create the tooltip entry
					FManifestContext CommandTooltipContext;
					CommandTooltipContext.Key = Identifier + TEXT("_ToolTip");
					CommandTooltipContext.SourceLocation = SourceLocation;
					CommandTooltipContext.PlatformName = CommandContext.PlatformName;

					Context.AddManifestText(GetToken(), Namespace, TooltipSourceText, CommandTooltipContext);
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FUICommandMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// UI_COMMAND(LocKey, DefaultLangString, DefaultLangTooltipString, ...)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			// Validate that we got the rightnumber of Arguments
			if (Arguments.Num() < GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected at least %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return;
			}
			
			// Parse all arguments found
			TryParseArgs(Text, Context, Arguments, 0);
		}
	}
}

void UGatherTextFromSourceCommandlet::FUICommandExtMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// UI_COMMAND_EXT(<IgnoredParam>, <IgnoredParam>, LocKey, DefaultLangString, DefaultLangTooltipString, ...)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			// Need at least 5 arguments
			if (Arguments.Num() < GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected at least %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return;
			}
			TryParseArgs(Text, Context, Arguments, 2);
		}
	}
}

void UGatherTextFromSourceCommandlet::FMetasoundParamMacroDescriptor::TryParseArgs(const FString& Text, FSourceFileParseContext& Context, const TArray<FString>& Arguments, const int32 ArgIndexOffset) const
{
	FString NameArgument = Arguments[ArgIndexOffset];
	// Remove whitespace at the start of the line.
	NameArgument.TrimStartInline();

	// NAME should neverbe be in quotes 
	bool HasQuotes = false;
	FString MacroDesc = FString::Printf(TEXT("%s(%d): \"InDescription\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
	FString SourceLocation = FSourceLocation(Context.Filename, Context.LineNumber).ToString();
	if (PrepareArgument(NameArgument, true, MacroDesc, HasQuotes))
	{
		if (HasQuotes)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has NAME argument enclosed in double quotes and cannot be gathered. Please make sure the argument is not enclosed in double quotes."), *SourceLocation, *GetToken());
			return;
		}
		else if (NameArgument.IsEmpty())
		{
			//The metasound param does not have a NAME so we cannot gather it 
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has an empty NAME argument and cannot be gathered."), *SourceLocation, *GetToken());
			return;
		}
	}

	FString NameText= Arguments[ArgIndexOffset + 1];
	// Remove whitespace at the start of the line.
	NameText.TrimStartInline();

	// NAME_TEXT should always be in quotes 
	HasQuotes = false;
	if (PrepareArgument(NameText, true, MacroDesc, HasQuotes))
	{
		if (!HasQuotes)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has NAME_TEXT argument not enclosed in double quotes and cannot be gathered. Please make sure the argument is enclosed in double quotes."), *SourceLocation, *GetToken());
			return;
		}
		else if (NameText.IsEmpty())
		{
			//The metasound param does not have a NAME_TEXT so we cannot gather it 
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has an empty NAME_TEXT argumentand cannot be gathered."), *SourceLocation, *GetToken());
			return;
		}
	}

	// parse TOOLTIP_TEXT argument- this arg will be in quotes without TEXT macro
	FString TooltipSourceText = Arguments[ArgIndexOffset + 2];
	TooltipSourceText.TrimStartInline();
	MacroDesc = FString::Printf(TEXT("%s(%d): \"InDescription\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
	if (PrepareArgument(TooltipSourceText, true, MacroDesc, HasQuotes))
	{
		if (!HasQuotes)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has a TOOLTIP_TEXT argument not enclosed in double quotes and cannot be gathered. Please ensure the argument is enclosed in double quotes."), *SourceLocation, *GetToken());
			return;
		}
		else if (TooltipSourceText.IsEmpty())
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has an empty TOOLTIP_TEXT and cannot be gathered."), *SourceLocation, *GetToken());
			return;
		}
		else
		{
			// All Metasound loc data is editor only 
			static const FString WithEditorString = TEXT("WITH_EDITOR");

			// Create the tooltip entry first 
			FManifestContext MetasoundArgumentContext;
			MetasoundArgumentContext.Key = NameArgument + TEXT("ToolTip");
			MetasoundArgumentContext.SourceLocation = SourceLocation;
			MetasoundArgumentContext.PlatformName = Context.FilePlatformName;

			// We manually append to the macro stack  for editor only due to the nature of METASOUND_PARAM
			Context.PushMacroBlock(WithEditorString);
			Context.AddManifestText(GetToken(), Context.Namespace, TooltipSourceText, MetasoundArgumentContext);

			// now add the display name entry. We can recycle a lot of the values used for tooltips  
			MetasoundArgumentContext.Key = NameArgument + TEXT("DisplayName");
			Context.AddManifestText(GetToken(), Context.Namespace, NameText, MetasoundArgumentContext);

			// Now we pop the editor only macro 
			Context.PopMacroBlock();
		}
	}
}

void UGatherTextFromSourceCommandlet::FMetasoundParamMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// METASOUND_PARAM(NAME, NAME_TEXT, TOOLTIP_TEXT)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			// Validate that we got the rightnumber of Arguments
			if (Arguments.Num() < GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected at least %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return;
			}

			TryParseArgs(Text, Context, Arguments, 0);
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// MACRONAME(param0, param1, param2)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> ArgArray;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), ArgArray, Context))
		{
			int32 NumArgs = ArgArray.Num();

			if (NumArgs != Arguments.Num())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, Arguments.Num(), *GetToken(), NumArgs, *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			}
			else
			{
				FString Identifier;
				FString SourceLocation = FSourceLocation(Context.Filename, Context.LineNumber).ToString();
				FString SourceText;

				TOptional<FString> Namespace;
				if (Context.WithinNamespaceDefineLineNumber != INDEX_NONE || !Context.Namespace.IsEmpty())
				{
					Namespace = Context.Namespace;
				}

				bool ArgParseError = false;
				for (int32 ArgIdx=0; ArgIdx<Arguments.Num(); ArgIdx++)
				{
					FMacroArg Arg = Arguments[ArgIdx];
					ArgArray[ArgIdx].TrimStartInline();
					FString ArgText = ArgArray[ArgIdx];

					bool HasQuotes;
					FString MacroDesc = FString::Printf(TEXT("%s(%d): argument %d of %d in %s macro"), *Context.Filename, Context.LineNumber, ArgIdx+1, Arguments.Num(), *GetToken());
					if (!PrepareArgument(ArgText, Arg.IsAutoText, MacroDesc, HasQuotes))
					{
						ArgParseError = true;
						break;
					}

					switch (Arg.Semantic)
					{
					case MAS_Namespace:
						{
							Namespace = ArgText;
						}
						break;
					case MAS_Identifier:
						{
							Identifier = ArgText;
						}
						break;
					case MAS_SourceText:
						{
							SourceText = ArgText;
						}
						break;
					}
				}

				if ( Identifier.IsEmpty() )
				{
					//The command doesn't have an identifier so we can't gather it
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro has an empty identifier and cannot be gathered."), *SourceLocation, *GetToken() );
					return;
				}

				if (!ArgParseError && !Identifier.IsEmpty() && !SourceText.IsEmpty())
				{
					if (!Namespace.IsSet())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro doesn't define a namespace and no external namespace was set. An empty namspace will be used."), *SourceLocation, *GetToken() );
						Namespace = FString();
					}

					FManifestContext MacroContext;
					MacroContext.Key = Identifier;
					MacroContext.SourceLocation = SourceLocation;
					MacroContext.PlatformName = Context.FilePlatformName;

					if (EnumHasAnyFlags(Context.FileTypes, EGatherTextSourceFileTypes::Ini))
					{
						// Gather the text without its package ID, as the INI will strip it on load at runtime
						TextNamespaceUtil::StripPackageNamespaceInline(Namespace.GetValue());
					}

					Context.AddManifestText( GetToken(), Namespace.GetValue(), SourceText, MacroContext );
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringTableMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// LOCTABLE_NEW(Id, Namespace)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			if (Arguments.Num() != GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			}
			else
			{
				Arguments[0].TrimStartInline();
				FString TableId = Arguments[0];
				Arguments[1].TrimStartInline();
				FString TableNamespace = Arguments[1];

				const FString TableIdMacroDesc = FString::Printf(TEXT("%s(%d): \"Id\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString TableNamespaceMacroDesc = FString::Printf(TEXT("%s(%d): \"Namespace\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());

				bool HasQuotes;
				if (PrepareArgument(TableId, true, TableIdMacroDesc, HasQuotes) && PrepareArgument(TableNamespace, true, TableNamespaceMacroDesc, HasQuotes))
				{
					const FName TableIdName = *TableId;

					if (TableIdName.IsNone())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): %s macro has an empty identifier and cannot be gathered."), *Context.Filename, Context.LineNumber, *GetToken());
					}
					else
					{
						Context.AddStringTable(TableIdName, TableNamespace);
					}
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringTableFromFileMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// LOCTABLE_FROMFILE_X(Id, Namespace, FilePath)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			if (Arguments.Num() != GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			}
			else
			{
				Arguments[0].TrimStartInline();
				FString TableId = Arguments[0];
				Arguments[1].TrimStartInline();
				FString TableNamespace = Arguments[1];
				Arguments[2].TrimStartInline();
				FString TableFilename = Arguments[2];

				const FString TableIdMacroDesc = FString::Printf(TEXT("%s(%d): \"Id\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString TableNamespaceMacroDesc = FString::Printf(TEXT("%s(%d): \"Namespace\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString TableFilenameMacroDesc = FString::Printf(TEXT("%s(%d): \"FilePath\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());

				bool HasQuotes;
				if (PrepareArgument(TableId, true, TableIdMacroDesc, HasQuotes) && PrepareArgument(TableNamespace, true, TableNamespaceMacroDesc, HasQuotes) && PrepareArgument(TableFilename, true, TableFilenameMacroDesc, HasQuotes))
				{
					const FName TableIdName = *TableId;

					if (TableIdName.IsNone())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): %s macro has an empty identifier and cannot be gathered."), *Context.Filename, Context.LineNumber, *GetToken());
					}
					else
					{
						Context.AddStringTableFromFile(TableIdName, TableNamespace, TableFilename, RootPath);
					}
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringTableEntryMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// LOCTABLE_SETSTRING(Id, Key, SourceString)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			if (Arguments.Num() != GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			}
			else
			{
				Arguments[0].TrimStartInline();
				FString TableId = Arguments[0];
				Arguments[1].TrimStartInline();
				FString Key = Arguments[1];
				Arguments[2].TrimStartInline();
				FString SourceString = Arguments[2];

				const FString TableIdMacroDesc = FString::Printf(TEXT("%s(%d): \"Id\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString KeyMacroDesc = FString::Printf(TEXT("%s(%d): \"Key\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString SourceStringMacroDesc = FString::Printf(TEXT("%s(%d): \"SourceString\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());

				bool HasQuotes;
				if (PrepareArgument(TableId, true, TableIdMacroDesc, HasQuotes) && PrepareArgument(Key, true, KeyMacroDesc, HasQuotes) && PrepareArgument(SourceString, true, SourceStringMacroDesc, HasQuotes))
				{
					const FName TableIdName = *TableId;

					if (TableIdName.IsNone() || Key.IsEmpty())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): %s macro has an empty identifier and cannot be gathered."), *Context.Filename, Context.LineNumber, *GetToken());
					}
					else if (!SourceString.IsEmpty())
					{
						Context.AddStringTableEntry(TableIdName, Key, SourceString);
					}
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringTableEntryMetaDataMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// LOCTABLE_SETMETA(Id, Key, SourceString)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			if (Arguments.Num() != GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): Expected %d arguments for %s macro, but got %d. %s"), *Context.Filename, Context.LineNumber, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
			}
			else
			{
				Arguments[0].TrimStartInline();
				FString TableId = Arguments[0];
				Arguments[1].TrimStartInline();
				FString Key = Arguments[1];
				Arguments[2].TrimStartInline();
				FString MetaDataId = Arguments[2];
				Arguments[3].TrimStartInline();
				FString MetaData = Arguments[3];

				const FString TableIdMacroDesc = FString::Printf(TEXT("%s(%d): \"Id\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString KeyMacroDesc = FString::Printf(TEXT("%s(%d): \"Key\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString MetaDataIdMacroDesc = FString::Printf(TEXT("%s(%d): \"MetaDataId\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());
				const FString MetaDataMacroDesc = FString::Printf(TEXT("%s(%d): \"MetaData\" argument in %s macro"), *Context.Filename, Context.LineNumber, *GetToken());

				bool HasQuotes;
				if (PrepareArgument(TableId, true, TableIdMacroDesc, HasQuotes) && PrepareArgument(Key, true, KeyMacroDesc, HasQuotes) && PrepareArgument(MetaDataId, true, MetaDataIdMacroDesc, HasQuotes) && PrepareArgument(MetaData, true, MetaDataMacroDesc, HasQuotes))
				{
					const FName TableIdName = *TableId;
					const FName MetaDataIdName = *MetaDataId;

					if (TableIdName.IsNone() || Key.IsEmpty() || MetaDataIdName.IsNone())
					{
						UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s(%d): %s macro has an empty identifier and cannot be gathered."), *Context.Filename, Context.LineNumber, *GetToken());
					}
					else if (!MetaData.IsEmpty())
					{
						Context.AddStringTableEntryMetaData(TableIdName, Key, MetaDataIdName, MetaData);
					}
				}
			}
		}
	}
}

UGatherTextFromSourceCommandlet::FStructuredLogMacroDescriptor::FStructuredLogMacroDescriptor(const TCHAR* InName, EFlags InFlags)
	: FMacroDescriptor(InName, ((int32)InFlags & (int32)EFlags::Namespace) ? 5 : 4)
	, Flags(InFlags)
{
}

void UGatherTextFromSourceCommandlet::FStructuredLogMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// UE_LOGFMT_LOC[_EX](CategoryName, Verbosity, Key, Format, ...)
	// UE_LOGFMT_NSLOC[_EX](CategoryName, Verbosity, Namespace, Key, Format, ...)
	int32 NextArg = 2;

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			const FString SourceLocation = FSourceLocation(Context.Filename, Context.LineNumber).ToString();

			if (Arguments.Num() < GetMinNumberOfArgument())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: Expected at least %d arguments for %s macro, but got %d. %s"), *SourceLocation, GetMinNumberOfArgument(), *GetToken(), Arguments.Num(), *FLocTextHelper::SanitizeLogOutput(Context.LineText.TrimStartAndEnd()));
				return;
			}

			bool bParseOk = true;
			const auto ParseArg = [this, &SourceLocation, &bParseOk](FString&& Arg, const TCHAR* ArgName) -> FString
			{
				FString Out = MoveTemp(Arg).TrimStart();
				constexpr bool bIsAutoText = true;
				const FString Desc = FString::Printf(TEXT("%s: \"%s\" argument in %s macro"), *SourceLocation, ArgName, *GetToken());
				bool bHasQuotes = false;
				bParseOk &= PrepareArgument(Out, bIsAutoText, Desc, bHasQuotes);
				return Out;
			};

			FString Namespace;
			if (const bool bHasNamespace = ((int32)Flags & (int32)EFlags::Namespace) != 0)
			{
				Namespace = ParseArg(MoveTemp(Arguments[NextArg++]), TEXT("Namespace"));
			}
			else
			{
				Namespace = Context.Namespace;
			}

			if (Namespace.IsEmpty())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("%s: %s macro doesn't define a namespace and no external namespace was set. An empty namspace will be used."), *SourceLocation, *GetToken());
			}

			FString Key = ParseArg(MoveTemp(Arguments[NextArg++]), TEXT("Key"));
			FString Format = ParseArg(MoveTemp(Arguments[NextArg++]), TEXT("Format"));

			if (bParseOk && !Key.IsEmpty())
			{
				FManifestContext MacroContext;
				MacroContext.Key = Key;
				MacroContext.SourceLocation = SourceLocation;
				MacroContext.PlatformName = Context.FilePlatformName;

				Context.AddManifestText(GetToken(), Namespace, Format, MacroContext);
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FIniNamespaceDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// [<config section name>]
	if (!Context.ExcludedRegion)
	{
		if( Context.LineText[ 0 ] == '[' )
		{
			int32 ClosingBracket;
			if( Text.FindChar( ']', ClosingBracket ) && ClosingBracket > 1 )
			{
				Context.Namespace = Text.Mid( 1, ClosingBracket - 1 );
				Context.EndParsingCurrentLine = true;
			}
		}
	}
}

#undef LOC_DEFINE_REGION
