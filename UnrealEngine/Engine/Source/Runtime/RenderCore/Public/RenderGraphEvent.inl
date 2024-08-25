// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

template <typename ScopeOpType>
TRDGScopeOpArray<ScopeOpType> TRDGScopeStackHelper<ScopeOpType>::CompilePassPrologue(const ScopeType* ParentScope, const TCHAR* PassName)
{
	const int32 OffsetIndex = Ops.Num();

	// Find out how many scopes needs to be popped.
	TStaticArray<const ScopeType*, kScopeStackDepthMax> TraversedScopes;
	int32 CommonScopeId = -1;
	int32 TraversedScopeCount = 0;

	// Find common ancestor between current stack and requested scope.
	while (ParentScope && TraversedScopeCount < kScopeStackDepthMax)
	{
		TraversedScopes[TraversedScopeCount] = ParentScope;

		for (int32 i = 0; i < ScopeStack.Num(); i++)
		{
			if (ScopeStack[i] == ParentScope)
			{
				CommonScopeId = i;
				break;
			}
		}

		if (CommonScopeId != -1)
		{
			break;
		}

		TraversedScopeCount++;
		ParentScope = ParentScope->ParentScope;
	}

	// Pop no longer used scopes.
	for (int32 Index = kScopeStackDepthMax - 1; Index >= CommonScopeId + 1; --Index)
	{
		if (ScopeStack[Index])
		{
			Ops.Emplace(ScopeOpType::Pop(ScopeStack[Index]));
			ScopeStack[Index] = nullptr;
		}
	}

	// Push new scopes.
	for (int32 Index = TraversedScopeCount - 1; Index >= 0 && CommonScopeId + 1 < static_cast<int32>(kScopeStackDepthMax); Index--)
	{
		Ops.Emplace(ScopeOpType::Push(TraversedScopes[Index]));
		CommonScopeId++;
		ScopeStack[CommonScopeId] = TraversedScopes[Index];
	}

	// Skip empty strings.
	if (PassName && *PassName)
	{
		Ops.Emplace(ScopeOpType::Push(PassName));
		bNamePushed = true;
	}

	return TRDGScopeOpArray<ScopeOpType>(Ops, OffsetIndex, Ops.Num() - OffsetIndex);
}

template <typename ScopeOpType>
TRDGScopeOpArray<ScopeOpType> TRDGScopeStackHelper<ScopeOpType>::CompilePassEpilogue()
{
	const int32 OffsetIndex = Ops.Num();

	if (bNamePushed)
	{
		bNamePushed = false;
		Ops.Emplace(ScopeOpType::Pop());
	}

	return TRDGScopeOpArray<ScopeOpType>(Ops, OffsetIndex, Ops.Num() - OffsetIndex);
}

template <typename ScopeOpType>
TRDGScopeOpArray<ScopeOpType> TRDGScopeStackHelper<ScopeOpType>::EndCompile()
{
	const int32 OffsetIndex = Ops.Num();

	for (int32 ScopeIndex = kScopeStackDepthMax - 1; ScopeIndex >= 0; --ScopeIndex)
	{
		if (ScopeStack[ScopeIndex])
		{
			Ops.Emplace(ScopeOpType::Pop(ScopeStack[ScopeIndex]));
		}
	}

	return TRDGScopeOpArray<ScopeOpType>(Ops, OffsetIndex, Ops.Num() - OffsetIndex);
}

// Lower overhead non-variadic version of constructor with arbitrary integer first argument to avoid overload resolution ambiguity.
// Avoids dynamic allocation of the formatted string and other overhead.
inline FRDGEventName::FRDGEventName(int32 NonVariadic, const TCHAR* InEventName)
#if RDG_EVENTS == RDG_EVENTS_STRING_REF || RDG_EVENTS == RDG_EVENTS_STRING_COPY
	: EventFormat(InEventName)
#endif
{
	check(InEventName != nullptr);
}

#if RDG_EVENTS != RDG_EVENTS_STRING_COPY
inline FRDGEventName::FRDGEventName(const TCHAR* InEventFormat, ...)
#if RDG_EVENTS == RDG_EVENTS_STRING_REF
	: EventFormat(InEventFormat)
#endif
{
	check(InEventFormat != nullptr);
}
#endif

inline FRDGEventName::FRDGEventName(const FRDGEventName& Other)
{
	*this = Other;
}

inline FRDGEventName::FRDGEventName(FRDGEventName&& Other)
{
	*this = Forward<FRDGEventName>(Other);
}

inline FRDGEventName& FRDGEventName::operator=(const FRDGEventName& Other)
{
#if RDG_EVENTS == RDG_EVENTS_STRING_REF
	EventFormat = Other.EventFormat;
#elif RDG_EVENTS == RDG_EVENTS_STRING_COPY
	EventFormat = Other.EventFormat;
	FormattedEventName = Other.FormattedEventName;
#endif
	return *this;
}

inline FRDGEventName& FRDGEventName::operator=(FRDGEventName&& Other)
{
#if RDG_EVENTS == RDG_EVENTS_STRING_REF
	EventFormat = Other.EventFormat;
	Other.EventFormat = TEXT("");
#elif RDG_EVENTS == RDG_EVENTS_STRING_COPY
	EventFormat = Other.EventFormat;
	Other.EventFormat = TEXT("");
	FormattedEventName = MoveTemp(Other.FormattedEventName);
#endif
	return *this;
}

inline const TCHAR* FRDGEventName::GetTCHAR() const
{
#if RDG_EVENTS == RDG_EVENTS_STRING_REF || RDG_EVENTS == RDG_EVENTS_STRING_COPY
	#if RDG_EVENTS == RDG_EVENTS_STRING_COPY
		if (!FormattedEventName.IsEmpty())
		{
			return *FormattedEventName;
		}
	#endif

	// The event has not been formated, at least return the event format to have
	// error messages that give some clue when GetEmitRDGEvents() == false.
	return EventFormat;
#else
	// Render graph draw events have been completely compiled for CPU performance reasons.
	return TEXT("[Compiled Out]");
#endif
}

inline const FRDGGPUScopeStacks& FRDGGPUScopeStacksByPipeline::GetScopeStacks(ERHIPipeline Pipeline) const
{
	switch (Pipeline)
	{
	case ERHIPipeline::Graphics:
		return Graphics;
	case ERHIPipeline::AsyncCompute:
		return AsyncCompute;
	default:
		checkNoEntry();
		return Graphics;
	}
}

inline FRDGGPUScopeStacks& FRDGGPUScopeStacksByPipeline::GetScopeStacks(ERHIPipeline Pipeline)
{
	switch (Pipeline)
	{
	case ERHIPipeline::Graphics:
		return Graphics;
	case ERHIPipeline::AsyncCompute:
		return AsyncCompute;
	default:
		checkNoEntry();
		return Graphics;
	}
}
