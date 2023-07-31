// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

#include "UniString.hpp"
#include "DG.h"

#include <vector>

BEGIN_NAMESPACE_UE_AC

// Return the current language
GSResID GetCurrentLanguage();

// Return a localized string
GS::UniString GetUniString(GSResID StringsListResID, Int32 StringIndex);

// Print to debugger all strings of the indexed ressource string
void DumpIndStrings(GSResID ResId);

// Call to speedup access to strings
class FMultiString
{
  public:
	FMultiString(GSResID StringsListResID, Int32 StartIndex = 1);

	// Destructor
	~FMultiString();

	// Get string
	const utf8_t* GetStdString(Int32 StringIndex);

	// Get string
	const GS::UniString& GetGSString(Int32 StringIndex);

  private:
	bool IsValidIndex(Int32 StringIndex);

	GSResID						 StringsListResID;
	Int32						 StartIndex;
	bool						 bInitialized;
	std::vector< utf8_string >	 StdStrings;
	std::vector< GS::UniString > mGSStrings;
};

END_NAMESPACE_UE_AC
