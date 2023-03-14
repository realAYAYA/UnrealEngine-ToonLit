// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TraceUtilLibrary.generated.h"

UCLASS()
class TRACEUTILITIES_API UTraceUtilLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
    public:
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool StartTraceToFile(const FString& FileName, const TArray<FString>& Channels);
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool StartTraceSendTo(const FString& Target, const TArray<FString>& Channels);
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool StopTracing();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool PauseTracing();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool ResumeTracing();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool IsTracing();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool ToggleChannel(const FString& ChannelName, bool enabled);
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static bool IsChannelEnabled(const FString& ChannelName);
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static TArray<FString> GetEnabledChannels();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static TArray<FString> GetAllChannels();
    
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static void TraceBookmark(const FString& Name);

    /**
     * Traces a "RegionStart:Name" bookmark, where Name is defined through the Name parameter.
     */
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static void TraceMarkRegionStart(const FString& Name);
 
	/**
	 * Traces a "RegionEnd:Name" bookmark, where Name is defined through the Name parameter.
	 */   
    UFUNCTION(BlueprintCallable, Category = "Perf | Insights Trace")
    static void TraceMarkRegionEnd(const FString& Name);
};