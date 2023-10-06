// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextKey.h"
#include "Containers/Map.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Hash/CityHash.h"
#include "Misc/ByteSwap.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/LazySingleton.h"
#include "Misc/ScopeLock.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextKey, Log, All);

class FTextKeyState
{
public:
	void FindOrAdd(const TCHAR* InStr, const int32 InStrLen, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(*InStr != 0);

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(InStr, InStrLen);
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			LLM_SCOPE(ELLMTag::Localization);
			FString StrCopy = CopyString(InStrLen, InStr); // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void FindOrAdd(const TCHAR* InStr, const int32 InStrLen, const uint32 InStrHash, const TCHAR*& OutStrPtr)
	{
		check(*InStr != 0);

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(InStr, InStrLen, InStrHash);
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			LLM_SCOPE(ELLMTag::Localization);
			FString StrCopy = CopyString(InStrLen, InStr); // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
	}

	void FindOrAdd(const FString& InStr, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(!InStr.IsEmpty());

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(*InStr, InStr.Len());
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			LLM_SCOPE(ELLMTag::Localization);
			FString StrCopy = InStr; // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}
		
		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void FindOrAdd(FString&& InStr, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(!InStr.IsEmpty());

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(*InStr, InStr.Len());
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			LLM_SCOPE(ELLMTag::Localization);
			const FKeyData DestKey(*InStr, InStr.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(InStr));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void Shrink()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		LLM_SCOPE(ELLMTag::Localization);
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
		FKeyData(const TCHAR* InStr, const int32 InStrLen)
			: Str(InStr)
			, StrLen(InStrLen)
			, StrHash(TextKeyUtil::HashString(InStr, InStrLen)) // Note: This hash gets serialized so *DO NOT* change it without fixing the serialization to discard the old hash method
		{
		}

		FKeyData(const TCHAR* InStr, const int32 InStrLen, const uint32 InStrHash)
			: Str(InStr)
			, StrLen(InStrLen)
			, StrHash(InStrHash)
		{
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
			return A.StrHash;
		}

		const TCHAR* Str;
		int32 StrLen;
		uint32 StrHash;
	};

	FORCEINLINE FString CopyString(const int32 InStrLen, const TCHAR* InStr)
	{
		// We do this rather than use the FString constructor directly, 
		// as this method avoids slack being added to the allocation
		FString Str;
		Str.Reserve(InStrLen);
		Str.AppendChars(InStr, InStrLen);
		return Str;
	}

	FCriticalSection SynchronizationObject;
	TMap<FKeyData, FString> KeysTable;
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

FTextKey::FTextKey(const TCHAR* InStr)
{
	if (*InStr == 0)
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(InStr, FCString::Strlen(InStr), StrPtr, StrHash);
	}
}

FTextKey::FTextKey(const FString& InStr)
{
	if (InStr.IsEmpty())
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(InStr, StrPtr, StrHash);
	}
}

FTextKey::FTextKey(FString&& InStr)
{
	if (InStr.IsEmpty())
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(MoveTemp(InStr), StrPtr, StrHash);
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
			FTextKeyState::GetState().FindOrAdd(StrBuffer.GetData(), StrBuffer.Num() - 1, StrPtr, StrHash);
		}
	}
	else
	{
		TextKeyUtil::SaveKeyString(Ar, StrPtr);
	}
}

void FTextKey::SerializeWithHash(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Ar << StrHash;

		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(StrBuffer.GetData(), StrBuffer.Num() - 1, StrHash, StrPtr);
		}
	}
	else
	{
		Ar << StrHash;

		TextKeyUtil::SaveKeyString(Ar, StrPtr);
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
			FTextKeyState::GetState().FindOrAdd(StrBuffer.GetData(), StrBuffer.Num() - 1, StrPtr, StrHash);
		}
	}
	else
	{
		Ar << StrHash;

		TextKeyUtil::SaveKeyString(Ar, StrPtr);
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
				FTextKeyState::GetState().FindOrAdd(MoveTemp(TmpStr), StrPtr, StrHash);
			}
		}
		else
		{
			FString TmpStr = StrPtr;
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
			Record << SA_VALUE(TEXT("Hash"), StrHash);

			FString TmpStr;
			Record << SA_VALUE(TEXT("Str"), TmpStr);

			if (TmpStr.IsEmpty())
			{
				Reset();
			}
			else
			{
				FTextKeyState::GetState().FindOrAdd(*TmpStr, TmpStr.Len(), StrHash, StrPtr);
			}
		}
		else
		{
			Record << SA_VALUE(TEXT("Hash"), StrHash);

			FString TmpStr = StrPtr;
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
				FTextKeyState::GetState().FindOrAdd(MoveTemp(TmpStr), StrPtr, StrHash);
			}
		}
		else
		{
			Record << SA_VALUE(TEXT("Hash"), StrHash);

			FString TmpStr = StrPtr;
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
	StrHash = 0;
}

void FTextKey::CompactDataStructures()
{
	FTextKeyState::GetState().Shrink();
}

void FTextKey::TearDown()
{
	FTextKeyState::TearDown();
}
