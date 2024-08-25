// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextKey.h"

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "CoreGlobals.h"
#include "HAL/LowLevelMemTracker.h"
#include "Hash/CityHash.h"
#include "Logging/LogMacros.h"
#include "Misc/ByteSwap.h"
#include "Misc/LazySingleton.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextKey, Log, All);

#ifndef UE_TEXTKEY_USE_SLAB_ALLOCATOR
	#define UE_TEXTKEY_USE_SLAB_ALLOCATOR (1)
#endif

class FTextKeyState
{
public:
	void FindOrAdd(FStringView InStr, FTextKey& OutTextKey)
	{
		check(!InStr.IsEmpty());
		FindOrAddImpl(FKeyData(InStr), OutTextKey);
	}

	void FindOrAdd(FStringView InStr, const uint32 InStrHash, FTextKey& OutTextKey)
	{
		check(!InStr.IsEmpty());
		FindOrAddImpl(FKeyData(InStr, InStrHash), OutTextKey);
	}

	void Shrink()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		LLM_SCOPE_BYNAME(TEXT("Localization/TextKeys"));
		KeysTable.Shrink();
	}

	static FTextKeyState& GetState()
	{
		return TLazySingleton<FTextKeyState>::Get();
	}

	static void TearDown()
	{
		return TLazySingleton<FTextKeyState>::TearDown();
	}

private:
	struct FKeyData
	{
		explicit FKeyData(FStringView InStr)
			: Str(InStr.GetData())
			, StrLen(InStr.Len())
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
			, StrHash(TextKeyUtil::HashString(InStr)) // Note: This hash gets serialized so *DO NOT* change it without fixing the serialization to discard the old hash method (also update FTextKey::GetTypeHash)
#endif
		{
		}

		FKeyData(FStringView InStr, const uint32 InStrHash)
			: Str(InStr.GetData())
			, StrLen(InStr.Len())
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
			, StrHash(InStrHash)
#endif
		{
		}

		FKeyData(FStringView InStr, const FKeyData& InOther)
			: Str(InStr.GetData())
			, StrLen(InStr.Len())
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
			, StrHash(InOther.StrHash)
#endif
		{
		}

		FStringView ToView() const
		{
			return FStringView(Str, StrLen);
		}

		friend FORCEINLINE bool operator==(const FKeyData& A, const FKeyData& B)
		{
			// We can use Memcmp here as we know we're comparing two blocks of the same size and don't care about lexical ordering
			return A.StrLen == B.StrLen && FMemory::Memcmp(A.Str, B.Str, A.StrLen * sizeof(TCHAR)) == 0;
		}

		friend FORCEINLINE bool operator!=(const FKeyData& A, const FKeyData& B)
		{
			// We can use Memcmp here as we know we're comparing two blocks of the same size and don't care about lexical ordering
			return A.StrLen != B.StrLen || FMemory::Memcmp(A.Str, B.Str, A.StrLen * sizeof(TCHAR)) != 0;
		}

		friend FORCEINLINE uint32 GetTypeHash(const FKeyData& A)
		{
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
			return A.StrHash;
#else
			return TextKeyUtil::HashString(A.ToView());
#endif
		}

		const TCHAR* Str;
		int32 StrLen;
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		uint32 StrHash;
#endif
	};

	void FindOrAddImpl(const FKeyData& KeyData, FTextKey& OutTextKey)
	{
		const TCHAR* StrPtr = FindOrAddString(KeyData);
		check(StrPtr);

		OutTextKey.StrPtr = StrPtr;
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		OutTextKey.StrHash = KeyData.StrHash;
#endif
	}

	const TCHAR* FindOrAddString(const FKeyData& KeyData)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
#if UE_TEXTKEY_USE_SLAB_ALLOCATOR
		const TCHAR* StrPtr = KeysTable.FindRef(KeyData);
		if (!StrPtr)
		{
			LLM_SCOPE_BYNAME(TEXT("Localization/TextKeys"));
			StrPtr = StringAllocations.Add(KeyData.ToView());
			KeysTable.Add(FKeyData(StrPtr, KeyData), StrPtr);
		}
		return StrPtr;
#else
		const FString* StrPtr = KeysTable.Find(KeyData);
		if (!StrPtr)
		{
			LLM_SCOPE_BYNAME(TEXT("Localization/TextKeys"));

			// Need to copy the string here so we can reference its internal allocation as the key
			FString StrCopy;
			{
				// We do this rather than use the FString constructor directly, 
				// as this method avoids slack being added to the allocation
				StrCopy.Reserve(KeyData.StrLen);
				StrCopy.AppendChars(KeyData.Str, KeyData.StrLen);
			}

			FKeyData DestKey(StrCopy, KeyData);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}
		return **StrPtr;
#endif
	}

#if UE_TEXTKEY_USE_SLAB_ALLOCATOR
	class FStringSlabAllocator
	{
	public:
		FStringSlabAllocator() = default;

		~FStringSlabAllocator()
		{
			uint64 TotalNumElementsWasted = 0;
			for (FStringSlab& Slab : Slabs)
			{
				TotalNumElementsWasted += (SlabSizeInElements - Slab.NumElementsUsed);
				FMemory::Free(Slab.Allocation);
			}
			//UE_LOG(LogLocalization, Log, TEXT("FTextKey slab allocator allocated %d slabs (of %d elements) and wasted %d elements (%d bytes)"), Slabs.Num(), SlabSizeInElements, TotalNumElementsWasted, TotalNumElementsWasted * sizeof(TCHAR));
		}

		const TCHAR* Add(FStringView InStr)
		{
			const int32 NumSlabElementsNeeded = InStr.Len() + 1;
			FStringSlab& Slab = GetSlab(NumSlabElementsNeeded);

			TCHAR* StringPtr = Slab.Allocation + Slab.NumElementsUsed;
			Slab.NumElementsUsed += NumSlabElementsNeeded;

			FMemory::Memcpy(StringPtr, InStr.GetData(), InStr.Len() * sizeof(TCHAR));
			*(StringPtr + InStr.Len()) = 0;

			return StringPtr;
		}

	private:
		static const int32 SlabSizeInElements = 8192;

		struct FStringSlab
		{
			TCHAR* Allocation = nullptr;
			int32 NumElementsUsed = 0;
		};

		FStringSlab& GetSlab(const int32 NumSlabElementsNeeded)
		{
			if (Slabs.Num() > 0)
			{
				// Always try the last slab first
				if (FStringSlab& Slab = Slabs.Last();
					(Slab.NumElementsUsed + NumSlabElementsNeeded) <= SlabSizeInElements)
				{
					return Slab;
				}

				if (Slabs.Num() > 1)
				{
					// We only add to the last slab in the array, so if we've run out of space, merge it back into the array based 
					// on its current used size (sorted most used first), and then check to see if the new last slab has space
					FStringSlab SlabToMerge = Slabs.Pop(EAllowShrinking::No);
					const int32 MergeIndex = Algo::LowerBound(Slabs, SlabToMerge, [](const FStringSlab& A, const FStringSlab& B)
					{
						return A.NumElementsUsed > B.NumElementsUsed;
					});
					Slabs.Insert(SlabToMerge, MergeIndex);
					if (MergeIndex != Slabs.Num())
					{
						if (FStringSlab& Slab = Slabs.Last();
							(Slab.NumElementsUsed + NumSlabElementsNeeded) <= SlabSizeInElements)
						{
							return Slab;
						}
					}
				}
			}

			// If no slabs have space then just allocate a new one
			checkf(NumSlabElementsNeeded <= SlabSizeInElements, TEXT("Tried to allocate a FTextKey string of %d elements, which is larger than the allowed slab size of %d elements!"), NumSlabElementsNeeded, SlabSizeInElements);
			FStringSlab& Slab = Slabs.AddDefaulted_GetRef();
			Slab.Allocation = reinterpret_cast<TCHAR*>(FMemory::Malloc(SlabSizeInElements * sizeof(TCHAR)));
			return Slab;
		}

		TArray<FStringSlab> Slabs;
	};
#endif

	FCriticalSection SynchronizationObject;
#if UE_TEXTKEY_USE_SLAB_ALLOCATOR
	FStringSlabAllocator StringAllocations;
	TMap<FKeyData, const TCHAR*> KeysTable;
#else
	TMap<FKeyData, FString> KeysTable;
#endif
};

namespace TextKeyUtil
{

static const int32 InlineStringSize = 128;
typedef TArray<TCHAR, TInlineAllocator<InlineStringSize>> FInlineStringBuffer;

static_assert(PLATFORM_LITTLE_ENDIAN, "FTextKey serialization needs updating to support big-endian platforms!");

bool SaveKeyString(FArchive& Ar, const TCHAR* InStrPtr)
{
	// Note: This serialization should be compatible with the FString serialization, but avoids creating an FString if the FTextKey is already cached
	// > 0 for ANSICHAR, < 0 for UTF16CHAR serialization
	check(!Ar.IsLoading());

	const bool bSaveUnicodeChar = Ar.IsForcingUnicode() || !FCString::IsPureAnsi(InStrPtr);
	if (bSaveUnicodeChar)
	{
		// Note: This is a no-op on platforms that are using a 16-bit TCHAR
		FTCHARToUTF16 UTF16String(InStrPtr);
		const int32 Num = UTF16String.Length() + 1; // include the null terminator

		int32 SaveNum = -Num;
		Ar << SaveNum;

		if (Num)
		{
			Ar.Serialize((void*)UTF16String.Get(), sizeof(UTF16CHAR) * Num);
		}
	}
	else
	{
		int32 Num = FCString::Strlen(InStrPtr) + 1; // include the null terminator
		Ar << Num;

		if (Num)
		{
			Ar.Serialize((void*)StringCast<ANSICHAR>(InStrPtr, Num).Get(), sizeof(ANSICHAR) * Num);
		}
	}

	return true;
}

bool LoadKeyString(FArchive& Ar, FInlineStringBuffer& OutStrBuffer)
{
	// Note: This serialization should be compatible with the FString serialization, but avoids creating an FString if the FTextKey is already cached
	// > 0 for ANSICHAR, < 0 for UTF16CHAR serialization
	check(Ar.IsLoading());

	int32 SaveNum = 0;
	Ar << SaveNum;

	const bool bLoadUnicodeChar = SaveNum < 0;
	if (bLoadUnicodeChar)
	{
		SaveNum = -SaveNum;
	}

	// If SaveNum is still less than 0, they must have passed in MIN_INT. Archive is corrupted.
	if (SaveNum < 0)
	{
		Ar.SetCriticalError();
		return false;
	}

	// Protect against network packets allocating too much memory
	const int64 MaxSerializeSize = Ar.GetMaxSerializeSize();
	if ((MaxSerializeSize > 0) && (SaveNum > MaxSerializeSize))
	{
		Ar.SetCriticalError();
		return false;
	}

	// Create a buffer of the correct size
	OutStrBuffer.AddUninitialized(SaveNum);

	if (SaveNum)
	{
		if (bLoadUnicodeChar)
		{
			// Read in the Unicode string
			auto Passthru = StringMemoryPassthru<UCS2CHAR, TCHAR, InlineStringSize>(OutStrBuffer.GetData(), SaveNum, SaveNum);
			Ar.Serialize(Passthru.Get(), SaveNum * sizeof(UCS2CHAR));
			Passthru.Get()[SaveNum - 1] = 0; // Ensure the string has a null terminator
			Passthru.Apply();

			// Inline combine any surrogate pairs in the data when loading into a UTF-32 string
			StringConv::InlineCombineSurrogates_Array(OutStrBuffer);
		}
		else
		{
			// Read in the ANSI string
			auto Passthru = StringMemoryPassthru<ANSICHAR, TCHAR, InlineStringSize>(OutStrBuffer.GetData(), SaveNum, SaveNum);
			Ar.Serialize(Passthru.Get(), SaveNum * sizeof(ANSICHAR));
			Passthru.Get()[SaveNum - 1] = 0; // Ensure the string has a null terminator
			Passthru.Apply();
		}

		UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	}

	return true;
}

uint32 HashString(const FTCHARToUTF16& InStr)
{
	const uint64 StrHash = CityHash64((const char*)InStr.Get(), InStr.Length() * sizeof(UTF16CHAR));
	return GetTypeHash(StrHash);
}

}

FTextKey::FTextKey()
{
	Reset();
}

FTextKey::FTextKey(FStringView InStr)
{
	if (InStr.IsEmpty())
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(InStr, *this);
	}
}

void FTextKey::SerializeAsString(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(FStringView(StrBuffer.GetData(), StrBuffer.Num() - 1), *this);
		}
	}
	else
	{
		TextKeyUtil::SaveKeyString(Ar, GetChars());
	}
}

void FTextKey::SerializeWithHash(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		uint32 TmpStrHash = 0;
		Ar << TmpStrHash;

		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(FStringView(StrBuffer.GetData(), StrBuffer.Num() - 1), TmpStrHash, *this);
		}
	}
	else
	{
		uint32 TmpStrHash = GetTypeHash(*this);
		Ar << TmpStrHash;

		TextKeyUtil::SaveKeyString(Ar, GetChars());
	}
}

void FTextKey::SerializeDiscardHash(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		uint32 DiscardedHash = 0;
		Ar << DiscardedHash;

		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(FStringView(StrBuffer.GetData(), StrBuffer.Num() - 1), *this);
		}
	}
	else
	{
		uint32 TmpStrHash = GetTypeHash(*this);
		Ar << TmpStrHash;

		TextKeyUtil::SaveKeyString(Ar, GetChars());
	}
}

void FTextKey::SerializeAsString(FStructuredArchiveSlot Slot)
{
	if (Slot.GetArchiveState().IsTextFormat())
	{
		if (Slot.GetUnderlyingArchive().IsLoading())
		{
			FString TmpStr;
			Slot << TmpStr;

			if (TmpStr.IsEmpty())
			{
				Reset();
			}
			else
			{
				FTextKeyState::GetState().FindOrAdd(TmpStr, *this);
			}
		}
		else
		{
			FString TmpStr = GetChars();
			Slot << TmpStr;
		}
	}
	else
	{
		Slot.EnterStream(); // Let the slot know that we will custom serialize
		SerializeAsString(Slot.GetUnderlyingArchive());
	}
}

void FTextKey::SerializeWithHash(FStructuredArchiveSlot Slot)
{
	if (Slot.GetArchiveState().IsTextFormat())
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();

		if (Slot.GetUnderlyingArchive().IsLoading())
		{
			uint32 TmpStrHash = 0;
			Record << SA_VALUE(TEXT("Hash"), TmpStrHash);

			FString TmpStr;
			Record << SA_VALUE(TEXT("Str"), TmpStr);

			if (TmpStr.IsEmpty())
			{
				Reset();
			}
			else
			{
				FTextKeyState::GetState().FindOrAdd(TmpStr, TmpStrHash, *this);
			}
		}
		else
		{
			uint32 TmpStrHash = GetTypeHash(*this);
			Record << SA_VALUE(TEXT("Hash"), TmpStrHash);

			FString TmpStr = GetChars();
			Record << SA_VALUE(TEXT("Str"), TmpStr);
		}
	}
	else
	{
		Slot.EnterStream(); // Let the slot know that we will custom serialize
		SerializeWithHash(Slot.GetUnderlyingArchive());
	}
}

void FTextKey::SerializeDiscardHash(FStructuredArchiveSlot Slot)
{
	if (Slot.GetArchiveState().IsTextFormat())
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();

		if (Slot.GetUnderlyingArchive().IsLoading())
		{
			uint32 DiscardedHash = 0;
			Record << SA_VALUE(TEXT("Hash"), DiscardedHash);

			FString TmpStr;
			Record << SA_VALUE(TEXT("Str"), TmpStr);

			if (TmpStr.IsEmpty())
			{
				Reset();
			}
			else
			{
				FTextKeyState::GetState().FindOrAdd(TmpStr, *this);
			}
		}
		else
		{
			uint32 TmpStrHash = GetTypeHash(*this);
			Record << SA_VALUE(TEXT("Hash"), TmpStrHash);

			FString TmpStr = GetChars();
			Record << SA_VALUE(TEXT("Str"), TmpStr);
		}
	}
	else
	{
		Slot.EnterStream(); // Let the slot know that we will custom serialize
		SerializeDiscardHash(Slot.GetUnderlyingArchive());
	}
}

void FTextKey::Reset()
{
	StrPtr = TEXT("");
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
	StrHash = 0;
#endif
}

void FTextKey::CompactDataStructures()
{
	FTextKeyState::GetState().Shrink();
}

void FTextKey::TearDown()
{
	FTextKeyState::TearDown();
}
