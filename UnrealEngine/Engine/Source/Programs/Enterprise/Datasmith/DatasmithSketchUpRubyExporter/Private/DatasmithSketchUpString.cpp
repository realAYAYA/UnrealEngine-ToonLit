// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpString.h"


class FDatasmithSketchUpString
{
public:
	// Get a wide string version of a SketchUp string.
	static FString GetWideString(
		SUStringRef InSStringRef // valid SketchUp string
	);
};



FString FDatasmithSketchUpString::GetWideString(SUStringRef InSStringRef)
{
	size_t SStringLength = 0;
	SUStringGetUTF16Length(InSStringRef, &SStringLength); // we can ignore the returned SU_RESULT

	unichar* WideString = new unichar[SStringLength + 1]; // account for the terminating NULL put by SketchUp
	SUStringGetUTF16(InSStringRef, SStringLength + 1, WideString, &SStringLength); // we can ignore the returned SU_RESULT
	FString Result = WideString;
	delete[] WideString;
	return MoveTemp(Result);
}

FString SuConvertString(SUStringRef StringRef)
{
	return FDatasmithSketchUpString::GetWideString(StringRef);
}
