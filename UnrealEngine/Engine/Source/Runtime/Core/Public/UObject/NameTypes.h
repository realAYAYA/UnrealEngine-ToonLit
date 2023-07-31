// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Containers/StringConv.h"
#include "Containers/StringFwd.h"
#include "UObject/UnrealNames.h"
#include "Templates/Atomic.h"
#include "Serialization/MemoryLayout.h"
#include "Misc/StringBuilder.h"
#include "Trace/Trace.h"

/*----------------------------------------------------------------------------
	Definitions.
----------------------------------------------------------------------------*/

/** 
 * Do we want to support case-variants for FName?
 * This will add an extra NAME_INDEX variable to FName, but means that ToString() will return you the exact same 
 * string that FName::Init was called with (which is useful if your FNames are shown to the end user)
 * Currently this is enabled for the Editor and any Programs (such as UHT), but not the Runtime
 */
#ifndef WITH_CASE_PRESERVING_NAME
	#define WITH_CASE_PRESERVING_NAME WITH_EDITORONLY_DATA
#endif

// Should the number part of the fname be stored in the name table or in the FName instance?
// Storing it in the name table may save memory overall but new number suffixes will cause the name table to grow like unique strings do.
#ifndef UE_FNAME_OUTLINE_NUMBER
	#define UE_FNAME_OUTLINE_NUMBER 0
#endif // UE_FNAME_OUTLINE_NUMBER

class FText;

/** Maximum size of name, including the null terminator. */
enum {NAME_SIZE	= 1024};

struct FMinimalName;
struct FScriptName;
class CORE_API FName;

/** Opaque id to a deduplicated name */
struct FNameEntryId
{
	// Default initialize to be equal to NAME_None
	FNameEntryId() : Value(0) {}
	FNameEntryId(ENoInit) {}

	bool IsNone() const
	{
		return Value == 0;
	}

	/** Slow alphabetical order that is stable / deterministic over process runs */
	CORE_API int32 CompareLexical(FNameEntryId Rhs) const;
	bool LexicalLess(FNameEntryId Rhs) const { return CompareLexical(Rhs) < 0; }

	/** Fast non-alphabetical order that is only stable during this process' lifetime */
	int32 CompareFast(FNameEntryId Rhs) const { return Value - Rhs.Value; };
	bool FastLess(FNameEntryId Rhs) const { return CompareFast(Rhs) < 0; }

	/** Fast non-alphabetical order that is only stable during this process' lifetime */
	bool operator<(FNameEntryId Rhs) const { return Value < Rhs.Value; }

	/** Fast non-alphabetical order that is only stable during this process' lifetime */
	bool operator>(FNameEntryId Rhs) const { return Rhs.Value < Value; }
	bool operator==(FNameEntryId Rhs) const { return Value == Rhs.Value; }
	bool operator!=(FNameEntryId Rhs) const { return Value != Rhs.Value; }

	// Returns true if this FNameEntryId is not equivalent to NAME_None
	explicit operator bool() const { return Value != 0; }

	UE_DEPRECATED(4.23, "NAME_INDEX is replaced by FNameEntryId, which is no longer a contiguous integer. "
						"Please use 'GetTypeHash(MyId)' instead of 'MyId' for hash functions. "
						"ToUnstableInt() can be used in other advanced cases.")
	operator int32() const;

	/** Get process specific integer */
	uint32 ToUnstableInt() const { return Value; }

	/** Create from unstable int produced by this process */
	static FNameEntryId FromUnstableInt(uint32 UnstableInt)
	{
		FNameEntryId Id;
		Id.Value = UnstableInt;
		return Id;
	}

	FORCEINLINE static FNameEntryId FromEName(EName Ename)
	{
		return Ename == NAME_None ? FNameEntryId() : FromValidEName(Ename);
	}

	friend inline bool operator==(FNameEntryId Id, EName Ename)
	{
		return Ename == NAME_None ? !Id : Id == FromValidENamePostInit(Ename);
	}

private:
	uint32 Value;

	CORE_API static FNameEntryId FromValidEName(EName Ename);
	CORE_API static FNameEntryId FromValidENamePostInit(EName Ename);
};

CORE_API uint32 GetTypeHash(FNameEntryId Id);
inline bool operator==(EName Ename, FNameEntryId Id) { return Id == Ename; }
inline bool operator!=(EName Ename, FNameEntryId Id) { return !(Id == Ename); }
inline bool operator!=(FNameEntryId Id, EName Ename) { return !(Id == Ename); }

/** Serialize as process specific unstable int */
CORE_API FArchive& operator<<(FArchive& Ar, FNameEntryId& InId);

/**
 * Legacy typedef - this is no longer an index
 *
 * Use GetTypeHash(FName) or GetTypeHash(FNameEntryId) for hashing
 * To compare with ENames use FName(EName) or FName::ToEName() instead
 */
typedef FNameEntryId NAME_INDEX;

#define checkName checkSlow

/** Externally, the instance number to represent no instance number is NAME_NO_NUMBER, 
    but internally, we add 1 to indices, so we use this #define internally for 
	zero'd memory initialization will still make NAME_None as expected */
#define NAME_NO_NUMBER_INTERNAL	0

/** Conversion routines between external representations and internal */
#define NAME_INTERNAL_TO_EXTERNAL(x) (x - 1)
#define NAME_EXTERNAL_TO_INTERNAL(x) (x + 1)

/** Special value for an FName with no number */
#define NAME_NO_NUMBER NAME_INTERNAL_TO_EXTERNAL(NAME_NO_NUMBER_INTERNAL)


/** this is the character used to separate a subobject root from its subobjects in a path name. */
#define SUBOBJECT_DELIMITER_ANSI		":"
#define SUBOBJECT_DELIMITER				TEXT(SUBOBJECT_DELIMITER_ANSI)

/** this is the character used to separate a subobject root from its subobjects in a path name, as a char */
#define SUBOBJECT_DELIMITER_CHAR		TEXT(':')

/** These are the characters that cannot be used in general FNames */
#define INVALID_NAME_CHARACTERS			TEXT("\"' ,\n\r\t")

/** These characters cannot be used in object names */
#define INVALID_OBJECTNAME_CHARACTERS	TEXT("\"' ,/.:|&!~\n\r\t@#(){}[]=;^%$`")

/** These characters cannot be used in ObjectPaths, which includes both the package path and part after the first . */
#define INVALID_OBJECTPATH_CHARACTERS	TEXT("\"' ,|&!~\n\r\t@#(){}[]=;^%$`")

/** These characters cannot be used in long package names */
#define INVALID_LONGPACKAGE_CHARACTERS	TEXT("\\:*?\"<>|' ,.&!~\n\r\t@#")

/** These characters can be used in relative directory names (lowercase versions as well) */
#define VALID_SAVEDDIRSUFFIX_CHARACTERS	TEXT("_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")

enum class ENameCase : uint8
{
	CaseSensitive,
	IgnoreCase,
};

enum ELinkerNameTableConstructor    {ENAME_LinkerConstructor};

/** Enumeration for finding name. */
enum EFindName
{
	/** 
	* Find a name; return 0/NAME_None/FName() if it doesn't exist.
	* When UE_FNAME_OUTLINE_NUMBER is set, we search for the exact name including the number suffix.
	* Otherwise we search only for the string part.
	*/
	FNAME_Find,

	/** Find a name or add it if it doesn't exist. */
	FNAME_Add,

	/** Finds a name and replaces it. Adds it if missing. This is only used by UHT and is generally not safe for threading. 
	 * All this really is used for is correcting the case of names. In MT conditions you might get a half-changed name.
	 */
	FNAME_Replace_Not_Safe_For_Threading,
};

/*----------------------------------------------------------------------------
	FNameEntry.
----------------------------------------------------------------------------*/

/** Implementation detail exposed for debug visualizers */
struct FNameEntryHeader
{
	uint16 bIsWide : 1;
#if WITH_CASE_PRESERVING_NAME
	uint16 Len : 15;
#else
	static constexpr uint32 ProbeHashBits = 5;
	uint16 LowercaseProbeHash : ProbeHashBits;
	uint16 Len : 10;
#endif
};

/**
 * A global deduplicated name stored in the global name table.
 */
struct FNameEntry
{
private:
#if WITH_CASE_PRESERVING_NAME
	FNameEntryId ComparisonId;
#endif
	FNameEntryHeader Header;
	union
	{
		ANSICHAR	AnsiName[NAME_SIZE];
		WIDECHAR	WideName[NAME_SIZE];
#if UE_FNAME_OUTLINE_NUMBER
		// These fields are valid when Header.Len == 0.
		// Stores a (string, number) fname pair to construct the string "string_(number-1)" on demand.
		// Id is a reference to another entry with Header.Len != 0 (no recursion).
		struct 
		{
			FNameEntryId	Id;
			uint32			Number;
		} NumberedName;
#endif // UE_FNAME_OUTLINE_NUMBER
	};

	FNameEntry(struct FClangKeepDebugInfo);
	FNameEntry(const FNameEntry&) = delete;
	FNameEntry(FNameEntry&&) = delete;
	FNameEntry& operator=(const FNameEntry&) = delete;
	FNameEntry& operator=(FNameEntry&&) = delete;

public:
	/** Returns whether this name entry is represented via WIDECHAR or ANSICHAR. */
	FORCEINLINE bool IsWide() const
	{
		return Header.bIsWide;
	}

	FORCEINLINE int32 GetNameLength() const
	{
		return Header.Len;
	}

	FORCEINLINE bool IsNumbered() const 
	{
#if UE_FNAME_OUTLINE_NUMBER
		return Header.Len == 0;
#else
		return false;
#endif
	}

	/**
	 * Copy unterminated name to TCHAR buffer without allocating.
	 *
	 * @param OutSize must be at least GetNameLength()
	 */
	void GetUnterminatedName(TCHAR* OutName, uint32 OutSize) const;

	/** Copy null-terminated name to TCHAR buffer without allocating. */
	void GetName(TCHAR(&OutName)[NAME_SIZE]) const;

	/** Copy null-terminated name to ANSICHAR buffer without allocating. Entry must not be wide. */
	CORE_API void GetAnsiName(ANSICHAR(&OutName)[NAME_SIZE]) const;

	/** Copy null-terminated name to WIDECHAR buffer without allocating. Entry must be wide. */
	CORE_API void GetWideName(WIDECHAR(&OutName)[NAME_SIZE]) const;

	/** Copy name to a dynamically allocated FString. */
	CORE_API FString GetPlainNameString() const;

	/** Appends name to string. May allocate. */
	CORE_API void AppendNameToString(FString& OutString) const;

	/** Appends name to string builder. */
	CORE_API void AppendNameToString(FWideStringBuilderBase& OutString) const;
	CORE_API void AppendNameToString(FUtf8StringBuilderBase& OutString) const;

	/** Appends name to string builder. Entry must not be wide. */
	CORE_API void AppendAnsiNameToString(FAnsiStringBuilderBase& OutString) const;

	/** Appends name to string with path separator using FString::PathAppend(). */
	CORE_API void AppendNameToPathString(FString& OutString) const;

	CORE_API void DebugDump(FOutputDevice& Out) const;

	/**
	 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
	 *
	 * @param	Length			Length of name
	 * @param	bIsPureAnsi		Whether name is pure ANSI or not
	 * @return	required size of FNameEntry structure to hold this string (might be wide or ansi)
	 */
	static int32 GetSize( int32 Length, bool bIsPureAnsi );
	static CORE_API int32 GetSize(const TCHAR* Name);

	CORE_API int32 GetSizeInBytes() const;

	CORE_API void Write(FArchive& Ar) const;

	static CORE_API int32 GetDataOffset();
	struct CORE_API FNameStringView MakeView(union FNameBuffer& OptionalDecodeBuffer) const;
private:
	friend class FName;
	friend struct FNameHelper;
	friend class FNameEntryAllocator;
	friend class FNamePoolShardBase;

	static void Encode(ANSICHAR* Name, uint32 Len);
	static void Encode(WIDECHAR* Name, uint32 Len);
	static void Decode(ANSICHAR* Name, uint32 Len);
	static void Decode(WIDECHAR* Name, uint32 Len);

	void StoreName(const ANSICHAR* InName, uint32 Len);
	void StoreName(const WIDECHAR* InName, uint32 Len);
	void CopyUnterminatedName(ANSICHAR* OutName) const;
	void CopyUnterminatedName(WIDECHAR* OutName) const;
	void CopyAndConvertUnterminatedName(TCHAR* OutName) const;
	const ANSICHAR* GetUnterminatedName(ANSICHAR(&OptionalDecodeBuffer)[NAME_SIZE]) const;
	const WIDECHAR* GetUnterminatedName(WIDECHAR(&OptionalDecodeBuffer)[NAME_SIZE]) const;

#if UE_FNAME_OUTLINE_NUMBER
	static int32 GetNumberedEntrySize();
	uint32 GetNumber() const;
#endif // UE_FNAME_OUTLINE_NUMBER
};

/**
 *  This struct is only used during loading/saving and is not part of the runtime costs
 */
struct FNameEntrySerialized
{
	FNameEntryId Index;
	bool bIsWide = false;

	union
	{
		ANSICHAR	AnsiName[NAME_SIZE];
		WIDECHAR	WideName[NAME_SIZE];
	};

	// These are not used anymore but recalculated on save to maintain serialization format
	uint16 NonCasePreservingHash = 0;
	uint16 CasePreservingHash = 0;

	FNameEntrySerialized(const FNameEntry& NameEntry);
	FNameEntrySerialized(enum ELinkerNameTableConstructor) {}

	/**
	 * Returns direct access to null-terminated name if narrow
	 */
	ANSICHAR const* GetAnsiName() const
	{
		check(!bIsWide);
		return AnsiName;
	}

	/**
	 * Returns direct access to null-terminated name if wide
	 */
	WIDECHAR const* GetWideName() const
	{
		check(bIsWide);
		return WideName;
	}

	/**
	 * Returns FString of name portion minus number.
	 */
	CORE_API FString GetPlainNameString() const;	

	friend CORE_API FArchive& operator<<(FArchive& Ar, FNameEntrySerialized& E);
	friend FArchive& operator<<(FArchive& Ar, FNameEntrySerialized* E)
	{
		return Ar << *E;
	}
};

/**
 * The minimum amount of data required to reconstruct a name
 * This is smaller than FName when WITH_CASE_PRESERVING_NAME is set, but you lose the case-preserving behavior.
 * The size of this type is not portable across different platforms and configurations, as with FName itself.
 */
struct FMinimalName
{
	friend uint32 GetTypeHash(FMinimalName Name);
	friend bool operator==(FMinimalName Lhs, FMinimalName Rhs);
	friend bool operator==(FName Lhs, FMinimalName Rhs);
	friend FName;

	FMinimalName() {}
	
	FMinimalName(EName N)
		: Index(FNameEntryId::FromEName(N))
	{
	}

	FORCEINLINE explicit FMinimalName(const FName& Name);
	FORCEINLINE bool IsNone() const;
	FORCEINLINE bool operator<(FMinimalName Rhs) const;
	
private:
	/** Index into the Names array (used to find String portion of the string/number pair) */
	FNameEntryId	Index;
#if !UE_FNAME_OUTLINE_NUMBER
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	int32			Number = NAME_NO_NUMBER_INTERNAL;
#endif // UE_FNAME_OUTLINE_NUMBE
};

/**
 * The full amount of data required to reconstruct a case-preserving name
 * This will be the maximum size of an FName across all values of WITH_CASE_PRESERVING_NAME and UE_FNAME_OUTLINE_NUMBER
 * and is used to store an FName in cases where  the size of a name must be constant between build configurations (eg, blueprint bytecode)
 * the layout is not guaranteed to be the same as FName even if the size is the same, so memory cannot be reinterpreted between the two.
 * The layout here must be as expected by FScriptBytecodeWriter and XFER_NAME
 */
struct FScriptName
{
	friend uint32 GetTypeHash(FScriptName Name);
	friend bool operator==(FScriptName Lhs, FScriptName Rhs);
	friend bool operator==(FName Lhs, FScriptName Rhs);
	friend FName;

	FScriptName() {}
	
	FScriptName(EName Ename)
		: ComparisonIndex(FNameEntryId::FromEName(Ename))
		, DisplayIndex(ComparisonIndex)
	{
	}

	FORCEINLINE explicit FScriptName(const FName& Name);
	FORCEINLINE bool IsNone() const;
	inline bool operator==(EName Name) { return *this == FScriptName(Name); }

	CORE_API FString ToString() const;

	// The internal structure of FScriptName is private in order to handle UE_FNAME_OUTLINE_NUMBER
private: 
	/** Encoded address of name entry  (used to find String portion of the string/number pair used for comparison) */
	FNameEntryId	ComparisonIndex;
	/** Encoded address of name entry  (used to find String portion of the string/number pair used for display) */
	FNameEntryId	DisplayIndex;
#if UE_FNAME_OUTLINE_NUMBER
	uint32			Dummy = 0; // Dummy to keep the size the same regardless of build configuration, but change the name so trying to use Number is a compile error
#else // UE_FNAME_OUTLINE_NUMBER
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	uint32			Number = NAME_NO_NUMBER_INTERNAL;
#endif // UE_FNAME_OUTLINE_NUMBER
};

struct FMemoryImageName;

namespace Freeze
{
	CORE_API void ApplyMemoryImageNamePatch(void* NameDst, const FMemoryImageName& Name, const FPlatformTypeLayoutParameters& LayoutParams);
	CORE_API void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const FMemoryImageName& Object, const FTypeLayoutDesc&);
	CORE_API uint32 IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const FMemoryImageName& Object, void* OutDst);
}

/**
 * Predictably sized structure for representing an FName in memory images while allowing the size to be smaller than FScriptName
 * when case-preserving behavior is not required.
 */
struct FMemoryImageName
{
	friend uint32 GetTypeHash(FMemoryImageName Name);
	friend bool operator==(FMemoryImageName Lhs, FMemoryImageName Rhs);
	friend bool operator==(FName Lhs, FMemoryImageName Rhs);
	friend FName;

	friend CORE_API void Freeze::ApplyMemoryImageNamePatch(void* NameDst, const FMemoryImageName& Name, const FPlatformTypeLayoutParameters& LayoutParams);
	friend CORE_API void Freeze::IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const FMemoryImageName& Object, const FTypeLayoutDesc&);
	friend CORE_API uint32 Freeze::IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const FMemoryImageName& Object, void* OutDst);

	FMemoryImageName();
	FMemoryImageName(EName Name);
	FORCEINLINE FMemoryImageName(const FName& Name);

	inline bool operator==(EName Name) const { return *this == FMemoryImageName(Name); }

	FORCEINLINE bool IsNone() const;
	CORE_API FString ToString() const;

	// The internal structure of FMemoryImageName is private in order to handle UE_FNAME_OUTLINE_NUMBER
private:
	/** Encoded address of name entry (used to find String portion of the string/number pair used for comparison) */
	FNameEntryId	ComparisonIndex;
#if UE_FNAME_OUTLINE_NUMBER
	uint32			Dummy = 0; // Dummy to keep the size the same regardless of build configuration, but change the name so trying to use Number is a compile error
#else // UE_FNAME_OUTLINE_NUMBER
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	uint32			Number = NAME_NO_NUMBER_INTERNAL;
#endif // UE_FNAME_OUTLINE_NUMBER
#if WITH_CASE_PRESERVING_NAME
	/** Encoded address of name entry (used to find String portion of the string/number pair used for display) */
	FNameEntryId	DisplayIndex;
#endif
};

/**
 * Public name, available to the world.  Names are stored as a combination of
 * an index into a table of unique strings and an instance number.
 * Names are case-insensitive, but case-preserving (when WITH_CASE_PRESERVING_NAME is 1)
 */
class CORE_API FName
{
public:
#if UE_FNAME_OUTLINE_NUMBER
	FNameEntryId GetComparisonIndex() const;
	FNameEntryId GetDisplayIndex() const;
#else // UE_FNAME_OUTLINE_NUMBER
	FORCEINLINE FNameEntryId GetComparisonIndex() const
	{
		checkName(IsWithinBounds(ComparisonIndex));
		return ComparisonIndex;
	}

	FORCEINLINE FNameEntryId GetDisplayIndex() const
	{
		const FNameEntryId Index = GetDisplayIndexFast();
		checkName(IsWithinBounds(Index));
		return Index;
	}
#endif //UE_FNAME_OUTLINE_NUMBER

#if UE_FNAME_OUTLINE_NUMBER
	int32 GetNumber() const;
	void SetNumber(const int32 NewNumber);
#else //UE_FNAME_OUTLINE_NUMBER
	FORCEINLINE int32 GetNumber() const
	{
		return Number;
	}

	FORCEINLINE void SetNumber(const int32 NewNumber)
	{
		Number = NewNumber;
	}
#endif //UE_FNAME_OUTLINE_NUMBER
	
	/** Get name without number part as a dynamically allocated string */
	FString GetPlainNameString() const;

	/** Convert name without number part into TCHAR buffer and returns string length. Doesn't allocate. */
	uint32 GetPlainNameString(TCHAR(&OutName)[NAME_SIZE]) const;

	/** Copy ANSI name without number part. Must *only* be used for ANSI FNames. Doesn't allocate. */
	void GetPlainANSIString(ANSICHAR(&AnsiName)[NAME_SIZE]) const;

	/** Copy wide name without number part. Must *only* be used for wide FNames. Doesn't allocate. */
	void GetPlainWIDEString(WIDECHAR(&WideName)[NAME_SIZE]) const;

	const FNameEntry* GetComparisonNameEntry() const;
	const FNameEntry* GetDisplayNameEntry() const;

	/**
	 * Converts an FName to a readable format
	 *
	 * @return String representation of the name
	 */
	FString ToString() const;

	/**
	 * Converts an FName to a readable format, in place
	 * 
	 * @param Out String to fill with the string representation of the name
	 */
	void ToString(FString& Out) const;

	/**
	 * Converts an FName to a readable format, in place
	 * 
	 * @param Out StringBuilder to fill with the string representation of the name
	 */
	void ToString(FWideStringBuilderBase& Out) const;
	void ToString(FUtf8StringBuilderBase& Out) const;

	/**
	 * Get the number of characters, excluding null-terminator, that ToString() would yield
	 */
	uint32 GetStringLength() const;

	/**
	 * Buffer size required for any null-terminated FName string, i.e. [name] '_' [digits] '\0'
	 */
	static constexpr uint32 StringBufferSize = NAME_SIZE + 1 + 10; // NAME_SIZE includes null-terminator

	/**
	 * Convert to string buffer to avoid dynamic allocations and returns string length
	 *
	 * Fails hard if OutLen < GetStringLength() + 1. StringBufferSize guarantees success.
	 *
	 * Note that a default constructed FName returns "None" instead of ""
	 */
	uint32 ToString(TCHAR* Out, uint32 OutSize) const;

	template<int N>
	uint32 ToString(TCHAR (&Out)[N]) const
	{
		return ToString(Out, N);
	}

	/**
	 * Converts an FName to a readable format, in place, appending to an existing string (ala GetFullName)
	 * 
	 * @param Out String to append with the string representation of the name
	 */
	void AppendString(FString& Out) const;

	/**
	 * Converts an FName to a readable format, in place, appending to an existing string (ala GetFullName)
	 * 
	 * @param Out StringBuilder to append with the string representation of the name
	 */
	void AppendString(FWideStringBuilderBase& Out) const;
	void AppendString(FUtf8StringBuilderBase& Out) const;

	/**
	 * Converts an ANSI FName to a readable format appended to the string builder.
	 *
	 * @param Out A string builder to write the readable representation of the name into.
	 *
	 * @return Whether the string is ANSI. A return of false indicates that the string was wide and was not written.
	 */
	bool TryAppendAnsiString(FAnsiStringBuilderBase& Out) const;

	/**
	 * Check to see if this FName matches the other FName, potentially also checking for any case variations
	 */
	FORCEINLINE bool IsEqual(const FName& Other, const ENameCase CompareMethod = ENameCase::IgnoreCase, const bool bCompareNumber = true) const;

	FORCEINLINE bool operator==(FName Other) const
	{
		return ToUnstableInt() == Other.ToUnstableInt();
	}

	FORCEINLINE bool operator!=(FName Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator==(EName Ename) const;
	
	FORCEINLINE bool operator!=(EName Ename) const;

	UE_DEPRECATED(4.23, "Please use FastLess() / FNameFastLess or LexicalLess() / FNameLexicalLess instead. "
		"Default lexical sort order is deprecated to avoid unintended expensive sorting. ")
	FORCEINLINE bool operator<( const FName& Other ) const
	{
		return LexicalLess(Other);
	}

	UE_DEPRECATED(4.23, "Please use B.FastLess(A) or B.LexicalLess(A) instead of A > B.")
	FORCEINLINE bool operator>(const FName& Other) const
	{
		return Other.LexicalLess(*this);
	}

	/** Fast non-alphabetical order that is only stable during this process' lifetime. */
	FORCEINLINE bool FastLess(const FName& Other) const
	{
		return CompareIndexes(Other) < 0;
	}

	/** Slow alphabetical order that is stable / deterministic over process runs. */
	FORCEINLINE bool LexicalLess(const FName& Other) const
	{
		return Compare(Other) < 0;
	}

	/** True for FName(), FName(NAME_None) and FName("None") */
	FORCEINLINE bool IsNone() const
	{
#if PLATFORM_64BITS && !WITH_CASE_PRESERVING_NAME
		return ToUnstableInt() == 0;
#else
		return ComparisonIndex.IsNone() && GetNumber() == NAME_NO_NUMBER_INTERNAL;
#endif
	}

	/**
	 * Paranoid sanity check
	 *
	 * All FNames are valid except for stomped memory, dangling pointers, etc.
	 * Should only be used to investigate such bugs and not in production code.
	 */
	bool IsValid() const { return IsWithinBounds(ComparisonIndex); }

	/** Paranoid sanity check, same as IsValid() */
	bool IsValidIndexFast() const { return IsValid(); }


	/**
	 * Checks to see that a given name-like string follows the rules that Unreal requires.
	 *
	 * @param	InName			String containing the name to test.
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain.
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InErrorCtx		Error context information to show in the error message (default is "Name").
	 *
	 * @return	true if the name is valid
	 */
	static bool IsValidXName( const FName InName, const FString& InInvalidChars, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr );
	static bool IsValidXName( const TCHAR* InName, const FString& InInvalidChars, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr );
	static bool IsValidXName( const FString& InName, const FString& InInvalidChars, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr );
	static bool IsValidXName( const FStringView& InName, const FString& InInvalidChars, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr );

	/**
	 * Checks to see that a FName follows the rules that Unreal requires.
	 *
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InErrorCtx		Error context information to show in the error message (default is "Name").
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( const FString& InInvalidChars = INVALID_NAME_CHARACTERS, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr ) const
	{
		return IsValidXName(*this, InInvalidChars, OutReason, InErrorCtx);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( FText& OutReason, const FString& InInvalidChars = INVALID_NAME_CHARACTERS ) const
	{
		return IsValidXName(*this, InInvalidChars, &OutReason);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for object names.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidObjectName( FText& OutReason ) const
	{
		return IsValidXName(*this, INVALID_OBJECTNAME_CHARACTERS, &OutReason);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for package or group names.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	bIsGroupName	if true, check legality for a group name, else check legality for a package name
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidGroupName( FText& OutReason, bool bIsGroupName=false ) const
	{
		return IsValidXName(*this, INVALID_LONGPACKAGE_CHARACTERS, &OutReason);
	}

	/**
	 * Compares name to passed in one. Sort is alphabetical ascending.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	int32 Compare( const FName& Other ) const;

	/**
	 * Fast non-alphabetical order that is only stable during this process' lifetime.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	FORCEINLINE int32 CompareIndexes(const FName& Other) const
	{
		if (int32 ComparisonDiff = ComparisonIndex.CompareFast(Other.ComparisonIndex))
		{
			return ComparisonDiff;
		}

#if UE_FNAME_OUTLINE_NUMBER
		return 0;  // If comparison indices are the same we are the same
#else //UE_FNAME_OUTLINE_NUMBER
		return GetNumber() - Other.GetNumber();
#endif //UE_FNAME_OUTLINE_NUMBER
	}

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param N The hardcoded value the string portion of the name will have. The number portion will be NAME_NO_NUMBER
	 */
	FORCEINLINE FName(EName Ename) : FName(Ename, NAME_NO_NUMBER_INTERNAL) {}

	/**
	 * Create an FName with a hardcoded string index and (instance).
	 *
	 * @param N The hardcoded value the string portion of the name will have
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FORCEINLINE FName(EName Ename, int32 InNumber)
		: FName(CreateNumberedNameIfNecessary(FNameEntryId::FromEName(Ename), InNumber))
	{
	}


	/**
	 * Create an FName from an existing string, but with a different instance.
	 *
	 * @param Other The FName to take the string values from
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FORCEINLINE FName(FName Other, int32 InNumber)
		: FName(CreateNumberedNameIfNecessary(Other.GetComparisonIndex(), Other.GetDisplayIndex(), InNumber))
	{
	}

	/**
	 * Create an FName from its component parts
	 * Only call this if you *really* know what you're doing
	 */
	FORCEINLINE FName(FNameEntryId InComparisonIndex, FNameEntryId InDisplayIndex, int32 InNumber)
		: FName(CreateNumberedNameIfNecessary(InComparisonIndex, InDisplayIndex, InNumber))
	{
	}

#if WITH_CASE_PRESERVING_NAME
	static FNameEntryId GetComparisonIdFromDisplayId(FNameEntryId DisplayId);
#else
	static FNameEntryId GetComparisonIdFromDisplayId(FNameEntryId DisplayId) { return DisplayId; }
#endif

	/**
	 * Only call this if you *really* know what you're doing
	 */
	static FName CreateFromDisplayId(FNameEntryId DisplayId, int32 Number)
	{
#if UE_FNAME_OUTLINE_NUMBER
		checkSlow(ResolveEntry(DisplayId)->IsNumbered() == false); // This id should be unnumbered i.e. returned from GetDisplayIndex on an FName.
#endif
		return FName(GetComparisonIdFromDisplayId(DisplayId), DisplayId, Number);
	}

#if UE_FNAME_OUTLINE_NUMBER
	static FName FindNumberedName(FNameEntryId DisplayId, int32 Number);
#endif //UE_FNAME_OUTLINE_NUMBER

	/**
	 * Default constructor, initialized to None
	 */
	FORCEINLINE FName()
#if !UE_FNAME_OUTLINE_NUMBER
		: Number(NAME_NO_NUMBER_INTERNAL)
#endif //!UE_FNAME_OUTLINE_NUMBER
	{
	}

	/**
	 * Scary no init constructor, used for something obscure in UObjectBase
	 */
	explicit FName(ENoInit)
		: ComparisonIndex(NoInit)
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex(NoInit)
#endif
	{}

	FORCEINLINE explicit FName(FMinimalName InName);
	FORCEINLINE explicit FName(FScriptName InName);
	FORCEINLINE FName(FMemoryImageName InName);

	/**
	 * Create an FName. If FindType is FNAME_Find, and the name 
	 * doesn't already exist, then the name will be NAME_None.
	 * The check for existance or not depends on UE_FNAME_OUTLINE_NUMBER.
	 * When UE_FNAME_OUTLINE_NUMBER is 0, we only check for the string part.
	 * When UE_FNAME_OUTLINE_NUMBER is 1, we check for whole name including the number.
	 *
	 * @param Name			Value for the string portion of the name
	 * @param FindType		Action to take (see EFindName)
	 */
	FName(const WIDECHAR* Name, EFindName FindType=FNAME_Add);
	FName(const ANSICHAR* Name, EFindName FindType=FNAME_Add);
	FName(const UTF8CHAR* Name, EFindName FindType=FNAME_Add);

	/** Create FName from non-null string with known length  */
	FName(int32 Len, const WIDECHAR* Name, EFindName FindType=FNAME_Add);
	FName(int32 Len, const ANSICHAR* Name, EFindName FindType=FNAME_Add);
	FName(int32 Len, const UTF8CHAR* Name, EFindName FindType=FNAME_Add);

	template <typename CharRangeType,
		typename CharType = typename TRemoveCV<typename TRemovePointer<decltype(GetData(DeclVal<CharRangeType>()))>::Type>::Type,
		typename = decltype(ImplicitConv<TStringView<CharType>>(DeclVal<CharRangeType>()))>
	inline explicit FName(CharRangeType&& Name, EFindName FindType = FNAME_Add)
		: FName(NoInit)
	{
		TStringView<CharType> View = Forward<CharRangeType>(Name);
		*this = FName(View.Len(), View.GetData(), FindType);
	}

	/**
	 * Create an FName. Will add the string to the name table if it does not exist.
	 * When UE_FNAME_OUTLINE_NUMBER is set, will also add the combination of base string and number to the name table if it doesn't exist.
	 *
	 * @param Name Value for the string portion of the name
	 * @param Number Value for the number portion of the name
	 */
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(const WIDECHAR* Name, int32 Number, EFindName FindType);
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(const ANSICHAR* Name, int32 Number, EFindName FindType);
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(const UTF8CHAR* Name, int32 Number, EFindName FindType);
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(int32 Len, const WIDECHAR* Name, int32 Number, EFindName FindType);
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(int32 Len, const ANSICHAR* Name, int32 Number, EFindName FindType);
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(int32 Len, const UTF8CHAR* Name, int32 Number, EFindName FindType);
	FName(const WIDECHAR* Name, int32 Number);
	FName(const ANSICHAR* Name, int32 Number);
	FName(const UTF8CHAR* Name, int32 Number);
	FName(int32 Len, const WIDECHAR* Name, int32 Number);
	FName(int32 Len, const ANSICHAR* Name, int32 Number);
	FName(int32 Len, const UTF8CHAR* Name, int32 Number);

	template <typename CharRangeType,
		typename CharType = typename TRemoveCV<typename TRemovePointer<decltype(GetData(DeclVal<CharRangeType>()))>::Type>::Type,
		typename = decltype(ImplicitConv<TStringView<CharType>>(DeclVal<CharRangeType>()))>
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	inline FName(CharRangeType&& Name, int32 InNumber, EFindName FindType)
		: FName(NoInit)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TStringView<CharType> View = Forward<CharRangeType>(Name);
		*this = FName(View.Len(), View.GetData(), InNumber, FindType);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	template <typename CharRangeType,
		typename CharType = typename TRemoveCV<typename TRemovePointer<decltype(GetData(DeclVal<CharRangeType>()))>::Type>::Type,
		typename = decltype(ImplicitConv<TStringView<CharType>>(DeclVal<CharRangeType>()))>
	inline FName(CharRangeType&& Name, int32 InNumber)
		: FName(NoInit)
	{
		TStringView<CharType> View = Forward<CharRangeType>(Name);
		*this = FName(View.Len(), View.GetData(), InNumber);
	}

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name Value for the string portion of the name
	 * @param Number Value for the number portion of the name
	 * @param FindType Action to take (see EFindName)
	 * @param bSplitName true if the trailing number should be split from the name when Number == NAME_NO_NUMBER_INTERNAL, or false to always use the name as-is
	 */
	UE_DEPRECATED(5.1, "EFindName has been removed from constructors taking a Number argument to add clarity around UE_FNAME_OUTLINE_NUMBER.")
	FName(const TCHAR* Name, int32 InNumber, EFindName FindType, bool bSplitName);
	FName(const TCHAR* Name, int32 InNumber, bool bSplitName);

	/**
	 * Constructor used by FLinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions. Also,
	 * this version skips calculating the hashes of the names if possible
	 */
	FName(const FNameEntrySerialized& LoadedEntry);

	/**
	 * Equality operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return true if name matches the string, false otherwise
	 */
	bool operator==(const ANSICHAR* Other) const;
	bool operator==(const WIDECHAR* Other) const;

	/**
	 * Inequality operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return true if name does not match the string, false otherwise
	 */
	template <typename CharType>
	bool operator!=(const CharType* Other) const
	{
		return !operator==(Other);
	}

	static void DisplayHash( class FOutputDevice& Ar );
	static FString SafeString(FNameEntryId InDisplayIndex, int32 InstanceNumber = NAME_NO_NUMBER_INTERNAL);

	/**
	 * @return Size of all name entries.
	 */
	static int32 GetNameEntryMemorySize();

	/**
	* @return Size of Name Table object as a whole
	*/
	static int32 GetNameTableMemorySize();

	/**
	 * @return number of ansi names in name table
	 */
	static int32 GetNumAnsiNames();

	/**
	 * @return number of wide names in name table
	 */
	static int32 GetNumWideNames();

#if UE_FNAME_OUTLINE_NUMBER
	/**
	 * @return number of numbered names in name table
	 */
	static int32 GetNumNumberedNames();
#endif

	static TArray<const FNameEntry*> DebugDump();

	static FNameEntry const* GetEntry(EName Ename);
	static FNameEntry const* GetEntry(FNameEntryId Id);

#if UE_TRACE_ENABLED
	static UE::Trace::FEventRef32 TraceName(const FName& Name);
	static void TraceNamesOnConnection();
#endif

	//@}

	/** Run autotest on FNames. */
	static void AutoTest();
	
	/**
	 * Takes a string and breaks it down into a human readable string.
	 * For example - "bCreateSomeStuff" becomes "Create Some Stuff?" and "DrawScale3D" becomes "Draw Scale 3D".
	 * 
	 * @param	InDisplayName	[In, Out] The name to sanitize
	 * @param	bIsBool				True if the name is a bool
	 *
	 * @return	the sanitized version of the display name
	 */
	static FString NameToDisplayString( const FString& InDisplayName, const bool bIsBool );

	/**
	 * Add/remove an exemption to the formatting applied by NameToDisplayString.
	 * Example: exempt the compound word "MetaHuman" to ensure its not reformatted
	 * as "Meta Human".
	 */
	static void AddNameToDisplayStringExemption(const FString& InExemption);
	static void RemoveNameToDisplayStringExemption(const FString& InExemption);

	/** Get the EName that this FName represents or nullptr */
	const EName* ToEName() const;

	/** 
		Tear down system and free all allocated memory 
	
		FName must not be used after teardown
	 */
	static void TearDown();

	/** Returns an integer that compares equal in the same way FNames do, only usable within the current process */
#if UE_FNAME_OUTLINE_NUMBER
	FORCEINLINE uint64 ToUnstableInt() const
	{
		return ComparisonIndex.ToUnstableInt();
	}
#else
	FORCEINLINE uint64 ToUnstableInt() const
	{
		static_assert(STRUCT_OFFSET(FName, ComparisonIndex) == 0);
		static_assert(STRUCT_OFFSET(FName, Number) == 4);
		static_assert((STRUCT_OFFSET(FName, Number) + sizeof(Number)) == sizeof(uint64));

		uint64 Out = 0;
		FMemory::Memcpy(&Out, this, sizeof(uint64));
		return Out;
	}
#endif

private:
	/** Index into the Names array (used to find String portion of the string/number pair used for comparison) */
	FNameEntryId	ComparisonIndex;
#if !UE_FNAME_OUTLINE_NUMBER
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	uint32			Number;
#endif// ! //UE_FNAME_OUTLINE_NUMBER
#if WITH_CASE_PRESERVING_NAME
	/** Index into the Names array (used to find String portion of the string/number pair used for display) */
	FNameEntryId	DisplayIndex;
#endif // WITH_CASE_PRESERVING_NAME

	friend const TCHAR* DebugFName(int32);
	friend const TCHAR* DebugFName(int32, int32);
	friend const TCHAR* DebugFName(FName&);

	friend struct FNameHelper;
	friend FScriptName NameToScriptName(FName InName);
	friend FMinimalName NameToMinimalName(FName InName);
	friend uint32 GetTypeHash(FName Name);
	friend FMinimalName::FMinimalName(const FName& Name);
	friend FScriptName::FScriptName(const FName& Name);
	friend FMemoryImageName::FMemoryImageName(const FName& Name);

	friend bool operator==(FName Lhs, FMinimalName Rhs);
	friend bool operator==(FName Lhs, FScriptName Rhs);
	friend bool operator==(FName Lhs, FMemoryImageName Rhs);

	template <typename StringBufferType>
	FORCEINLINE void AppendStringInternal(StringBufferType& Out) const;

	FORCEINLINE FNameEntryId GetDisplayIndexFast() const
	{
#if WITH_CASE_PRESERVING_NAME
		return DisplayIndex;
#else
		return ComparisonIndex;
#endif
	}

	// Accessor for unmodified comparison index when UE_FNAME_OUTLINE_NUMBER is set
	FORCEINLINE FNameEntryId GetComparisonIndexInternal() const
	{
		return ComparisonIndex;
	}

	// Accessor for unmodified display index when UE_FNAME_OUTLINE_NUMBER is set
	FORCEINLINE FNameEntryId GetDisplayIndexInternal() const
	{
#if WITH_CASE_PRESERVING_NAME
		return DisplayIndex;
#else // WITH_CASE_PRESERVING_NAME
		return ComparisonIndex;
#endif // WITH_CASE_PRESERVING_NAME
	}

	// Resolve the entry directly referred to by LookupId
	static const FNameEntry* ResolveEntry(FNameEntryId LookupId);
	// Recursively resolve through the entry referred to by LookupId to reach the allocated string entry, in the case of UE_FNAME_OUTLINE_NUMBER=1
	static const FNameEntry* ResolveEntryRecursive(FNameEntryId LookupId);

	static bool IsWithinBounds(FNameEntryId Id);

	// These FNameEntryIds are passed in from user code so they must be non-numbered if Number != NAME_NO_NUMBER_INTERNAL
#if UE_FNAME_OUTLINE_NUMBER
	static FName CreateNumberedName(FNameEntryId ComparisonId, FNameEntryId DisplayId, int32 Number);
#endif

	FORCEINLINE static FName CreateNumberedNameIfNecessary(FNameEntryId ComparisonId, FNameEntryId DisplayId, int32 Number)
	{
#if UE_FNAME_OUTLINE_NUMBER
		if (Number != NAME_NO_NUMBER_INTERNAL)
		{
			// We need to store a new entry in the name table
			return CreateNumberedName(ComparisonId, DisplayId, Number);
		}
		// Otherwise we can just set the index members
#endif
		FName Out;
		Out.ComparisonIndex = ComparisonId;
#if WITH_CASE_PRESERVING_NAME
		Out.DisplayIndex = DisplayId;
#endif
#if !UE_FNAME_OUTLINE_NUMBER
		Out.Number = Number;
#endif
		return Out;
	}

	FORCEINLINE static FName CreateNumberedNameIfNecessary(FNameEntryId ComparisonId, int32 Number)
	{
		return CreateNumberedNameIfNecessary(ComparisonId, ComparisonId, Number);
	}
};

template<> struct TIsZeroConstructType<class FName> { enum { Value = true }; };
Expose_TNameOf(FName)

namespace Freeze
{
	// These structures mirror the layout of FMemoryImageName depending on the value of WITH_CASE_PRESERVING_NAME
	// for use in memory image writing/unfreezing
	template<bool bCasePreserving>
	struct TMemoryImageNameLayout;

	template<>
	struct TMemoryImageNameLayout<false>
	{
		FNameEntryId	ComparisonIndex;
		uint32			NumberOrDummy = 0;
	};

	template<>
	struct TMemoryImageNameLayout<true> : public TMemoryImageNameLayout<false>
	{
		FNameEntryId	DisplayIndex;
	};

	CORE_API void ApplyMemoryImageNamePatch(void* NameDst, const FMemoryImageName& Name, const FPlatformTypeLayoutParameters& LayoutParams);

	CORE_API uint32 IntrinsicAppendHash(const FMemoryImageName* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher);
	CORE_API void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const FMemoryImageName& Object, const FTypeLayoutDesc&);
	CORE_API uint32 IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const FMemoryImageName& Object, void* OutDst);
	CORE_API void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const FScriptName& Object, const FTypeLayoutDesc&);
}

FORCEINLINE FMemoryImageName::FMemoryImageName()
{
	// The structure must match the layout of Freeze::TMemoryImageNameLayout for the matching value of WITH_CASE_PRESERVING_NAME
	static_assert(sizeof(FMemoryImageName) == sizeof(Freeze::TMemoryImageNameLayout<WITH_CASE_PRESERVING_NAME>));
	static_assert(STRUCT_OFFSET(FMemoryImageName, ComparisonIndex) == STRUCT_OFFSET(Freeze::TMemoryImageNameLayout<WITH_CASE_PRESERVING_NAME>, ComparisonIndex));
#if UE_FNAME_OUTLINE_NUMBER
	static_assert(STRUCT_OFFSET(FMemoryImageName, Dummy) == STRUCT_OFFSET(Freeze::TMemoryImageNameLayout<WITH_CASE_PRESERVING_NAME>, NumberOrDummy));
#else
	static_assert(STRUCT_OFFSET(FMemoryImageName, Number) == STRUCT_OFFSET(Freeze::TMemoryImageNameLayout<WITH_CASE_PRESERVING_NAME>, NumberOrDummy));
#endif
#if WITH_CASE_PRESERVING_NAME
	static_assert(STRUCT_OFFSET(FMemoryImageName, DisplayIndex) == STRUCT_OFFSET(Freeze::TMemoryImageNameLayout<WITH_CASE_PRESERVING_NAME>, DisplayIndex));
#endif

}

FORCEINLINE FMemoryImageName::FMemoryImageName(EName Name)
	: FMemoryImageName(FName(Name))
{
}

DECLARE_INTRINSIC_TYPE_LAYOUT(FMemoryImageName);
DECLARE_INTRINSIC_TYPE_LAYOUT(FScriptName);

#if UE_FNAME_OUTLINE_NUMBER
FORCEINLINE uint32 GetTypeHash(FName Name)
{
	return ::GetTypeHash(Name.GetComparisonIndexInternal());
}

FORCEINLINE uint32 GetTypeHash(FMinimalName Name)
{
	return ::GetTypeHash(Name.Index);
}

FORCEINLINE uint32 GetTypeHash(FScriptName Name)
{
	return ::GetTypeHash(Name.ComparisonIndex);
}

FORCEINLINE uint32 GetTypeHash(FMemoryImageName Name)
{
	return ::GetTypeHash(Name.ComparisonIndex);
}

FORCEINLINE FName::FName(FMinimalName InName)
	: ComparisonIndex(InName.Index)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.Index)
#endif
{
}

FORCEINLINE FName::FName(FScriptName InName)
	: ComparisonIndex(InName.ComparisonIndex)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.DisplayIndex)
#endif
{

}

FORCEINLINE FName::FName(FMemoryImageName InName)
	: ComparisonIndex(InName.ComparisonIndex)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.DisplayIndex)
#endif
{
}

FORCEINLINE FMinimalName::FMinimalName(const FName& Name)
	: Index(Name.GetComparisonIndexInternal())
{
}

FORCEINLINE FScriptName::FScriptName(const FName& Name)
	: ComparisonIndex(Name.GetComparisonIndexInternal())
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(Name.GetDisplayIndexInternal())
#endif
{
}

FORCEINLINE FMemoryImageName::FMemoryImageName(const FName& Name)
	: ComparisonIndex(Name.GetComparisonIndexInternal())
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(Name.GetDisplayIndexInternal())
#endif
{
}

FORCEINLINE bool FMinimalName::IsNone() const
{
	return Index.IsNone();
}

FORCEINLINE bool FMinimalName::operator<(FMinimalName Rhs) const
{
	return Index < Rhs.Index;
}

FORCEINLINE bool FScriptName::IsNone() const
{
	return ComparisonIndex.IsNone();
}

FORCEINLINE bool FMemoryImageName::IsNone() const
{
	return ComparisonIndex.IsNone();
}

FORCEINLINE bool FName::operator==(EName Ename) const
{
	return GetComparisonIndex() == Ename && GetNumber() == NAME_NO_NUMBER_INTERNAL;
}

FORCEINLINE bool operator==(FMinimalName Lhs, FMinimalName Rhs)
{
	return Lhs.Index == Rhs.Index;
}

FORCEINLINE bool operator==(FScriptName Lhs, FScriptName Rhs)
{
	return Lhs.ComparisonIndex == Rhs.ComparisonIndex;
}

FORCEINLINE bool operator==(FMemoryImageName Lhs, FMemoryImageName Rhs)
{
	return Lhs.ComparisonIndex == Rhs.ComparisonIndex;
}

FORCEINLINE bool operator==(FName Lhs, FMinimalName Rhs)
{
	return Lhs.GetComparisonIndexInternal() == Rhs.Index;
}

FORCEINLINE bool operator==(FName Lhs, FScriptName Rhs)
{
	return Lhs.GetComparisonIndexInternal() == Rhs.ComparisonIndex;
}

FORCEINLINE bool operator==(FName Lhs, FMemoryImageName Rhs)
{
	return Lhs.GetComparisonIndexInternal() == Rhs.ComparisonIndex;
}

FORCEINLINE bool FName::IsEqual(const FName& Rhs, const ENameCase CompareMethod /*= ENameCase::IgnoreCase*/, const bool bCompareNumber /*= true*/) const
{
	return bCompareNumber ?
		(CompareMethod == ENameCase::IgnoreCase ? GetComparisonIndexInternal() == Rhs.GetComparisonIndexInternal() : GetDisplayIndexInternal() == Rhs.GetDisplayIndexInternal()) :  // Unresolved indices include the number, are stored in instances
		(CompareMethod == ENameCase::IgnoreCase ? GetComparisonIndex() == Rhs.GetComparisonIndex() : GetDisplayIndex() == Rhs.GetDisplayIndex()); // Resolved indices, have to hit the name table
}

#else // UE_FNAME_OUTLINE_NUMBER
FORCEINLINE uint32 GetTypeHash(FName Name)
{
	return ::GetTypeHash(Name.GetComparisonIndex()) + Name.GetNumber();
}

FORCEINLINE uint32 GetTypeHash(FMinimalName Name)
{
	return ::GetTypeHash(Name.Index) + Name.Number;
}

FORCEINLINE uint32 GetTypeHash(FScriptName Name)
{
	return ::GetTypeHash(Name.ComparisonIndex) + Name.Number;
}

FORCEINLINE uint32 GetTypeHash(FMemoryImageName Name)
{
	return ::GetTypeHash(Name.ComparisonIndex) + Name.Number;
}

FORCEINLINE FName::FName(FMinimalName InName)
	: ComparisonIndex(InName.Index)
	, Number(InName.Number)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.Index)
#endif
{
}

FORCEINLINE FName::FName(FScriptName InName)
	: ComparisonIndex(InName.ComparisonIndex)
	, Number(InName.Number)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.DisplayIndex)
#endif
{
}

FORCEINLINE FName::FName(FMemoryImageName InName)
	: ComparisonIndex(InName.ComparisonIndex)
	, Number(InName.Number)
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(InName.DisplayIndex)
#endif
{
}

FORCEINLINE FMinimalName::FMinimalName(const FName& Name)
	: Index(Name.GetComparisonIndexInternal())
	, Number(Name.GetNumber())
{
}

FORCEINLINE FScriptName::FScriptName(const FName& Name)
	: ComparisonIndex(Name.GetComparisonIndexInternal())
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(Name.GetDisplayIndexInternal())
#endif
	, Number(Name.GetNumber())
{
}

FORCEINLINE FMemoryImageName::FMemoryImageName(const FName& Name)
	: ComparisonIndex(Name.GetComparisonIndex())
	, Number(Name.GetNumber())
#if WITH_CASE_PRESERVING_NAME
	, DisplayIndex(Name.GetDisplayIndex())
#endif
{
}

FORCEINLINE bool FMinimalName::IsNone() const
{
	return Index.IsNone() && Number == NAME_NO_NUMBER_INTERNAL;
}

FORCEINLINE bool FMinimalName::operator<(FMinimalName Rhs) const
{
	return Index == Rhs.Index ? Number < Rhs.Number : Index < Rhs.Index;
}

FORCEINLINE bool FScriptName::IsNone() const
{
	return ComparisonIndex.IsNone() && Number == NAME_NO_NUMBER_INTERNAL;
}

FORCEINLINE bool FMemoryImageName::IsNone() const
{
	return ComparisonIndex.IsNone() && Number == NAME_NO_NUMBER_INTERNAL;
}

FORCEINLINE bool FName::operator==(EName Ename) const
{
	return GetComparisonIndex() == Ename && GetNumber() == NAME_NO_NUMBER_INTERNAL;
}

FORCEINLINE bool operator==(FMinimalName Lhs, FMinimalName Rhs)
{
	return Lhs.Index == Rhs.Index && Lhs.Number == Rhs.Number;
}

FORCEINLINE bool operator==(FScriptName Lhs, FScriptName Rhs)
{
	return Lhs.ComparisonIndex == Rhs.ComparisonIndex && Lhs.Number == Rhs.Number;
}

FORCEINLINE bool operator==(FMemoryImageName Lhs, FMemoryImageName Rhs)
{
	return Lhs.ComparisonIndex == Rhs.ComparisonIndex && Lhs.Number == Rhs.Number;
}

FORCEINLINE bool operator==(FName Lhs, FMinimalName Rhs)
{
	return Lhs.GetComparisonIndex() == Rhs.Index && Lhs.GetNumber() == Rhs.Number;
}

FORCEINLINE bool operator==(FName Lhs, FScriptName Rhs)
{
	return Lhs.GetComparisonIndex() == Rhs.ComparisonIndex && Lhs.GetNumber() == Rhs.Number;
}

FORCEINLINE bool operator==(FName Lhs, FMemoryImageName Rhs)
{
	return Lhs.GetComparisonIndex() == Rhs.ComparisonIndex && Lhs.GetNumber() == Rhs.Number;
}

FORCEINLINE bool FName::IsEqual(const FName& Rhs, const ENameCase CompareMethod /*= ENameCase::IgnoreCase*/, const bool bCompareNumber /*= true*/) const
{
	return ((CompareMethod == ENameCase::IgnoreCase) ? GetComparisonIndex() == Rhs.GetComparisonIndex() : GetDisplayIndexFast() == Rhs.GetDisplayIndexFast())
		&& (!bCompareNumber || GetNumber() == Rhs.GetNumber());
}
#endif // UE_FNAME_OUTLINE_NUMBER

FORCEINLINE FName MinimalNameToName(FMinimalName InName)
{
	return FName(InName);
}

FORCEINLINE FName ScriptNameToName(FScriptName InName)
{
	return FName(InName);
}

FORCEINLINE FMinimalName NameToMinimalName(FName InName)
{
	return FMinimalName(InName);
}

FORCEINLINE FScriptName NameToScriptName(FName InName)
{
	return FScriptName(InName);
}

FORCEINLINE bool operator==(FMinimalName Lhs, FName Rhs)
{
	return operator==(Rhs, Lhs);
}

FORCEINLINE bool operator==(FScriptName Lhs, FName Rhs)
{
	return operator==(Rhs, Lhs);
}

FORCEINLINE bool operator==(FMemoryImageName Lhs, FName Rhs)
{
	return operator==(Rhs, Lhs);
}

FORCEINLINE bool operator!=(FMinimalName Lhs, FMinimalName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FMinimalName Lhs, FName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FScriptName Lhs, FScriptName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FScriptName Lhs, FName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FMemoryImageName Lhs, FMemoryImageName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FMemoryImageName Lhs, FName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FName Lhs, FMinimalName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FName Lhs, FScriptName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool operator!=(FName Lhs, FMemoryImageName Rhs)
{
	return !operator==(Lhs, Rhs);
}

FORCEINLINE bool FName::operator!=(EName Ename) const
{
	return !(*this == Ename);
}

FORCEINLINE FString LexToString(const FName& Name)
{
	return Name.ToString();
}

FORCEINLINE void LexFromString(FName& Name, const TCHAR* Str)
{
	Name = FName(Str);
}

inline FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, const FName& Name)
{
	Name.AppendString(Builder);
	return Builder;
}

inline FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, const FName& Name)
{
	Name.AppendString(Builder);
	return Builder;
}

CORE_API FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, FNameEntryId Id);
CORE_API FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, FNameEntryId Id);

/**
 * Equality operator with CharType* on left hand side and FName on right hand side
 * 
 * @param	LHS		CharType to compare to FName
 * @param	RHS		FName to compare to CharType
 * @return True if strings match, false otherwise.
 */
template <typename CharType>
inline bool operator==(const CharType *LHS, const FName &RHS)
{
	return RHS == LHS;
}

/**
 * Inequality operator with CharType* on left hand side and FName on right hand side
 *
 * @param	LHS		CharType to compare to FName
 * @param	RHS		FName to compare to CharType
 * @return True if strings don't match, false otherwise.
 */
template <typename CharType>
inline bool operator!=(const CharType *LHS, const FName &RHS)
{
	return RHS != LHS;
}

/** FNames act like PODs. */
template <> struct TIsPODType<FName> { enum { Value = true }; };

/** Fast non-alphabetical order that is only stable during this process' lifetime */
struct FNameFastLess
{
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.CompareIndexes(B) < 0;
	}

	FORCEINLINE bool operator()(FNameEntryId A, FNameEntryId B) const
	{
		return A.FastLess(B);
	}
};

UE_DEPRECATED(4.23, "Please use FNameFastLess instead.")
typedef FNameFastLess FNameSortIndexes;

/** Slow alphabetical order that is stable / deterministic over process runs */
struct FNameLexicalLess
{
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.Compare(B) < 0;
	}

	FORCEINLINE bool operator()(FNameEntryId A, FNameEntryId B) const
	{
		return A.LexicalLess(B);
	}
};

#ifndef WITH_CUSTOM_NAME_ENCODING
inline void FNameEntry::Encode(ANSICHAR*, uint32) {}
inline void FNameEntry::Encode(WIDECHAR*, uint32) {}
inline void FNameEntry::Decode(ANSICHAR*, uint32) {}
inline void FNameEntry::Decode(WIDECHAR*, uint32) {}
#endif

struct FNameDebugVisualizer
{
	CORE_API static uint8** GetBlocks();
private:
	static constexpr uint32 EntryStride = alignof(FNameEntry);
	static constexpr uint32 OffsetBits = 16;
	static constexpr uint32 BlockBits = 13;
	static constexpr uint32 OffsetMask = (1 << OffsetBits) - 1;
	static constexpr uint32 UnusedMask = UINT32_MAX << BlockBits << OffsetBits;
	static constexpr uint32 MaxLength = NAME_SIZE;
};

/** Lazily constructed FName that helps avoid allocating FNames during static initialization */
class FLazyName
{
public:
	FLazyName()
		: Either(FNameEntryId())
	{}

	/** @param Literal must be a string literal */
	template<int N>
	FLazyName(const WIDECHAR(&Literal)[N])
		: Either(Literal)
		, Number(ParseNumber(Literal, N - 1))
		, bLiteralIsWide(true)
	{}

	/** @param Literal must be a string literal */
	template<int N>
	FLazyName(const ANSICHAR(&Literal)[N])
		: Either(Literal)
		, Number(ParseNumber(Literal, N - 1))
		, bLiteralIsWide(false)
	{}

	explicit FLazyName(FName Name)
		: Either(Name.GetComparisonIndex())
		, Number(Name.GetNumber())
	{}
	
	operator FName() const
	{
		return Resolve();
	}

	CORE_API FName Resolve() const;

private:
	struct FLiteralOrName
	{
		// NOTE: uses high bit of pointer for flag; this may be an issue in future when high byte of address may be used for features like hardware ASAN
		static constexpr uint64 LiteralFlag = uint64(1) << (sizeof(uint64) * 8 - 1);

		explicit FLiteralOrName(const ANSICHAR* Literal)
			: Int(reinterpret_cast<uint64>(Literal) | LiteralFlag)
		{}
		
		explicit FLiteralOrName(const WIDECHAR* Literal)
			: Int(reinterpret_cast<uint64>(Literal) | LiteralFlag)
		{}

		explicit FLiteralOrName(FNameEntryId Name)
			: Int(Name.ToUnstableInt())
		{}

		bool IsName() const
		{
			return (LiteralFlag & Int) == 0;
		}

		bool IsLiteral() const
		{
			return (LiteralFlag & Int) != 0;
		}

		FNameEntryId AsName() const
		{
			return FNameEntryId::FromUnstableInt(static_cast<uint32>(Int));
		}
		
		const ANSICHAR* AsAnsiLiteral() const
		{
			return reinterpret_cast<const ANSICHAR*>(Int & ~LiteralFlag);
		}

		const WIDECHAR* AsWideLiteral() const
		{
			return reinterpret_cast<const WIDECHAR*>(Int & ~LiteralFlag);
		}

		uint64 Int;
	};

	mutable FLiteralOrName Either;
	mutable uint32 Number = 0;

	// Distinguishes WIDECHAR* and ANSICHAR* literals, doesn't indicate if literal contains any wide characters 
	bool bLiteralIsWide = false;
	
	CORE_API static uint32 ParseNumber(const WIDECHAR* Literal, int32 Len);
	CORE_API static uint32 ParseNumber(const ANSICHAR* Literal, int32 Len);

public:

	friend bool operator==(FName Name, const FLazyName& Lazy)
	{
		// If !Name.IsNone(), we have started creating FNames
		// and might as well resolve and cache Lazy
		if (Lazy.Either.IsName() || !Name.IsNone())
		{
			return Name == Lazy.Resolve();
		}
		else if (!Lazy.bLiteralIsWide)
		{
			return Name == Lazy.Either.AsAnsiLiteral();
		}
		else
		{
			return Name == Lazy.Either.AsWideLiteral();
		}
	}

	friend bool operator==(const FLazyName& Lazy, FName Name)
	{
		return Name == Lazy;
	}

	CORE_API friend bool operator==(const FLazyName& A, const FLazyName& B);

};

/**
 * Serialization util that optimizes WITH_CASE_PRESERVING_NAME-loading by reducing comparison id lookups
 *
 * Stores 32-bit display entry id with an unused bit to indicate if FName::GetComparisonIdFromDisplayId lookup is needed.
 *
 * Note that only display entries should be saved to make output deterministic.
 */
class FDisplayNameEntryId
{
public:
	FDisplayNameEntryId() : FDisplayNameEntryId(FName()) {}
	explicit FDisplayNameEntryId(FName Name) : FDisplayNameEntryId(Name.GetDisplayIndex(), Name.GetComparisonIndex()) {}
	FORCEINLINE FName ToName(uint32 Number) const { return FName(GetComparisonId(), GetDisplayId(), Number); }

private:
#if WITH_CASE_PRESERVING_NAME
	static constexpr uint32 DifferentIdsFlag = 1u << 31;
	static constexpr uint32 DisplayIdMask = ~DifferentIdsFlag;

	uint32 Value = 0;

	FDisplayNameEntryId(FNameEntryId Id, FNameEntryId CmpId) : Value(Id.ToUnstableInt() | (Id != CmpId) * DifferentIdsFlag) {}
	FORCEINLINE bool SameIds() const { return (Value & DifferentIdsFlag) == 0; }
	FORCEINLINE FNameEntryId GetDisplayId() const { return FNameEntryId::FromUnstableInt(Value & DisplayIdMask); }
	FORCEINLINE FNameEntryId GetComparisonId() const { return SameIds() ? GetDisplayId() : FName::GetComparisonIdFromDisplayId(GetDisplayId()); }
	friend bool operator==(FDisplayNameEntryId A, FDisplayNameEntryId B) { return A.Value == B.Value; }
#else
	FNameEntryId Id;

	FDisplayNameEntryId(FNameEntryId InId, FNameEntryId) : Id(InId) {}
	FORCEINLINE FNameEntryId GetDisplayId() const { return Id; }
	FORCEINLINE FNameEntryId GetComparisonId() const { return Id; }
	friend bool operator==(FDisplayNameEntryId A, FDisplayNameEntryId B) { return A.Id == B.Id; }
#endif
	friend bool operator==(FNameEntryId A, FDisplayNameEntryId B) { return A == B.GetDisplayId(); }
	friend bool operator==(FDisplayNameEntryId A, FNameEntryId B) { return A.GetDisplayId() == B; }
	friend uint32 GetTypeHash(FDisplayNameEntryId InId) { return GetTypeHash(InId.GetDisplayId()); }

public: // Internal functions for batch serialization code - intentionally lacking CORE_API
	static FDisplayNameEntryId FromComparisonId(FNameEntryId ComparisonId);
	FNameEntryId ToDisplayId() const;
	void SetLoadedComparisonId(FNameEntryId ComparisonId); // Called first
#if WITH_CASE_PRESERVING_NAME
	void SetLoadedDifferentDisplayId(FNameEntryId DisplayId); // Called second if display id differs
	FNameEntryId GetLoadedComparisonId() const; // Get the already loaded comparison id
#endif
};

/**
 * A string builder with inline storage for FNames.
 */
class FNameBuilder : public TStringBuilder<FName::StringBufferSize>
{
public:
	FNameBuilder() = default;

	inline explicit FNameBuilder(const FName InName)
	{
		InName.AppendString(*this);
	}
};

template <> struct TIsContiguousContainer<FNameBuilder> { static constexpr bool Value = true; };

/** Update the Hash with the FName's text and number */
class FBlake3;
CORE_API void AppendHash(FBlake3& Builder, FName In);