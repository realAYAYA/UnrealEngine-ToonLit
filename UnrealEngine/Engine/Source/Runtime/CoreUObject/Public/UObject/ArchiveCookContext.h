// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/CookTagList.h"
#include "UObject/CookEnums.h"

/**
*	Accessor for data about the package being cooked during UObject::Serialize calls.
*/
struct FArchiveCookContext
{
public:
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookType")
	typedef UE::Cook::ECookType ECookType;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookingDLC")
	typedef UE::Cook::ECookingDLC ECookingDLC;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookType::Unknown")
	static const UE::Cook::ECookType ECookTypeUnknown = UE::Cook::ECookType::Unknown;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookType::OnTheFly")
	static const UE::Cook::ECookType ECookOnTheFly = UE::Cook::ECookType::OnTheFly;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookType::ByTheBook")
	static const UE::Cook::ECookType ECookByTheBook = UE::Cook::ECookType::ByTheBook;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookingDLC::Unknown")
	static const UE::Cook::ECookingDLC ECookingDLCUnknown = UE::Cook::ECookingDLC::Unknown;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookingDLC::Yes")
	static const UE::Cook::ECookingDLC ECookingDLCYes = UE::Cook::ECookingDLC::Yes;
	UE_DEPRECATED(5.4, "Use UE::Cook::ECookingDLC::No")
	static const UE::Cook::ECookingDLC ECookingDLCNo = UE::Cook::ECookingDLC::No;

private:

	/** CookTagList is only valid for cook by the book; it is not publically accessible otherwise. */
	FCookTagList CookTagList;
	const ITargetPlatform* TargetPlatform = nullptr;
	bool bCookTagListEnabled = false;
	UE::Cook::ECookType CookType = UE::Cook::ECookType::Unknown;
	UE::Cook::ECookingDLC CookingDLC = UE::Cook::ECookingDLC::Unknown;

public:

	UE_DEPRECATED(5.4, "Call version that takes the TargetPlatform")
	FArchiveCookContext(UPackage* InPackage, UE::Cook::ECookType InCookType, UE::Cook::ECookingDLC InCookingDLC)
		: FArchiveCookContext(InPackage, InCookType, InCookingDLC, nullptr)
	{
	}

	FArchiveCookContext(UPackage* InPackage, UE::Cook::ECookType InCookType, UE::Cook::ECookingDLC InCookingDLC,
		const ITargetPlatform* InTargetPlatform)
		: CookTagList(InPackage)
		, TargetPlatform(InTargetPlatform)
		, bCookTagListEnabled(InPackage && InCookType == UE::Cook::ECookType::ByTheBook)
		, CookType(InCookType)
		, CookingDLC(InCookingDLC)
	{
	}

	void Reset()
	{
		CookTagList.Reset();
	}

	FCookTagList* GetCookTagList() { return bCookTagListEnabled ? &CookTagList : nullptr; }
	const ITargetPlatform* GetTargetPlatform() const { return TargetPlatform; }

	bool IsCookByTheBook() const { return CookType == UE::Cook::ECookType::ByTheBook; }
	bool IsCookOnTheFly() const { return CookType == UE::Cook::ECookType::OnTheFly; }
	bool IsCookTypeUnknown() const { return CookType == UE::Cook::ECookType::Unknown; }
	UE::Cook::ECookType GetCookType() const { return CookType; }
	UE::Cook::ECookingDLC GetCookingDLC() const { return CookingDLC; }
};