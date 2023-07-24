// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "RigVMDrawInstruction.generated.h"

class FMaterialRenderProxy;

UENUM()
namespace ERigVMDrawSettings
{
	enum Primitive : int
	{
		Points,
		Lines,
		LineStrip, 
		DynamicMesh
	};
}

USTRUCT()
struct RIGVM_API FRigVMDrawInstruction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	TEnumAsByte<ERigVMDrawSettings::Primitive> PrimitiveType;

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	TArray<FVector> Positions;

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	float Thickness;

	UPROPERTY(EditAnywhere, Category = "DrawInstruction")
	FTransform Transform;

	// This is to draw cone, they're not UPROPERTY
	TArray<FDynamicMeshVertex> MeshVerts;
	TArray<uint32> MeshIndices;
	FMaterialRenderProxy* MaterialRenderProxy = nullptr;

	FRigVMDrawInstruction()
		: Name(NAME_None)
		, PrimitiveType(ERigVMDrawSettings::Points)
		, Color(FLinearColor::Red)
		, Thickness(0.f)
		, Transform(FTransform::Identity)
	{}

	FRigVMDrawInstruction(ERigVMDrawSettings::Primitive InPrimitiveType, const FLinearColor& InColor, float InThickness = 0.f, FTransform InTransform = FTransform::Identity)
		: Name(NAME_None)
		, PrimitiveType(InPrimitiveType)
		, Color(InColor)
		, Thickness(InThickness)
		, Transform(InTransform)
	{}

	bool IsValid() const
	{
		// if primitive type is dynamicmesh, we expect these data to be there. 
		// otherwise, we can't draw
		if (PrimitiveType == ERigVMDrawSettings::DynamicMesh)
		{
			return MeshVerts.Num() != 0 && MeshIndices.Num() != 0 && MaterialRenderProxy != nullptr;
		}

		return Positions.Num() != 0;
	}
};
