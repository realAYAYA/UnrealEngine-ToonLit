// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLoggerKismetLibrary.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"
#include "Logging/MessageLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisualLoggerKismetLibrary)

namespace
{
	const FName NAME_Empty;
	const FName NAME_BlueprintVLog = TEXT("VisLogBP");
}

UVisualLoggerKismetLibrary::UVisualLoggerKismetLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVisualLoggerKismetLibrary::RedirectVislog(UObject* SourceOwner, UObject* DestinationOwner)
{
	if (SourceOwner && DestinationOwner)
	{
		REDIRECT_OBJECT_TO_VLOG(SourceOwner, DestinationOwner);
	}
}

void UVisualLoggerKismetLibrary::EnableRecording(bool bEnabled)
{
#if ENABLE_VISUAL_LOG
	FVisualLogger::Get().SetIsRecording(bEnabled);
#endif // ENABLE_VISUAL_LOG
}

void UVisualLoggerKismetLibrary::LogText(UObject* WorldContextObject, FString Text, FName CategoryName, bool bAddToMessageLog)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::CategorizedLogf(WorldContextObject, CategoryName, DefaultVerbosity
		, TEXT("%s"), *Text);
#endif
	if (bAddToMessageLog)
	{
		FMessageLog(CategoryName).Info(FText::FromString(FString::Printf(TEXT("LogText: '%s'"), *Text)));
	}
}

void UVisualLoggerKismetLibrary::LogLocation(UObject* WorldContextObject, FVector Location, FString Text, FLinearColor Color, float Radius, FName CategoryName, bool bAddToMessageLog)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, CategoryName, DefaultVerbosity
		, Location, Radius, Color.ToFColor(true), TEXT("%s"), *Text);
#endif
	if (bAddToMessageLog)
	{
		FMessageLog(CategoryName).Info(FText::FromString(FString::Printf(TEXT("LogLocation: '%s' - Location: (%s)")
			, *Text, *Location.ToString())));
	}
}

void UVisualLoggerKismetLibrary::LogBox(UObject* WorldContextObject, FBox Box, FString Text, FLinearColor ObjectColor, FName CategoryName, bool bAddToMessageLog)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryBoxLogf(WorldContextObject, CategoryName, DefaultVerbosity
		, Box, FMatrix::Identity, ObjectColor.ToFColor(true), TEXT("%s"), *Text);
#endif
	if (bAddToMessageLog)
	{
		FMessageLog(CategoryName).Info(FText::FromString(FString::Printf(TEXT("LogBox: '%s' - BoxMin: (%s) | BoxMax: (%s)")
			, *Text, *Box.Min.ToString(), *Box.Max.ToString())));
	}
}

void UVisualLoggerKismetLibrary::LogSegment(UObject* WorldContextObject, const FVector SegmentStart, const FVector SegmentEnd, FString Text, FLinearColor ObjectColor, const float Thickness, FName CategoryName, bool bAddToMessageLog)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, CategoryName, DefaultVerbosity, SegmentStart, SegmentEnd, ObjectColor.ToFColor(true), (uint16)Thickness, TEXT("%s"), *Text);
#endif
	if (bAddToMessageLog)
	{
		FMessageLog(CategoryName).Info(FText::FromString(FString::Printf(TEXT("Log: '%s' - segment from (%s) to (%s)")
			, *Text, *SegmentStart.ToString(), *SegmentStart.ToString())));
	}
}

