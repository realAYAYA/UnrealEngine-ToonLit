// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreRedirects.h: Object/Class/Field redirects read from ini files or registered at startup
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class IPakFile;
class UClass;
struct FTopLevelAssetPath;

#define WITH_COREREDIRECTS_MULTITHREAD_WARNING !UE_BUILD_SHIPPING && !IS_PROGRAM && !WITH_EDITOR

DECLARE_LOG_CATEGORY_EXTERN(LogCoreRedirects, Log, All);

/** 
 * Flags describing the type and properties of this redirect
 */
enum class ECoreRedirectFlags : uint32
{
	None = 0,

	// Core type of the Thing being redirected, multiple can be set.  A Query will only find Redirects that have at least one of the same Type bits set.
	Type_Object =			0x00000001, // UObject
	Type_Class =			0x00000002, // UClass
	Type_Struct =			0x00000004, // UStruct
	Type_Enum =				0x00000008, // UEnum
	Type_Function =			0x00000010, // UFunction
	Type_Property =			0x00000020, // FProperty
	Type_Package =			0x00000040, // UPackage
	Type_AllMask =			0x0000FFFF, // Bit mask of all possible Types

	// Category flags.  A Query will only match Redirects that have the same value for every category bit.
	Category_InstanceOnly = 0x00010000, // Only redirect instances of this type, not the type itself
	Category_Removed =		0x00020000, // This type was explicitly removed, new name isn't valid
	Category_AllMask =		0x00FF0000, // Bit mask of all possible Categories

	// Option flags.  Does not behave as a bit-match between Queries and Redirects.  Each one specifies a custom rule for how FCoreRedirects handles the Redirect.
	Option_MatchSubstring = 0x01000000, // Does a slow substring match
	Option_MissingLoad =	0x02000000, // An automatically-created redirect that was created in response to a missing Thing during load. Redirect will be removed if and when the Thing is loaded.
	Option_AllMask =		0xFF000000, // Bit mask of all possible Options
};
ENUM_CLASS_FLAGS(ECoreRedirectFlags);

enum class ECoreRedirectMatchFlags
{
	None = 0,
	/** The passed-in CoreRedirectObjectName has null fields in Package, Outer, or Name, and should still be allowed to match
	 against redirectors that were created with a full Package.[Outer:]Name. */
	AllowPartialMatch = (1 << 0),
};
ENUM_CLASS_FLAGS(ECoreRedirectMatchFlags);

/**
 * An object path extracted into component names for matching. TODO merge with FSoftObjectPath?
 */
struct FCoreRedirectObjectName
{
	/** Raw name of object */
	FName ObjectName;

	/** String of outer chain, may be empty */
	FName OuterName;

	/** Package this was in before, may be extracted out of OldName */
	FName PackageName;

	/** Default to invalid names */
	FCoreRedirectObjectName() = default;

	/** Construct from FNames that are already expanded */
	FCoreRedirectObjectName(FName InObjectName, FName InOuterName, FName InPackageName)
		: ObjectName(InObjectName), OuterName(InOuterName), PackageName(InPackageName)
	{

	}

	COREUOBJECT_API FCoreRedirectObjectName(const FTopLevelAssetPath& TopLevelAssetPath);

	/** Construct from a path string, this handles full paths with packages, or partial paths without */
	COREUOBJECT_API FCoreRedirectObjectName(const FString& InString);

	/** Construct from object in memory */
	COREUOBJECT_API FCoreRedirectObjectName(const class UObject* Object);

	/** Creates FString version */
	COREUOBJECT_API FString ToString() const;

	/** Sets back to invalid state */
	COREUOBJECT_API void Reset();

	/** Checks for exact equality */
	bool operator==(const FCoreRedirectObjectName& Other) const
	{
		return ObjectName == Other.ObjectName && OuterName == Other.OuterName && PackageName == Other.PackageName;
	}

	bool operator!=(const FCoreRedirectObjectName& Other) const
	{
		return !(*this == Other);
	}

	/** Flags for the Matches function. These flags overlap but are lower-level than ECoreRedirectMatchFlags. */
	enum class EMatchFlags
	{
		None = 0,
		/** Do not match if LHS (aka *this) has null fields that RHS (aka Other) does not. Default is to match. */
		DisallowPartialLHSMatch = (1 << 0),
		/** Match even if RHS (aka Other) has null fields that LHS (aka *this) does not. Default is to NOT match. */
		AllowPartialRHSMatch = (1 << 1),
		/**
		 * LHS fields (aka *this) are searchstrings; RHS (aka Other) fields are searched for that substring.
		 * Default is to require a complete string match LHS == RHS.
		 * This flag makes the match more expensive and should be avoided when possible.
		 */
		CheckSubString = (1 << 2),
	};
	/** Returns true if the passed in name matches requirements. */
	COREUOBJECT_API bool Matches(const FCoreRedirectObjectName& Other, EMatchFlags MatchFlags = EMatchFlags::None) const;

	UE_DEPRECATED(5.1, "Use EMatchFlags::CheckSubString to pass in bCheckSubstring=true.")
	COREUOBJECT_API bool Matches(const FCoreRedirectObjectName& Other, bool bCheckSubstring) const;

	/** Returns integer of degree of match. 0 if doesn't match at all, higher integer for better matches */
	COREUOBJECT_API int32 MatchScore(const FCoreRedirectObjectName& Other) const;

	/** Fills in any empty fields on this with the corresponding fields from Other. */
	COREUOBJECT_API void UnionFieldsInline(const FCoreRedirectObjectName& Other);

	/** Returns the name used as the key into the acceleration map */
	FName GetSearchKey(ECoreRedirectFlags Type) const
	{
		if ((Type & ECoreRedirectFlags::Option_MatchSubstring) == ECoreRedirectFlags::Option_MatchSubstring)
		{
			static FName SubstringName = FName(TEXT("*SUBSTRING*"));

			// All substring matches pass initial test as they need to be manually checked
			return SubstringName;
		}

		if ((Type & ECoreRedirectFlags::Type_Package) == ECoreRedirectFlags::Type_Package)
		{
			return PackageName;
		}

		return ObjectName;
	}

	/** Returns true if this refers to an actual object */
	bool IsValid() const
	{
		return ObjectName != NAME_None || PackageName != NAME_None;
	}

	/** Returns true if all names have valid characters */
	COREUOBJECT_API bool HasValidCharacters(ECoreRedirectFlags Type) const;

	/** Expand OldName/NewName as needed */
	static COREUOBJECT_API bool ExpandNames(const FString& FullString, FName& OutName, FName& OutOuter, FName &OutPackage);

	/** Turn it back into an FString */
	static COREUOBJECT_API FString CombineNames(FName NewName, FName NewOuter, FName NewPackage);
};
ENUM_CLASS_FLAGS(FCoreRedirectObjectName::EMatchFlags);

/** 
 * A single redirection from an old name to a new name, parsed out of an ini file
 */
struct FCoreRedirect
{
	/** Flags of this redirect */
	ECoreRedirectFlags RedirectFlags;

	/** Name of object to look for */
	FCoreRedirectObjectName OldName;

	/** Name to replace with */
	FCoreRedirectObjectName NewName;

	/** Change the class of this object when doing a redirect */
	FCoreRedirectObjectName OverrideClassName;

	/** Map of value changes, from old value to new value */
	TMap<FString, FString> ValueChanges;

	/** Construct from name strings, which may get parsed out */
	FCoreRedirect(ECoreRedirectFlags InRedirectFlags, FString InOldName, FString InNewName)
		: RedirectFlags(InRedirectFlags), OldName(InOldName), NewName(InNewName)
	{
		NormalizeNewName();
	}
	
	/** Construct parsed out object names */
	FCoreRedirect(ECoreRedirectFlags InRedirectFlags, const FCoreRedirectObjectName& InOldName, const FCoreRedirectObjectName& InNewName)
		: RedirectFlags(InRedirectFlags), OldName(InOldName), NewName(InNewName)
	{
		NormalizeNewName();
	}

	/** Normalizes NewName with data from OldName */
	COREUOBJECT_API void NormalizeNewName();

	/** Parses a char buffer into the ValueChanges map */
	COREUOBJECT_API const TCHAR* ParseValueChanges(const TCHAR* Buffer);

	/** Returns true if the passed in name and flags match requirements */
	COREUOBJECT_API bool Matches(ECoreRedirectFlags InFlags, const FCoreRedirectObjectName& InName,
		ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None) const;
	/** Returns true if the passed in name matches requirements */
	COREUOBJECT_API bool Matches(const FCoreRedirectObjectName& InName,
		ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None) const;

	/** Returns true if this has value redirects */
	COREUOBJECT_API bool HasValueChanges() const;

	/** Returns true if this is a substring match */
	COREUOBJECT_API bool IsSubstringMatch() const;

	/** Convert to new names based on mapping */
	COREUOBJECT_API FCoreRedirectObjectName RedirectName(const FCoreRedirectObjectName& OldObjectName) const;

	/** See if search criteria is identical */
	COREUOBJECT_API bool IdenticalMatchRules(const FCoreRedirect& Other) const;

	/** Returns the name used as the key into the acceleration map */
	FName GetSearchKey() const
	{
		return OldName.GetSearchKey(RedirectFlags);
	}
};

/**
 * A container for all of the registered core-level redirects 
 */
struct FCoreRedirects
{
	/** Run initialization steps that are needed before any data can be stored in FCoreRedirects. Reads can occur before this, but no redirects will exist and redirect queries will all return empty. */
	static COREUOBJECT_API void Initialize();

	/** Returns a redirected version of the object name. If there are no valid redirects, it will return the original name */
	static COREUOBJECT_API FCoreRedirectObjectName GetRedirectedName(ECoreRedirectFlags Type,
		const FCoreRedirectObjectName& OldObjectName, ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None);

	/** Returns map of String->String value redirects for the object name, or nullptr if none found */
	static COREUOBJECT_API const TMap<FString, FString>* GetValueRedirects(ECoreRedirectFlags Type,
		const FCoreRedirectObjectName& OldObjectName, ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None);

	/** Performs both a name redirect and gets a value redirect struct if it exists. Returns true if either redirect found */
	static COREUOBJECT_API bool RedirectNameAndValues(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName,
		FCoreRedirectObjectName& NewObjectName, const FCoreRedirect** FoundValueRedirect,
		ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None);

	/** Returns true if this name has been registered as explicitly missing */
	static COREUOBJECT_API bool IsKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName);

	/**
	  * Adds the given combination of (Type, ObjectName, Channel) as a missing name; IsKnownMissing queries will now find it
	  *
	  * @param Type Combination of the ECoreRedirectFlags::Type_* flags specifying the type of the object now known to be missing
	  * @param ObjectName The name of the object now known to be missing
	  * @param Channel may be Option_MissingLoad or Option_None; used to distinguish between detected-at-runtime and specified-by-ini
	  */
	static COREUOBJECT_API bool AddKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName, ECoreRedirectFlags Channel = ECoreRedirectFlags::Option_MissingLoad);

	/**
	  * Removes the given combination of (Type, ObjectName, Channel) as a missing name
	  *
	  * @param Type Combination of the ECoreRedirectFlags::Type_* flags specifying the type of the object that has just been loaded.
	  * @param ObjectName The name of the object that has just been loaded.
	  * @param Channel may be Option_MissingLoad or Option_None; used to distinguish between detected-at-runtime and specified-by-ini
	  */
	static COREUOBJECT_API bool RemoveKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName, ECoreRedirectFlags Channel = ECoreRedirectFlags::Option_MissingLoad);

	static COREUOBJECT_API void ClearKnownMissing(ECoreRedirectFlags Type, ECoreRedirectFlags Channel = ECoreRedirectFlags::Option_MissingLoad);

	/** Returns list of names it may have been before */
	static COREUOBJECT_API bool FindPreviousNames(ECoreRedirectFlags Type, const FCoreRedirectObjectName& NewObjectName, TArray<FCoreRedirectObjectName>& PreviousNames);

	/** Returns list of all core redirects that match requirements */
	static COREUOBJECT_API bool GetMatchingRedirects(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName,
		TArray<const FCoreRedirect*>& FoundRedirects, ECoreRedirectMatchFlags MatchFlags = ECoreRedirectMatchFlags::None);

	/** Parse all redirects out of a given ini file */
	static COREUOBJECT_API bool ReadRedirectsFromIni(const FString& IniName);

	/** Adds an array of redirects to global list */
	static COREUOBJECT_API bool AddRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString);

	/** Removes an array of redirects from global list */
	static COREUOBJECT_API bool RemoveRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString);

	/** Returns true if this has ever been initialized */
	static bool IsInitialized() { return bInitialized; }

	/** Returns true if this is in debug mode that slows loading and adds additional warnings */
	static bool IsInDebugMode() { return bInDebugMode; }

	/** Validate a named list of redirects */
	static COREUOBJECT_API void ValidateRedirectList(TArrayView<const FCoreRedirect> Redirects, const FString& SourceString);

	/** Validates all known redirects and warn if they seem to point to missing things */
	static COREUOBJECT_API void ValidateAllRedirects();

	/** Gets map from config key -> Flags */
	static const TMap<FName, ECoreRedirectFlags>& GetConfigKeyMap() { return ConfigKeyMap; }

	/** Goes from the containing package and name of the type to the type flag */
	static COREUOBJECT_API ECoreRedirectFlags GetFlagsForTypeName(FName PackageName, FName TypeName);

	/** Goes from UClass Type to the type flag */
	static COREUOBJECT_API ECoreRedirectFlags GetFlagsForTypeClass(UClass *TypeClass);

	/** Runs set of redirector tests, returns false on failure */
	static COREUOBJECT_API bool RunTests();

private:
	/** Static only class, never constructed */
	COREUOBJECT_API FCoreRedirects();

	/** Add a single redirect to a type map */
	static COREUOBJECT_API bool AddSingleRedirect(const FCoreRedirect& NewRedirect, const FString& SourceString);

	/** Remove a single redirect from a type map */
	static COREUOBJECT_API bool RemoveSingleRedirect(const FCoreRedirect& OldRedirect, const FString& SourceString);

	/** Add native redirects, called before ini is parsed for the first time */
	static COREUOBJECT_API void RegisterNativeRedirects();

#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	/** Mark that CoreRedirects is about to start being used from multiple threads, and writes to new types of redirects are no longer allowed.
	  * ReadRedirectsFromIni and all other AddRedirectList calls must be called before this
	  */
	static COREUOBJECT_API void EnterMultithreadedPhase();
#endif

	/** There is one of these for each registered set of redirect flags */
	struct FRedirectNameMap
	{
		/** Map from name of thing being mapped to full list. List must be filtered further */
		TMap<FName, TArray<FCoreRedirect> > RedirectMap;
	};

	/** Whether this has been initialized at least once */
	static COREUOBJECT_API bool bInitialized;

	/** True if we are in debug mode that does extra validation */
	static COREUOBJECT_API bool bInDebugMode;

	/** True if we have done our initial validation. After initial validation, each change to redirects will validate independently */
	static COREUOBJECT_API bool bValidatedOnce;

#if WITH_COREREDIRECTS_MULTITHREAD_WARNING
	/** Whether CoreRedirects is now being used multithreaded and therefore does not support writes to RedirectTypeMap keyvalue pairs */
	static COREUOBJECT_API bool bIsInMultithreadedPhase;
#endif

	/** Map from config name to flag */
	static COREUOBJECT_API TMap<FName, ECoreRedirectFlags> ConfigKeyMap;

	/** Map from name of thing being mapped to full list. List must be filtered further */
	struct FRedirectTypeMap
	{
	public:
		FRedirectNameMap& FindOrAdd(ECoreRedirectFlags Key);
		FRedirectNameMap* Find(ECoreRedirectFlags Key);
		void Empty();

		TArray<TPair<ECoreRedirectFlags, FRedirectNameMap>>::RangedForIteratorType begin() { return FastIterable.begin(); }
		TArray<TPair<ECoreRedirectFlags, FRedirectNameMap>>::RangedForIteratorType end() { return FastIterable.end(); }
	private:
		TMap<ECoreRedirectFlags, FRedirectNameMap*> Map;
		TArray<TPair<ECoreRedirectFlags, FRedirectNameMap>> FastIterable;
	};
	static COREUOBJECT_API FRedirectTypeMap RedirectTypeMap;

	/**
	 * Lock to protect multithreaded access to *KnownMissing functions, which can be called from the async loading threads. 
	 * TODO: The KnownMissing functions use RedirectTypeMap, which is unguarded; there is race condition vulnerability if asyncloading thread is active before all categories are added to RedirectTypeMap.
	 */
	static COREUOBJECT_API FRWLock KnownMissingLock;
};
