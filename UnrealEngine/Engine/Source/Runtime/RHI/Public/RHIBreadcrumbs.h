// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "RHIDefinitions.h"

#if !defined(RHI_WANT_BREADCRUMB_EVENTS)
#define RHI_WANT_BREADCRUMB_EVENTS 0
#endif

#if RHI_WANT_BREADCRUMB_EVENTS
#define IF_RHI_WANT_BREADCRUMB_EVENTS(X) X
#else
#define IF_RHI_WANT_BREADCRUMB_EVENTS(X)
#endif

#if RHI_WANT_BREADCRUMB_EVENTS

class FMemStackBase;

struct FRHIBreadcrumb
{
	FRHIBreadcrumb* Parent{};
	const TCHAR* Name{};
};

template <typename AllocatorType = FDefaultAllocator>
struct TRHIBreadcrumbState
{
	void PushBreadcrumb(const TCHAR* InText)
	{
		NameOffsets.Push(NameBuffer.Num());
		NameBuffer.Append(InText, FCString::Strlen(InText) + 1);
	}

	void PopBreadcrumb()
	{
		check(!NameOffsets.IsEmpty());
		NameBuffer.SetNum(NameOffsets.Last(), EAllowShrinking::No);
		NameOffsets.Pop();
	}

	TArray<TCHAR, TInlineAllocator<1024, AllocatorType>> NameBuffer;
	TArray<int32, TInlineAllocator<8, AllocatorType>> NameOffsets;
};

using FRHIBreadcrumbState = TRHIBreadcrumbState<>;

class FRHIBreadcrumbStack
{
public:
	RHI_API void Reset();

	RHI_API FRHIBreadcrumb* PushBreadcrumb(FMemStackBase& Allocator, const TCHAR* InText, int32 InLen = 0);

	RHI_API FRHIBreadcrumb* PushBreadcrumbPrintf(FMemStackBase& Allocator, const TCHAR* InFormat, ...);

	RHI_API FRHIBreadcrumb* PopBreadcrumb();

	RHI_API FRHIBreadcrumb* PopFirstUnsubmittedBreadcrumb();

	template <typename AllocatorType>
	void ExportBreadcrumbState(TRHIBreadcrumbState<AllocatorType>& State) const
	{
		TArray<const TCHAR*, TInlineAllocator<16>> NameStack;

		for (FRHIBreadcrumb* Breadcrumb = BreadcrumbStackTop; Breadcrumb; Breadcrumb = Breadcrumb->Parent)
		{
			NameStack.Push(Breadcrumb->Name);
		}

		while (!NameStack.IsEmpty())
		{
			const TCHAR* Name = NameStack.Pop();
			const int32 NameLen = FCString::Strlen(Name);
			const int32 NameOffset = State.NameBuffer.Num();

			State.NameBuffer.Append(Name, NameLen + 1);
			State.NameOffsets.Add(NameOffset);
		}
	}

	template <typename AllocatorType>
	void ImportBreadcrumbState(FMemStackBase& Allocator, const TRHIBreadcrumbState<AllocatorType>& State)
	{
		Reset();

		for (int32 NameOffset : State.NameOffsets)
		{
			const TCHAR* BreadcrumbName = &State.NameBuffer[NameOffset];
			PushBreadcrumb(Allocator, BreadcrumbName);
		}
	}

	RHI_API void DeepCopy(FMemStackBase& Allocator, const FRHIBreadcrumbStack& Parent);

	RHI_API void ValidateEmpty() const;

	RHI_API void DebugLog() const;

#if WITH_ADDITIONAL_CRASH_CONTEXTS
	RHI_API static void WriteRenderBreadcrumbs(struct FCrashContextExtendedWriter& Writer, const FRHIBreadcrumb** BreadcrumbStack, uint32 BreadcrumbStackIndex, const TCHAR* ThreadName);
#endif

private:
	FRHIBreadcrumb* BreadcrumbStackTop{};
	FRHIBreadcrumb* FirstUnsubmittedBreadcrumb{};
};

#endif