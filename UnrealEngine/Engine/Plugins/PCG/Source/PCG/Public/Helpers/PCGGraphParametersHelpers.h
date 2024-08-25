// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGGraphParametersHelpers.generated.h"

class UPCGGraphInterface;

/**
* Blueprint Library to get or set graph parameters on graphs and graph instances
*/
UCLASS()
class PCG_API UPCGGraphParametersHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static bool IsOverridden(const UPCGGraphInterface* GraphInterface, const FName Name);

	////////////
	// Getters
	////////////

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static float GetFloatParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static double GetDoubleParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static bool GetBoolParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static uint8 GetByteParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static int32 GetInt32Parameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static int64 GetInt64Parameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FName GetNameParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FString GetStringParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static uint8 GetEnumParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FSoftObjectPath GetSoftObjectPathParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static TSoftObjectPtr<UObject> GetSoftObjectParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static TSoftClassPtr<UObject> GetSoftClassParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static UObject* GetObjectParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static UClass* GetClassParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FVector GetVectorParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FRotator GetRotatorParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FTransform GetTransformParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FVector4 GetVector4Parameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FVector2D GetVector2DParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static FQuat GetQuaternionParameter(const UPCGGraphInterface* GraphInterface, const FName Name);

	////////////
	// Setters
	////////////

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetFloatParameter(UPCGGraphInterface* GraphInterface, const FName Name, const float Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetDoubleParameter(UPCGGraphInterface* GraphInterface, const FName Name, const double Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetBoolParameter(UPCGGraphInterface* GraphInterface, const FName Name, const bool bValue);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetByteParameter(UPCGGraphInterface* GraphInterface, const FName Name, const uint8 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetInt32Parameter(UPCGGraphInterface* GraphInterface, const FName Name, const int32 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetInt64Parameter(UPCGGraphInterface* GraphInterface, const FName Name, const int64 Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetNameParameter(UPCGGraphInterface* GraphInterface, const FName Name, const FName Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetStringParameter(UPCGGraphInterface* GraphInterface, const FName Name, const FString Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetEnumParameter(UPCGGraphInterface* GraphInterface, const FName Name, const uint8 Value, const UEnum* Enum = nullptr);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetSoftObjectPathParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FSoftObjectPath& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetSoftObjectParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const TSoftObjectPtr<UObject>& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetSoftClassParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const TSoftClassPtr<UObject>& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetObjectParameter(UPCGGraphInterface* GraphInterface, const FName Name, UObject* Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetClassParameter(UPCGGraphInterface* GraphInterface, const FName Name, UClass* Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetVectorParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetRotatorParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FRotator& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetTransformParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FTransform& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetVector4Parameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FVector4& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetVector2DParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FVector2D& Value);

	UFUNCTION(BlueprintCallable, Category = "PCG|Graph Parameters")
	static void SetQuaternionParameter(UPCGGraphInterface* GraphInterface, const FName Name, UPARAM(ref) const FQuat& Value);
};
