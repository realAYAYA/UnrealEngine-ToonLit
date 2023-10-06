// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGGraphParametersHelpers.generated.h"

class UPCGGraphInstance;

/**
* Helpers to dynamically set parameters on graph instances.
*/
UCLASS()
class PCG_API UPCGGraphParametersHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static bool IsOverridden(UPCGGraphInstance* GraphInstance, const FName Name);

	////////////
	// Getters
	////////////

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static float GetFloatParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static double GetDoubleParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static bool GetBoolParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static uint8 GetByteParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static int32 GetInt32Parameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static int64 GetInt64Parameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FName GetNameParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FString GetStringParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static TSoftObjectPtr<UObject> GetSoftObjectParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static TSoftClassPtr<UObject> GetSoftClassParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FVector GetVectorParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FRotator GetRotatorParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FTransform GetTransformParameter(UPCGGraphInstance* GraphInstance, const FName Name);

	////////////
	// Setters
	////////////

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetFloatParameter(UPCGGraphInstance* GraphInstance, const FName Name, const float Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetDoubleParameter(UPCGGraphInstance* GraphInstance, const FName Name, const double Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetBoolParameter(UPCGGraphInstance* GraphInstance, const FName Name, const bool bValue);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetByteParameter(UPCGGraphInstance* GraphInstance, const FName Name, const uint8 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetInt32Parameter(UPCGGraphInstance* GraphInstance, const FName Name, const int32 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetInt64Parameter(UPCGGraphInstance* GraphInstance, const FName Name, const int64 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetNameParameter(UPCGGraphInstance* GraphInstance, const FName Name, const FName Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetStringParameter(UPCGGraphInstance* GraphInstance, const FName Name, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetEnumParameter(UPCGGraphInstance* GraphInstance, const FName Name, const UEnum* Enum, const uint8 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetSoftObjectParameter(UPCGGraphInstance* GraphInstance, const FName Name, UPARAM(ref) const TSoftObjectPtr<UObject>& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetSoftClassParameter(UPCGGraphInstance* GraphInstance, const FName Name, UPARAM(ref) const TSoftClassPtr<UObject>& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetVectorParameter(UPCGGraphInstance* GraphInstance, const FName Name, UPARAM(ref) const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetRotatorParameter(UPCGGraphInstance* GraphInstance, const FName Name, UPARAM(ref) const FRotator& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetTransformParameter(UPCGGraphInstance* GraphInstance, const FName Name, UPARAM(ref) const FTransform& Value);
};
