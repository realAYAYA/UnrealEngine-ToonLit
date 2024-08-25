// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsTracer.h"
#include "ProfilingDebugging/MiscTrace.h"

UE_DISABLE_OPTIMIZATION_SHIP
 
static void AggregateAttributes(TArray<FAnalyticsEventAttribute>& AggregatedAttibutes, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// Aggregates all attributes
	for (const FAnalyticsEventAttribute& Attribute : Attributes)
	{
		bool AttributeWasFound = false;

		for (FAnalyticsEventAttribute& AggregatedAttribute : AggregatedAttibutes)
		{
			if (Attribute.GetName() == AggregatedAttribute.GetName())
			{
				AggregatedAttribute += Attribute;

				// If we already have this attribute then great no more to do for this attribute
				AttributeWasFound = true;
				break;
			}
		}

		if (AttributeWasFound == false)
		{
			// No matching attribute so append
			AggregatedAttibutes.Add(Attribute);
		}
	}
}

void FAnalyticsSpan::SetProvider(TSharedPtr<IAnalyticsProvider> Provider)
{
	AnalyticsProvider = Provider;
}

void FAnalyticsSpan::SetStackDepth(uint32 Depth)
{
	StackDepth = Depth;
}

double FAnalyticsSpan::GetDuration() const 
{
	return Duration;
}

void FAnalyticsSpan::SetParentSpan(TSharedPtr<IAnalyticsSpan> NewSpanParent)
{
	ParentSpan = NewSpanParent;
}

void FAnalyticsSpan::Start(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	// Create a new Guid for this flow, can we assume it is unique?
	Guid			= FGuid::NewGuid();
	Attributes		= AdditionalAttributes;
	ThreadId		= FPlatformTLS::GetCurrentThreadId();
	StartTime		= FDateTime::UtcNow();
	EndTime			= FDateTime::UtcNow();
	Duration		= 0;	
	IsActive		= true;

	TRACE_BEGIN_REGION(*Name.ToString());
}

bool FAnalyticsSpan::GetIsActive() const
{
	return IsActive;
}

void FAnalyticsSpan::End(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	// Only End the span once
	if (IsActive == false)
	{
		return;
	}

	// Calculate the duration
	EndTime = FDateTime::UtcNow();
	Duration = (EndTime - StartTime).GetTotalSeconds();
		
	TRACE_END_REGION(*Name.ToString());

	// Append the parent attributes and the the additional attributes to the current span attributes, these will get passed down to the child spans
	AddAttributes(AdditionalAttributes);

	TSharedPtr< IAnalyticsSpan> ParentSpanShared = ParentSpan.Pin();

	if (ParentSpanShared.IsValid())
	{
		AddAttributes(ParentSpanShared->GetAttributes());
	}

	const uint32 SpanSchemaVersion = 1;
	const FString SpanEventName = TEXT("Span");

	TArray<FAnalyticsEventAttribute> EventAttributes = Attributes;

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SchemaVersion"), SpanSchemaVersion));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_Name"), Name.ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_GUID"), Guid.ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_ParentName"), ParentSpanShared.IsValid() ? ParentSpanShared->GetName().ToString() : TEXT("")));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_ThreadId"), ThreadId));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_Depth"), StackDepth));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_StartUTC"), StartTime.ToUnixTimestampDecimal()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_EndUTC"), EndTime.ToUnixTimestampDecimal()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_TimeInSec"), Duration));

	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->RecordEvent(SpanEventName, EventAttributes);
	}

	IsActive = false;
}

void FAnalyticsSpan::AddAttributes(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	AggregateAttributes(Attributes, AdditionalAttributes);
}

void FAnalyticsSpan::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	if (AnalyticsProvider.IsValid())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes = Attributes;
		AggregateAttributes(EventAttributes, AdditionalAttributes);
		AnalyticsProvider->RecordEvent(EventName, EventAttributes);
	}
}

const FName& FAnalyticsSpan::GetName() const
{
	return Name;
}

const TArray<FAnalyticsEventAttribute>& FAnalyticsSpan::GetAttributes() const
{
	return Attributes;
}

uint32 FAnalyticsSpan::GetStackDepth() const
{
	return StackDepth;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsSpan::GetParentSpan() const
{
	return ParentSpan.Pin();
}

void FAnalyticsTracer::SetProvider(TSharedPtr<IAnalyticsProvider> InProvider)
{
	AnalyticsProvider = InProvider;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetCurrentSpan() const
{
	return ActiveSpanStack.Num()? ActiveSpanStack.Top() : TSharedPtr<IAnalyticsSpan>();
}

void FAnalyticsTracer::StartSession()
{
	SessionSpan = StartSpan(TEXT("Session"), TSharedPtr<IAnalyticsSpan>());
}

void FAnalyticsTracer::EndSession()
{
	FScopeLock ScopeLock(&CriticalSection);	

	EndSpan(SessionSpan);
	SessionSpan.Reset();

	// Stop any active spans, go from stack bottom first so parent spans will end their children 
	while (ActiveSpanStack.Num())
	{	
		TSharedPtr<IAnalyticsSpan> Span = ActiveSpanStack[0];

		if (Span.IsValid())
		{
			EndSpan(Span);
		}	
		else
		{
			ActiveSpanStack.Remove(Span);
		}
	}

	ActiveSpanStack.Reset();

	AnalyticsProvider.Reset();
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::StartSpan(const FName NewSpanName, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	TSharedPtr<IAnalyticsSpan> NewSpan = MakeShared<FAnalyticsSpan>(NewSpanName);
	NewSpan->SetParentSpan(ParentSpan);

	return StartSpanInternal(NewSpan, AdditionalAttributes) ? NewSpan : TSharedPtr<IAnalyticsSpan>();
}

bool FAnalyticsTracer::StartSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	if (Span.IsValid() && Span->GetIsActive() == false)
	{
		return StartSpanInternal(Span, AdditionalAttributes);
	}

	return false;
}

bool FAnalyticsTracer::StartSpanInternal(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	TSharedPtr<IAnalyticsSpan> LastAdddedActiveSpan = ActiveSpanStack.Num()? ActiveSpanStack.Top(): TSharedPtr<IAnalyticsSpan>();
	Span->SetStackDepth(LastAdddedActiveSpan.IsValid()? LastAdddedActiveSpan->GetStackDepth()+1 : 0);
	Span->SetProvider(AnalyticsProvider);
	Span->Start(AdditionalAttributes);

	// Add span to active spans list
	ActiveSpanStack.Emplace(Span);

	return true;
}

bool FAnalyticsTracer::EndSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	return EndSpanInternal(Span, AdditionalAttributes);
}

bool FAnalyticsTracer::EndSpanInternal(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	if (Span.IsValid())
	{
		Span->End(AdditionalAttributes);

		ActiveSpanStack.Remove(Span);

		// Remove any active children of this span, inherit on the parent's attributes in the child
		bool RemovedActiveChild = false;

		do 
		{
			RemovedActiveChild = false;

			for (int32 i = 0; i < ActiveSpanStack.Num(); ++i)
			{
				TSharedPtr<IAnalyticsSpan> ChildSpan = ActiveSpanStack[i];

				if (ChildSpan.IsValid() && ChildSpan->GetParentSpan() == Span)
				{
					EndSpanInternal(ChildSpan, Span->GetAttributes());
					RemovedActiveChild = true;
					break;
				}
			}

		} while (RemovedActiveChild==true);

		return true;
	}

	return false;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSessionSpan() const
{
	return SessionSpan;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSpanInternal(const FName Name)
{
	for (int32 i = 0; i < ActiveSpanStack.Num(); ++i)
	{
		TSharedPtr<IAnalyticsSpan> Span = ActiveSpanStack[i];
		
		if (Span.IsValid() && Span->GetName() == Name)
		{
			return Span;
		}
	}

	return TSharedPtr<IAnalyticsSpan>();
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSpan(const FName Name)
{
	FScopeLock ScopeLock(&CriticalSection);
	return GetSpanInternal(Name);
}

UE_ENABLE_OPTIMIZATION_SHIP