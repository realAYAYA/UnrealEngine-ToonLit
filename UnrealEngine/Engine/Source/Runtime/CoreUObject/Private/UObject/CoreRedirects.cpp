// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/CoreRedirects.h"

#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/PropertyHelper.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UnrealType.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/DeferredMessageLog.h"
#include "Templates/Casts.h"

DEFINE_LOG_CATEGORY(LogCoreRedirects);

#if !defined UE_WITH_CORE_REDIRECTS
#	define UE_WITH_CORE_REDIRECTS 1
#endif

FCoreRedirectObjectName::FCoreRedirectObjectName(const FTopLevelAssetPath& TopLevelAssetPath)
	: FCoreRedirectObjectName(TopLevelAssetPath.GetAssetName(), NAME_None, TopLevelAssetPath.GetPackageName())
{
}

FCoreRedirectObjectName::FCoreRedirectObjectName(const FString& InString)
{
	if (!ExpandNames(InString, ObjectName, OuterName, PackageName))
	{
		Reset();
	}
}

FCoreRedirectObjectName::FCoreRedirectObjectName(const class UObject* Object)
{
	// This is more efficient than going to path string and back to FNames
	if (Object)
	{	
		UObject* Outer = Object->GetOuter();

		if (!Outer)
		{
			PackageName = Object->GetFName();
			// This is a package
		}
		else
		{
			FString OuterString;
			ObjectName = Object->GetFName();

			// Follow outer chain,
			while (Outer)
			{
				UObject* NextOuter = Outer->GetOuter();
				if (!NextOuter)
				{
					OuterName = FName(*OuterString);
					PackageName = Outer->GetFName();
					break;
				}
				if (!OuterString.IsEmpty())
				{
					OuterString.AppendChar(TEXT('.'));
				}
				OuterString.Append(Outer->GetName());

				Outer = NextOuter;
			}
		}
	}
}

FString FCoreRedirectObjectName::ToString() const
{
	return CombineNames(ObjectName, OuterName, PackageName);
}

void FCoreRedirectObjectName::Reset()
{
	ObjectName = OuterName = PackageName = NAME_None;
}

bool FCoreRedirectObjectName::Matches(const FCoreRedirectObjectName& Other, bool bCheckSubstring) const
{
	return Matches(Other, EMatchFlags::CheckSubString);
}

bool FCoreRedirectObjectName::Matches(const FCoreRedirectObjectName& Other, EMatchFlags MatchFlags) const
{
	bool bPartialLHS = !EnumHasAnyFlags(MatchFlags, EMatchFlags::DisallowPartialLHSMatch);
	bool bPartialRHS = EnumHasAnyFlags(MatchFlags, EMatchFlags::AllowPartialRHSMatch);
	bool bSubString = EnumHasAllFlags(MatchFlags, EMatchFlags::CheckSubString);

	auto FieldMatches = [bPartialLHS, bPartialRHS, bSubString](FName LHS, FName RHS)
	{
		if (LHS.IsNone() || RHS.IsNone())
		{
			return LHS == RHS || ((!LHS.IsNone() || bPartialLHS) && (!RHS.IsNone() || bPartialRHS));
		}
		else if (bSubString)
		{
			// Much slower, see if RHS contains the substring LHS.
			return INDEX_NONE != UE::String::FindFirst(WriteToString<FName::StringBufferSize>(RHS), WriteToString<FName::StringBufferSize>(LHS), ESearchCase::IgnoreCase);
		}
		else
		{
			return LHS == RHS;
		}
	};
	if (!FieldMatches(ObjectName, Other.ObjectName))
	{
		return false;
	}
	if (!FieldMatches(OuterName, Other.OuterName))
	{
		return false;
	}
	if (!FieldMatches(PackageName, Other.PackageName))
	{
		return false;
	}
	return true;
}

int32 FCoreRedirectObjectName::MatchScore(const FCoreRedirectObjectName& Other) const
{
	int32 MatchScore = 1;

	if (ObjectName != NAME_None)
	{
		if (ObjectName == Other.ObjectName)
		{
			// Object name most important
			MatchScore += 8;
		}
		else
		{
			return 0;
		}
	}

	if (OuterName != NAME_None)
	{
		if (OuterName == Other.OuterName)
		{
			MatchScore += 4;
		}
		else
		{
			return 0;
		}
	}

	if (PackageName != NAME_None)
	{
		if (PackageName == Other.PackageName)
		{
			MatchScore += 2;
		}
		else
		{
			return 0;
		}
	}

	return MatchScore;
}

void FCoreRedirectObjectName::UnionFieldsInline(const FCoreRedirectObjectName& Other)
{
	if (ObjectName.IsNone())
	{
		ObjectName = Other.ObjectName;
	}
	if (OuterName.IsNone())
	{
		OuterName = Other.OuterName;
	}
	if (PackageName.IsNone())
	{
		PackageName = Other.PackageName;
	}
}

bool FCoreRedirectObjectName::HasValidCharacters(ECoreRedirectFlags Type) const
{
	static FString InvalidRedirectCharacters = TEXT("\"',|&!~\n\r\t@#(){}[]=;^%$`");
	static FString InvalidObjectRedirectCharacters = TEXT(".\n\r\t"); // Object and field names in Blueprints are very permissive...

	return ObjectName.IsValidXName(EnumHasAnyFlags(Type, ECoreRedirectFlags::Type_Object | ECoreRedirectFlags::Type_Property | ECoreRedirectFlags::Type_Function) ? InvalidObjectRedirectCharacters : InvalidRedirectCharacters)
		&& OuterName.IsValidXName(InvalidRedirectCharacters) && PackageName.IsValidXName(InvalidRedirectCharacters);
}

bool FCoreRedirectObjectName::ExpandNames(const FString& InString, FName& OutName, FName& OutOuter, FName &OutPackage)
{
	FString FullString = InString.TrimStartAndEnd();

	// Parse (/path.)?(outerchain.)?(name) where path and outerchain are optional
	// We also need to support (/path.)?(singleouter:)?(name) because the second delimiter in a chain is : for historical reasons
	int32 SlashIndex = INDEX_NONE;
	int32 FirstPeriodIndex = INDEX_NONE;
	int32 LastPeriodIndex = INDEX_NONE;
	int32 FirstColonIndex = INDEX_NONE;
	int32 LastColonIndex = INDEX_NONE;

	FullString.FindChar(TEXT('/'), SlashIndex);

	FullString.FindChar(TEXT('.'), FirstPeriodIndex);
	FullString.FindChar(TEXT(':'), FirstColonIndex);

	if (FirstColonIndex != INDEX_NONE && (FirstPeriodIndex == INDEX_NONE || FirstColonIndex < FirstPeriodIndex))
	{
		// If : is before . treat it as the first period
		FirstPeriodIndex = FirstColonIndex;
	}

	if (FirstPeriodIndex == INDEX_NONE)
	{
		// If start with /, fill in package name, otherwise name
		if (SlashIndex != INDEX_NONE)
		{
			OutPackage = FName(*FullString);
		}
		else
		{
			OutName = FName(*FullString);
		}
		return true;
	}

	FullString.FindLastChar(TEXT('.'), LastPeriodIndex);
	FullString.FindLastChar(TEXT(':'), LastColonIndex);

	if (LastColonIndex != INDEX_NONE && (LastPeriodIndex == INDEX_NONE || LastColonIndex > LastPeriodIndex))
	{
		// If : is after . treat it as the last period
		LastPeriodIndex = LastColonIndex;
	}

	if (SlashIndex == INDEX_NONE)
	{
		// No /, so start from beginning. There must be an outer if we got this far
		OutOuter = FName(*FullString.Mid(0, LastPeriodIndex));
	}
	else
	{
		OutPackage = FName(*FullString.Left(FirstPeriodIndex));
		if (FirstPeriodIndex != LastPeriodIndex)
		{
			// Extract Outer between periods
			OutOuter = FName(*FullString.Mid(FirstPeriodIndex + 1, LastPeriodIndex - FirstPeriodIndex - 1));
		}
	}

	OutName = FName(*FullString.Mid(LastPeriodIndex + 1));

	return true;
}

FString FCoreRedirectObjectName::CombineNames(FName NewName, FName NewOuter, FName NewPackage)
{
	TArray<FString> CombineStrings;

	if (NewOuter != NAME_None)
	{
		// If Outer is simple, need to use : instead of . because : is used for second delimiter only
		FString OuterString = NewOuter.ToString();

		int32 DelimIndex = INDEX_NONE;

		if (OuterString.FindChar(TEXT('.'), DelimIndex) || OuterString.FindChar(TEXT(':'), DelimIndex))
		{
			if (NewPackage != NAME_None)
			{
				return FString::Printf(TEXT("%s.%s.%s"), *NewPackage.ToString(), *NewOuter.ToString(), *NewName.ToString());
			}
			else
			{
				return FString::Printf(TEXT("%s.%s"), *NewOuter.ToString(), *NewName.ToString());
			}
		}
		else
		{
			if (NewPackage != NAME_None)
			{
				return FString::Printf(TEXT("%s.%s:%s"), *NewPackage.ToString(), *NewOuter.ToString(), *NewName.ToString());
			}
			else
			{
				return FString::Printf(TEXT("%s:%s"), *NewOuter.ToString(), *NewName.ToString());
			}
		}
	}
	else if (NewPackage != NAME_None)
	{
		if (NewName != NAME_None)
		{
			return FString::Printf(TEXT("%s.%s"), *NewPackage.ToString(), *NewName.ToString());
		}
		return NewPackage.ToString();
	}
	return NewName.ToString();
}

void FCoreRedirect::NormalizeNewName()
{
	// Fill in NewName as needed
	if (NewName.ObjectName == NAME_None)
	{
		NewName.ObjectName = OldName.ObjectName;
	}
	if (NewName.OuterName == NAME_None)
	{
		NewName.OuterName = OldName.OuterName;
	}
	if (NewName.PackageName == NAME_None)
	{
		NewName.PackageName = OldName.PackageName;
	}
}

const TCHAR* FCoreRedirect::ParseValueChanges(const TCHAR* Buffer)
{
	// If we export an empty array we export an empty string, so ensure that if we're passed an empty string
	// we interpret it as an empty array.
	if (*Buffer++ != TCHAR('('))
	{
		return nullptr;
	}

	SkipWhitespace(Buffer);
	if (*Buffer == TCHAR(')'))
	{
		return Buffer + 1;
	}

	for (;;)
	{
		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR('('))
		{
			return nullptr;
		}

		// Parse the key and value
		FString KeyString, ValueString;
		Buffer = FPropertyHelpers::ReadToken(Buffer, KeyString, true);
		if (!Buffer)
		{
			return nullptr;
		}

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(','))
		{
			return nullptr;
		}

		// Parse the value
		SkipWhitespace(Buffer);
		Buffer = FPropertyHelpers::ReadToken(Buffer, ValueString, true);
		if (!Buffer)
		{
			return nullptr;
		}

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(')'))
		{
			return nullptr;
		}

		ValueChanges.Add(KeyString, ValueString);

		switch (*Buffer++)
		{
		case TCHAR(')'):
			return Buffer;

		case TCHAR(','):
			break;

		default:
			return nullptr;
		}
	}
}

static FORCEINLINE bool CheckRedirectFlagsMatch(ECoreRedirectFlags FlagsA, ECoreRedirectFlags FlagsB)
{
	// For type, check it includes the matching type
	const bool bTypesOverlap = !!((FlagsA & FlagsB) & ECoreRedirectFlags::Type_AllMask);

	// For category, the bits must be an exact match
	const bool bCategoriesMatch = (FlagsA & ECoreRedirectFlags::Category_AllMask) == (FlagsB & ECoreRedirectFlags::Category_AllMask);

	// Options are not considered in this function; Flags will match at this point regardless of their bits in Options_AllMask

	return bTypesOverlap && bCategoriesMatch;
}

bool FCoreRedirect::Matches(ECoreRedirectFlags InFlags, const FCoreRedirectObjectName& InName,
	ECoreRedirectMatchFlags MatchFlags) const
{
	// Check flags for Type/Category match
	if (!CheckRedirectFlagsMatch(InFlags, RedirectFlags))
	{
		return false;
	}
	return Matches(InName, MatchFlags);
}


bool FCoreRedirect::Matches(const FCoreRedirectObjectName& InName, ECoreRedirectMatchFlags MatchFlags) const
{
	FCoreRedirectObjectName::EMatchFlags NameMatchFlags = FCoreRedirectObjectName::EMatchFlags::None;
	if (IsSubstringMatch())
	{
		NameMatchFlags |= FCoreRedirectObjectName::EMatchFlags::CheckSubString;
	}
	if (EnumHasAllFlags(MatchFlags, ECoreRedirectMatchFlags::AllowPartialMatch))
	{
		NameMatchFlags |= FCoreRedirectObjectName::EMatchFlags::AllowPartialRHSMatch;
	}
	return OldName.Matches(InName, NameMatchFlags);
}

bool FCoreRedirect::HasValueChanges() const
{
	return ValueChanges.Num() > 0;
}

bool FCoreRedirect::IsSubstringMatch() const
{
	return !!(RedirectFlags & ECoreRedirectFlags::Option_MatchSubstring);
}

FCoreRedirectObjectName FCoreRedirect::RedirectName(const FCoreRedirectObjectName& OldObjectName) const
{
	FCoreRedirectObjectName ModifyName(OldObjectName);

	// Convert names that are different and non empty
	if (OldName.ObjectName != NewName.ObjectName && !ModifyName.ObjectName.IsNone())
	{
		if (IsSubstringMatch())
		{
			ModifyName.ObjectName = FName(*OldObjectName.ObjectName.ToString().Replace(*OldName.ObjectName.ToString(), *NewName.ObjectName.ToString()));
		}
		else
		{
			ModifyName.ObjectName = NewName.ObjectName;
		}
	}
	// If package name and object name are specified, copy outer also it was set to null explicitly
	if ((OldName.OuterName != NewName.OuterName || (NewName.PackageName != NAME_None && NewName.ObjectName != NAME_None)) && !ModifyName.OuterName.IsNone())
	{
		if (IsSubstringMatch())
		{
			ModifyName.OuterName = FName(*OldObjectName.OuterName.ToString().Replace(*OldName.OuterName.ToString(), *NewName.OuterName.ToString()));
		}
		else
		{
			ModifyName.OuterName = NewName.OuterName;
		}
	}
	if (OldName.PackageName != NewName.PackageName && !ModifyName.PackageName.IsNone())
	{
		if (IsSubstringMatch())
		{
			ModifyName.PackageName = FName(*OldObjectName.PackageName.ToString().Replace(*OldName.PackageName.ToString(), *NewName.PackageName.ToString()));
		}
		else
		{
			ModifyName.PackageName = NewName.PackageName;
		}
	}

	return ModifyName;
}

bool FCoreRedirect::IdenticalMatchRules(const FCoreRedirect& Other) const
{
	// All types now use the full path
	ECoreRedirectFlags TypeFlags = RedirectFlags & ECoreRedirectFlags::Type_AllMask;
	return RedirectFlags == Other.RedirectFlags && OldName == Other.OldName;
}


bool FCoreRedirects::bInitialized = false;
bool FCoreRedirects::bInDebugMode = false;
bool FCoreRedirects::bValidatedOnce = false;
#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
bool FCoreRedirects::bIsInMultithreadedPhase = false;
#endif

TMap<FName, ECoreRedirectFlags> FCoreRedirects::ConfigKeyMap;
FCoreRedirects::FRedirectTypeMap FCoreRedirects::RedirectTypeMap;
FRWLock FCoreRedirects::KnownMissingLock;

FCoreRedirects::FRedirectNameMap& FCoreRedirects::FRedirectTypeMap::FindOrAdd(ECoreRedirectFlags Key)
{
	FRedirectNameMap*& NameMap = Map.FindOrAdd(Key);
	if (NameMap)
	{
		return *NameMap;
	}

	TPair<ECoreRedirectFlags, FRedirectNameMap>* OldData = FastIterable.GetData();
	TPair<ECoreRedirectFlags, FRedirectNameMap>& NewPair =
		FastIterable.Emplace_GetRef(Key, FRedirectNameMap());
	if (FastIterable.GetData() == OldData)
	{
		NameMap = &NewPair.Value;
	}
	else
	{
		Map.Reset();
		for (TPair<ECoreRedirectFlags, FRedirectNameMap>& Pair : FastIterable)
		{
			Map.Add(Pair.Key, &Pair.Value);
		}
	}
	return NewPair.Value;
}

FCoreRedirects::FRedirectNameMap* FCoreRedirects::FRedirectTypeMap::Find(ECoreRedirectFlags Key)
{
	return Map.FindRef(Key);
}

void FCoreRedirects::FRedirectTypeMap::Empty()
{
	Map.Empty();
	FastIterable.Empty();
}

void FCoreRedirects::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

#if !UE_BUILD_SHIPPING && !NO_LOGGING
	if (FParse::Param(FCommandLine::Get(), TEXT("FullDebugCoreRedirects")))
	{
		// Enable debug mode and set to maximum verbosity
		bInDebugMode = true;
		LogCoreRedirects.SetVerbosity(ELogVerbosity::VeryVerbose);
		FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(ValidateAllRedirects);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("DebugCoreRedirects")))
	{
		// Enable debug mode and increase log levels but don't show every message
		bInDebugMode = true;
		LogCoreRedirects.SetVerbosity(ELogVerbosity::Verbose);
		FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(ValidateAllRedirects);
	}
#endif

#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	// Setting IsInMultithreadedPhase has to occur after LoadModule("AssetRegistry") (which can write to FCoreRedirects) and
	// before the first package is queued onto the async loading thread (which reads from FCoreRedirects multithreaded). OnPostEngineInit is in that window.
	FCoreDelegates::OnPostEngineInit.AddStatic(EnterMultithreadedPhase);
#endif

	// Setup map
	ConfigKeyMap.Add(TEXT("ObjectRedirects"), ECoreRedirectFlags::Type_Object);
	ConfigKeyMap.Add(TEXT("ClassRedirects"), ECoreRedirectFlags::Type_Class);
	ConfigKeyMap.Add(TEXT("StructRedirects"), ECoreRedirectFlags::Type_Struct);
	ConfigKeyMap.Add(TEXT("EnumRedirects"), ECoreRedirectFlags::Type_Enum);
	ConfigKeyMap.Add(TEXT("FunctionRedirects"), ECoreRedirectFlags::Type_Function);
	ConfigKeyMap.Add(TEXT("PropertyRedirects"), ECoreRedirectFlags::Type_Property);
	ConfigKeyMap.Add(TEXT("PackageRedirects"), ECoreRedirectFlags::Type_Package);

	RegisterNativeRedirects();

	// Prepopulate RedirectTypeMap entries that some threads write to after the engine goes multi-threaded.
	// Most RedirectTypeMap entries are written to only from InitUObject's call to ReadRedirectsFromIni, and at that point the Engine is single-threaded.
	// Known missing packages and plugin loads can add entries to existing lists but will not add brand new types.
	// Taking advantage of this, we treat the list of Key/Value pairs of RedirectTypeMap as immutable and read from it without synchronization.
	// Note that the values for those written-during-multithreading entries need to be synchronized; it is only the list of Key/Value pairs that is immutable.
#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	check(!bIsInMultithreadedPhase);
#endif
	RedirectTypeMap.FindOrAdd(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Category_Removed | ECoreRedirectFlags::Option_MissingLoad);

	// Enable to run startup tests
	//RunTests();
}

#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
void FCoreRedirects::EnterMultithreadedPhase()
{
	bIsInMultithreadedPhase = true;
}
#endif

bool FCoreRedirects::RedirectNameAndValues(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName,
	FCoreRedirectObjectName& NewObjectName, const FCoreRedirect** FoundValueRedirect, ECoreRedirectMatchFlags MatchFlags)
{
	NewObjectName = OldObjectName;
	TArray<const FCoreRedirect*> FoundRedirects;

	if (GetMatchingRedirects(Type, OldObjectName, FoundRedirects, MatchFlags))
	{
		// Sort them based on match
		FoundRedirects.Sort([&OldObjectName](const FCoreRedirect& A, const FCoreRedirect& B) { return A.OldName.MatchScore(OldObjectName) > B.OldName.MatchScore(OldObjectName); });

		// Apply in order
		for (int32 i = 0; i < FoundRedirects.Num(); i++)
		{
			const FCoreRedirect* Redirect = FoundRedirects[i];

			if (!Redirect)
			{
				continue;
			}

			// Only apply if name match is still valid, if it already renamed part of it it may not apply any more. Don't want to check flags as those were checked in the gather step
			if (Redirect->Matches(NewObjectName, MatchFlags))
			{
				if (FoundValueRedirect && (Redirect->HasValueChanges() || Redirect->OverrideClassName.IsValid()))
				{
					if (*FoundValueRedirect)
					{
						if ((*FoundValueRedirect)->ValueChanges.OrderIndependentCompareEqual(Redirect->ValueChanges) == false)
						{
							UE_LOG(LogCoreRedirects, Error, TEXT("RedirectNameAndValues(%s) found multiple conflicting value redirects, %s and %s!"), *OldObjectName.ToString(), *(*FoundValueRedirect)->OldName.ToString(), *Redirect->OldName.ToString());
						}
					}
					else
					{
						// Set value redirects for processing outside
						*FoundValueRedirect = Redirect;
					}
				}

				NewObjectName = Redirect->RedirectName(NewObjectName);
			}
		}
	}

	UE_CLOG(bInDebugMode && NewObjectName != OldObjectName, LogCoreRedirects, Verbose, 
		TEXT("RedirectNameAndValues(%s) replaced by %s"), *OldObjectName.ToString(), *NewObjectName.ToString());

	return NewObjectName != OldObjectName;
}

FCoreRedirectObjectName FCoreRedirects::GetRedirectedName(ECoreRedirectFlags Type,
	const FCoreRedirectObjectName& OldObjectName, ECoreRedirectMatchFlags MatchFlags)
{
	FCoreRedirectObjectName NewObjectName;

	RedirectNameAndValues(Type, OldObjectName, NewObjectName, nullptr, MatchFlags);

	return NewObjectName;
}

const TMap<FString, FString>* FCoreRedirects::GetValueRedirects(ECoreRedirectFlags Type,
	const FCoreRedirectObjectName& OldObjectName, ECoreRedirectMatchFlags MatchFlags)
{
	FCoreRedirectObjectName NewObjectName;
	const FCoreRedirect* FoundRedirect = nullptr;

	RedirectNameAndValues(Type, OldObjectName, NewObjectName, &FoundRedirect, MatchFlags);

	if (FoundRedirect && FoundRedirect->ValueChanges.Num() > 0)
	{
		UE_CLOG(bInDebugMode, LogCoreRedirects, VeryVerbose, TEXT("GetValueRedirects found %d matches for %s"), FoundRedirect->ValueChanges.Num(), *OldObjectName.ToString());

		return &FoundRedirect->ValueChanges;
	}

	return nullptr;
}

bool FCoreRedirects::GetMatchingRedirects(ECoreRedirectFlags SearchFlags, const FCoreRedirectObjectName& OldObjectName,
	TArray<const FCoreRedirect*>& FoundRedirects, ECoreRedirectMatchFlags MatchFlags)
{
	// Look for all redirects that match the given names and flags
	bool bFound = false;
	
	// If we're not explicitly searching for packages, and not looking for removed things, and not searching for partial matches
	// based on ObjectName only, add the implicit (Type=Package,Category=None) redirects
	const bool bSearchPackageRedirects = !(SearchFlags & ECoreRedirectFlags::Type_Package) &&
		!(SearchFlags & ECoreRedirectFlags::Category_Removed) &&
			(!(MatchFlags & ECoreRedirectMatchFlags::AllowPartialMatch) || !OldObjectName.PackageName.IsNone());

	// Determine list of maps to look over, need to handle being passed multiple types in a bit mask
	for (const TPair<ECoreRedirectFlags, FRedirectNameMap>& Pair : RedirectTypeMap)
	{
		ECoreRedirectFlags PairFlags = Pair.Key;

		// We need to check all maps that match the search or package flags
		if (CheckRedirectFlagsMatch(PairFlags, SearchFlags) || (bSearchPackageRedirects && CheckRedirectFlagsMatch(PairFlags, ECoreRedirectFlags::Type_Package)))
		{
			const TArray<FCoreRedirect>* RedirectsForName = Pair.Value.RedirectMap.Find(OldObjectName.GetSearchKey(PairFlags));

			if (RedirectsForName)
			{
				for (const FCoreRedirect& CheckRedirect : *RedirectsForName)
				{
					if (CheckRedirect.Matches(PairFlags, OldObjectName, MatchFlags))
					{
						bFound = true;
						FoundRedirects.Add(&CheckRedirect);
					}
				}
			}
		}
	}

	return bFound;
}

bool FCoreRedirects::FindPreviousNames(ECoreRedirectFlags SearchFlags, const FCoreRedirectObjectName& NewObjectName, TArray<FCoreRedirectObjectName>& PreviousNames)
{
	// Look for reverse direction redirects
	bool bFound = false;

	// If we're not explicitly searching for packages or looking for removed things, add the implicit (Type=Package,Category=None) redirects
	const bool bSearchPackageRedirects = !(SearchFlags & ECoreRedirectFlags::Type_Package) && !(SearchFlags & ECoreRedirectFlags::Category_Removed);

	// Determine list of maps to look over, need to handle being passed multiple Flagss in a bit mask
	for (const TPair<ECoreRedirectFlags, FRedirectNameMap>& Pair : RedirectTypeMap)
	{
		ECoreRedirectFlags PairFlags = Pair.Key;

		// We need to check all maps that match the search or package flags
		if (CheckRedirectFlagsMatch(PairFlags, SearchFlags) || (bSearchPackageRedirects && CheckRedirectFlagsMatch(PairFlags, ECoreRedirectFlags::Type_Package)))
		{
			for (const TPair<FName, TArray<FCoreRedirect>>& RedirectPair : Pair.Value.RedirectMap)
			{
				for (const FCoreRedirect& Redirect : RedirectPair.Value)
				{
					FCoreRedirectObjectName::EMatchFlags MatchFlags = FCoreRedirectObjectName::EMatchFlags::None;
					if (Redirect.IsSubstringMatch())
					{
						MatchFlags |= FCoreRedirectObjectName::EMatchFlags::CheckSubString;
					}
					if (Redirect.NewName.Matches(NewObjectName, MatchFlags))
					{
						// Construct a reverse redirect
						FCoreRedirect ReverseRedirect = FCoreRedirect(Redirect);
						ReverseRedirect.OldName = Redirect.NewName;
						ReverseRedirect.NewName = Redirect.OldName;

						FCoreRedirectObjectName OldName = ReverseRedirect.RedirectName(NewObjectName);

						if (OldName != NewObjectName)
						{
							bFound = true;
							PreviousNames.AddUnique(OldName);
						}
					}
				}
			}
		}
	}

	UE_CLOG(bFound && bInDebugMode, LogCoreRedirects, VeryVerbose, TEXT("FindPreviousNames found %d previous names for %s"), PreviousNames.Num(), *NewObjectName.ToString());

	return bFound;
}

bool FCoreRedirects::IsKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName)
{
	TArray<const FCoreRedirect*> FoundRedirects;

	FRWScopeLock ScopeLock(KnownMissingLock, FRWScopeLockType::SLT_ReadOnly);
	return FCoreRedirects::GetMatchingRedirects(Type | ECoreRedirectFlags::Category_Removed, ObjectName, FoundRedirects);
}

bool FCoreRedirects::AddKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName, ECoreRedirectFlags Channel)
{
	Initialize();

	check((Channel & ~ECoreRedirectFlags::Option_MissingLoad) == ECoreRedirectFlags::None);
	FCoreRedirect NewRedirect(Type | ECoreRedirectFlags::Category_Removed | Channel, ObjectName, FCoreRedirectObjectName());

	FRWScopeLock ScopeLock(KnownMissingLock, FRWScopeLockType::SLT_Write);
	return AddRedirectList(TArrayView<const FCoreRedirect>(&NewRedirect, 1), TEXT("AddKnownMissing"));
}

bool FCoreRedirects::RemoveKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName, ECoreRedirectFlags Channel)
{
	check((Channel & ~ECoreRedirectFlags::Option_MissingLoad) == ECoreRedirectFlags::None);
	FCoreRedirect RedirectToRemove(Type | ECoreRedirectFlags::Category_Removed | Channel, ObjectName, FCoreRedirectObjectName());

	FRWScopeLock ScopeLock(KnownMissingLock, FRWScopeLockType::SLT_Write);
	return RemoveRedirectList(TArrayView<const FCoreRedirect>(&RedirectToRemove, 1), TEXT("RemoveKnownMissing"));
}

void FCoreRedirects::ClearKnownMissing(ECoreRedirectFlags Type, ECoreRedirectFlags Channel)
{
	check((Channel & ~ECoreRedirectFlags::Option_MissingLoad) == ECoreRedirectFlags::None);
	ECoreRedirectFlags RedirectFlags = Type | ECoreRedirectFlags::Category_Removed | Channel;

	FRWScopeLock ScopeLock(KnownMissingLock, FRWScopeLockType::SLT_Write);
	FRedirectNameMap* RedirectNameMap = RedirectTypeMap.Find(RedirectFlags);
	if (RedirectNameMap)
	{
		RedirectNameMap->RedirectMap.Empty();
	}
}

bool FCoreRedirects::RunTests()
{
	bool bSuccess = true;
	FRedirectTypeMap BackupMap = MoveTemp(RedirectTypeMap);
#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	bool BackupIsInMultithreadedPhase = bIsInMultithreadedPhase;
	bIsInMultithreadedPhase = false;
#endif
	RedirectTypeMap.Empty();

	TArray<FCoreRedirect> NewRedirects;

	NewRedirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("Property"), TEXT("Property2"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("Class.Property"), TEXT("Property3"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Game/PackageSpecific.Class.Property"), TEXT("Property4"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Game/Package.Class.OtherProperty"), TEXT("/Game/Package.Class.OtherProperty2"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("Class"), TEXT("Class2"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Game/Package.Class"), TEXT("Class3"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Category_InstanceOnly, TEXT("/Game/Package.Class"), TEXT("/Game/Package.ClassInstance"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/Game/Package"), TEXT("/Game/Package2"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring, TEXT("/oldgame"), TEXT("/newgame"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Category_Removed, TEXT("/Game/RemovedPackage"), TEXT("/Game/RemovedPackage"));
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Category_Removed | ECoreRedirectFlags::Option_MissingLoad, TEXT("/Game/MissingLoadPackage"), TEXT("/Game/MissingLoadPackage"));

	AddRedirectList(NewRedirects, TEXT("RunTests"));

	struct FRedirectTest
	{
		FString OldName;
		FString NewName;
		ECoreRedirectFlags Type;

		FRedirectTest(const FString& InOldName, const FString& InNewName, ECoreRedirectFlags InType)
			: OldName(InOldName), NewName(InNewName), Type(InType)
		{}
	};

	TArray<FRedirectTest> Tests;

	UE_LOG(LogCoreRedirects, Log, TEXT("Running FCoreRedirect Tests"));

	// Package-specific property rename and package rename apply
	Tests.Emplace(TEXT("/Game/PackageSpecific.Class:Property"), TEXT("/Game/PackageSpecific.Class:Property4"), ECoreRedirectFlags::Type_Property);
	// Verify . works as well
	Tests.Emplace(TEXT("/Game/PackageSpecific.Class.Property"), TEXT("/Game/PackageSpecific.Class:Property4"), ECoreRedirectFlags::Type_Property);
	// Wrong type, no replacement
	Tests.Emplace(TEXT("/Game/PackageSpecific.Class:Property"), TEXT("/Game/PackageSpecific.Class:Property"), ECoreRedirectFlags::Type_Function);
	// Class-specific property rename and package rename apply
	Tests.Emplace(TEXT("/Game/Package.Class:Property"), TEXT("/Game/Package2.Class:Property3"), ECoreRedirectFlags::Type_Property);
	// Package-Specific class rename applies
	Tests.Emplace(TEXT("/Game/Package.Class"), TEXT("/Game/Package2.Class3"), ECoreRedirectFlags::Type_Class);
	// Generic class rename applies
	Tests.Emplace(TEXT("/Game/PackageOther.Class"), TEXT("/Game/PackageOther.Class2"), ECoreRedirectFlags::Type_Class);
	// Check instance option
	Tests.Emplace(TEXT("/Game/Package.Class"), TEXT("/Game/Package2.ClassInstance"), ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Category_InstanceOnly);
	// Substring test
	Tests.Emplace(TEXT("/oldgame/Package.DefaultClass"), TEXT("/newgame/Package.DefaultClass"), ECoreRedirectFlags::Type_Class);

	for (FRedirectTest& Test : Tests)
	{
		FCoreRedirectObjectName OldName = FCoreRedirectObjectName(Test.OldName);
		FCoreRedirectObjectName NewName = GetRedirectedName(Test.Type, OldName);

		if (NewName.ToString() != Test.NewName)
		{
			bSuccess = false;
			UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: %s to %s, should be %s!"), *OldName.ToString(), *NewName.ToString(), *Test.NewName);
		}
	}

	// Check reverse lookup
	TArray<FCoreRedirectObjectName> OldNames;

	FindPreviousNames(ECoreRedirectFlags::Type_Class, FCoreRedirectObjectName(TEXT("/Game/PackageOther.Class2")), OldNames);

	if (OldNames.Num() != 1 || OldNames[0].ToString() != TEXT("/Game/PackageOther.Class"))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: ReverseLookup!"));
	}

	// Check removed
	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/RemovedPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/RemovedPackage should be removed!"));
	}

	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/MissingLoadPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/MissingLoadPackage should be removed!"));
	}

	if (IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/NotRemovedPackage should be removed!"));
	}

	AddKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad")), ECoreRedirectFlags::Option_MissingLoad);

	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/NotRemovedMissingLoad should be removed now!"));
	}

	RemoveKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad")), ECoreRedirectFlags::None);

	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: RemoveKnownMissing of /Game/NotRemovedMissingLoad but with bIsMissingLoad=false should not have removed the redirect!"));
	}

	RemoveKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad")), ECoreRedirectFlags::Option_MissingLoad);

	if (IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedMissingLoad"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/NotRemovedMissingLoad should no longer be removed!"));
	}

	AddKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage")), ECoreRedirectFlags::None);

	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/NotRemovedPackage should be removed now!"));
	}

	RemoveKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage")), ECoreRedirectFlags::Option_MissingLoad);

	if (!IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: RemoveKnownMissing of /Game/NotRemovedPackage but with bIsMissingLoad=true should not have removed the redirect!"));
	}

	RemoveKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage")), ECoreRedirectFlags::None);

	if (IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(TEXT("/Game/NotRemovedPackage"))))
	{
		bSuccess = false;
		UE_LOG(LogCoreRedirects, Error, TEXT("FCoreRedirect Test Failed: /Game/NotRemovedPackage should no longer be removed!"));
	}

	// Restore old state
	RedirectTypeMap = MoveTemp(BackupMap);
#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	bIsInMultithreadedPhase = BackupIsInMultithreadedPhase;
#endif

	return bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRedirectTest, "System.Core.Misc.CoreRedirects", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCoreRedirectTest::RunTest(const FString& Parameters)
{
	return FCoreRedirects::RunTests();
}

bool FCoreRedirects::ReadRedirectsFromIni(const FString& IniName)
{
	Initialize();

	if (GConfig)
	{
		const FConfigSection* RedirectSection = GConfig->GetSection(TEXT("CoreRedirects"), false, IniName);
		if (RedirectSection)
		{
			TArray<FCoreRedirect> NewRedirects;

			for (FConfigSection::TConstIterator It(*RedirectSection); It; ++It)
			{
				FString OldName, NewName, OverrideClassName;

				bool bInstanceOnly = false;
				bool bRemoved = false;
				bool bMatchSubstring = false;

				const FString& ValueString = It.Value().GetValue();

				FParse::Bool(*ValueString, TEXT("InstanceOnly="), bInstanceOnly);
				FParse::Bool(*ValueString, TEXT("Removed="), bRemoved);
				FParse::Bool(*ValueString, TEXT("MatchSubstring="), bMatchSubstring);

				FParse::Value(*ValueString, TEXT("OldName="), OldName);
				FParse::Value(*ValueString, TEXT("NewName="), NewName);

				FParse::Value(*ValueString, TEXT("OverrideClassName="), OverrideClassName);

				ECoreRedirectFlags* FlagPtr = ConfigKeyMap.Find(It.Key());

				// If valid type
				if (FlagPtr)
				{
					ECoreRedirectFlags NewFlags = *FlagPtr;

					if (bInstanceOnly)
					{
						NewFlags |= ECoreRedirectFlags::Category_InstanceOnly;
					}

					if (bRemoved)
					{
						NewFlags |= ECoreRedirectFlags::Category_Removed;
					}

					if (bMatchSubstring)
					{
						NewFlags |= ECoreRedirectFlags::Option_MatchSubstring;
					}

					FCoreRedirect* Redirect = &NewRedirects.Emplace_GetRef(NewFlags, FCoreRedirectObjectName(OldName), FCoreRedirectObjectName(NewName));

					if (!OverrideClassName.IsEmpty())
					{
						Redirect->OverrideClassName = FCoreRedirectObjectName(OverrideClassName);
					}

					int32 ValueChangesIndex = ValueString.Find(TEXT("ValueChanges="));
				
					if (ValueChangesIndex != INDEX_NONE)
					{
						// Look for first (
						ValueChangesIndex = ValueString.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueChangesIndex);

						FString ValueChangesString = ValueString.Mid(ValueChangesIndex);
						const TCHAR* Buffer = Redirect->ParseValueChanges(GetData(ValueChangesString));

						if (!Buffer)
						{
							UE_LOG(LogCoreRedirects, Error, TEXT("ReadRedirectsFromIni(%s) failed to parse ValueChanges for redirect %s!"), *IniName, *ValueString);

							// Remove added redirect
							Redirect = nullptr;
							NewRedirects.RemoveAt(NewRedirects.Num() - 1);
						}
					}
				}
				else
				{
					UE_LOG(LogCoreRedirects, Error, TEXT("ReadRedirectsFromIni(%s) failed to parse type for redirect %s!"), *IniName, *ValueString);
				}
			}

			return AddRedirectList(NewRedirects, IniName);
		}
		else
		{
			UE_LOG(LogCoreRedirects, Verbose, TEXT("ReadRedirectsFromIni(%s) did not find any redirects"), *IniName);
		}
	}
	else
	{
		UE_LOG(LogCoreRedirects, Warning, TEXT(" **** CORE REDIRECTS UNABLE TO INITIALIZE! **** "));
	}
	return false;
}

bool FCoreRedirects::AddRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString)
{
	Initialize();

	UE_LOG(LogCoreRedirects, Verbose, TEXT("AddRedirect(%s) adding %d redirects"), *SourceString, Redirects.Num());

	if (bInDebugMode && bValidatedOnce)
	{
		// Validate on apply because we finished our initial validation pass
		ValidateRedirectList(Redirects, SourceString);
	}

	bool bAddedAny = false;
	for (const FCoreRedirect& NewRedirect : Redirects)
	{
		if (!NewRedirect.OldName.IsValid() || !NewRedirect.NewName.IsValid())
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("AddRedirect(%s) failed to add redirect from %s to %s with empty name!"), *SourceString, *NewRedirect.OldName.ToString(), *NewRedirect.NewName.ToString());
			continue;
		}

		if ((!NewRedirect.OldName.HasValidCharacters(NewRedirect.RedirectFlags) && !FPackageName::IsVersePackage(NewRedirect.OldName.PackageName.ToString()))
			|| (!NewRedirect.NewName.HasValidCharacters(NewRedirect.RedirectFlags) && !FPackageName::IsVersePackage(NewRedirect.NewName.PackageName.ToString())))
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("AddRedirect(%s) failed to add redirect from %s to %s with invalid characters!"), *SourceString, *NewRedirect.OldName.ToString(), *NewRedirect.NewName.ToString());
			continue;
		}

		if (NewRedirect.IsSubstringMatch())
		{
			UE_LOG(LogCoreRedirects, Log, TEXT("AddRedirect(%s) has substring redirect %s, these are very slow and should be resolved as soon as possible!"), *SourceString, *NewRedirect.OldName.ToString());
		}
		
		if (AddSingleRedirect(NewRedirect, SourceString))
		{
			bAddedAny = true;

			// If value redirect, add a value redirect from NewName->NewName as well, this will merge with existing ones as needed
			if (NewRedirect.OldName != NewRedirect.NewName && NewRedirect.HasValueChanges())
			{
				FCoreRedirect ValueRedirect = NewRedirect;
				ValueRedirect.OldName = ValueRedirect.NewName;

				AddSingleRedirect(ValueRedirect, SourceString);
			}
		}
	}

	return bAddedAny;
}

bool FCoreRedirects::AddSingleRedirect(const FCoreRedirect& NewRedirect, const FString& SourceString)
{
	FRedirectNameMap* ExistingNameMap;
#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	if (bIsInMultithreadedPhase)
	{
		ExistingNameMap = RedirectTypeMap.Find(NewRedirect.RedirectFlags);
		checkf(ExistingNameMap, TEXT("Once EnterMultithreadedPhase has been called, it is no longer valid to add redirects of new types."));
	}
	else
#endif
	{
		ExistingNameMap = &RedirectTypeMap.FindOrAdd(NewRedirect.RedirectFlags);
	}
	TArray<FCoreRedirect>& ExistingRedirects = ExistingNameMap->RedirectMap.FindOrAdd(NewRedirect.GetSearchKey());

	bool bFoundDuplicate = false;

	// Check for duplicate
	for (FCoreRedirect& ExistingRedirect : ExistingRedirects)
	{
		if (ExistingRedirect.IdenticalMatchRules(NewRedirect))
		{
			bFoundDuplicate = true;
			bool bIsSameNewName = ExistingRedirect.NewName == NewRedirect.NewName;
			bool bOneIsPartialOtherIsFull = false;
			if (!bIsSameNewName &&
				ExistingRedirect.OldName.Matches(NewRedirect.OldName, FCoreRedirectObjectName::EMatchFlags::AllowPartialRHSMatch) &&
				ExistingRedirect.NewName.Matches(NewRedirect.NewName, FCoreRedirectObjectName::EMatchFlags::AllowPartialRHSMatch))
			{
				bIsSameNewName = true;
				bOneIsPartialOtherIsFull = true;
			}

			if (bIsSameNewName)
			{
				// Merge fields from the two duplicate redirects
				bool bBothHaveValueChanges = ExistingRedirect.HasValueChanges() && NewRedirect.HasValueChanges();
				ExistingRedirect.OldName.UnionFieldsInline(NewRedirect.OldName);
				ExistingRedirect.NewName.UnionFieldsInline(NewRedirect.NewName);
				ExistingRedirect.ValueChanges.Append(NewRedirect.ValueChanges);
				if (bBothHaveValueChanges)
				{
					// No warning when there are values changes to merge
					UE_LOG(LogCoreRedirects, Verbose, TEXT("AddRedirect(%s) merging value redirects for %s"), *SourceString, *ExistingRedirect.NewName.ToString());
				}
				else if (bOneIsPartialOtherIsFull)
				{
					UE_LOG(LogCoreRedirects, Warning, TEXT("AddRedirect(%s) found duplicate redirects for %s to %s, one a FullPath and the other ObjectName-only.")
						TEXT(" This used to be required for StructRedirects but now you should remove the ObjectName-only redirect and keep the FullPath."),
						*SourceString, *ExistingRedirect.OldName.ToString(), *ExistingRedirect.NewName.ToString());
				}
				else
				{
					UE_LOG(LogCoreRedirects, Verbose, TEXT("AddRedirect(%s) ignoring duplicate redirects for %s to %s"), *SourceString, *ExistingRedirect.OldName.ToString(), *ExistingRedirect.NewName.ToString());
				}
			}
			else
			{
				UE_LOG(LogCoreRedirects, Error, TEXT("AddRedirect(%s) found conflicting redirects for %s! Old: %s, New: %s"), *SourceString, *ExistingRedirect.OldName.ToString(), *ExistingRedirect.NewName.ToString(), *NewRedirect.NewName.ToString());
			}
			break;
		}
	}

	if (bFoundDuplicate)
	{
		return false;
	}

	ExistingRedirects.Add(NewRedirect);
	return true;
}

bool FCoreRedirects::RemoveRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString)
{
	UE_LOG(LogCoreRedirects, Verbose, TEXT("RemoveRedirect(%s) Removing %d redirects"), *SourceString, Redirects.Num());

	bool bRemovedAny = false;
	for (const FCoreRedirect& RedirectToRemove : Redirects)
	{
		if (!RedirectToRemove.OldName.IsValid() || !RedirectToRemove.NewName.IsValid())
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("RemoveRedirect(%s) failed to remove redirect from %s to %s with empty name!"), *SourceString, *RedirectToRemove.OldName.ToString(), *RedirectToRemove.NewName.ToString());
			continue;
		}

		if (RedirectToRemove.HasValueChanges())
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("RemoveRedirect(%s) failed to remove redirect from %s to %s as it contains value changes!"), *SourceString, *RedirectToRemove.OldName.ToString(), *RedirectToRemove.NewName.ToString());
			continue;
		}

		if (!RedirectToRemove.OldName.HasValidCharacters(RedirectToRemove.RedirectFlags) || !RedirectToRemove.NewName.HasValidCharacters(RedirectToRemove.RedirectFlags))
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("RemoveRedirect(%s) failed to remove redirect from %s to %s with invalid characters!"), *SourceString, *RedirectToRemove.OldName.ToString(), *RedirectToRemove.NewName.ToString());
			continue;
		}

		if (RedirectToRemove.NewName.PackageName != RedirectToRemove.OldName.PackageName && RedirectToRemove.OldName.OuterName != NAME_None)
		{
			UE_LOG(LogCoreRedirects, Error, TEXT("RemoveRedirect(%s) failed to remove redirect, it's not valid to modify package from %s to %s while specifying outer!"), *SourceString, *RedirectToRemove.OldName.ToString(), *RedirectToRemove.NewName.ToString());
			continue;
		}

		if (RedirectToRemove.IsSubstringMatch())
		{
			UE_LOG(LogCoreRedirects, Log, TEXT("RemoveRedirect(%s) has substring redirect %s, these are very slow and should be resolved as soon as possible!"), *SourceString, *RedirectToRemove.OldName.ToString());
		}

		bRemovedAny |= RemoveSingleRedirect(RedirectToRemove, SourceString);
	}

	return bRemovedAny;
}

bool FCoreRedirects::RemoveSingleRedirect(const FCoreRedirect& RedirectToRemove, const FString& SourceString)
{
	FRedirectNameMap* ExistingNameMap = RedirectTypeMap.Find(RedirectToRemove.RedirectFlags);
	if (!ExistingNameMap)
	{
		return false;
	}
	TArray<FCoreRedirect>* ExistingRedirects = ExistingNameMap->RedirectMap.Find(RedirectToRemove.GetSearchKey());
	if (!ExistingRedirects)
	{
		return false;
	}

	bool bRemovedRedirect = false;
	for (int32 ExistingRedirectIndex = 0; ExistingRedirectIndex < ExistingRedirects->Num(); ++ExistingRedirectIndex)
	{
		FCoreRedirect& ExistingRedirect = (*ExistingRedirects)[ExistingRedirectIndex];

		if (ExistingRedirect.IdenticalMatchRules(RedirectToRemove))
		{
			if (ExistingRedirect.NewName != RedirectToRemove.NewName)
			{
				// This isn't the redirect we were looking for... move on in case there's another match for our old name
				continue;
			}

			bRemovedRedirect = true;
			ExistingRedirects->RemoveAt(ExistingRedirectIndex);
			break;
		}
	}

	return bRemovedRedirect;
}

void FCoreRedirects::ValidateRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString)
{
	for (const FCoreRedirect& Redirect : Redirects)
	{
		if (Redirect.NewName.IsValid())
		{
			// If the new package is loaded but the new name isn't, this is very likely a bug
			// If the new package isn't loaded the redirect can't be validated either way, so report it for manual follow up
			UObject* NewPackage = FindObjectFast<UPackage>(nullptr, Redirect.NewName.PackageName);
			FString NewPath = Redirect.NewName.ToString();
			FString OldPath = Redirect.OldName.ToString();

			if (CheckRedirectFlagsMatch(Redirect.RedirectFlags, ECoreRedirectFlags::Type_Class))
			{
				if (Redirect.NewName.PackageName.IsNone())
				{
					UE_LOG(LogCoreRedirects, Warning, TEXT("ValidateRedirect(%s) has missing package for Class redirect from %s to %s!"), *SourceString, *OldPath, *NewPath);
				}
				else
				{
					const UClass* FindClass = FindObject<const UClass>(FTopLevelAssetPath(NewPath));
					if (!FindClass)
					{
						if (NewPackage)
						{
							UE_LOG(LogCoreRedirects, Error, TEXT("ValidateRedirect(%s) failed to find destination Class for redirect from %s to %s with loaded package!"), *SourceString, *OldPath, *NewPath);
						}
						else
						{
							UE_LOG(LogCoreRedirects, Log, TEXT("ValidateRedirect(%s) can't validate destination Class for redirect from %s to %s with unloaded package"), *SourceString, *OldPath, *NewPath);
						}
					}
				}
			}

			if (CheckRedirectFlagsMatch(Redirect.RedirectFlags, ECoreRedirectFlags::Type_Struct))
			{
				if (Redirect.NewName.PackageName.IsNone())
				{
					UE_LOG(LogCoreRedirects, Warning, TEXT("ValidateRedirect(%s) has missing package for Struct redirect from %s to %s!"), *SourceString, *OldPath, *NewPath);
				}
				else
				{
					const UScriptStruct* FindStruct = FindObject<const UScriptStruct>(FTopLevelAssetPath(NewPath));
					if (!FindStruct)
					{
						if (NewPackage)
						{
							UE_LOG(LogCoreRedirects, Error, TEXT("ValidateRedirect(%s) failed to find destination Struct for redirect from %s to %s with loaded package!"), *SourceString, *OldPath, *NewPath);
						}
						else
						{
							UE_LOG(LogCoreRedirects, Log, TEXT("ValidateRedirect(%s) can't validate destination Struct for redirect from %s to %s with unloaded package"), *SourceString, *OldPath, *NewPath);
						}
					}
				}
			}

			if (CheckRedirectFlagsMatch(Redirect.RedirectFlags, ECoreRedirectFlags::Type_Enum))
			{
				if (Redirect.NewName.PackageName.IsNone())
				{
					// If the name is the same that's fine and this is just for value redirects
					if (Redirect.NewName != Redirect.OldName)
					{
						UE_LOG(LogCoreRedirects, Warning, TEXT("ValidateRedirect(%s) has missing package for Enum redirect from %s to %s!"), *SourceString, *OldPath, *NewPath);
					}
				}
				else
				{
					const UEnum* FindEnum = FindObject<const UEnum>(FTopLevelAssetPath(NewPath));
					if (!FindEnum)
					{
						if (NewPackage)
						{
							UE_LOG(LogCoreRedirects, Error, TEXT("ValidateRedirect(%s) failed to find destination Enum for redirect from %s to %s with loaded package!"), *SourceString, *OldPath, *NewPath);
						}
						else
						{
							UE_LOG(LogCoreRedirects, Log, TEXT("ValidateRedirect(%s) can't validate destination Enum for redirect from %s to %s with unloaded package"), *SourceString, *OldPath, *NewPath);
						}
					}
				}
			}
		}
	}
}

void FCoreRedirects::ValidateAllRedirects()
{
	bValidatedOnce = true;

	// Validate all existing redirects

	for (const TPair<ECoreRedirectFlags, FRedirectNameMap>& Pair : RedirectTypeMap)
	{
		ECoreRedirectFlags PairFlags = Pair.Key;
		FString ListName = FString::Printf(TEXT("Type %d"), (int32)PairFlags);

		for (const TPair<FName, TArray<FCoreRedirect> >& ArrayPair : Pair.Value.RedirectMap)
		{
			ValidateRedirectList(ArrayPair.Value, ListName);
		}
	}
}

ECoreRedirectFlags FCoreRedirects::GetFlagsForTypeName(FName PackageName, FName TypeName)
{
	if (PackageName == GLongCoreUObjectPackageName)
	{
		if (TypeName == NAME_Class)
		{
			return ECoreRedirectFlags::Type_Class;
		}
		else if (TypeName == NAME_ScriptStruct)
		{
			return ECoreRedirectFlags::Type_Struct;
		}
		else if (TypeName == NAME_Enum)
		{
			return ECoreRedirectFlags::Type_Enum;
		}
		else if (TypeName == NAME_Package)
		{
			return ECoreRedirectFlags::Type_Package;
		}
		else if (TypeName == NAME_Function)
		{
			return ECoreRedirectFlags::Type_Function;
		}

		// If ending with property, it's a property
		if (TypeName.ToString().EndsWith(TEXT("Property")))
		{
			return ECoreRedirectFlags::Type_Property;
		}
	}

	// If ending with GeneratedClass this has to be a class subclass, some of these are in engine or plugins
	if (TypeName.ToString().EndsWith(TEXT("GeneratedClass")))
	{
		return ECoreRedirectFlags::Type_Class;
	}

	if (TypeName == NAME_UserDefinedEnum)
	{
		return ECoreRedirectFlags::Type_Enum;
	}

	return ECoreRedirectFlags::Type_Object;
}

ECoreRedirectFlags FCoreRedirects::GetFlagsForTypeClass(UClass *TypeClass)
{
	// Use Name version for consistency, if we can't figure it out from just the name it isn't safe
	return GetFlagsForTypeName(TypeClass->GetOutermost()->GetFName(), TypeClass->GetFName());
}

// We want to only load these redirects in editor builds, but Matinee needs them at runtime still 

#if UE_WITH_CORE_REDIRECTS

UE_DISABLE_OPTIMIZATION_SHIP

// The compiler doesn't like having a massive string table in a single function so split it up
#define CLASS_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Class, TEXT(OldName), TEXT(NewName))
#define CLASS_REDIRECT_INSTANCES(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Category_InstanceOnly, TEXT(OldName), TEXT(NewName))
#define STRUCT_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Struct, TEXT(OldName), TEXT(NewName))
#define ENUM_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, TEXT(OldName), TEXT(NewName))
#define PROPERTY_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Property, TEXT(OldName), TEXT(NewName))
#define FUNCTION_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Function, TEXT(OldName), TEXT(NewName))
#define PACKAGE_REDIRECT(OldName, NewName) Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Package, TEXT(OldName), TEXT(NewName))

static void RegisterNativeRedirects40(TArray<FCoreRedirect>& Redirects)
{
	// CLASS_REDIRECT("AnimTreeInstance", "/Script/Engine.AnimInstance"); // Leaving some of these disabled as they can cause issues with the reverse class lookup in asset registry scans 
	CLASS_REDIRECT("AnimationCompressionAlgorithm", "/Script/Engine.AnimCompress");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_BitwiseCompressOnly", "/Script/Engine.AnimCompress_BitwiseCompressOnly");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_LeastDestructive", "/Script/Engine.AnimCompress_LeastDestructive");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_PerTrackCompression", "/Script/Engine.AnimCompress_PerTrackCompression");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_RemoveEverySecondKey", "/Script/Engine.AnimCompress_RemoveEverySecondKey");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_RemoveLinearKeys", "/Script/Engine.AnimCompress_RemoveLinearKeys");
	CLASS_REDIRECT("AnimationCompressionAlgorithm_RemoveTrivialKeys", "/Script/Engine.AnimCompress_RemoveTrivialKeys");
	// CLASS_REDIRECT("BlueprintActorBase", "/Script/Engine.Actor");
	CLASS_REDIRECT("DefaultPawnMovement", "/Script/Engine.FloatingPawnMovement");
	CLASS_REDIRECT("DirectionalLightMovable", "/Script/Engine.DirectionalLight");
	CLASS_REDIRECT("DirectionalLightStatic", "/Script/Engine.DirectionalLight");
	CLASS_REDIRECT("DirectionalLightStationary", "/Script/Engine.DirectionalLight");
	CLASS_REDIRECT("DynamicBlockingVolume", "/Script/Engine.BlockingVolume");
	CLASS_REDIRECT("DynamicPhysicsVolume", "/Script/Engine.PhysicsVolume");
	CLASS_REDIRECT("DynamicTriggerVolume", "/Script/Engine.TriggerVolume");
	// CLASS_REDIRECT("GameInfo", "/Script/Engine.GameMode");
	// CLASS_REDIRECT("GameReplicationInfo", "/Script/Engine.GameState");
	CLASS_REDIRECT("InterpActor", "/Script/Engine.StaticMeshActor");
	CLASS_REDIRECT("K2Node_CallSuperFunction", "/Script/BlueprintGraph.K2Node_CallParentFunction");
	CLASS_REDIRECT("MaterialSpriteComponent", "/Script/Engine.MaterialBillboardComponent");
	CLASS_REDIRECT("MovementComp_Character", "/Script/Engine.CharacterMovementComponent");
	CLASS_REDIRECT("MovementComp_Projectile", "/Script/Engine.ProjectileMovementComponent");
	CLASS_REDIRECT("MovementComp_Rotating", "/Script/Engine.RotatingMovementComponent");
	CLASS_REDIRECT("NavAreaDefault", "/Script/NavigationSystem.NavArea_Default");
	CLASS_REDIRECT("NavAreaDefinition", "/Script/NavigationSystem.NavArea");
	CLASS_REDIRECT("NavAreaNull", "/Script/NavigationSystem.NavArea_Null");
	CLASS_REDIRECT("PhysicsActor", "/Script/Engine.StaticMeshActor");
	CLASS_REDIRECT("PhysicsBSJointActor", "/Script/Engine.PhysicsConstraintActor");
	CLASS_REDIRECT("PhysicsHingeActor", "/Script/Engine.PhysicsConstraintActor");
	CLASS_REDIRECT("PhysicsPrismaticActor", "/Script/Engine.PhysicsConstraintActor");
	// CLASS_REDIRECT("PlayerCamera", "/Script/Engine.PlayerCameraManager");
	// CLASS_REDIRECT("PlayerReplicationInfo", "/Script/Engine.PlayerState");
	CLASS_REDIRECT("PointLightMovable", "/Script/Engine.PointLight");
	CLASS_REDIRECT("PointLightStatic", "/Script/Engine.PointLight");
	CLASS_REDIRECT("PointLightStationary", "/Script/Engine.PointLight");
	CLASS_REDIRECT("RB_BSJointSetup", "/Script/Engine.PhysicsConstraintTemplate");
	CLASS_REDIRECT("RB_BodySetup", "/Script/Engine.BodySetup");
	CLASS_REDIRECT("RB_ConstraintActor", "/Script/Engine.PhysicsConstraintActor");
	CLASS_REDIRECT("RB_ConstraintComponent", "/Script/Engine.PhysicsConstraintComponent");
	CLASS_REDIRECT("RB_ConstraintSetup", "/Script/Engine.PhysicsConstraintTemplate");
	CLASS_REDIRECT("RB_Handle", "/Script/Engine.PhysicsHandleComponent");
	CLASS_REDIRECT("RB_HingeSetup", "/Script/Engine.PhysicsConstraintTemplate");
	CLASS_REDIRECT("RB_PrismaticSetup", "/Script/Engine.PhysicsConstraintTemplate");
	CLASS_REDIRECT("RB_RadialForceComponent", "/Script/Engine.RadialForceComponent");
	CLASS_REDIRECT("RB_SkelJointSetup", "/Script/Engine.PhysicsConstraintTemplate");
	CLASS_REDIRECT("RB_Thruster", "/Script/Engine.PhysicsThruster");
	CLASS_REDIRECT("RB_ThrusterComponent", "/Script/Engine.PhysicsThrusterComponent");
	CLASS_REDIRECT("SensingComponent", "/Script/AIModule.PawnSensingComponent");
	CLASS_REDIRECT("SingleAnimSkeletalActor", "/Script/Engine.SkeletalMeshActor");
	CLASS_REDIRECT("SingleAnimSkeletalComponent", "/Script/Engine.SkeletalMeshComponent");
	CLASS_REDIRECT("SkeletalMeshReplicatedComponent", "/Script/Engine.SkeletalMeshComponent");
	CLASS_REDIRECT("SkeletalPhysicsActor", "/Script/Engine.SkeletalMeshActor");
	CLASS_REDIRECT("SoundMode", "/Script/Engine.SoundMix");
	CLASS_REDIRECT("SpotLightMovable", "/Script/Engine.SpotLight");
	CLASS_REDIRECT("SpotLightStatic", "/Script/Engine.SpotLight");
	CLASS_REDIRECT("SpotLightStationary", "/Script/Engine.SpotLight");
	CLASS_REDIRECT("SpriteComponent", "/Script/Engine.BillboardComponent");
	CLASS_REDIRECT("StaticMeshReplicatedComponent", "/Script/Engine.StaticMeshComponent");
	CLASS_REDIRECT("VimBlueprint", "/Script/Engine.AnimBlueprint");
	CLASS_REDIRECT("VimGeneratedClass", "/Script/Engine.AnimBlueprintGeneratedClass");
	CLASS_REDIRECT("VimInstance", "/Script/Engine.AnimInstance");
	CLASS_REDIRECT("WorldInfo", "/Script/Engine.WorldSettings");
	CLASS_REDIRECT_INSTANCES("NavAreaMeta", "/Script/NavigationSystem.NavArea_Default");

	STRUCT_REDIRECT("VimDebugData", "/Script/Engine.AnimBlueprintDebugData");

	FUNCTION_REDIRECT("Actor.GetController", "Pawn.GetController");
	FUNCTION_REDIRECT("Actor.GetTouchingActors", "Actor.GetOverlappingActors");
	PROPERTY_REDIRECT("Actor.GetOverlappingActors.OutTouchingActors", "OverlappingActors");
	FUNCTION_REDIRECT("Actor.GetTouchingComponents", "Actor.GetOverlappingComponents");
	PROPERTY_REDIRECT("Actor.GetOverlappingComponents.TouchingComponents", "OverlappingComponents");
	FUNCTION_REDIRECT("Actor.HasTag", "Actor.ActorHasTag");
	FUNCTION_REDIRECT("Actor.ReceiveActorTouch", "Actor.ReceiveActorBeginOverlap");
	PROPERTY_REDIRECT("Actor.ReceiveActorBeginOverlap.Other", "OtherActor");
	FUNCTION_REDIRECT("Actor.ReceiveActorUntouch", "Actor.ReceiveActorEndOverlap");
	PROPERTY_REDIRECT("Actor.ReceiveActorEndOverlap.Other", "OtherActor");
	PROPERTY_REDIRECT("Actor.ReceiveHit.NormalForce", "NormalImpulse");
	FUNCTION_REDIRECT("Actor.SetActorHidden", "Actor.SetActorHiddenInGame");
	PROPERTY_REDIRECT("Actor.LifeSpan", "Actor.InitialLifeSpan");
	PROPERTY_REDIRECT("Actor.OnActorTouch", "OnActorBeginOverlap");
	PROPERTY_REDIRECT("Actor.OnActorUnTouch", "OnActorEndOverlap");

	FUNCTION_REDIRECT("AnimInstance.GetSequencePlayerLength", "GetAnimAssetPlayerLength");
	FUNCTION_REDIRECT("AnimInstance.GetSequencePlayerTimeFraction", "GetAnimAssetPlayerTimeFraction");
	FUNCTION_REDIRECT("AnimInstance.GetSequencePlayerTimeFromEnd", "GetAnimAssetPlayerTimeFromEnd");
	FUNCTION_REDIRECT("AnimInstance.GetSequencePlayerTimeFromEndFraction", "GetAnimAssetPlayerTimeFromEndFraction");
	FUNCTION_REDIRECT("AnimInstance.KismetInitializeAnimation", "AnimInstance.BlueprintInitializeAnimation");
	FUNCTION_REDIRECT("AnimInstance.KismetUpdateAnimation", "AnimInstance.BlueprintUpdateAnimation");
	PROPERTY_REDIRECT("AnimInstance.GetAnimAssetPlayerLength.Sequence", "AnimAsset");
	PROPERTY_REDIRECT("AnimInstance.GetAnimAssetPlayerTimeFraction.Sequence", "AnimAsset");
	PROPERTY_REDIRECT("AnimInstance.GetAnimAssetPlayerTimeFromEnd.Sequence", "AnimAsset");
	PROPERTY_REDIRECT("AnimInstance.GetAnimAssetPlayerTimeFromEndFraction.Sequence", "AnimAsset");
	PROPERTY_REDIRECT("AnimInstance.VimVertexAnims", "AnimInstance.VertexAnims");

	FUNCTION_REDIRECT("GameplayStatics.ClearSoundMode", "GameplayStatics.ClearSoundMixModifiers");
	FUNCTION_REDIRECT("GameplayStatics.GetGameInfo", "GetGameMode");
	FUNCTION_REDIRECT("GameplayStatics.GetGameReplicationInfo", "GetGameState");
	FUNCTION_REDIRECT("GameplayStatics.GetPlayerCamera", "GameplayStatics.GetPlayerCameraManager");
	FUNCTION_REDIRECT("GameplayStatics.K2_SetSoundMode", "GameplayStatics.SetBaseSoundMix");
	FUNCTION_REDIRECT("GameplayStatics.PopSoundMixModifier.InSoundMode", "InSoundMixModifier");
	FUNCTION_REDIRECT("GameplayStatics.PopSoundMode", "GameplayStatics.PopSoundMixModifier");
	FUNCTION_REDIRECT("GameplayStatics.PushSoundMixModifier.InSoundMode", "InSoundMixModifier");
	FUNCTION_REDIRECT("GameplayStatics.PushSoundMode", "GameplayStatics.PushSoundMixModifier");
	FUNCTION_REDIRECT("GameplayStatics.SetBaseSoundMix.InSoundMode", "InSoundMix");
	FUNCTION_REDIRECT("GameplayStatics.SetTimeDilation", "GameplayStatics.SetGlobalTimeDilation");

	FUNCTION_REDIRECT("KismetMaterialLibrary.CreateMaterialInstanceDynamic", "KismetMaterialLibrary.CreateDynamicMaterialInstance");
	FUNCTION_REDIRECT("KismetMaterialParameterCollectionLibrary.GetScalarParameterValue", "KismetMaterialLibrary.GetScalarParameterValue");
	FUNCTION_REDIRECT("KismetMaterialParameterCollectionLibrary.GetVectorParameterValue", "KismetMaterialLibrary.GetVectorParameterValue");
	FUNCTION_REDIRECT("KismetMaterialParameterCollectionLibrary.SetScalarParameterValue", "KismetMaterialLibrary.SetScalarParameterValue");
	FUNCTION_REDIRECT("KismetMaterialParameterCollectionLibrary.SetVectorParameterValue", "KismetMaterialLibrary.SetVectorParameterValue");

	FUNCTION_REDIRECT("KismetMathLibrary.BreakTransform.Translation", "Location");
	FUNCTION_REDIRECT("KismetMathLibrary.Conv_VectorToTransform.InTranslation", "InLocation");
	FUNCTION_REDIRECT("KismetMathLibrary.FRand", "RandomFloat");
	FUNCTION_REDIRECT("KismetMathLibrary.FRandFromStream", "RandomFloatFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.FRandRange", "RandomFloatInRange");
	FUNCTION_REDIRECT("KismetMathLibrary.FRandRangeFromStream", "RandomFloatInRangeFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.InverseTransformPosition", "KismetMathLibrary.InverseTransformLocation");
	PROPERTY_REDIRECT("KismetMathLibrary.InverseTransformLocation.Position", "Location");
	PROPERTY_REDIRECT("KismetMathLibrary.MakeTransform.Translation", "Location");
	FUNCTION_REDIRECT("KismetMathLibrary.Rand", "RandomInteger");
	FUNCTION_REDIRECT("KismetMathLibrary.RandBool", "RandomBool");
	FUNCTION_REDIRECT("KismetMathLibrary.RandBoolFromStream", "RandomBoolFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.RandFromStream", "RandomIntegerFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.RandRange", "RandomIntegerInRange");
	FUNCTION_REDIRECT("KismetMathLibrary.RandRangeFromStream", "RandomIntegerInRangeFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.RotRand", "RandomRotator");
	FUNCTION_REDIRECT("KismetMathLibrary.RotRandFromStream", "RandomRotatorFromStream");
	FUNCTION_REDIRECT("KismetMathLibrary.TransformPosition", "KismetMathLibrary.TransformLocation");
	PROPERTY_REDIRECT("KismetMathLibrary.TransformLocation.Position", "Location");
	FUNCTION_REDIRECT("KismetMathLibrary.VRand", "RandomUnitVector");
	FUNCTION_REDIRECT("KismetMathLibrary.VRandFromStream", "RandomUnitVectorFromStream");

	PROPERTY_REDIRECT("KismetSystemLibrary.CapsuleTraceMultiForObjects.ObjectsToTrace", "ObjectTypes");
	PROPERTY_REDIRECT("KismetSystemLibrary.CapsuleTraceSingleForObjects.ObjectsToTrace", "ObjectTypes");
	PROPERTY_REDIRECT("KismetSystemLibrary.LineTraceMultiForObjects.ObjectsToTrace", "ObjectTypes");
	PROPERTY_REDIRECT("KismetSystemLibrary.LineTraceSingleForObjects.ObjectsToTrace", "ObjectTypes");
	PROPERTY_REDIRECT("KismetSystemLibrary.PrintKismetWarning", "PrintWarning");
	PROPERTY_REDIRECT("KismetSystemLibrary.SphereTraceMultiForObjects.ObjectsToTrace", "ObjectTypes");
	PROPERTY_REDIRECT("KismetSystemLibrary.SphereTraceSingleForObjects.ObjectsToTrace", "ObjectTypes");

	FUNCTION_REDIRECT("AIController.ClearFocus", "AIController.K2_ClearFocus");
	FUNCTION_REDIRECT("AIController.SetFocalPoint", "AIController.K2_SetFocalPoint");
	FUNCTION_REDIRECT("AIController.SetFocus", "AIController.K2_SetFocus");
	FUNCTION_REDIRECT("ArrowComponent.SetArrowColor_New", "ArrowComponent.SetArrowColor");
	FUNCTION_REDIRECT("Character.Launch", "Character.LaunchCharacter");
	FUNCTION_REDIRECT("Controller.K2_GetActorRotation", "Controller.GetControlRotation");
	FUNCTION_REDIRECT("DecalActor.CreateMIDForDecal", "DecalActor.CreateDynamicMaterialInstance");
	FUNCTION_REDIRECT("DecalComponent.CreateMIDForDecal", "DecalComponent.CreateDynamicMaterialInstance");
	PROPERTY_REDIRECT("HUD.AddHitBox.InPos", "Position");
	PROPERTY_REDIRECT("HUD.AddHitBox.InPriority", "Priority");
	PROPERTY_REDIRECT("HUD.AddHitBox.InSize", "Size");
	PROPERTY_REDIRECT("HUD.AddHitBox.bInConsumesInput", "bConsumesInput");
	FUNCTION_REDIRECT("LevelScriptActor.BeginGame", "Actor.ReceiveBeginPlay");
	FUNCTION_REDIRECT("LevelScriptActor.LoadStreamLevel", "GameplayStatics.LoadStreamLevel");
	FUNCTION_REDIRECT("LevelScriptActor.OpenLevel", "GameplayStatics.OpenLevel");
	FUNCTION_REDIRECT("LevelScriptActor.UnloadStreamLevel", "GameplayStatics.UnloadStreamLevel");
	FUNCTION_REDIRECT("MovementComponent.ConstrainPositionToPlane", "MovementComponent.ConstrainLocationToPlane");
	PROPERTY_REDIRECT("MovementComponent.ConstrainLocationToPlane.Position", "Location");
	FUNCTION_REDIRECT("PlayerCameraManager.KismetUpdateCamera", "BlueprintUpdateCamera");
	FUNCTION_REDIRECT("PlayerController.AddLookUpInput", "PlayerController.AddPitchInput");
	FUNCTION_REDIRECT("PlayerController.AddTurnInput", "PlayerController.AddYawInput");
	PROPERTY_REDIRECT("PlayerController.DeprojectMousePositionToWorld.Direction", "WorldDirection");
	PROPERTY_REDIRECT("PlayerController.DeprojectMousePositionToWorld.WorldPosition", "WorldLocation");
	FUNCTION_REDIRECT("PrimitiveComponent.AddForceAtPosition", "PrimitiveComponent.AddForceAtLocation");
	PROPERTY_REDIRECT("PrimitiveComponent.AddForceAtLocation.Position", "Location");
	FUNCTION_REDIRECT("PrimitiveComponent.AddImpulseAtPosition", "PrimitiveComponent.AddImpulseAtLocation");
	PROPERTY_REDIRECT("PrimitiveComponent.AddImpulseAtLocation.Position", "Location");
	FUNCTION_REDIRECT("PrimitiveComponent.CreateAndSetMaterialInstanceDynamic", "PrimitiveComponent.CreateDynamicMaterialInstance");
	FUNCTION_REDIRECT("PrimitiveComponent.CreateAndSetMaterialInstanceDynamicFromMaterial", "PrimitiveComponent.CreateDynamicMaterialInstance");
	PROPERTY_REDIRECT("PrimitiveComponent.CreateDynamicMaterialInstance.Parent", "SourceMaterial");
	FUNCTION_REDIRECT("PrimitiveComponent.GetRBAngularVelocity", "GetPhysicsAngularVelocity");
	FUNCTION_REDIRECT("PrimitiveComponent.GetRBLinearVelocity", "GetPhysicsLinearVelocity");
	FUNCTION_REDIRECT("PrimitiveComponent.GetTouchingActors", "PrimitiveComponent.GetOverlappingActors");
	PROPERTY_REDIRECT("PrimitiveComponent.GetOverlappingActors.TouchingActors", "OverlappingActors");
	FUNCTION_REDIRECT("PrimitiveComponent.GetTouchingComponents", "PrimitiveComponent.GetOverlappingComponents");
	PROPERTY_REDIRECT("PrimitiveComponent.GetOverlappingComponents.TouchingComponents", "OverlappingComponents");
	FUNCTION_REDIRECT("PrimitiveComponent.KismetTraceComponent", "PrimitiveComponent.K2_LineTraceComponent");
	FUNCTION_REDIRECT("PrimitiveComponent.SetAllRBLinearVelocity", "SetAllPhysicsLinearVelocity");
	FUNCTION_REDIRECT("PrimitiveComponent.SetMovementChannel", "PrimitiveComponent.SetCollisionObjectType");
	FUNCTION_REDIRECT("PrimitiveComponent.SetRBAngularVelocity", "SetPhysicsAngularVelocity");
	FUNCTION_REDIRECT("PrimitiveComponent.SetRBLinearVelocity", "SetPhysicsLinearVelocity");
	FUNCTION_REDIRECT("ProjectileMovementComponent.StopMovement", "ProjectileMovementComponent.StopSimulating");
	FUNCTION_REDIRECT("SceneComponent.GetComponentToWorld", "K2_GetComponentToWorld");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.GetPlayRate", "SkeletalMeshComponent.GetPlayRate");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.GetPosition", "SkeletalMeshComponent.GetPosition");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.IsPlaying", "SkeletalMeshComponent.IsPlaying");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.Play", "SkeletalMeshComponent.Play");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.SetAnim", "SkeletalMeshComponent.SetAnimation");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.SetPlayRate", "SkeletalMeshComponent.SetPlayRate");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.SetPosition", "SkeletalMeshComponent.SetPosition");
	FUNCTION_REDIRECT("SingleAnimSkeletalComponent.Stop", "SkeletalMeshComponent.Stop");
	FUNCTION_REDIRECT("SkinnedMeshComponent.MatchRefBone", "SkinnedMeshComponent.GetBoneIndex");

	PROPERTY_REDIRECT("AnimNotifyEvent.Time", "AnimNotifyEvent.DisplayTime");
	PROPERTY_REDIRECT("AnimSequence.BasePose", "AnimSequence.RetargetSource");
	PROPERTY_REDIRECT("AudioComponent.PitchMultiplierMax", "AudioComponent.PitchModulationMax");
	PROPERTY_REDIRECT("AudioComponent.PitchMultiplierMin", "AudioComponent.PitchModulationMin");
	PROPERTY_REDIRECT("AudioComponent.VolumeMultiplierMax", "AudioComponent.VolumeModulationMax");
	PROPERTY_REDIRECT("AudioComponent.VolumeMultiplierMin", "AudioComponent.VolumeModulationMin");
	PROPERTY_REDIRECT("BodyInstance.MovementChannel", "BodyInstance.ObjectType");
	PROPERTY_REDIRECT("BranchingPoint.Time", "BranchingPoint.DisplayTime");
	PROPERTY_REDIRECT("CapsuleComponent.CapsuleHeight", "CapsuleComponent.CapsuleHalfHeight");
	PROPERTY_REDIRECT("CharacterMovementComponent.AccelRate", "CharacterMovementComponent.MaxAcceleration");
	PROPERTY_REDIRECT("CharacterMovementComponent.BrakingDeceleration", "CharacterMovementComponent.BrakingDecelerationWalking");
	PROPERTY_REDIRECT("CharacterMovementComponent.CrouchHeight", "CharacterMovementComponent.CrouchedHalfHeight");
	PROPERTY_REDIRECT("CollisionResponseContainer.Dynamic", "CollisionResponseContainer.WorldDynamic");
	PROPERTY_REDIRECT("CollisionResponseContainer.RigidBody", "CollisionResponseContainer.PhysicsBody");
	PROPERTY_REDIRECT("CollisionResponseContainer.Static", "CollisionResponseContainer.WorldStatic");
	PROPERTY_REDIRECT("Controller.PlayerReplicationInfo", "Controller.PlayerState");
	PROPERTY_REDIRECT("DefaultPawn.DefaultPawnMovement", "DefaultPawn.MovementComponent");
	PROPERTY_REDIRECT("DirectionalLightComponent.MovableWholeSceneDynamicShadowRadius", "DirectionalLightComponent.DynamicShadowDistanceMovableLight");
	PROPERTY_REDIRECT("DirectionalLightComponent.StationaryWholeSceneDynamicShadowRadius", "DirectionalLightComponent.DynamicShadowDistanceStationaryLight");
	PROPERTY_REDIRECT("FloatingPawnMovement.AccelRate", "FloatingPawnMovement.Acceleration");
	PROPERTY_REDIRECT("FloatingPawnMovement.DecelRate", "FloatingPawnMovement.Deceleration");
	PROPERTY_REDIRECT("GameMode.GameReplicationInfoClass", "GameMode.GameStateClass");
	PROPERTY_REDIRECT("GameMode.PlayerReplicationInfoClass", "GameMode.PlayerStateClass");
	PROPERTY_REDIRECT("GameState.GameClass", "GameState.GameModeClass");
	PROPERTY_REDIRECT("K2Node_TransitionRuleGetter.AssociatedSequencePlayerNode", "K2Node_TransitionRuleGetter.AssociatedAnimAssetPlayerNode");
	PROPERTY_REDIRECT("LightComponent.InverseSquaredFalloff", "PointLightComponent.bUseInverseSquaredFalloff");
	PROPERTY_REDIRECT("LightComponentBase.Brightness", "LightComponentBase.Intensity");
	PROPERTY_REDIRECT("Material.RefractionBias", "Material.RefractionDepthBias");
	PROPERTY_REDIRECT("MaterialEditorInstanceConstant.RefractionBias", "MaterialEditorInstanceConstant.RefractionDepthBias");
	PROPERTY_REDIRECT("NavLinkProxy.NavLinks", "NavLinkProxy.PointLinks");
	PROPERTY_REDIRECT("NavLinkProxy.NavSegmentLinks", "NavLinkProxy.SegmentLinks");
	PROPERTY_REDIRECT("Pawn.ControllerClass", "Pawn.AIControllerClass");
	PROPERTY_REDIRECT("Pawn.PlayerReplicationInfo", "Pawn.PlayerState");
	PROPERTY_REDIRECT("PawnSensingComponent.SightCounterInterval", "PawnSensingComponent.SensingInterval");
	PROPERTY_REDIRECT("PawnSensingComponent.bWantsSeePlayerNotify", "PawnSensingComponent.bSeePawns");
	PROPERTY_REDIRECT("PlayerController.LookRightScale", "PlayerController.InputYawScale");
	PROPERTY_REDIRECT("PlayerController.LookUpScale", "PlayerController.InputPitchScale");
	PROPERTY_REDIRECT("PlayerController.InputYawScale", "PlayerController.InputYawScale_DEPRECATED");
	PROPERTY_REDIRECT("PlayerController.InputPitchScale", "PlayerController.InputPitchScale_DEPRECATED");
	PROPERTY_REDIRECT("PlayerController.InputRollScale", "PlayerController.InputRollScale_DEPRECATED");
	PROPERTY_REDIRECT("PlayerController.PlayerCamera", "PlayerController.PlayerCameraManager");
	PROPERTY_REDIRECT("PlayerController.PlayerCameraClass", "PlayerController.PlayerCameraManagerClass");
	PROPERTY_REDIRECT("PointLightComponent.Radius", "PointLightComponent.AttenuationRadius");
	PROPERTY_REDIRECT("PostProcessSettings.ExposureOffset", "PostProcessSettings.AutoExposureBias");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptationHighPercent", "PostProcessSettings.AutoExposureHighPercent");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptationLowPercent", "PostProcessSettings.AutoExposureLowPercent");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptationMaxBrightness", "PostProcessSettings.AutoExposureMaxBrightness");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptationMinBrightness", "PostProcessSettings.AutoExposureMinBrightness");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptionSpeedDown", "PostProcessSettings.AutoExposureSpeedDown");
	PROPERTY_REDIRECT("PostProcessSettings.EyeAdaptionSpeedUp", "PostProcessSettings.AutoExposureSpeedUp");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_ExposureOffset", "PostProcessSettings.bOverride_AutoExposureBias");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptationHighPercent", "PostProcessSettings.bOverride_AutoExposureHighPercent");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptationLowPercent", "PostProcessSettings.bOverride_AutoExposureLowPercent");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptationMaxBrightness", "PostProcessSettings.bOverride_AutoExposureMaxBrightness");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptationMinBrightness", "PostProcessSettings.bOverride_AutoExposureMinBrightness");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptionSpeedDown", "PostProcessSettings.bOverride_AutoExposureSpeedDown");
	PROPERTY_REDIRECT("PostProcessSettings.bOverride_EyeAdaptionSpeedUp", "PostProcessSettings.bOverride_AutoExposureSpeedUp");
	PROPERTY_REDIRECT("SceneComponent.ModifyFrequency", "SceneComponent.Mobility");
	PROPERTY_REDIRECT("SceneComponent.RelativeTranslation", "SceneComponent.RelativeLocation");
	PROPERTY_REDIRECT("SceneComponent.bAbsoluteTranslation", "SceneComponent.bAbsoluteLocation");
	PROPERTY_REDIRECT("SceneComponent.bComputeBoundsOnceDuringCook", "SceneComponent.bComputeBoundsOnceForGame");
	PROPERTY_REDIRECT("SkeletalMeshComponent.AnimationBlueprint", "SkeletalMeshComponent.AnimBlueprintGeneratedClass");
	PROPERTY_REDIRECT("SlateBrush.TextureName", "SlateBrush.ResourceName");
	PROPERTY_REDIRECT("SlateBrush.TextureObject", "SlateBrush.ResourceObject");
	PROPERTY_REDIRECT("WorldSettings.DefaultGameType", "WorldSettings.DefaultGameMode");

	FCoreRedirect& PointLightComponent = CLASS_REDIRECT("PointLightComponent", "/Script/Engine.PointLightComponent");
	PointLightComponent.ValueChanges.Add(TEXT("PointLightComponent0"), TEXT("LightComponent0"));

	FCoreRedirect& DirectionalLightComponent = CLASS_REDIRECT("DirectionalLightComponent", "/Script/Engine.DirectionalLightComponent");
	DirectionalLightComponent.ValueChanges.Add(TEXT("DirectionalLightComponent0"), TEXT("LightComponent0"));

	FCoreRedirect& SpotLightComponent = CLASS_REDIRECT("SpotLightComponent", "/Script/Engine.SpotLightComponent");
	SpotLightComponent.ValueChanges.Add(TEXT("SpotLightComponent0"), TEXT("LightComponent0"));

	FCoreRedirect& ETransitionGetterType = ENUM_REDIRECT("ETransitionGetterType", "/Script/AnimGraph.ETransitionGetter");
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_ArbitraryState_GetBlendWeight"), TEXT("ETransitionGetter::ArbitraryState_GetBlendWeight"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_CurrentState_ElapsedTime"), TEXT("ETransitionGetter::CurrentState_ElapsedTime"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_CurrentState_GetBlendWeight"), TEXT("ETransitionGetter::CurrentState_GetBlendWeight"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_CurrentTransitionDuration"), TEXT("ETransitionGetter::CurrentTransitionDuration"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_SequencePlayer_GetCurrentTime"), TEXT("ETransitionGetter::AnimationAsset_GetCurrentTime"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_SequencePlayer_GetCurrentTimeFraction"), TEXT("ETransitionGetter::AnimationAsset_GetCurrentTimeFraction"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_SequencePlayer_GetLength"), TEXT("ETransitionGetter::AnimationAsset_GetLength"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_SequencePlayer_GetTimeFromEnd"), TEXT("ETransitionGetter::AnimationAsset_GetTimeFromEnd"));
	ETransitionGetterType.ValueChanges.Add(TEXT("TGT_SequencePlayer_GetTimeFromEndFraction"), TEXT("ETransitionGetter::AnimationAsset_GetTimeFromEndFraction"));

	FCoreRedirect& EModifyFrequency = ENUM_REDIRECT("EModifyFrequency", "/Script/Engine.EComponentMobility");
	EModifyFrequency.ValueChanges.Add(TEXT("MF_Dynamic"), TEXT("EComponentMobility::Movable"));
	EModifyFrequency.ValueChanges.Add(TEXT("MF_OccasionallyModified"), TEXT("EComponentMobility::Stationary"));
	EModifyFrequency.ValueChanges.Add(TEXT("MF_Static"), TEXT("EComponentMobility::Static"));

	FCoreRedirect& EAttachLocationType = ENUM_REDIRECT("EAttachLocationType", "/Script/Engine.EAttachLocation");
	EAttachLocationType.ValueChanges.Add(TEXT("EAttachLocationType_AbsoluteWorld"), TEXT("EAttachLocation::KeepWorldPosition"));
	EAttachLocationType.ValueChanges.Add(TEXT("EAttachLocationType_RelativeOffset"), TEXT("EAttachLocation::KeepRelativeOffset"));
	EAttachLocationType.ValueChanges.Add(TEXT("EAttachLocationType_SnapTo"), TEXT("EAttachLocation::SnapToTarget"));

	FCoreRedirect& EAxis = ENUM_REDIRECT("EAxis", "/Script/CoreUObject.EAxis");
	EAxis.ValueChanges.Add(TEXT("AXIS_BLANK"), TEXT("EAxis::None"));
	EAxis.ValueChanges.Add(TEXT("AXIS_NONE"), TEXT("EAxis::None"));
	EAxis.ValueChanges.Add(TEXT("AXIS_X"), TEXT("EAxis::X"));
	EAxis.ValueChanges.Add(TEXT("AXIS_Y"), TEXT("EAxis::Y"));
	EAxis.ValueChanges.Add(TEXT("AXIS_Z"), TEXT("EAxis::Z"));

	FCoreRedirect& EMaxConcurrentResolutionRule = ENUM_REDIRECT("EMaxConcurrentResolutionRule", "/Script/Engine.EMaxConcurrentResolutionRule");
	EMaxConcurrentResolutionRule.ValueChanges.Add(TEXT("EMaxConcurrentResolutionRule::StopFarthest"), TEXT("EMaxConcurrentResolutionRule::StopFarthestThenPreventNew"));

	FCoreRedirect& EParticleEventType = ENUM_REDIRECT("EParticleEventType", "/Script/Engine.EParticleEventType");
	EParticleEventType.ValueChanges.Add(TEXT("EPET_Kismet"), TEXT("EPET_Blueprint"));

	FCoreRedirect& ETranslucencyLightingMode = ENUM_REDIRECT("ETranslucencyLightingMode", "/Script/Engine.ETranslucencyLightingMode");
	ETranslucencyLightingMode.ValueChanges.Add(TEXT("TLM_PerPixel"), TEXT("TLM_VolumetricDirectional"));
	ETranslucencyLightingMode.ValueChanges.Add(TEXT("TLM_PerPixelNonDirectional"), TEXT("TLM_VolumetricNonDirectional"));
}

static void RegisterNativeRedirects46(TArray<FCoreRedirect>& Redirects)
{
	// 4.1-4.4 

	CLASS_REDIRECT("K2Node_CastToInterface", "/Script/BlueprintGraph.K2Node_DynamicCast");
	CLASS_REDIRECT("K2Node_MathExpression", "/Script/BlueprintGraph.K2Node_MathExpression");
	CLASS_REDIRECT("EmitterSpawnable", "/Script/Engine.Emitter");
	CLASS_REDIRECT("SlateWidgetStyleAsset", "/Script/SlateCore.SlateWidgetStyleAsset");
	CLASS_REDIRECT("SlateWidgetStyleContainerBase", "/Script/SlateCore.SlateWidgetStyleContainerBase");
	CLASS_REDIRECT("SmartNavLinkComponent", "/Script/NavigationSystem.NavLinkCustomComponent");
	CLASS_REDIRECT("WidgetBlueprint", "/Script/UMGEditor.WidgetBlueprint");

	PROPERTY_REDIRECT("AnimNotify.Received_Notify.AnimSeq", "Animation");
	PROPERTY_REDIRECT("AnimNotifyState.Received_NotifyBegin.AnimSeq", "Animation");
	PROPERTY_REDIRECT("AnimNotifyState.Received_NotifyEnd.AnimSeq", "Animation");
	PROPERTY_REDIRECT("AnimNotifyState.Received_NotifyTick.AnimSeq", "Animation");
	FUNCTION_REDIRECT("Character.IsJumping", "Character.IsJumpProvidingForce");
	PROPERTY_REDIRECT("CharacterMovementComponent.AddImpulse.InMomentum", "Impulse");
	PROPERTY_REDIRECT("CharacterMovementComponent.AddImpulse.bMassIndependent", "bVelocityChange");
	FUNCTION_REDIRECT("CharacterMovementComponent.AddMomentum", "CharacterMovementComponent.AddImpulse");
	FUNCTION_REDIRECT("Controller.GetControlledPawn", "Controller.K2_GetPawn");
	FUNCTION_REDIRECT("DefaultPawn.LookUp", "Pawn.AddControllerPitchInput");
	FUNCTION_REDIRECT("DefaultPawn.Turn", "Pawn.AddControllerYawInput");
	FUNCTION_REDIRECT("KismetSystemLibrary.EXPERIMENTAL_ShowGameCenterLeaderboard", "KismetSystemLibrary.ShowPlatformSpecificLeaderboardScreen");
	FUNCTION_REDIRECT("MovementComponent.GetMaxSpeedModifier", "MovementComponent.K2_GetMaxSpeedModifier");
	FUNCTION_REDIRECT("MovementComponent.GetModifiedMaxSpeed", "MovementComponent.K2_GetModifiedMaxSpeed");
	FUNCTION_REDIRECT("Pawn.AddLookUpInput", "Pawn.AddControllerPitchInput");
	FUNCTION_REDIRECT("Pawn.AddPitchInput", "Pawn.AddControllerPitchInput");
	FUNCTION_REDIRECT("Pawn.AddRollInput", "Pawn.AddControllerRollInput");
	FUNCTION_REDIRECT("Pawn.AddTurnInput", "Pawn.AddControllerYawInput");
	FUNCTION_REDIRECT("Pawn.AddYawInput", "Pawn.AddControllerYawInput");
	FUNCTION_REDIRECT("PawnMovementComponent.StopActiveMovement", "NavMovementComponent.StopActiveMovement");
	FUNCTION_REDIRECT("PointLightComponent.SetRadius", "PointLightComponent.SetAttenuationRadius");
	FUNCTION_REDIRECT("SkeletalMeshComponent.SetAnimBlueprint", "SkeletalMeshComponent.SetAnimInstanceClass");
	FUNCTION_REDIRECT("SkeletalMeshComponent.SetAnimClass", "SkeletalMeshComponent.SetAnimInstanceClass");
	PROPERTY_REDIRECT("SkeletalMeshComponent.SetAnimInstanceClass.NewBlueprint", "NewClass");

	PROPERTY_REDIRECT("StringClassReference.ClassName", "StringClassReference.AssetLongPathname");
	PROPERTY_REDIRECT("Material.LightingModel", "Material.ShadingModel");
	PROPERTY_REDIRECT("MaterialInstanceBasePropertyOverrides.LightingModel", "MaterialInstanceBasePropertyOverrides.ShadingModel");
	PROPERTY_REDIRECT("MaterialInstanceBasePropertyOverrides.bOverride_LightingModel", "MaterialInstanceBasePropertyOverrides.bOverride_ShadingModel");
	PROPERTY_REDIRECT("PassiveSoundMixModifier.VolumeThreshold", "PassiveSoundMixModifier.MinVolumeThreshold");
	PROPERTY_REDIRECT("PrimitiveComponent.CanBeCharacterBase", "PrimitiveComponent.CanCharacterStepUpOn");
	PROPERTY_REDIRECT("SkeletalMeshLODInfo.DisplayFactor", "SkeletalMeshLODInfo.ScreenSize");
	PROPERTY_REDIRECT("SplineMeshComponent.SplineXDir", "SplineMeshComponent.SplineUpDir");
	PROPERTY_REDIRECT("TextureFactory.LightingModel", "TextureFactory.ShadingModel");

	FCoreRedirect& EKinematicBonesUpdateToPhysics = ENUM_REDIRECT("EKinematicBonesUpdateToPhysics", "/Script/Engine.EKinematicBonesUpdateToPhysics");
	EKinematicBonesUpdateToPhysics.ValueChanges.Add(TEXT("EKinematicBonesUpdateToPhysics::SkipFixedAndSimulatingBones"), TEXT("EKinematicBonesUpdateToPhysics::SkipAllBones"));

	FCoreRedirect& EMaterialLightingModel = ENUM_REDIRECT("EMaterialLightingModel", "/Script/Engine.EMaterialShadingModel");
	EMaterialLightingModel.ValueChanges.Add(TEXT("MLM_DefaultLit"), TEXT("MSM_DefaultLit"));
	EMaterialLightingModel.ValueChanges.Add(TEXT("MLM_PreintegratedSkin"), TEXT("MSM_PreintegratedSkin"));
	EMaterialLightingModel.ValueChanges.Add(TEXT("MLM_Subsurface"), TEXT("MSM_Subsurface"));
	EMaterialLightingModel.ValueChanges.Add(TEXT("MLM_Unlit"), TEXT("MSM_Unlit"));

	FCoreRedirect& ESmartNavLinkDir = ENUM_REDIRECT("ESmartNavLinkDir", "/Script/Engine.ENavLinkDirection");
	ESmartNavLinkDir.ValueChanges.Add(TEXT("ESmartNavLinkDir::BothWays"), TEXT("ENavLinkDirection::BothWays"));
	ESmartNavLinkDir.ValueChanges.Add(TEXT("ESmartNavLinkDir::OneWay"), TEXT("ENavLinkDirection::LeftToRight"));

	// 4.5

	CLASS_REDIRECT("AIController", "/Script/AIModule.AIController");
	CLASS_REDIRECT("AIResourceInterface", "/Script/AIModule.AIResourceInterface");
	CLASS_REDIRECT("AISystem", "/Script/AIModule.AISystem");
	CLASS_REDIRECT("BTAuxiliaryNode", "/Script/AIModule.BTAuxiliaryNode");
	CLASS_REDIRECT("BTCompositeNode", "/Script/AIModule.BTCompositeNode");
	CLASS_REDIRECT("BTComposite_Selector", "/Script/AIModule.BTComposite_Selector");
	CLASS_REDIRECT("BTComposite_Sequence", "/Script/AIModule.BTComposite_Sequence");
	CLASS_REDIRECT("BTComposite_SimpleParallel", "/Script/AIModule.BTComposite_SimpleParallel");
	CLASS_REDIRECT("BTDecorator", "/Script/AIModule.BTDecorator");
	CLASS_REDIRECT("BTDecorator_Blackboard", "/Script/AIModule.BTDecorator_Blackboard");
	CLASS_REDIRECT("BTDecorator_BlackboardBase", "/Script/AIModule.BTDecorator_BlackboardBase");
	CLASS_REDIRECT("BTDecorator_BlueprintBase", "/Script/AIModule.BTDecorator_BlueprintBase");
	CLASS_REDIRECT("BTDecorator_CompareBBEntries", "/Script/AIModule.BTDecorator_CompareBBEntries");
	CLASS_REDIRECT("BTDecorator_ConeCheck", "/Script/AIModule.BTDecorator_ConeCheck");
	CLASS_REDIRECT("BTDecorator_Cooldown", "/Script/AIModule.BTDecorator_Cooldown");
	CLASS_REDIRECT("BTDecorator_DoesPathExist", "/Script/AIModule.BTDecorator_DoesPathExist");
	CLASS_REDIRECT("BTDecorator_ForceSuccess", "/Script/AIModule.BTDecorator_ForceSuccess");
	CLASS_REDIRECT("BTDecorator_KeepInCone", "/Script/AIModule.BTDecorator_KeepInCone");
	CLASS_REDIRECT("BTDecorator_Loop", "/Script/AIModule.BTDecorator_Loop");
	CLASS_REDIRECT("BTDecorator_Optional", "/Script/AIModule.BTDecorator_ForceSuccess");
	CLASS_REDIRECT("BTDecorator_ReachedMoveGoal", "/Script/AIModule.BTDecorator_ReachedMoveGoal");
	CLASS_REDIRECT("BTDecorator_TimeLimit", "/Script/AIModule.BTDecorator_TimeLimit");
	CLASS_REDIRECT("BTFunctionLibrary", "/Script/AIModule.BTFunctionLibrary");
	CLASS_REDIRECT("BTNode", "/Script/AIModule.BTNode");
	CLASS_REDIRECT("BTService", "/Script/AIModule.BTService");
	CLASS_REDIRECT("BTService_BlackboardBase", "/Script/AIModule.BTService_BlackboardBase");
	CLASS_REDIRECT("BTService_BlueprintBase", "/Script/AIModule.BTService_BlueprintBase");
	CLASS_REDIRECT("BTService_DefaultFocus", "/Script/AIModule.BTService_DefaultFocus");
	CLASS_REDIRECT("BTTaskNode", "/Script/AIModule.BTTaskNode");
	CLASS_REDIRECT("BTTask_BlackboardBase", "/Script/AIModule.BTTask_BlackboardBase");
	CLASS_REDIRECT("BTTask_BlueprintBase", "/Script/AIModule.BTTask_BlueprintBase");
	CLASS_REDIRECT("BTTask_MakeNoise", "/Script/AIModule.BTTask_MakeNoise");
	CLASS_REDIRECT("BTTask_MoveDirectlyToward", "/Script/AIModule.BTTask_MoveDirectlyToward");
	CLASS_REDIRECT("BTTask_MoveTo", "/Script/AIModule.BTTask_MoveTo");
	CLASS_REDIRECT("BTTask_PlaySound", "/Script/AIModule.BTTask_PlaySound");
	CLASS_REDIRECT("BTTask_RunBehavior", "/Script/AIModule.BTTask_RunBehavior");
	CLASS_REDIRECT("BTTask_RunEQSQuery", "/Script/AIModule.BTTask_RunEQSQuery");
	CLASS_REDIRECT("BTTask_Wait", "/Script/AIModule.BTTask_Wait");
	CLASS_REDIRECT("BehaviorTree", "/Script/AIModule.BehaviorTree");
	CLASS_REDIRECT("BehaviorTreeComponent", "/Script/AIModule.BehaviorTreeComponent");
	CLASS_REDIRECT("BehaviorTreeManager", "/Script/AIModule.BehaviorTreeManager");
	CLASS_REDIRECT("BehaviorTreeTypes", "/Script/AIModule.BehaviorTreeTypes");
	CLASS_REDIRECT("BlackboardComponent", "/Script/AIModule.BlackboardComponent");
	CLASS_REDIRECT("BlackboardData", "/Script/AIModule.BlackboardData");
	CLASS_REDIRECT("BlackboardKeyType", "/Script/AIModule.BlackboardKeyType");
	CLASS_REDIRECT("BlackboardKeyType_Bool", "/Script/AIModule.BlackboardKeyType_Bool");
	CLASS_REDIRECT("BlackboardKeyType_Class", "/Script/AIModule.BlackboardKeyType_Class");
	CLASS_REDIRECT("BlackboardKeyType_Enum", "/Script/AIModule.BlackboardKeyType_Enum");
	CLASS_REDIRECT("BlackboardKeyType_Float", "/Script/AIModule.BlackboardKeyType_Float");
	CLASS_REDIRECT("BlackboardKeyType_Int", "/Script/AIModule.BlackboardKeyType_Int");
	CLASS_REDIRECT("BlackboardKeyType_Name", "/Script/AIModule.BlackboardKeyType_Name");
	CLASS_REDIRECT("BlackboardKeyType_NativeEnum", "/Script/AIModule.BlackboardKeyType_NativeEnum");
	CLASS_REDIRECT("BlackboardKeyType_Object", "/Script/AIModule.BlackboardKeyType_Object");
	CLASS_REDIRECT("BlackboardKeyType_String", "/Script/AIModule.BlackboardKeyType_String");
	CLASS_REDIRECT("BlackboardKeyType_Vector", "/Script/AIModule.BlackboardKeyType_Vector");
	CLASS_REDIRECT("BrainComponent", "/Script/AIModule.BrainComponent");
	CLASS_REDIRECT("CrowdAgentInterface", "/Script/AIModule.CrowdAgentInterface");
	CLASS_REDIRECT("CrowdFollowingComponent", "/Script/AIModule.CrowdFollowingComponent");
	CLASS_REDIRECT("CrowdManager", "/Script/AIModule.CrowdManager");
	CLASS_REDIRECT("EQSQueryResultSourceInterface", "/Script/AIModule.EQSQueryResultSourceInterface");
	CLASS_REDIRECT("EQSRenderingComponent", "/Script/AIModule.EQSRenderingComponent");
	CLASS_REDIRECT("EQSTestingPawn", "/Script/AIModule.EQSTestingPawn");
	CLASS_REDIRECT("EnvQuery", "/Script/AIModule.EnvQuery");
	CLASS_REDIRECT("EnvQueryContext", "/Script/AIModule.EnvQueryContext");
	CLASS_REDIRECT("EnvQueryContext_BlueprintBase", "/Script/AIModule.EnvQueryContext_BlueprintBase");
	CLASS_REDIRECT("EnvQueryContext_Item", "/Script/AIModule.EnvQueryContext_Item");
	CLASS_REDIRECT("EnvQueryContext_Querier", "/Script/AIModule.EnvQueryContext_Querier");
	CLASS_REDIRECT("EnvQueryGenerator", "/Script/AIModule.EnvQueryGenerator");
	CLASS_REDIRECT("EnvQueryGenerator_Composite", "/Script/AIModule.EnvQueryGenerator_Composite");
	CLASS_REDIRECT("EnvQueryGenerator_OnCircle", "/Script/AIModule.EnvQueryGenerator_OnCircle");
	CLASS_REDIRECT("EnvQueryGenerator_PathingGrid", "/Script/AIModule.EnvQueryGenerator_PathingGrid");
	CLASS_REDIRECT("EnvQueryGenerator_ProjectedPoints", "/Script/AIModule.EnvQueryGenerator_ProjectedPoints");
	CLASS_REDIRECT("EnvQueryGenerator_SimpleGrid", "/Script/AIModule.EnvQueryGenerator_SimpleGrid");
	CLASS_REDIRECT("EnvQueryItemType", "/Script/AIModule.EnvQueryItemType");
	CLASS_REDIRECT("EnvQueryItemType_Actor", "/Script/AIModule.EnvQueryItemType_Actor");
	CLASS_REDIRECT("EnvQueryItemType_ActorBase", "/Script/AIModule.EnvQueryItemType_ActorBase");
	CLASS_REDIRECT("EnvQueryItemType_Direction", "/Script/AIModule.EnvQueryItemType_Direction");
	CLASS_REDIRECT("EnvQueryItemType_Point", "/Script/AIModule.EnvQueryItemType_Point");
	CLASS_REDIRECT("EnvQueryItemType_VectorBase", "/Script/AIModule.EnvQueryItemType_VectorBase");
	CLASS_REDIRECT("EnvQueryManager", "/Script/AIModule.EnvQueryManager");
	CLASS_REDIRECT("EnvQueryOption", "/Script/AIModule.EnvQueryOption");
	CLASS_REDIRECT("EnvQueryTest", "/Script/AIModule.EnvQueryTest");
	CLASS_REDIRECT("EnvQueryTest_Distance", "/Script/AIModule.EnvQueryTest_Distance");
	CLASS_REDIRECT("EnvQueryTest_Dot", "/Script/AIModule.EnvQueryTest_Dot");
	CLASS_REDIRECT("EnvQueryTest_Pathfinding", "/Script/AIModule.EnvQueryTest_Pathfinding");
	CLASS_REDIRECT("EnvQueryTest_Trace", "/Script/AIModule.EnvQueryTest_Trace");
	CLASS_REDIRECT("EnvQueryTypes", "/Script/AIModule.EnvQueryTypes");
	CLASS_REDIRECT("KismetAIAsyncTaskProxy", "/Script/AIModule.AIAsyncTaskBlueprintProxy");
	CLASS_REDIRECT("KismetAIHelperLibrary", "/Script/AIModule.AIBlueprintHelperLibrary");
	CLASS_REDIRECT("PathFollowingComponent", "/Script/AIModule.PathFollowingComponent");
	CLASS_REDIRECT("PawnSensingComponent", "/Script/AIModule.PawnSensingComponent");

	STRUCT_REDIRECT("SReply", "/Script/UMG.EventReply");

	PROPERTY_REDIRECT("Actor.AddTickPrerequisiteActor.DependentActor", "PrerequisiteActor");
	FUNCTION_REDIRECT("Actor.AttachRootComponentTo", "Actor.K2_AttachRootComponentTo");
	FUNCTION_REDIRECT("Actor.AttachRootComponentToActor", "Actor.K2_AttachRootComponentToActor");
	FUNCTION_REDIRECT("Actor.SetTickPrerequisite", "Actor.AddTickPrerequisiteActor");
	PROPERTY_REDIRECT("BTTask_MoveDirectlyToward.bForceMoveToLocation", "bDisablePathUpdateOnGoalLocationChange");
	PROPERTY_REDIRECT("KismetSystemLibrary.DrawDebugPlane.Loc", "Location");
	PROPERTY_REDIRECT("KismetSystemLibrary.DrawDebugPlane.P", "PlaneCoordinates");
	FUNCTION_REDIRECT("KismetSystemLibrary.EXPERIMENTAL_CloseAdBanner", "KismetSystemLibrary.ForceCloseAdBanner");
	FUNCTION_REDIRECT("KismetSystemLibrary.EXPERIMENTAL_HideAdBanner", "KismetSystemLibrary.HideAdBanner");
	FUNCTION_REDIRECT("KismetSystemLibrary.EXPERIMENTAL_ShowAdBanner", "KismetSystemLibrary.ShowAdBanner");
	FUNCTION_REDIRECT("LightComponent.SetBrightness", "LightComponent.SetIntensity");
	FUNCTION_REDIRECT("NavigationPath.GetPathLenght", "NavigationPath.GetPathLength");
	FUNCTION_REDIRECT("Pawn.GetMovementInputVector", "Pawn.K2_GetMovementInputVector");
	FUNCTION_REDIRECT("PawnMovementComponent.GetInputVector", "PawnMovementComponent.GetPendingInputVector");
	FUNCTION_REDIRECT("SceneComponent.AttachTo", "SceneComponent.K2_AttachTo");
	FUNCTION_REDIRECT("SkyLightComponent.SetBrightness", "SkyLightComponent.SetIntensity");

	// 4.6

	CLASS_REDIRECT("ControlPointMeshComponent", "/Script/Landscape.ControlPointMeshComponent");
	CLASS_REDIRECT("Landscape", "/Script/Landscape.Landscape");
	CLASS_REDIRECT("LandscapeComponent", "/Script/Landscape.LandscapeComponent");
	CLASS_REDIRECT("LandscapeGizmoActiveActor", "/Script/Landscape.LandscapeGizmoActiveActor");
	CLASS_REDIRECT("LandscapeGizmoActor", "/Script/Landscape.LandscapeGizmoActor");
	CLASS_REDIRECT("LandscapeGizmoRenderComponent", "/Script/Landscape.LandscapeGizmoRenderComponent");
	CLASS_REDIRECT("LandscapeHeightfieldCollisionComponent", "/Script/Landscape.LandscapeHeightfieldCollisionComponent");
	CLASS_REDIRECT("LandscapeInfo", "/Script/Landscape.LandscapeInfo");
	CLASS_REDIRECT("LandscapeInfoMap", "/Script/Landscape.LandscapeInfoMap");
	CLASS_REDIRECT("LandscapeLayerInfoObject", "/Script/Landscape.LandscapeLayerInfoObject");
	CLASS_REDIRECT("LandscapeMaterialInstanceConstant", "/Script/Landscape.LandscapeMaterialInstanceConstant");
	CLASS_REDIRECT("LandscapeMeshCollisionComponent", "/Script/Landscape.LandscapeMeshCollisionComponent");
	CLASS_REDIRECT("LandscapeProxy", "/Script/Landscape.LandscapeProxy");
	CLASS_REDIRECT("LandscapeSplineControlPoint", "/Script/Landscape.LandscapeSplineControlPoint");
	CLASS_REDIRECT("LandscapeSplineSegment", "/Script/Landscape.LandscapeSplineSegment");
	CLASS_REDIRECT("LandscapeSplinesComponent", "/Script/Landscape.LandscapeSplinesComponent");
	CLASS_REDIRECT("MaterialExpressionLandscapeLayerBlend", "/Script/Landscape.MaterialExpressionLandscapeLayerBlend");
	CLASS_REDIRECT("MaterialExpressionLandscapeLayerCoords", "/Script/Landscape.MaterialExpressionLandscapeLayerCoords");
	CLASS_REDIRECT("MaterialExpressionLandscapeLayerSwitch", "/Script/Landscape.MaterialExpressionLandscapeLayerSwitch");
	CLASS_REDIRECT("MaterialExpressionLandscapeLayerWeight", "/Script/Landscape.MaterialExpressionLandscapeLayerWeight");
	CLASS_REDIRECT("MaterialExpressionLandscapeVisibilityMask", "/Script/Landscape.MaterialExpressionLandscapeVisibilityMask");
	CLASS_REDIRECT("MaterialExpressionTerrainLayerCoords", "/Script/Landscape.MaterialExpressionLandscapeLayerCoords");
	CLASS_REDIRECT("MaterialExpressionTerrainLayerSwitch", "/Script/Landscape.MaterialExpressionLandscapeLayerSwitch");
	CLASS_REDIRECT("MaterialExpressionTerrainLayerWeight", "/Script/Landscape.MaterialExpressionLandscapeLayerWeight");
	CLASS_REDIRECT("ReverbVolume", "/Script/Engine.AudioVolume");
	CLASS_REDIRECT("ReverbVolumeToggleable", "/Script/Engine.AudioVolume");

	STRUCT_REDIRECT("KeyboardEvent", "/Script/SlateCore.KeyEvent");
	STRUCT_REDIRECT("KeyboardFocusEvent", "/Script/SlateCore.FocusEvent");

	FUNCTION_REDIRECT("Actor.AddActorLocalOffset", "Actor.K2_AddActorLocalOffset");
	FUNCTION_REDIRECT("Actor.AddActorLocalRotation", "Actor.K2_AddActorLocalRotation");
	FUNCTION_REDIRECT("Actor.AddActorLocalTransform", "Actor.K2_AddActorLocalTransform");
	FUNCTION_REDIRECT("Actor.AddActorLocalTranslation", "Actor.K2_AddActorLocalOffset");
	PROPERTY_REDIRECT("Actor.K2_AddActorLocalOffset.DeltaTranslation", "DeltaLocation");
	FUNCTION_REDIRECT("Actor.AddActorWorldOffset", "Actor.K2_AddActorWorldOffset");
	FUNCTION_REDIRECT("Actor.AddActorWorldRotation", "Actor.K2_AddActorWorldRotation");
	FUNCTION_REDIRECT("Actor.AddActorWorldTransform", "Actor.K2_AddActorWorldTransform");
	FUNCTION_REDIRECT("Actor.SetActorLocation", "Actor.K2_SetActorLocation");
	FUNCTION_REDIRECT("Actor.SetActorLocationAndRotation", "Actor.K2_SetActorLocationAndRotation");
	FUNCTION_REDIRECT("Actor.SetActorRelativeLocation", "Actor.K2_SetActorRelativeLocation");
	PROPERTY_REDIRECT("Actor.K2_SetActorRelativeLocation.NewRelativeTranslation", "NewRelativeLocation");
	FUNCTION_REDIRECT("Actor.SetActorRelativeRotation", "Actor.K2_SetActorRelativeRotation");
	FUNCTION_REDIRECT("Actor.SetActorRelativeTransform", "Actor.K2_SetActorRelativeTransform");
	FUNCTION_REDIRECT("Actor.SetActorRelativeTranslation", "Actor.K2_SetActorRelativeLocation");
	FUNCTION_REDIRECT("Actor.SetActorTransform", "Actor.K2_SetActorTransform");
	FUNCTION_REDIRECT("BTFunctionLibrary.GetBlackboard", "BTFunctionLibrary.GetOwnersBlackboard");
	FUNCTION_REDIRECT("KismetMathLibrary.NearlyEqual_RotatorRotator", "EqualEqual_RotatorRotator");
	FUNCTION_REDIRECT("KismetMathLibrary.NearlyEqual_VectorVector", "EqualEqual_VectorVector");
	FUNCTION_REDIRECT("KismetMathLibrary.ProjectOnTo", "ProjectVectorOnToVector");
	PROPERTY_REDIRECT("KismetMathLibrary.ProjectVectorOnToVector.X", "V");
	PROPERTY_REDIRECT("KismetMathLibrary.ProjectVectorOnToVector.Y", "Target");
	PROPERTY_REDIRECT("LightComponent.SetIntensity.NewBrightness", "NewIntensity");
	FUNCTION_REDIRECT("SceneComponent.AddLocalOffset", "SceneComponent.K2_AddLocalOffset");
	FUNCTION_REDIRECT("SceneComponent.AddLocalRotation", "SceneComponent.K2_AddLocalRotation");
	FUNCTION_REDIRECT("SceneComponent.AddLocalTransform", "SceneComponent.K2_AddLocalTransform");
	FUNCTION_REDIRECT("SceneComponent.AddLocalTranslation", "SceneComponent.K2_AddLocalOffset");
	PROPERTY_REDIRECT("SceneComponent.K2_AddLocalOffset.DeltaTranslation", "DeltaLocation");
	FUNCTION_REDIRECT("SceneComponent.AddRelativeLocation", "SceneComponent.K2_AddRelativeLocation");
	PROPERTY_REDIRECT("SceneComponent.K2_AddRelativeLocation.DeltaTranslation", "DeltaLocation");
	FUNCTION_REDIRECT("SceneComponent.AddRelativeRotation", "SceneComponent.K2_AddRelativeRotation");
	FUNCTION_REDIRECT("SceneComponent.AddRelativeTranslation", "SceneComponent.K2_AddRelativeLocation");
	FUNCTION_REDIRECT("SceneComponent.AddWorldOffset", "SceneComponent.K2_AddWorldOffset");
	FUNCTION_REDIRECT("SceneComponent.AddWorldRotation", "SceneComponent.K2_AddWorldRotation");
	FUNCTION_REDIRECT("SceneComponent.AddWorldTransform", "SceneComponent.K2_AddWorldTransform");
	FUNCTION_REDIRECT("SceneComponent.SetRelativeLocation", "SceneComponent.K2_SetRelativeLocation");
	PROPERTY_REDIRECT("SceneComponent.K2_SetRelativeLocation.NewTranslation", "NewLocation");
	FUNCTION_REDIRECT("SceneComponent.SetRelativeRotation", "SceneComponent.K2_SetRelativeRotation");
	FUNCTION_REDIRECT("SceneComponent.SetRelativeTransform", "SceneComponent.K2_SetRelativeTransform");
	FUNCTION_REDIRECT("SceneComponent.SetRelativeTranslation", "SceneComponent.K2_SetRelativeLocation");
	FUNCTION_REDIRECT("SceneComponent.SetWorldLocation", "SceneComponent.K2_SetWorldLocation");
	PROPERTY_REDIRECT("SceneComponent.K2_SetWorldLocation.NewTranslation", "NewLocation");
	FUNCTION_REDIRECT("SceneComponent.SetWorldRotation", "SceneComponent.K2_SetWorldRotation");
	FUNCTION_REDIRECT("SceneComponent.SetWorldTransform", "SceneComponent.K2_SetWorldTransform");
	FUNCTION_REDIRECT("SceneComponent.SetWorldTranslation", "SceneComponent.K2_SetWorldLocation");
	PROPERTY_REDIRECT("SkyLightComponent.SetIntensity.NewBrightness", "NewIntensity");
}

static void RegisterNativeRedirects49(TArray<FCoreRedirect>& Redirects)
{
	// 4.7

	CLASS_REDIRECT("EdGraphNode_Comment", "/Script/UnrealEd.EdGraphNode_Comment");
	CLASS_REDIRECT("K2Node_Comment", "/Script/UnrealEd.EdGraphNode_Comment");
	CLASS_REDIRECT("VimBlueprintFactory", "/Script/UnrealEd.AnimBlueprintFactory");

	FUNCTION_REDIRECT("Actor.SetTickEnabled", "Actor.SetActorTickEnabled");
	PROPERTY_REDIRECT("UserWidget.OnKeyboardFocusLost.InKeyboardFocusEvent", "InFocusEvent");
	PROPERTY_REDIRECT("UserWidget.OnControllerAnalogValueChanged.ControllerEvent", "InAnalogInputEvent");
	PROPERTY_REDIRECT("UserWidget.OnControllerButtonPressed.ControllerEvent", "InKeyEvent");
	PROPERTY_REDIRECT("UserWidget.OnControllerButtonReleased.ControllerEvent", "InKeyEvent");
	PROPERTY_REDIRECT("UserWidget.OnKeyDown.InKeyboardEvent", "InKeyEvent");
	PROPERTY_REDIRECT("UserWidget.OnKeyUp.InKeyboardEvent", "InKeyEvent");
	PROPERTY_REDIRECT("UserWidget.OnKeyboardFocusReceived.InKeyboardFocusEvent", "InFocusEvent");
	PROPERTY_REDIRECT("UserWidget.OnPreviewKeyDown.InKeyboardEvent", "InKeyEvent");
	
	PROPERTY_REDIRECT("MeshComponent.Materials", "MeshComponent.OverrideMaterials");
	PROPERTY_REDIRECT("Pawn.AutoPossess", "Pawn.AutoPossessPlayer");

	FCoreRedirect& ECollisionChannel = ENUM_REDIRECT("ECollisionChannel", "/Script/Engine.ECollisionChannel");
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_Default"), TEXT("ECC_Visibility"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_Dynamic"), TEXT("ECC_WorldDynamic"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_OverlapAll"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_OverlapAllDynamic"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_OverlapAllDynamic_Deprecated"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_OverlapAllStatic"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_OverlapAllStatic_Deprecated"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_PawnMovement"), TEXT("ECC_Pawn"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_RigidBody"), TEXT("ECC_PhysicsBody"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_RigidBodyInteractable"), TEXT("ECC_PhysicsBody"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_TouchAll"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_TouchAllDynamic"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_TouchAllStatic"), TEXT("ECC_OverlapAll_Deprecated"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_VehicleMovement"), TEXT("ECC_Vehicle"));
	ECollisionChannel.ValueChanges.Add(TEXT("ECC_WorldTrace"), TEXT("ECC_WorldStatic"));

	// 4.8

	CLASS_REDIRECT("EditorGameAgnosticSettings", "/Script/UnrealEd.EditorSettings");
	CLASS_REDIRECT("FoliageType", "/Script/Foliage.FoliageType");
	CLASS_REDIRECT("FoliageType_InstancedStaticMesh", "/Script/Foliage.FoliageType_InstancedStaticMesh");
	CLASS_REDIRECT("InstancedFoliageActor", "/Script/Foliage.InstancedFoliageActor");
	CLASS_REDIRECT("InstancedFoliageSettings", "/Script/Foliage.FoliageType_InstancedStaticMesh");
	CLASS_REDIRECT("InteractiveFoliageComponent", "/Script/Foliage.InteractiveFoliageComponent");
	CLASS_REDIRECT("ProceduralFoliage", "/Script/Foliage.ProceduralFoliageSpawner");
	CLASS_REDIRECT("ProceduralFoliageActor", "/Script/Foliage.ProceduralFoliageVolume");
	
	STRUCT_REDIRECT("ProceduralFoliageTypeData", "/Script/Foliage.FoliageTypeObject");

	FCoreRedirect& EComponentCreationMethod = ENUM_REDIRECT("EComponentCreationMethod", "/Script/Engine.EComponentCreationMethod");
	EComponentCreationMethod.ValueChanges.Add(TEXT("EComponentCreationMethod::ConstructionScript"), TEXT("EComponentCreationMethod::SimpleConstructionScript"));

	FCoreRedirect& EConstraintTransform = ENUM_REDIRECT("EConstraintTransform", "/Script/Engine.EConstraintTransform");
	EConstraintTransform.ValueChanges.Add(TEXT("EConstraintTransform::Absoluate"), TEXT("EConstraintTransform::Absolute"));

	FCoreRedirect& ELockedAxis = ENUM_REDIRECT("ELockedAxis", "/Script/Engine.EDOFMode");
	ELockedAxis.ValueChanges.Add(TEXT("Custom"), TEXT("EDOFMode::CustomPlane"));
	ELockedAxis.ValueChanges.Add(TEXT("X"), TEXT("EDOFMode::YZPlane"));
	ELockedAxis.ValueChanges.Add(TEXT("Y"), TEXT("EDOFMode::XZPlane"));
	ELockedAxis.ValueChanges.Add(TEXT("Z"), TEXT("EDOFMode::XYPlane"));

	FCoreRedirect& EEndPlayReason = ENUM_REDIRECT("EEndPlayReason", "/Script/Engine.EEndPlayReason");
	EEndPlayReason.ValueChanges.Add(TEXT("EEndPlayReason::ActorDestroyed"), TEXT("EEndPlayReason::Destroyed"));

	FUNCTION_REDIRECT("ActorComponent.ReceiveInitializeComponent", "ActorComponent.ReceiveBeginPlay");
	FUNCTION_REDIRECT("ActorComponent.ReceiveUninitializeComponent", "ActorComponent.ReceiveEndPlay");

	PROPERTY_REDIRECT("CameraComponent.bUseControllerViewRotation", "CameraComponent.bUsePawnControlRotation");
	PROPERTY_REDIRECT("CameraComponent.bUsePawnViewRotation", "CameraComponent.bUsePawnControlRotation");
	PROPERTY_REDIRECT("CharacterMovementComponent.AirSpeed", "CharacterMovementComponent.MaxFlySpeed");
	PROPERTY_REDIRECT("CharacterMovementComponent.CrouchedSpeedPercent", "CharacterMovementComponent.CrouchedSpeedMultiplier");
	PROPERTY_REDIRECT("CharacterMovementComponent.GroundSpeed", "CharacterMovementComponent.MaxWalkSpeed");
	PROPERTY_REDIRECT("CharacterMovementComponent.JumpZ", "CharacterMovementComponent.JumpZVelocity");
	PROPERTY_REDIRECT("CharacterMovementComponent.WaterSpeed", "CharacterMovementComponent.MaxSwimSpeed");
	PROPERTY_REDIRECT("CharacterMovementComponent.bCrouchMovesCharacterDown", "CharacterMovementComponent.bCrouchMaintainsBaseLocation");
	PROPERTY_REDIRECT("CharacterMovementComponent.bOrientToMovement", "CharacterMovementComponent.bOrientRotationToMovement");
	PROPERTY_REDIRECT("FunctionalTest.GetAdditionalTestFinishedMessage", "FunctionalTest.OnAdditionalTestFinishedMessageRequest");
	PROPERTY_REDIRECT("FunctionalTest.WantsToRunAgain", "FunctionalTest.OnWantsReRunCheck");
	PROPERTY_REDIRECT("ProjectileMovementComponent.Speed", "ProjectileMovementComponent.InitialSpeed");
	PROPERTY_REDIRECT("SpringArmComponent.bUseControllerViewRotation", "SpringArmComponent.bUsePawnControlRotation");
	PROPERTY_REDIRECT("SpringArmComponent.bUsePawnViewRotation", "SpringArmComponent.bUsePawnControlRotation");
	PROPERTY_REDIRECT("BodyInstance.CustomLockedAxis", "BodyInstance.CustomDOFPlaneNormal");
	PROPERTY_REDIRECT("BodyInstance.LockedAxisMode", "BodyInstance.DOFMode");
	PROPERTY_REDIRECT("CharacterMovementComponent.NavMeshProjectionCapsuleHeightScaleDown", "CharacterMovementComponent.NavMeshProjectionHeightScaleDown");
	PROPERTY_REDIRECT("CharacterMovementComponent.NavMeshProjectionCapsuleHeightScaleUp", "CharacterMovementComponent.NavMeshProjectionHeightScaleUp");
	PROPERTY_REDIRECT("LandscapeSplineControlPoint.MeshComponent", "LandscapeSplineControlPoint.LocalMeshComponent");
	PROPERTY_REDIRECT("LandscapeSplineSegment.MeshComponents", "LandscapeSplineSegment.LocalMeshComponents");
	PROPERTY_REDIRECT("ProceduralFoliageComponent.Overlap", "ProceduralFoliageComponent.TileOverlap");
	PROPERTY_REDIRECT("ProceduralFoliageComponent.ProceduralFoliage", "ProceduralFoliageComponent.FoliageSpawner");
	PROPERTY_REDIRECT("ProceduralFoliageSpawner.Types", "ProceduralFoliageSpawner.FoliageTypes");
	PROPERTY_REDIRECT("SpriteGeometryCollection.Polygons", "SpriteGeometryCollection.Shapes");

	// 4.9

	CLASS_REDIRECT("EditorUserSettings", "/Script/UnrealEd.EditorPerProjectUserSettings");	
	CLASS_REDIRECT("MovieScene", "/Script/MovieScene.MovieScene");
	CLASS_REDIRECT("MovieScene3DTransformSection", "/Script/MovieSceneTracks.MovieScene3DTransformSection");
	CLASS_REDIRECT("MovieScene3DTransformTrack", "/Script/MovieSceneTracks.MovieScene3DTransformTrack");
	CLASS_REDIRECT("MovieSceneAudioSection", "/Script/MovieSceneTracks.MovieSceneAudioSection");
	CLASS_REDIRECT("MovieSceneAudioTrack", "/Script/MovieSceneTracks.MovieSceneAudioTrack");
	CLASS_REDIRECT("MovieSceneBoolTrack", "/Script/MovieSceneTracks.MovieSceneBoolTrack");
	CLASS_REDIRECT("MovieSceneByteSection", "/Script/MovieSceneTracks.MovieSceneByteSection");
	CLASS_REDIRECT("MovieSceneByteTrack", "/Script/MovieSceneTracks.MovieSceneByteTrack");
	CLASS_REDIRECT("MovieSceneColorSection", "/Script/MovieSceneTracks.MovieSceneColorSection");
	CLASS_REDIRECT("MovieSceneColorTrack", "/Script/MovieSceneTracks.MovieSceneColorTrack");
	CLASS_REDIRECT("MovieSceneFloatSection", "/Script/MovieSceneTracks.MovieSceneFloatSection");
	CLASS_REDIRECT("MovieSceneFloatTrack", "/Script/MovieSceneTracks.MovieSceneFloatTrack");
	CLASS_REDIRECT("MovieSceneParticleSection", "/Script/MovieSceneTracks.MovieSceneParticleSection");
	CLASS_REDIRECT("MovieSceneParticleTrack", "/Script/MovieSceneTracks.MovieSceneParticleTrack");
	CLASS_REDIRECT("MovieScenePropertyTrack", "/Script/MovieSceneTracks.MovieScenePropertyTrack");
	CLASS_REDIRECT("MovieSceneSection", "/Script/MovieScene.MovieSceneSection");
	CLASS_REDIRECT("MovieSceneTrack", "/Script/MovieScene.MovieSceneTrack");

	PACKAGE_REDIRECT("/Script/MovieSceneCore", "/Script/MovieScene");
	PACKAGE_REDIRECT("/Script/MovieSceneCoreTypes", "/Script/MovieSceneTracks");

	STRUCT_REDIRECT("Anchors", "/Script/Slate.Anchors");
	STRUCT_REDIRECT("AnimNode_BoneDrivenController", "/Script/AnimGraphRuntime.AnimNode_BoneDrivenController");
	STRUCT_REDIRECT("AnimNode_CopyBone", "/Script/AnimGraphRuntime.AnimNode_CopyBone");
	STRUCT_REDIRECT("AnimNode_HandIKRetargeting", "/Script/AnimGraphRuntime.AnimNode_HandIKRetargeting");
	STRUCT_REDIRECT("AnimNode_LookAt", "/Script/AnimGraphRuntime.AnimNode_LookAt");
	STRUCT_REDIRECT("AnimNode_ModifyBone", "/Script/AnimGraphRuntime.AnimNode_ModifyBone");
	STRUCT_REDIRECT("AnimNode_RotationMultiplier", "/Script/AnimGraphRuntime.AnimNode_RotationMultiplier");
	STRUCT_REDIRECT("AnimNode_SkeletalControlBase", "/Script/AnimGraphRuntime.AnimNode_SkeletalControlBase");
	STRUCT_REDIRECT("AnimNode_SpringBone", "/Script/AnimGraphRuntime.AnimNode_SpringBone");
	STRUCT_REDIRECT("AnimNode_Trail", "/Script/AnimGraphRuntime.AnimNode_Trail");
	STRUCT_REDIRECT("AnimNode_TwoBoneIK", "/Script/AnimGraphRuntime.AnimNode_TwoBoneIK");
	STRUCT_REDIRECT("MovieSceneEditorData", "/Script/MovieScene.MovieSceneEditorData");
	STRUCT_REDIRECT("MovieSceneObjectBinding", "/Script/MovieScene.MovieSceneBinding");
	STRUCT_REDIRECT("MovieScenePossessable", "/Script/MovieScene.MovieScenePossessable");
	STRUCT_REDIRECT("MovieSceneSpawnable", "/Script/MovieScene.MovieSceneSpawnable");
	STRUCT_REDIRECT("SpritePolygon", "/Script/Paper2D.SpriteGeometryShape");
	STRUCT_REDIRECT("SpritePolygonCollection", "/Script/Paper2D.SpriteGeometryCollection");

	FUNCTION_REDIRECT("GameplayStatics.PlayDialogueAttached", "GameplayStatics.SpawnDialogueAttached");
	FUNCTION_REDIRECT("GameplayStatics.PlaySoundAttached", "GameplayStatics.SpawnSoundAttached");
	FUNCTION_REDIRECT("KismetMathLibrary.BreakRot", "KismetMathLibrary.BreakRotator");
	FUNCTION_REDIRECT("KismetMathLibrary.MakeRot", "KismetMathLibrary.MakeRotator");
	FUNCTION_REDIRECT("KismetMathLibrary.MapRange", "KismetMathLibrary.MapRangeUnclamped");
	FUNCTION_REDIRECT("PrimitiveComponent.GetMoveIgnoreActors", "PrimitiveComponent.CopyArrayOfMoveIgnoreActors");
	FUNCTION_REDIRECT("SplineComponent.GetNumSplinePoints", "SplineComponent.GetNumberOfSplinePoints");
	FUNCTION_REDIRECT("VerticalBox.AddChildVerticalBox", "VerticalBox.AddChildToVerticalBox");
	
	PROPERTY_REDIRECT("ComponentKey.VariableGuid", "ComponentKey.AssociatedGuid");
	PROPERTY_REDIRECT("ComponentKey.VariableName", "ComponentKey.SCSVariableName");
	PROPERTY_REDIRECT("FoliageType.InitialMaxAge", "FoliageType.MaxInitialAge");
	PROPERTY_REDIRECT("FoliageType.bGrowsInShade", "FoliageType.bSpawnsInShade");
	PROPERTY_REDIRECT("MemberReference.MemberParentClass", "MemberReference.MemberParent");
	PROPERTY_REDIRECT("SimpleMemberReference.MemberParentClass", "SimpleMemberReference.MemberParent");
	PROPERTY_REDIRECT("SoundNodeModPlayer.SoundMod", "SoundNodeModPlayer.SoundModAssetPtr");
	PROPERTY_REDIRECT("SoundNodeWavePlayer.SoundWave", "SoundNodeWavePlayer.SoundWaveAssetPtr");

	ENUM_REDIRECT("ECheckBoxState", "/Script/SlateCore.ECheckBoxState");
	ENUM_REDIRECT("ESlateCheckBoxState", "/Script/SlateCore.ECheckBoxState");
	ENUM_REDIRECT("EAxisOption", "/Script/Engine.EAxisOption");
	ENUM_REDIRECT("EBoneAxis", "/Script/Engine.EBoneAxis");
	ENUM_REDIRECT("EBoneModificationMode", "/Script/AnimGraphRuntime.EBoneModificationMode");
	ENUM_REDIRECT("EComponentType", "/Script/Engine.EComponentType");
	ENUM_REDIRECT("EInterpolationBlend", "/Script/AnimGraphRuntime.EInterpolationBlend");
}

UE_ENABLE_OPTIMIZATION_SHIP

void FCoreRedirects::RegisterNativeRedirects()
{
	// Registering redirects here instead of in baseengine.ini is faster to parse and can clean up the ini, but is not required
	TArray<FCoreRedirect> Redirects;

	RegisterNativeRedirects40(Redirects);
	RegisterNativeRedirects46(Redirects);
	RegisterNativeRedirects49(Redirects);

	// 4.10 and later are in baseengine.ini

	AddRedirectList(Redirects, TEXT("RegisterNativeRedirects"));
}
#else
void FCoreRedirects::RegisterNativeRedirects()
{
}
#endif // UE_WITH_CORE_REDIRECTS
