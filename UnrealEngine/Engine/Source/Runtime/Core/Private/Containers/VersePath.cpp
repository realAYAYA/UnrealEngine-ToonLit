// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/VersePath.h"
#include "Misc/Char.h"

namespace UE::VersePath::Private
{
	struct FNullTerminal
	{
		explicit FNullTerminal() = default;
	};

	FORCEINLINE bool operator==(const TCHAR* Ch, FNullTerminal) { return *Ch == TEXT('\0'); }
	FORCEINLINE bool operator!=(const TCHAR* Ch, FNullTerminal) { return *Ch != TEXT('\0'); }

	auto Alpha = [](TCHAR Ch)
	{
		return Ch == '_' || (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z');
	};

	auto AlphaNum = [](TCHAR Ch)
	{
		return Ch == '_' || (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z') || (Ch >= '0' && Ch <= '9');
	};

	template <typename EndType>
	bool ParseChar(TCHAR Ch, const TCHAR*& Ptr, EndType End)
	{
		const TCHAR* LocalPtr = Ptr;
		if (LocalPtr == End || *LocalPtr != Ch)
		{
			return false;
		}

		Ptr = LocalPtr + 1;
		return true;
	}

	template <typename EndType>
	bool ParseLabel(const TCHAR*& Ptr, EndType End)
	{
		const TCHAR* LocalPtr = Ptr;
		if (LocalPtr == End || !AlphaNum(*LocalPtr))
		{
			return false;
		}

		++LocalPtr;
		for (;;)
		{
			if (LocalPtr == End)
			{
				break;
			}

			TCHAR Ch = *LocalPtr;
			if (Ch != TEXT('-') && Ch != TEXT('.') && !AlphaNum(Ch))
			{
				break;
			}

			++LocalPtr;
		}

		Ptr = LocalPtr;
		return true;
	}

	template <typename EndType>
	bool ParseIdent(const TCHAR*& Ptr, EndType End)
	{
		const TCHAR* LocalPtr = Ptr;
		if (LocalPtr == End || !Alpha(*LocalPtr))
		{
			return false;
		}

		++LocalPtr;
		while (LocalPtr != End && AlphaNum(*LocalPtr))
		{
			++LocalPtr;
		}

		Ptr = LocalPtr;
		return true;
	}

	template <typename EndType>
	bool ParseDomain(const TCHAR*& Ptr, EndType End)
	{
		if (!ParseLabel(Ptr, End))
		{
			return false;
		}

		if (ParseChar('@', Ptr, End))
		{
			if (!ParseLabel(Ptr, End))
			{
				return false;
			}
		}

		return true;
	}

	template <typename EndType>
	bool ParseSubpath(const TCHAR*& Ptr, EndType End)
	{
		for (;;)
		{
			if (!ParseIdent(Ptr, End))
			{
				return false;
			}

			if (!ParseChar('/', Ptr, End))
			{
				return true;
			}
		}
	}

	template <typename EndType>
	bool ParsePath(const TCHAR*& Ptr, EndType End)
	{
		if (!ParseChar('/', Ptr, End))
		{
			return false;
		}

		if (!ParseDomain(Ptr, End))
		{
			return false;
		}

		if (ParseChar('/', Ptr, End))
		{
			if (!ParseSubpath(Ptr, End))
			{
				return false;
			}
		}

		return true;
	}

	template <typename EndType>
	bool IsValidVersePath(const TCHAR* Ptr, EndType End)
	{
		bool bResult = UE::VersePath::Private::ParsePath(Ptr, End);

		// Make sure the entire string was parsed.
		return bResult && Ptr == End;
	}

	template <typename EndType>
	bool IsValidDomain(const TCHAR* Ptr, EndType End)
	{
		bool bResult = UE::VersePath::Private::ParseDomain(Ptr, End);

		// Make sure the entire string was parsed.
		return bResult && Ptr == End;
	}

	template <typename EndType>
	bool IsValidSubpath(const TCHAR* Ptr, EndType End)
	{
		bool bResult = UE::VersePath::Private::ParseSubpath(Ptr, End);

		// Make sure the entire string was parsed.
		return bResult && Ptr == End;
	}

	template <typename EndType>
	bool IsValidIdent(const TCHAR* Ptr, EndType End)
	{
		bool bResult = UE::VersePath::Private::ParseIdent(Ptr, End);

		// Make sure the entire string was parsed.
		return bResult && Ptr == End;
	}

	template <typename EndType>
	void NormalizeDomainCase(TCHAR* Path, EndType End)
	{
		checkSlow(IsValidVersePath(Path, End));
		++Path;

		// Everything up to the second slash (if there is one) is normalized to lowercase
		for (;;)
		{
			if (Path == End)
			{
				return;
			}

			TCHAR Ch = *Path;
			if (Ch == '/')
			{
				return;
			}

			*Path++ = FChar::ToLower(Ch);
		}
	}
}

bool UE::Core::FVersePath::IsValidFullPath(const TCHAR* String)
{
	return UE::VersePath::Private::IsValidVersePath(String, UE::VersePath::Private::FNullTerminal{});
}

bool UE::Core::FVersePath::IsValidFullPath(const TCHAR* String, int32 Len)
{
	return UE::VersePath::Private::IsValidVersePath(String, String + Len);
}

bool UE::Core::FVersePath::IsValidDomain(const TCHAR* String)
{
	return UE::VersePath::Private::IsValidDomain(String, UE::VersePath::Private::FNullTerminal{});
}

bool UE::Core::FVersePath::IsValidDomain(const TCHAR* String, int32 Len)
{
	return UE::VersePath::Private::IsValidDomain(String, String + Len);
}

bool UE::Core::FVersePath::IsValidSubpath(const TCHAR* String)
{
	return UE::VersePath::Private::IsValidSubpath(String, UE::VersePath::Private::FNullTerminal{});
}

bool UE::Core::FVersePath::IsValidSubpath(const TCHAR* String, int32 Len)
{
	return UE::VersePath::Private::IsValidSubpath(String, String + Len);
}

bool UE::Core::FVersePath::IsValidIdent(const TCHAR* String)
{
	return UE::VersePath::Private::IsValidIdent(String, UE::VersePath::Private::FNullTerminal{});
}

bool UE::Core::FVersePath::IsValidIdent(const TCHAR* String, int32 Len)
{
	return UE::VersePath::Private::IsValidIdent(String, String + Len);
}

bool UE::Core::FVersePath::TryMake(FVersePath& OutPath, const FString& Path)
{
	if (!IsValidFullPath(*Path))
	{
		return false;
	}

	OutPath.PathString = Path;
	TCHAR* OutPathPtr = OutPath.PathString.GetCharArray().GetData();
	UE::VersePath::Private::NormalizeDomainCase(OutPathPtr, OutPathPtr + OutPath.PathString.Len());
	return true;
}

bool UE::Core::FVersePath::TryMake(FVersePath& OutPath, FString&& Path)
{
	if (!IsValidFullPath(*Path))
	{
		return false;
	}

	OutPath.PathString = MoveTemp(Path);
	TCHAR* OutPathPtr = OutPath.PathString.GetCharArray().GetData();
	UE::VersePath::Private::NormalizeDomainCase(OutPathPtr, OutPathPtr + OutPath.PathString.Len());
	return true;
}

FString UE::Core::MangleGuidToVerseIdent(const FString& Guid)
{
	FString Ident = TEXT("_") + Guid;
	Ident.ReplaceInline(TEXT("-"), TEXT(""), ESearchCase::CaseSensitive);
	Ident.ReplaceInline(TEXT("{"), TEXT(""), ESearchCase::CaseSensitive);
	Ident.ReplaceInline(TEXT("}"), TEXT(""), ESearchCase::CaseSensitive);
	return Ident;
}
