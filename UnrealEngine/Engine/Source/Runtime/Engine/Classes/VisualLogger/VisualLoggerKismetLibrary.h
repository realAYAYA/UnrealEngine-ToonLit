// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisualLoggerKismetLibrary.generated.h"

UCLASS(MinimalAPI, meta=(ScriptName="VisualLoggerLibrary"))
class UVisualLoggerKismetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (DisplayName = "Enable VisLog Recording", CallableWithoutWorldContext, DevelopmentOnly))
	static void EnableRecording(bool bEnabled);

	/** Makes SourceOwner log to DestinationOwner's vislog*/
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (CallableWithoutWorldContext, DevelopmentOnly))
	static void RedirectVislog(UObject* SourceOwner, UObject* DestinationOwner);

	/** Logs simple text string with Visual Logger - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (DisplayName = "VisLog Text", AdvancedDisplay = "WorldContextObject, bAddToMessageLog", CallableWithoutWorldContext, DevelopmentOnly, DefaultToSelf = "WorldContextObject"))
	static void LogText(UObject* WorldContextObject, FString Text, FName LogCategory = TEXT("VisLogBP"), bool bAddToMessageLog = false);

	/** Logs location as sphere with given radius - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (DisplayName = "VisLog Location", AdvancedDisplay = "WorldContextObject, bAddToMessageLog", CallableWithoutWorldContext, DevelopmentOnly, DefaultToSelf = "WorldContextObject"))
	static void LogLocation(UObject* WorldContextObject, FVector Location, FString Text, FLinearColor ObjectColor = FLinearColor::Blue, float Radius = 10, FName LogCategory = TEXT("VisLogBP"), bool bAddToMessageLog = false);

	/** Logs box shape - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (DisplayName = "VisLog Box Shape", AdvancedDisplay = "WorldContextObject, bAddToMessageLog", CallableWithoutWorldContext, DevelopmentOnly, DefaultToSelf = "WorldContextObject"))
	static void LogBox(UObject* WorldContextObject, FBox BoxShape, FString Text, FLinearColor ObjectColor = FLinearColor::Blue, FName LogCategory = TEXT("VisLogBP"), bool bAddToMessageLog = false);

	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (DisplayName = "VisLog Segment", AdvancedDisplay = "WorldContextObject, bAddToMessageLog, Thickness", CallableWithoutWorldContext, DevelopmentOnly, DefaultToSelf = "WorldContextObject"))
	static void LogSegment(UObject* WorldContextObject, const FVector SegmentStart, const FVector SegmentEnd, FString Text, FLinearColor ObjectColor = FLinearColor::Blue, const float Thickness = 0.f, FName CategoryName = TEXT("VisLogBP"), bool bAddToMessageLog = false);
};
