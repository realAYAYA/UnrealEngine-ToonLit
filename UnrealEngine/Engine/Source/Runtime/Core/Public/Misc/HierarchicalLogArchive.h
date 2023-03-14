// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/ArchiveProxy.h"

class FArchive;

struct CORE_API FHierarchicalLogArchive : private FArchiveProxy
{
public:
	FHierarchicalLogArchive(FArchive& InInnerArchive);

	struct FIndentScope
	{
		FIndentScope(FHierarchicalLogArchive* InAr) : Ar(InAr) { Ar->Indentation++; }
		~FIndentScope() { check(Ar->Indentation); Ar->Indentation--; }
		FHierarchicalLogArchive* Ar;
	};

	void Print(const TCHAR* InLine)
	{
		WriteLine(InLine, false);
	}

	UE_NODISCARD FIndentScope PrintIndent(const TCHAR* InLine)
	{
		WriteLine(InLine, true);
		return FIndentScope(this);
	}

	template <typename FmtType, typename... Types>
	void Printf(const FmtType& Fmt, Types... Args)
	{
		WriteLine(FString::Printf(Fmt, Args...), false);
	}

	template <typename FmtType, typename... Types>
	UE_NODISCARD FIndentScope PrintfIndent(const FmtType& Fmt, Types... Args)
	{
		WriteLine(FString::Printf(Fmt, Args...), true);
		return FIndentScope(this);
	}

private:
	void WriteLine(const FString& InLine, bool bIndent = false);

	int32 Indentation;
};

#if NO_LOGGING
#define UE_SCOPED_INDENT_LOG_ARCHIVE(Code)
#else
#define UE_SCOPED_INDENT_LOG_ARCHIVE(Code) \
	FHierarchicalLogArchive::FIndentScope BODY_MACRO_COMBINE(Scoped,Indent,_,__LINE__) = Code
#endif
