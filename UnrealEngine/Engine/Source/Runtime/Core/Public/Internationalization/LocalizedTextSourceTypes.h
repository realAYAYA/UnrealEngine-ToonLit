// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"

/**
 * Categories of localized text.
 * @note This enum is mirrored in NoExportTypes.h for UHT.
 */
enum class ELocalizedTextSourceCategory : uint8
{
	Game,
	Engine,
	Editor,
};

/**
 * Result codes from calling QueryLocalizedResourceResult.
 */
enum class EQueryLocalizedResourceResult : uint8
{
	/** Indicates the query found a matching entry and added its result */
	Found,
	/** Indicates that the query failed to find a matching entry */
	NotFound,
	/** Indicates that the query failed as this text source doesn't support queries */
	NotImplemented,
};

/**
 * Load flags used in localization initialization.
 */
enum class ELocalizationLoadFlags : uint8
{
	/** Load no data */
	None = 0,

	/** Load native data */
	Native = 1<<0,

	/** Load editor localization data */
	Editor = 1<<1,

	/** Load game localization data */
	Game = 1<<2,

	/** Load engine localization data */
	Engine = 1<<3,

	/** Load additional (eg, plugin) localization data */
	Additional = 1<<4,

	/** Force localized game data to be loaded, even when running in the editor */
	ForceLocalizedGame = 1<<5,
};
ENUM_CLASS_FLAGS(ELocalizationLoadFlags);

/**
 * Pre-defined priorities for ILocalizedTextSource.
 */
struct ELocalizedTextSourcePriority
{
	enum Enum
	{
		Lowest = -1000,
		Low = -100,
		Normal = 0,
		High = 100,
		Highest = 1000,
	};
};

#ifndef UE_TEXT_DISPLAYSTRING_USE_REFCOUNTPTR
	#define UE_TEXT_DISPLAYSTRING_USE_REFCOUNTPTR (1)
#endif

#if UE_TEXT_DISPLAYSTRING_USE_REFCOUNTPTR

namespace UE::Text::Private
{

class FRefCountedDisplayString : public TRefCountingMixin<FRefCountedDisplayString>
{
public:
	explicit FRefCountedDisplayString(FString&& InDisplayString)
		: DisplayString(MoveTemp(InDisplayString))
	{
	}

	[[nodiscard]] FString& Private_GetDisplayString()
	{
		return DisplayString;
	}

private:
	FString DisplayString;
};

/**
 * Wrapper to give TRefCountPtr a minimal TSharedRef/TSharedPtr interface for backwards compatibility with code that was already using FTextDisplayStringRef/FTextDisplayStringPtr
 */
template <typename ObjectType>
class TDisplayStringPtrBase
{
	static_assert(TPointerIsConvertibleFromTo<ObjectType, const FString>::Value, "TDisplayStringPtrBase can only be constructed with FString types");

public:
	TDisplayStringPtrBase() = default;

	explicit TDisplayStringPtrBase(const TRefCountPtr<FRefCountedDisplayString>& InDisplayStringPtr)
		: DisplayStringPtr(InDisplayStringPtr)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return IsValid();
	}

	[[nodiscard]] bool IsValid() const
	{
		return DisplayStringPtr.IsValid();
	}

	[[nodiscard]] ObjectType& operator*() const
	{
		return GetDisplayString();
	}

	[[nodiscard]] ObjectType* operator->() const
	{
		return &GetDisplayString();
	}

	[[nodiscard]] const TRefCountPtr<FRefCountedDisplayString>& Private_GetDisplayStringPtr() const
	{
		return DisplayStringPtr;
	}

protected:
	[[nodiscard]] FString& GetDisplayString() const
	{
		check(IsValid());
		return DisplayStringPtr.GetReference()->Private_GetDisplayString();
	}

	TRefCountPtr<FRefCountedDisplayString> DisplayStringPtr;
};

/**
 * Wrapper to give TRefCountPtr a minimal TSharedRef interface for backwards compatibility with code that was already using FTextDisplayStringRef
 */
template <typename ObjectType>
class TDisplayStringRef : public TDisplayStringPtrBase<ObjectType>
{
public:
	TDisplayStringRef() = default;

	explicit TDisplayStringRef(const TRefCountPtr<FRefCountedDisplayString>& InDisplayStringPtr)
		: TDisplayStringPtrBase<ObjectType>(InDisplayStringPtr)
	{
		check(this->IsValid());
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringRef(const TDisplayStringRef<OtherType>& InOther)
		: TDisplayStringPtrBase<ObjectType>(InOther.Private_GetDisplayStringPtr())
	{
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringRef& operator=(const TDisplayStringRef<OtherType>& InOther)
	{
		if (this->DisplayStringPtr != InOther.Private_GetDisplayStringPtr())
		{
			this->DisplayStringPtr = InOther.Private_GetDisplayStringPtr();
		}
		return *this;
	}

	[[nodiscard]] ObjectType& Get() const
	{
		return this->GetDisplayString();
	}
};

/**
 * Wrapper to give TRefCountPtr a minimal TSharedPtr interface for backwards compatibility with code that was already using FTextDisplayStringPtr
 */
template <typename ObjectType>
class TDisplayStringPtr : public TDisplayStringPtrBase<ObjectType>
{
public:
	TDisplayStringPtr() = default;

	TDisplayStringPtr(TYPE_OF_NULLPTR)
		: TDisplayStringPtr()
	{
	}

	explicit TDisplayStringPtr(const TRefCountPtr<FRefCountedDisplayString>& InDisplayStringPtr)
		: TDisplayStringPtrBase<ObjectType>(InDisplayStringPtr)
	{
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringPtr(const TDisplayStringPtr<OtherType>& InOther)
		: TDisplayStringPtrBase<ObjectType>(InOther.Private_GetDisplayStringPtr())
	{
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringPtr(const TDisplayStringRef<OtherType>& InOther)
		: TDisplayStringPtrBase<ObjectType>(InOther.Private_GetDisplayStringPtr())
	{
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringPtr& operator=(const TDisplayStringPtr<OtherType>& InOther)
	{
		if (this->DisplayStringPtr != InOther.Private_GetDisplayStringPtr())
		{
			this->DisplayStringPtr = InOther.Private_GetDisplayStringPtr();
		}
		return *this;
	}

	template <typename OtherType, typename = decltype(ImplicitConv<ObjectType*>((OtherType*)nullptr))>
	TDisplayStringPtr& operator=(const TDisplayStringRef<OtherType>& InOther)
	{
		if (this->DisplayStringPtr != InOther.Private_GetDisplayStringPtr())
		{
			this->DisplayStringPtr = InOther.Private_GetDisplayStringPtr();
		}
		return *this;
	}

	[[nodiscard]] ObjectType* Get() const
	{
		return this->IsValid()
			? &this->GetDisplayString()
			: nullptr;
	}

	void Reset()
	{
		this->DisplayStringPtr = TRefCountPtr<FRefCountedDisplayString>();
	}

	[[nodiscard]] TDisplayStringRef<ObjectType> ToSharedRef() const
	{
		check(this->IsValid());
		return TDisplayStringRef<ObjectType>(this->DisplayStringPtr);
	}
};

} // namespace UE::Text::Private

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator==(const UE::Text::Private::TDisplayStringRef<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringRef<ObjectTypeB>& B)
{
	return &A.Get() == &B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator==(const UE::Text::Private::TDisplayStringRef<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringPtr<ObjectTypeB>& B)
{
	return &A.Get() == B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator==(const UE::Text::Private::TDisplayStringPtr<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringRef<ObjectTypeB>& B)
{
	return A.Get() == &B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator==(const UE::Text::Private::TDisplayStringPtr<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringPtr<ObjectTypeB>& B)
{
	return A.Get() == B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator!=(const UE::Text::Private::TDisplayStringRef<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringRef<ObjectTypeB>& B)
{
	return &A.Get() != &B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator!=(const UE::Text::Private::TDisplayStringRef<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringPtr<ObjectTypeB>& B)
{
	return &A.Get() != B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator!=(const UE::Text::Private::TDisplayStringPtr<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringRef<ObjectTypeB>& B)
{
	return A.Get() != &B.Get();
}

template <typename ObjectTypeA, typename ObjectTypeB>
[[nodiscard]] bool operator!=(const UE::Text::Private::TDisplayStringPtr<ObjectTypeA>& A, const UE::Text::Private::TDisplayStringPtr<ObjectTypeB>& B)
{
	return A.Get() != B.Get();
}

template <typename ObjectType>
[[nodiscard]] uint32 GetTypeHash(const UE::Text::Private::TDisplayStringRef<ObjectType>& A)
{
	return ::PointerHash(&A.Get());
}

template <typename ObjectType>
[[nodiscard]] uint32 GetTypeHash(const UE::Text::Private::TDisplayStringPtr<ObjectType>& A)
{
	return ::PointerHash(A.Get());
}

using FTextDisplayStringRef = UE::Text::Private::TDisplayStringRef<FString>;
using FTextDisplayStringPtr = UE::Text::Private::TDisplayStringPtr<FString>;
using FTextConstDisplayStringRef = UE::Text::Private::TDisplayStringRef<const FString>;
using FTextConstDisplayStringPtr = UE::Text::Private::TDisplayStringPtr<const FString>;

[[nodiscard]] inline FTextDisplayStringRef MakeTextDisplayString(FString&& InDisplayString)
{
	return FTextDisplayStringRef(MakeRefCount<UE::Text::Private::FRefCountedDisplayString>(MoveTemp(InDisplayString)));
}

#else	// UE_TEXT_DISPLAYSTRING_USE_REFCOUNTPTR

using FTextDisplayStringRef = TSharedRef<FString, ESPMode::ThreadSafe>;
using FTextDisplayStringPtr = TSharedPtr<FString, ESPMode::ThreadSafe>;
using FTextConstDisplayStringRef = TSharedRef<const FString, ESPMode::ThreadSafe>;
using FTextConstDisplayStringPtr = TSharedPtr<const FString, ESPMode::ThreadSafe>;

[[nodiscard]] inline FTextDisplayStringRef MakeTextDisplayString(FString&& InDisplayString)
{
	return MakeShared<FString, ESPMode::ThreadSafe>(MoveTemp(InDisplayString));
}

#endif	// UE_TEXT_DISPLAYSTRING_USE_REFCOUNTPTR
