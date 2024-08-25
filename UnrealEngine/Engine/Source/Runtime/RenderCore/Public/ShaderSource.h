// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/AnsiString.h"
#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"

#define SHADER_SOURCE_ANSI 1

#if SHADER_SOURCE_ANSI
#define SHADER_SOURCE_LITERAL(S) S
#define SHADER_SOURCE_VIEWLITERAL(S) ANSITEXTVIEW(S)
#else
#define SHADER_SOURCE_LITERAL(S) TEXT(S)
#define SHADER_SOURCE_VIEWLITERAL(S) TEXTVIEW(S)
#endif

/* Class used in shader compilation pipelines which wraps source code and ensures sufficient padding such that 16-byte wide SIMD
 * operations on the source are guaranteed to read valid memory even if starting from the last character.
 */
class FShaderSource
{
private:
	inline void SetLen(int32 Num, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		Source.SetNumUninitialized(Num + ShaderSourceSimdPadding, AllowShrinking);
		FMemory::Memzero(Source.GetData() + Num, sizeof(CharType) * ShaderSourceSimdPadding);

		SourceCompressed.Empty();
		DecompressedSize = 0;
	}

public:
#if SHADER_SOURCE_ANSI
	typedef ANSICHAR CharType;
	typedef FAnsiStringView FViewType;
	typedef FAnsiString FStringType;
	typedef FCStringAnsi FCStringType;
#else
	typedef TCHAR CharType;
	typedef FStringView FViewType;
	typedef FString FStringType;
	typedef FCString FCStringType;
#endif

	/** Constexpr predicate indicating whether wide or ansi chars are used */
	static constexpr bool IsWide() { return sizeof(CharType) == 2; }
	/** Constexpr function returning the number of characters read in a single SIMD compare op */
	static constexpr int32 GetSimdCharCount() { return IsWide() ? 8 : 16; }
	/** Constexpr function returning a mask value for a single character */
	static constexpr int32 GetSingleCharMask() { return IsWide() ? 3 : 1; }

	/* Construct an empty shader source object; will still contain padding */
	FShaderSource() { SetLen(0); };

	/* Given a string view construct a shader source object containing the contents of that string.
	 * Note that this will incur a memcpy of the string contents.
	 * @param InSrc The source string to be copied
	 * @param AdditionalSlack optional additional space to allocate; this is on top of the automatic padding.
	 */
	RENDERCORE_API explicit FShaderSource(FViewType InSrc, int32 AdditionalSlack = 0);

	/* Set the given string as the contents of this shader source object. The inner allocation will grow to fit
	 * the string contents as needed.
	 * Note that this will incur a memcpy of the string contents.
	 * @param InSrc The source string to be copied
	 * @param AdditionalSlack optional additional space to allocate; this is on top of the automatic padding.
	 */
	RENDERCORE_API void Set(FViewType InSrc, int32 AdditionalSlack = 0);
#if !SHADER_SOURCE_ANSI
	/* Set the given ANSI string as the contents of this shader source object. The inner allocation will grow to fit
	 * the string contents as needed. Note that this will incur a conversion to TCHAR and memcpy of the string contents.
	 * Note that this is only needed if SHADER_SOURCE_ANSI is 0; redundant with Set function accepting a FViewType otherwise.
	 * @param InSrc The source ANSI string to be copied
	 */
	RENDERCORE_API void Set(FAnsiStringView InSrc);
#endif

	/* Move assignment operator accepting a string object. This will append padding bytes to the existing string, as such it's
	 * best if there's sufficient extra capacity in the string storage to avoid incurring a realloc-and-copy here.
	 * @param InSrc The source string whose data this object will take ownership of.
	 */
	RENDERCORE_API FShaderSource& operator=(FStringType&& InSrc);

	/* Reduces the set size of the stored string length, optionally shrinking the allocation.
	 * @param Num the desired allocation size (padding bytes will be added on top of this)
	 * @param AllowShrinking whether to reallocate or keep the existing larger size allocation
	 */
	inline void ShrinkToLen(int32 Num, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		checkf(Num <= Len(), TEXT("Trying to shrink to %d characters but existing allocation is smaller (%d characters)"), Num, Len());
		SetLen(Num, AllowShrinking);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("ShrinkToLen")
	FORCEINLINE void ShrinkToLen(int32 Num, bool bShrink)
	{
		ShrinkToLen(Num, bShrink ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/* String view accessor. Will decompress data if it was previously compressed.
	 * @return a string view pointing to the source contents, excluding padding.
	 */
	inline FViewType GetView() const
	{ 
		checkf(!IsCompressed(), TEXT("FShaderSource is compressed; must decompress prior to calling GetView"));
		return { Source.GetData(), Len() }; 
	}

	/* Direct data pointer accessor. Will decompress data if it was previously compressed.
	 * @return a pointer to the source data; will be null terminated by the SIMD padding.
	 */
	inline CharType* GetData()
	{
		checkf(!IsCompressed(), TEXT("FShaderSource is compressed; must decompress prior to calling GetData"));
		return Source.GetData();
	}

	/* IsEmpty predicate
	 * @return true if this source object is empty excluding the SIMD padding, false otherwise.
	 */
	inline bool IsEmpty() const { return Len() == 0; }

	/* Length accessor.
	 * @return the non-padded length of the source (also excluding null terminator)
	 */
	inline int32 Len() const
	{ 
		checkf(!IsCompressed(), TEXT("Len should not be called on compressed FShaderSource."))
		return Source.Num() - ShaderSourceSimdPadding;
	};

	inline bool IsCompressed() const
	{
		return DecompressedSize != 0; // DecompressedSize member is only set when compression occurs
	}

	/* FArchive serialization operator. Note this currently serializes padding for simplicity's sake.
	 * @param Ar The archive to serialize from/to
	 * @param ShaderSource the source object to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar, FShaderSource& ShaderSource);

	RENDERCORE_API void Compress();
	RENDERCORE_API void Decompress();
	
private:
#if SHADER_SOURCE_ANSI
	static constexpr int32 ShaderSourceSimdPadding = 15;
#else
	static constexpr int32 ShaderSourceSimdPadding = 7;
#endif
	TArray<CharType> Source;
	TArray<uint8> SourceCompressed;
	int32 DecompressedSize = 0;
};

