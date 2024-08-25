// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "Graph/GraphHandle.h"

#include "GraphElement.generated.h"

class UGraph;

UENUM()
enum class EGraphElementType
{
	Node,
	Edge,
	Island,
	Unknown
};

UCLASS(abstract)
class GAMEPLAYGRAPH_API UGraphElement : public UObject
{
	GENERATED_BODY()
public:
	explicit UGraphElement(EGraphElementType InElementType);
	EGraphElementType GetElementType() const { return ElementType; }

	friend class UGraph;
protected:
	UGraphElement();

	void SetUniqueIndex(FGraphUniqueIndex InUniqueIndex) { UniqueIndex = InUniqueIndex; }
	FGraphUniqueIndex GetUniqueIndex() const { return UniqueIndex; }

	void SetParentGraph(TObjectPtr<UGraph> InGraph);
	TObjectPtr<UGraph> GetGraph() const;

	/** Called when we create this element and prior to setting any properties. */
	virtual void OnCreate() {}

private:
	UPROPERTY()
	EGraphElementType ElementType = EGraphElementType::Unknown;

	/** Will match the UniqueIndex in the UGraphHandle that references this element. */
	UPROPERTY()
	FGraphUniqueIndex UniqueIndex = FGraphUniqueIndex();

	UPROPERTY()
	TWeakObjectPtr<UGraph> ParentGraph;
};