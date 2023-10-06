// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetRegistry/CookTagList.h"


/**
*	Accessor for data about the package being cooked during UObject::Serialize calls.
*/
struct FArchiveCookContext
{
public:
	enum ECookType
	{
		ECookTypeUnknown,
		ECookOnTheFly,
		ECookByTheBook
	};

	enum ECookingDLC
	{
		ECookingDLCUnknown,
		ECookingDLCYes,
		ECookingDLCNo,
	};

private:

	// Only valid for cook by the book.
	FCookTagList CookTagList;
	bool bCookTagListEnabled = false;
	ECookType CookType;
	ECookingDLC CookingDLC = ECookingDLCUnknown;

public:


	FArchiveCookContext(UPackage* InPackage, ECookType InCookType, ECookingDLC CookingDLC) :
		CookTagList(InPackage),
		bCookTagListEnabled(InPackage && InCookType == ECookByTheBook),
		CookType(InCookType),
		CookingDLC(CookingDLC)
	{
		
	}

	void Reset()
	{
		CookTagList.Reset();
	}

	FCookTagList* GetCookTagList() { return bCookTagListEnabled ? &CookTagList : nullptr; }

	bool IsCookByTheBook() const { return CookType == ECookType::ECookByTheBook; }
	bool IsCookOnTheFly() const { return CookType == ECookType::ECookOnTheFly; }
	bool IsCookTypeUnknown() const { return CookType == ECookType::ECookTypeUnknown; }
	ECookType GetCookType() const { return CookType; }
	ECookingDLC GetCookingDLC() const { return CookingDLC; }
};