// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpSummary.h"

// Datasmith SDK.
#include "Misc/FileHelper.h"


FDatasmithSketchUpSummary& FDatasmithSketchUpSummary::GetSingleton()
{
	static FDatasmithSketchUpSummary Singleton;

	return Singleton;
}

void FDatasmithSketchUpSummary::LogSummary(
	FString const& InFilePath
) const
{
	FFileHelper::SaveStringToFile(Summary, *InFilePath);
}
