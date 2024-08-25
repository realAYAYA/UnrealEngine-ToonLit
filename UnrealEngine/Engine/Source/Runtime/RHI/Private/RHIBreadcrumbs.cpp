// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIBreadcrumbs.h"
#include "Misc/MemStack.h"
#include "RHI.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Trace/Trace.inl"

#if RHI_WANT_BREADCRUMB_EVENTS

void FRHIBreadcrumbStack::Reset()
{
	BreadcrumbStackTop = nullptr;
	FirstUnsubmittedBreadcrumb = nullptr;
}

FRHIBreadcrumb* FRHIBreadcrumbStack::PushBreadcrumb(FMemStackBase& Allocator, const TCHAR* InText, int32 InLen)
{
	FRHIBreadcrumb* NewBreadcrumb = (FRHIBreadcrumb*)Allocator.Alloc(sizeof(FRHIBreadcrumb), alignof(FRHIBreadcrumb));

	int32 Len = (InLen > 0 ? InLen : FCString::Strlen(InText)) + 1;
	TCHAR* NewName = (TCHAR*)Allocator.Alloc(Len * sizeof(TCHAR), alignof(TCHAR));
	FCString::Strcpy(NewName, Len, InText);

	NewBreadcrumb->Parent = BreadcrumbStackTop;
	NewBreadcrumb->Name = NewName;

	BreadcrumbStackTop = NewBreadcrumb;
	if (FirstUnsubmittedBreadcrumb == nullptr)
	{
		FirstUnsubmittedBreadcrumb = NewBreadcrumb;
	}

	return NewBreadcrumb;
}

FRHIBreadcrumb* FRHIBreadcrumbStack::PushBreadcrumbPrintf(FMemStackBase& Allocator, const TCHAR* InFormat, ...)
{
	TCHAR BreadcrumbString[1024];
	int32 WrittenLength = 0;

	GET_TYPED_VARARGS_RESULT(TCHAR, BreadcrumbString, UE_ARRAY_COUNT(BreadcrumbString), UE_ARRAY_COUNT(BreadcrumbString) - 1, InFormat, InFormat, WrittenLength);

	return PushBreadcrumb(Allocator, BreadcrumbString, WrittenLength);
}

FRHIBreadcrumb* FRHIBreadcrumbStack::PopBreadcrumb()
{
	check(BreadcrumbStackTop != nullptr); // popping more than pushing
	BreadcrumbStackTop = BreadcrumbStackTop->Parent;
	return BreadcrumbStackTop;
}

FRHIBreadcrumb* FRHIBreadcrumbStack::PopFirstUnsubmittedBreadcrumb()
{
	FRHIBreadcrumb* Breadcrumb = FirstUnsubmittedBreadcrumb;
	FirstUnsubmittedBreadcrumb = nullptr;
	return Breadcrumb;
}

void FRHIBreadcrumbStack::DeepCopy(FMemStackBase& Allocator, const FRHIBreadcrumbStack& Parent)
{
	// We have to do a deep copy of each breadcrumb since their data lives inside each stack.
	TArray<const TCHAR*, TInlineAllocator<8>> BreadcrumbStack;
	for (FRHIBreadcrumb* Breadcrumb = Parent.BreadcrumbStackTop; Breadcrumb; Breadcrumb = Breadcrumb->Parent)
	{
		BreadcrumbStack.Add(Breadcrumb->Name);
	}

	const int32 Size = BreadcrumbStack.Num();
	for (int32 i = 0; i < Size; ++i)
	{
		PushBreadcrumb(Allocator, BreadcrumbStack[Size - 1 - i]);
	}
}

void FRHIBreadcrumbStack::ValidateEmpty() const
{
	check(BreadcrumbStackTop == nullptr);

	// Should be null if we submitted
	checkf(FirstUnsubmittedBreadcrumb == nullptr, TEXT("RHI breadcrumb not submitted. Name:%s"), FirstUnsubmittedBreadcrumb->Name);
}

void FRHIBreadcrumbStack::DebugLog() const
{
	UE_LOG(LogRHI, Log, TEXT("[%p] RHI breadcrumb log:\n"), this);

	for (FRHIBreadcrumb* Breadcrumb = BreadcrumbStackTop; Breadcrumb; Breadcrumb = Breadcrumb->Parent)
	{
		UE_LOG(LogRHI, Log, TEXT("[%p]\t%s\n"), this, Breadcrumb->Name);
	}
}

#if WITH_ADDITIONAL_CRASH_CONTEXTS
void FRHIBreadcrumbStack::WriteRenderBreadcrumbs(FCrashContextExtendedWriter& Writer, const FRHIBreadcrumb** BreadcrumbStack, uint32 BreadcrumbStackIndex, const TCHAR* ThreadName)
{
	enum
	{
		MAX_BREADCRUMBS = 64,
		MAX_BREADCRUMB_STRING = 4096,
		MAX_BREADCRUMB_NAME_STRING = 128,
	};
	static int BreadcrumbId = 0;

	TCHAR StaticBreadcrumbStackString[MAX_BREADCRUMB_STRING];
	size_t BreadcrumbStackStringSize = 0;

	auto WriteBreadcrumbLine = [&](const TCHAR* InFormat, ...)
	{
		if (BreadcrumbStackStringSize < MAX_BREADCRUMB_STRING)
		{
			int32 WrittenLength = 0;
			GET_TYPED_VARARGS_RESULT(
				TCHAR,
				&StaticBreadcrumbStackString[BreadcrumbStackStringSize],
				MAX_BREADCRUMB_STRING - BreadcrumbStackStringSize,
				MAX_BREADCRUMB_STRING - BreadcrumbStackStringSize - 1,
				InFormat, InFormat, WrittenLength);

			BreadcrumbStackStringSize += WrittenLength;
		}
	};

	WriteBreadcrumbLine(TEXT("Breadcrumbs '%s'\n"), ThreadName);

	const uint32 NumBreadcrumbStacks = BreadcrumbStackIndex + 1;

	for (uint32 BreadcrumbIndex = 0; BreadcrumbIndex < NumBreadcrumbStacks; BreadcrumbIndex++)
	{
		if (const FRHIBreadcrumb* CurrentBreadcrumb = BreadcrumbStack[BreadcrumbStackIndex - BreadcrumbIndex])
		{
			const TCHAR* BreadcrumbNames[MAX_BREADCRUMBS];

			const FRHIBreadcrumb* Breadcrumb = CurrentBreadcrumb;
			int32 NameIndex = 0;
			while (Breadcrumb && NameIndex < MAX_BREADCRUMBS)
			{
				BreadcrumbNames[NameIndex++] = Breadcrumb->Name;
				Breadcrumb = Breadcrumb->Parent;
			}

			WriteBreadcrumbLine(TEXT("Context %d/%d\n"), BreadcrumbIndex + 1, NumBreadcrumbStacks);

			uint32 StackPos = 0;
			for (int32 i = NameIndex - 1; i >= 0; --i)
			{
				WriteBreadcrumbLine(TEXT("\t%02d %s\n"), StackPos++, BreadcrumbNames[i]);
			}
		}
	}

	TCHAR StaticBreadcrumbName[MAX_BREADCRUMB_NAME_STRING];
	FCString::Snprintf(StaticBreadcrumbName, MAX_BREADCRUMB_NAME_STRING, TEXT("Breadcrumbs_%s_%d"), ThreadName, BreadcrumbId++);
	Writer.AddString(StaticBreadcrumbName, StaticBreadcrumbStackString);
	UE_LOG(LogRHI, Error, TEXT("%s"), StaticBreadcrumbStackString);
}
#endif

#endif
