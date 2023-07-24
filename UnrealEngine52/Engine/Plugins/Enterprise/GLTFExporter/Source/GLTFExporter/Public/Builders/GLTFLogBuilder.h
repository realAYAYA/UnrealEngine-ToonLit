// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFBuilder.h"

#if WITH_EDITOR
class IMessageLogListing;
#endif

class GLTFEXPORTER_API FGLTFLogBuilder : public FGLTFBuilder
{
public:

	FGLTFLogBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	void LogSuggestion(const FString& Message);

	void LogWarning(const FString& Message);

	void LogError(const FString& Message);

	const TArray<FString>& GetLoggedSuggestions() const;

	const TArray<FString>& GetLoggedWarnings() const;

	const TArray<FString>& GetLoggedErrors() const;

	bool HasLoggedMessages() const;

	void OpenLog() const;

	void ClearLog();

private:

	enum class ELogLevel
	{
		Suggestion,
		Warning,
		Error,
	};

	void PrintToLog(ELogLevel Level, const FString& Message) const;

	TArray<FString> Suggestions;
	TArray<FString> Warnings;
	TArray<FString> Errors;

#if WITH_EDITOR
	TSharedPtr<IMessageLogListing> LogListing;
#endif
};
