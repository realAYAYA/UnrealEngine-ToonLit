// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderCore.h"
#include "mcpp.h"
#include "ShaderCompilerCore.h"

enum class EMessageType
{
	Error = 0,
	Warn = 1,
	ShaderMetaData = 2,
};

/**
 * Filter preprocessor errors.
 * @param ErrorMsg - The error message.
 * @returns true if the message is valid and has not been filtered out.
 */
inline EMessageType FilterPreprocessorError(const FString& ErrorMsg)
{
	const TCHAR* SubstringsToFilter[] =
	{
		TEXT("Unknown encoding:"),
		TEXT("with no newline, supplemented newline"),
		TEXT("Converted [CR+LF] to [LF]")
	};
	const int32 FilteredSubstringCount = UE_ARRAY_COUNT(SubstringsToFilter);

	if (ErrorMsg.Contains(TEXT("UESHADERMETADATA")))
	{
		return EMessageType::ShaderMetaData;
	}

	for (int32 SubstringIndex = 0; SubstringIndex < FilteredSubstringCount; ++SubstringIndex)
	{
		if (ErrorMsg.Contains(SubstringsToFilter[SubstringIndex]))
		{
			return EMessageType::Warn;
		}
	}
	return EMessageType::Error;
}

static void ExtractDirective(FString& OutString, FString WarningString)
{
	static const FString PrefixString = TEXT("UESHADERMETADATA_");
	uint32 DirectiveStartPosition = WarningString.Find(PrefixString) + PrefixString.Len();
	uint32 DirectiveEndPosition = WarningString.Find(TEXT("\n"));
	if (DirectiveEndPosition == INDEX_NONE)
	{
		DirectiveEndPosition = WarningString.Len();
	}
	OutString = WarningString.Mid(DirectiveStartPosition, (DirectiveEndPosition - DirectiveStartPosition));
}

/**
 * Parses MCPP error output.
 * @param ShaderOutput - Shader output to which to add errors.
 * @param McppErrors - MCPP error output.
 */
static bool ParseMcppErrors(TArray<FShaderCompilerError>& OutErrors, TArray<FString>& OutStrings, const FString& McppErrors)
{
	bool bSuccess = true;
	if (McppErrors.Len() > 0)
	{
		TArray<FString> Lines;
		McppErrors.ParseIntoArray(Lines, TEXT("\n"), true);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			int32 SepIndex1 = Line.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 2);
			int32 SepIndex2 = Line.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SepIndex1 + 1);
			if (SepIndex1 != INDEX_NONE && SepIndex2 != INDEX_NONE && SepIndex1 < SepIndex2)
			{
				FString Filename = Line.Left(SepIndex1);
				FString LineNumStr = Line.Mid(SepIndex1 + 1, SepIndex2 - SepIndex1 - 1);
				FString Message = Line.Mid(SepIndex2 + 1, Line.Len() - SepIndex2 - 1);
				if (Filename.Len() && LineNumStr.Len() && LineNumStr.IsNumeric() && Message.Len())
				{
					while (++LineIndex < Lines.Num() && Lines[LineIndex].Len() && Lines[LineIndex].StartsWith(TEXT(" "),ESearchCase::CaseSensitive))
					{
						Message += FString(TEXT("\n")) + Lines[LineIndex];
					}
					--LineIndex;
					Message.TrimStartAndEndInline();

					// Ignore the warning about files that don't end with a newline.
					switch (FilterPreprocessorError(Message))
					{
						case EMessageType::Error:
						{
							FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
							CompilerError->ErrorVirtualFilePath = Filename;
							CompilerError->ErrorLineString = LineNumStr;
							CompilerError->StrippedErrorMessage = Message;
							bSuccess = false;
						}
							break;
						case EMessageType::Warn:
						{
							// Warnings are ignored.
						}
							break;
						case EMessageType::ShaderMetaData:
						{
							FString Directive;
							ExtractDirective(Directive, Message);
							OutStrings.Add(Directive);
						}
							break;
						default:
							break;
					}
				}
				else
				{
					// Presume message is an error
					FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
					CompilerError->StrippedErrorMessage = Line;
					bSuccess = false;
				}
			}
			else
			{
				// Presume message is an error
				FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
				CompilerError->StrippedErrorMessage = Line;
				bSuccess = false;
			}
		}
	}
	return bSuccess;
}
