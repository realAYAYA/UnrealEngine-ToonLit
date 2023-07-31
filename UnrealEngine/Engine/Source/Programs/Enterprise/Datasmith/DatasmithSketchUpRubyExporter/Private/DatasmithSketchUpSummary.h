// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Datasmith SDK.
#include "Containers/UnrealString.h"


#define ADD_SUMMARY_LINE(...) FDatasmithSketchUpSummary::GetSingleton().AddSummaryLine(FString::Printf(__VA_ARGS__))
#define ADD_TRACE_LINE(...)   FDatasmithSketchUpSummary::GetSingleton().AddSummaryLine(FString::Printf(__VA_ARGS__))

class FDatasmithSketchUpSummary
{
public:

	static FDatasmithSketchUpSummary& GetSingleton();
	
	// Clear the summary of the export process.
	void ClearSummary();

	// Append a line to the summary of the export process.
	void AddSummaryLine(
		FString const& InLine // line to append (without a newline)
	);

	// Get the summary of the export process.
	FString const& GetSummary() const;

	// Log the summary of the export process into a file.
	void LogSummary(
		FString const& InFilePath // log file path
	) const;

private:

	// Summary of the export process.
	FString Summary;
};


inline void FDatasmithSketchUpSummary::ClearSummary()
{
	Summary.Reset();
}

inline void FDatasmithSketchUpSummary::AddSummaryLine(
	FString const& InLine
)
{
	Summary.Append(InLine);
	Summary.AppendChar(TEXT('\n'));
}

inline FString const& FDatasmithSketchUpSummary::GetSummary() const
{
	return Summary;
}
