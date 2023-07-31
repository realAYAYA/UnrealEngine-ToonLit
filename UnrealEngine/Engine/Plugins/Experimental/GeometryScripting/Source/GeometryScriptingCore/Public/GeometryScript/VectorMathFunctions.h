// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "VectorMathFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_VectorMath"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_VectorMathFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Compute the length/magnitude of each vector in VectorListA and return in new ScalarList.
	 * Note that SquaredLength can be computed using VectorDot(A,A).
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	VectorLength(FGeometryScriptVectorList VectorList);

	/**
	 * Compute the dot-product between each pair of vectors in VectorListA and VectorListB and return in new ScalarList
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	VectorDot(FGeometryScriptVectorList VectorListA, FGeometryScriptVectorList VectorListB);

	/**
	* Compute the cross-product between each pair of vectors in VectorListA and VectorListB and return in new VectorList
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptVectorList 
	VectorCross(FGeometryScriptVectorList VectorListA, FGeometryScriptVectorList VectorListB);

	/**
	 * Normalize each vector in VectorList, and store in VectorList. 
	 * If a vector is degenerate, set the normal to the SetOnFailure vector.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	VectorNormalizeInPlace(UPARAM(ref) FGeometryScriptVectorList& VectorList, FVector SetOnFailure = FVector::ZeroVector);

	/**
	 * Compute (ConstantA * A) + (ConstantB * B) for each pair of vectors in VectorListA and VectorListB and return in new VectorList.
	 * By default (constants = 1) this just adds the two vectors. Set ConstantB = -1 to subtract B from A. 
	 * Can also be used to Linear Interpolate, by setting ConstantB = (1-ConstantA)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptVectorList 
	VectorBlend(FGeometryScriptVectorList VectorListA, FGeometryScriptVectorList VectorListB, double ConstantA = 1.0, double ConstantB = 1.0);

	/**
	* Compute (ConstantA * A) + (ConstantB * B) for each pair of vectors in VectorListA and VectorListB, and store in VectorListB
	* By default (constants = 1) this just adds the two vectors. Set ConstantB = -1 to subtract B from A. 
	* Can also be used to Linear Interpolate, by setting ConstantB = (1-ConstantA)
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	VectorBlendInPlace(FGeometryScriptVectorList VectorListA, UPARAM(ref) FGeometryScriptVectorList& VectorListB, double ConstantA = 1.0, double ConstantB = 1.0);

	/**
	 * Compute (ScalarMultiplier * Scalar * Vector) for each scalar/vector pair in the two input lists, and return in a new VectorList.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptVectorList 
	ScalarVectorMultiply(FGeometryScriptScalarList ScalarList, FGeometryScriptVectorList VectorList, double ScalarMultiplier = 1.0);

	/**
	 * Compute (ScalarMultiplier * Scalar * Vector) for each scalar/vector pair in the two input lists, and store in the input VectorList
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	ScalarVectorMultiplyInPlace(FGeometryScriptScalarList ScalarList, UPARAM(ref) FGeometryScriptVectorList& VectorList, double ScalarMultiplier = 1.0);

	/**
	 * Compute (Constant * Vector) for each element in VectorList, and return in a new list
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath")
	static FGeometryScriptVectorList 
	ConstantVectorMultiply(double Constant, FGeometryScriptVectorList VectorList);

	/**
	 * Compute (Constant * Vector) for each element in VectorList, and store in VectorList
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath")
	static void 
	ConstantVectorMultiplyInPlace(double Constant, UPARAM(ref) FGeometryScriptVectorList& VectorList);


	/**
	 * Convert each Vector in VectorList to a Scalar by computing (ConstantX*Vector.X + ConstantY*Vector.Y + ConstantZ*Vector.Z), and return in a new ScalarList.
	 * This can be used to extract the X/Y/Z values from a Vector, or other component-wise math
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	VectorToScalar(FGeometryScriptVectorList VectorList, double ConstantX = 1.0, double ConstantY = 0.0, double ConstantZ = 0.0);



	/**
	 * Compute (Numerator / Scalar) for each element of ScalarList and return in a new ScalarList. 
	 * If Abs(Scalar) < Epsilon, set to SetOnFailure value.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	ScalarInvert(FGeometryScriptScalarList ScalarList, double Numerator = 1.0, double SetOnFailure = 0.0, double Epsilon = 0.0);

	/**
	* Compute (Numerator / Scalar) for each element of ScalarList and store in input ScalarList
	* If Abs(Scalar) < Epsilon, set to SetOnFailure value.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	ScalarInvertInPlace(UPARAM(ref) FGeometryScriptScalarList& ScalarList, double Numerator = 1.0, double SetOnFailure = 0.0, double Epsilon = 0.0);

	/**
	 * Compute (ConstantA * A) + (ConstantB * B) for each pair of values in ScalarListA and ScalarListB and return in new ScalarList.
	 * By default (constants = 1) this just adds the two values. Set ConstantB = -1 to subtract B from A. 
	 * Can also be used to Linear Interpolate, by setting ConstantB = (1-ConstantA)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	ScalarBlend(FGeometryScriptScalarList ScalarListA, FGeometryScriptScalarList ScalarListB, double ConstantA = 1.0, double ConstantB = 1.0);

	/**
	 * Compute (ConstantA * A) + (ConstantB * B) for each pair of values in ScalarListA and ScalarListB and return in ScalarListB.
	 * By default (constants = 1) this just adds the two values. Set ConstantB = -1 to subtract B from A. 
	 * Can also be used to Linear Interpolate, by setting ConstantB = (1-ConstantA)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	ScalarBlendInPlace(FGeometryScriptScalarList ScalarListA, UPARAM(ref) FGeometryScriptScalarList& ScalarListB, double ConstantA = 1.0, double ConstantB = 1.0);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static FGeometryScriptScalarList 
	ScalarMultiply(FGeometryScriptScalarList ScalarListA, FGeometryScriptScalarList ScalarListB, double ConstantMultiplier = 1.0);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath", meta=(ScriptMethod))
	static void 
	ScalarMultiplyInPlace(FGeometryScriptScalarList ScalarListA, UPARAM(ref) FGeometryScriptScalarList& ScalarListB, double ConstantMultiplier = 1.0);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath")
	static FGeometryScriptScalarList 
	ConstantScalarMultiply(double Constant, FGeometryScriptScalarList ScalarList);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Math|VectorMath")
	static void 
	ConstantScalarMultiplyInPlace(double Constant, UPARAM(ref) FGeometryScriptScalarList& ScalarList);

};