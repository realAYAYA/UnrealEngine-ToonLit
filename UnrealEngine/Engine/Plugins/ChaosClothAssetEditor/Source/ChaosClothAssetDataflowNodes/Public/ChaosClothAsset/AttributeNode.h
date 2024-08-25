// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/ConnectableValue.h"
#include "AttributeNode.generated.h"

/**
 * The managed array collection group used for the attribute creation.
 * This separate structure is required to allow for customization of the UI.
 */
USTRUCT()
struct FChaosClothAssetNodeAttributeGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Attribute Group")
	FString Name;
};

UENUM()
enum class EChaosClothAssetNodeAttributeType : uint8
{
	Integer,
	Float,
	Vector
};

/** Create a new attribute for the specified group. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetAttributeNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetAttributeNode, "Attribute", "Cloth", "Cloth Attribute")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name of the attribute to create. */
	UPROPERTY(EditAnywhere, Category = "Attribute", Meta = (DataflowOutput))
	FString Name;

	/** The attribute group. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	FChaosClothAssetNodeAttributeGroup Group;

	/** The attribute type. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	EChaosClothAssetNodeAttributeType Type = EChaosClothAssetNodeAttributeType::Integer;

	/** Default integer value. */
	UPROPERTY(EditAnywhere, Category = "Attribute", Meta = (EditCondition = "Type == EChaosClothAssetNodeAttributeType::Integer", EditConditionHides))
	int32 IntValue = 0;

	/** Default float value. */
	UPROPERTY(EditAnywhere, Category = "Attribute", Meta = (EditCondition = "Type == EChaosClothAssetNodeAttributeType::Float", EditConditionHides))
	float FloatValue = 0.f;

	/** Default vector value. */
	UPROPERTY(EditAnywhere, Category = "Attribute", Meta = (EditCondition = "Type == EChaosClothAssetNodeAttributeType::Vector", EditConditionHides))
	FVector3f VectorValue = FVector3f::ZeroVector;

	FChaosClothAssetAttributeNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	/** Return a cached array of all the groups used by the input collection during at the time of the latest evaluation. */
	const TArray<FName>& GetCachedCollectionGroupNames() const { return CachedCollectionGroupNames; }

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void OnSelected(Dataflow::FContext& Context) override;
	virtual void OnDeselected() override;
	virtual bool IsExperimental() override { return true; }
	//~ End FDataflowNode interface

	TArray<FName> CachedCollectionGroupNames;
};
