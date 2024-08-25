// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuT/NodeLayout.h"
#include "CustomizableObjectLayout.generated.h"


UENUM()
enum class ECustomizableObjectTextureLayoutPackingStrategy : uint8
{
	// The layout increases its size to fit all the blocks.
	Resizable = 0 UMETA(DisplayName = "Resizable Layout"),
	// The layout resizes the blocks to keep its size.
	Fixed = 1 UMETA(DisplayName = "Fixed Layout"),
	// The layout is not modified and blocks are ignored. Extend material nodes just add their layouts on top of the base one.
	Overlay = 2 UMETA(DisplayName = "Overlay Layout")
};

mu::EPackStrategy ConvertLayoutStrategy(const ECustomizableObjectTextureLayoutPackingStrategy LayoutPackStrategy);

// Fixed Layout reduction methods
UENUM()
enum class ECustomizableObjectLayoutBlockReductionMethod : uint8
{
	// Layout blocks will be reduced by halves
	Halve = 0 UMETA(DisplayName = "Reduce by Half"),
	// LAyout blocks will be reduced by a grid unit
	Unitary = 1 UMETA(DisplayName = "Reduce by Unit")
};

USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectLayoutBlock
{
	GENERATED_USTRUCT_BODY()

	FCustomizableObjectLayoutBlock(FIntPoint InMin = FIntPoint(0, 0), FIntPoint InMax = FIntPoint(1, 1))
	{
		Min = InMin;
		Max = InMax;
		Priority = 0;
		Id = FGuid::NewGuid();
		bReduceBothAxes = false;
		bReduceByTwo = false;
	}

	/** Top left coordinate. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FIntPoint Min;

	/** Bottom right coordinate. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FIntPoint Max;

	/** Priority to be reduced. Only functional in fixed layouts. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	uint32 Priority;

	/** Unique unchangeable id used to reference this block from other nodes. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid Id;

	/** Block will be reduced on both sizes at the same time on each reduction. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReduceBothAxes = false;

	/** Block will be reduced by two in an Unitary Layout reduction. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReduceByTwo = false;
};

UCLASS()
class UCustomizableObjectLayout : public UObject
{
	GENERATED_BODY()

public:

	UCustomizableObjectLayout();

	// Sets the layout parameters
	void SetLayout(UObject* InMesh, int32 LODIndex, int32 MatIndex, int32 UVIndex);
	void SetPackingStrategy(ECustomizableObjectTextureLayoutPackingStrategy Strategy);
	void SetGridSize(FIntPoint Size);
	void SetMaxGridSize(FIntPoint Size);
	void SetLayoutName(FString Name);
	void SetIgnoreVertexLayoutWarnings(bool bValue);
	void SetIgnoreWarningsLOD(int32 LODValue);
	void SetBlockReductionMethod(ECustomizableObjectLayoutBlockReductionMethod Method);


	int32 GetLOD() const { return LOD; }
	int32 GetMaterial() const { return Material; }
	int32 GetUVChannel() const { return UVChannel; }
	FString GetLayoutName() const { return LayoutName; }
	UObject* GetMesh() const { return Mesh; }
	FIntPoint GetGridSize() const { return GridSize; }
	FIntPoint GetMaxGridSize() const { return MaxGridSize; }
	ECustomizableObjectTextureLayoutPackingStrategy GetPackingStrategy() const { return PackingStrategy; }
	bool GetIgnoreVertexLayoutWarnings() const { return bIgnoreUnassignedVertexWarning; };
	int32 GetFirstLODToIgnoreWarnings() { return FirstLODToIgnore; };
	ECustomizableObjectLayoutBlockReductionMethod GetBlockReductionMethod()const { return BlockReductionMethod; }

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

	/* If true, vertex warning messages will be ignored */
	UPROPERTY()
	bool bIgnoreUnassignedVertexWarning = false;

	/* First LOD from which unassigned vertices warning will be ignored */
	UPROPERTY()
	int32 FirstLODToIgnore = 0;

	UPROPERTY()
	ECustomizableObjectLayoutBlockReductionMethod BlockReductionMethod = ECustomizableObjectLayoutBlockReductionMethod::Halve;

};
