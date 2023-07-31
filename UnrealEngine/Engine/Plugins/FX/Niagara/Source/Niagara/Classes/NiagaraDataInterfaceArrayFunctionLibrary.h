// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.generated.h"

class UNiagaraComponent;

/**
* C++ and Blueprint library for accessing array types
*/
UCLASS()
class NIAGARA_API UNiagaraDataInterfaceArrayFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Sets Niagara Array Float Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta=(DisplayName = "Niagara Set Float Array"))
	static void SetNiagaraArrayFloat(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<float>& ArrayData);
	/** Sets Niagara Array FVector2D Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector 2D Array"))
	static void SetNiagaraArrayVector2D(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FVector2D>& ArrayData);
	/** Sets Niagara Array FVector Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector Array"))
	static void SetNiagaraArrayVector(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FVector>& ArrayData);
	/** Sets Niagara Array FVector Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Position Array"))
	static void SetNiagaraArrayPosition(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FVector>& ArrayData);
	/** Sets Niagara Array FVector4 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector 4 Array"))
	static void SetNiagaraArrayVector4(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FVector4>& ArrayData);
	/** Sets Niagara Array FLinearColor Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Color Array"))
	static void SetNiagaraArrayColor(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FLinearColor>& ArrayData);
	/** Sets Niagara Array FQuat Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Quaternion Array"))
	static void SetNiagaraArrayQuat(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<FQuat>& ArrayData);
	/** Sets Niagara Array Int32 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Int32 Array"))
	static void SetNiagaraArrayInt32(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<int32>& ArrayData);
	/** Sets Niagara Array UInt8 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set UInt8 Array"))
	static void SetNiagaraArrayUInt8(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<int32>& ArrayData);
	/** Sets Niagara Array Bool Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Bool Array"))
	static void SetNiagaraArrayBool(UNiagaraComponent* NiagaraSystem, FName OverrideName, const TArray<bool>& ArrayData);

	/** Gets a copy of Niagara Float Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Float Array"))
	static TArray<float> GetNiagaraArrayFloat(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara FVector2D Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector 2D Array"))
	static TArray<FVector2D> GetNiagaraArrayVector2D(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara FVector Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector Array"))
	static TArray<FVector> GetNiagaraArrayVector(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara Position Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Position Array"))
	static TArray<FVector> GetNiagaraArrayPosition(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara FVector4 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector 4 Array"))
	static TArray<FVector4> GetNiagaraArrayVector4(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara FLinearColor Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Color Array"))
	static TArray<FLinearColor> GetNiagaraArrayColor(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara FQuat Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Quaternion Array"))
	static TArray<FQuat> GetNiagaraArrayQuat(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara Int32 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Int32 Array"))
	static TArray<int32> GetNiagaraArrayInt32(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara UInt8 Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get UInt8 Array"))
	static TArray<int32> GetNiagaraArrayUInt8(UNiagaraComponent* NiagaraSystem, FName OverrideName);
	/** Gets a copy of Niagara Bool Data. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Bool Array"))
	static TArray<bool> GetNiagaraArrayBool(UNiagaraComponent* NiagaraSystem, FName OverrideName);

	/** Sets a single value within a Niagara Array Float. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Float Array Value"))
	static void SetNiagaraArrayFloatValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, float Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array FVector2D. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector 2D Array Value"))
	static void SetNiagaraArrayVector2DValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FVector2D& Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array FVector. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector Array Value"))
	static void SetNiagaraArrayVectorValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FVector& Value, bool bSizeToFit);
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Position Array Value"))
	static void SetNiagaraArrayPositionValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FVector& Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array FVector4. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Vector 4 Array Value"))
	static void SetNiagaraArrayVector4Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FVector4& Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array FLinearColor. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Color Array Value"))
	static void SetNiagaraArrayColorValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FLinearColor& Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array FQuat. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Quaternion Array Value"))
	static void SetNiagaraArrayQuatValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const FQuat& Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array Int32. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Int32 Array Value"))
	static void SetNiagaraArrayInt32Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, int32 Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array UInt8. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set UInt8 Array Value"))
	static void SetNiagaraArrayUInt8Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, int32 Value, bool bSizeToFit);
	/** Sets a single value within a Niagara Array Bool. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Set Bool Array Value"))
	static void SetNiagaraArrayBoolValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index, const bool& Value, bool bSizeToFit);

	/** Gets a single value within a Niagara Array Float. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Float Array Value"))
	static float GetNiagaraArrayFloatValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array FVector2D. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector 2D Array Value"))
	static FVector2D GetNiagaraArrayVector2DValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array FVector. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector Array Value"))
	static FVector GetNiagaraArrayVectorValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array Position. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Position Array Value"))
	static FVector GetNiagaraArrayPositionValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array FVector4. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Vector 4 Array Value"))
	static FVector4 GetNiagaraArrayVector4Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array FLinearColor. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Color Array Value"))
	static FLinearColor GetNiagaraArrayColorValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array FQuat. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Quaternion Array Value"))
	static FQuat GetNiagaraArrayQuatValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array Int32. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Int32 Array Value"))
	static int32 GetNiagaraArrayInt32Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array UInt8. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get UInt8 Array Value"))
	static int32 GetNiagaraArrayUInt8Value(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);
	/** Gets a single value within a Niagara Array Bool. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Niagara Get Bool Array Value"))
	static bool GetNiagaraArrayBoolValue(UNiagaraComponent* NiagaraSystem, FName OverrideName, int Index);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// None BP compatable set functions
	static void SetNiagaraArrayVector2D(UNiagaraComponent* NiagaraSystem, FName OverrideName, TConstArrayView<FVector2f> ArrayData);
	static void SetNiagaraArrayVector(UNiagaraComponent* NiagaraSystem, FName OverrideName, TConstArrayView<FVector3f> ArrayData);
	static void SetNiagaraArrayVector4(UNiagaraComponent* NiagaraSystem, FName OverrideName, TConstArrayView<FVector4f> ArrayData);
	static void SetNiagaraArrayQuat(UNiagaraComponent* NiagaraSystem, FName OverrideName, TConstArrayView<FQuat4f> ArrayData);
	static void SetNiagaraArrayUInt8(UNiagaraComponent* NiagaraSystem, FName OverrideName, TConstArrayView<uint8> ArrayData);
};
