// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectLayout.generated.h"

UENUM()
enum class ECustomizableObjectTextureLayoutPackingStrategy : uint8
{
	// The layout increases its size to fit all the blocks.
	Resizable = 0 UMETA(DisplayName = "Resizable Layout"),
	// The layout resizes the blocks to keep its size.
	Fixed = 1 UMETA(DisplayName = "Fixed Layout")
};

USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectLayoutBlock
{
	GENERATED_USTRUCT_BODY()

		FCustomizableObjectLayoutBlock()
	{
		Min = FIntPoint(0, 0);
		Max = FIntPoint(1, 1);
		Priority = 0;
	}

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
		FIntPoint Min;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
		FIntPoint Max;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
		uint32 Priority;

	//! Unique unchangeable id used to reference this block from other nodes.
	UPROPERTY()
		FGuid Id;
};

UCLASS()
class UCustomizableObjectLayout : public UObject
{
	GENERATED_BODY()

public:

	UCustomizableObjectLayout();

	void SetLayout(UObject* InMesh, int32 LODIndex, int32 MatIndex, int32 UVIndex);
	void SetPackingStrategy(ECustomizableObjectTextureLayoutPackingStrategy Strategy);
	void SetGridSize(FIntPoint Size);
	void SetMaxGridSize(FIntPoint Size);
	void SetLayoutName(FString Name);

	int32 GetLOD() const { return LOD; }
	int32 GetMaterial() const { return Material; }
	int32 GetUVChannel() const { return UVChannel; }
	FString GetLayoutName() const { return LayoutName; }
	UObject* GetMesh() const { return Mesh; }
	FIntPoint GetGridSize() const { return GridSize; }
	FIntPoint GetMaxGridSize() const { return MaxGridSize; }
	ECustomizableObjectTextureLayoutPackingStrategy GetPackingStrategy() const { return PackingStrategy; }

	void GetUVChannel(TArray<FVector2f>& UVs, int32 UVChannelIndex = 0) const;

	// Get a block index in the array from its id. Return -1 if not found.
	int32 FindBlock(const FGuid& InId) const;

	void GenerateBlocksFromUVs();

	UPROPERTY()
	TArray<FCustomizableObjectLayoutBlock> Blocks;

	TArray< TArray<FVector2f> > UnassignedUVs;

private:

	UPROPERTY()
	TObjectPtr<UObject> Mesh = nullptr;

	UPROPERTY()
	int32 LOD;

	UPROPERTY()
	int32 Material;

	UPROPERTY()
	int32 UVChannel;

	UPROPERTY()
	FIntPoint GridSize;

	/** Used with the fixed layout strategy. */
	UPROPERTY()
	FIntPoint MaxGridSize;

	UPROPERTY()
	ECustomizableObjectTextureLayoutPackingStrategy PackingStrategy = ECustomizableObjectTextureLayoutPackingStrategy::Resizable;

	UPROPERTY()
	FString LayoutName;

};
