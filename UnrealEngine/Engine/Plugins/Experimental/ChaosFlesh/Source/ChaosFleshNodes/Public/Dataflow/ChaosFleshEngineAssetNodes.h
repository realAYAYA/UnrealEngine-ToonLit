// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Chaos/Math/Poisson.h"
#include "ChaosFlesh/TetrahedralCollection.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosLog.h"

#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"
#include "Field/FieldSystemTypes.h"

#include "ChaosFleshEngineAssetNodes.generated.h"

USTRUCT(meta = (DataflowFlesh))
struct FGetFleshAssetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetFleshAssetDataflowNode, "GetFleshAsset", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (DisplayName = "FleshAsset"))
	TObjectPtr<const UFleshAsset> FleshAsset = nullptr;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Output;

	FGetFleshAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Output);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


USTRUCT(meta = (DataflowFlesh, DataflowTerminal))
struct FFleshAssetTerminalDataflowNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFleshAssetTerminalDataflowNode, "FleshAssetTerminal", "Terminal", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;


	FFleshAssetTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowTerminalNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


USTRUCT(meta = (DataflowFlesh))
struct FSetFleshDefaultPropertiesNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetFleshDefaultPropertiesNode, "SetFleshDefaultProperties", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	float Density = 1.f;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	float VertexStiffness = 1e6;

	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float VertexDamping = 0.f;

	/*Sets incompressibility on vertex basis. 0.6 is default behavior. 
	1 means total incompressibility. 0.00001 means almost no incompressibility*/
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.00001", ClampMax = "1.0", UIMin = "0.00001", UIMax = "1.0"))
	float VertexIncompressibility = 0.6f;

	/*Sets inflation on vertex basis. 0.5 means no inflation/deflation.
	1 means total inflation. 0 means the material is deflated.*/
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float VertexInflation = 0.5f;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	FSetFleshDefaultPropertiesNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
* Computes a muscle fiber direction per tetrahedron from a GeometryCollection containing tetrahedra, 
* vertices, and origin & insertion vertex fields.  Fiber directions should smoothly follow the geometry
* oriented from the origin vertices pointing to the insertion vertices.
*/
USTRUCT(meta = (DataflowFlesh))
struct FComputeFiberFieldNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FComputeFiberFieldNode, "ComputeFiberField", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	//typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowInput, DisplayName = "OriginVertexIndices"))
	TArray<int32> OriginIndices;

	UPROPERTY(meta = (DataflowInput, DisplayName = "InsertionVertexIndices"))
	TArray<int32> InsertionIndices;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString OriginInsertionGroupName = FString();

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString OriginVertexFieldName = FString("Origin");

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString InsertionVertexFieldName = FString("Insertion");

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	int32 MaxIterations = 100;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	float Tolerance = 1.0e-7;

	FComputeFiberFieldNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&OriginIndices);
		RegisterInputConnection(&InsertionIndices);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

	TArray<int32> GetNonZeroIndices(const TArray<uint8>& Map) const;

	TArray<FVector3f> ComputeFiberField(
		const TManagedArray<FIntVector4>& Elements,
		const TManagedArray<FVector3f>& Vertex,
		const TManagedArray<TArray<int32>>& IncidentElements,
		const TManagedArray<TArray<int32>>& IncidentElementsLocalIndex,
		const TArray<int32>& Origin,
		const TArray<int32>& Insertion) const;
};

/**
* Visualizes a muscle fiber direction per tetrahedron from a GeometryCollection containing tetrahedra.
*/
USTRUCT(meta = (DataflowFlesh))
struct FVisualizeFiberFieldNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVisualizeFiberFieldNode, "VisualizeFiberField", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FFieldCollection::StaticType(), "VectorField")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	float VectorScale = 1.0;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "VectorField"))
	FFieldCollection VectorField;
	
	FVisualizeFiberFieldNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&VectorField);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowFlesh))
struct FComputeIslandsNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FComputeIslandsNode, "ComputeIslands", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	FComputeIslandsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

//Given two sets of vertex indices, generate two sets of vertex indices for origins and insertions that are within X distance away.
USTRUCT(meta = (DataflowFlesh))
struct FGenerateOriginInsertionNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGenerateOriginInsertionNode, "GenerateOriginInsertion", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	//typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowInput, DisplayName = "OriginIndicesIn"))
	TArray<int32> OriginIndicesIn;

	UPROPERTY(meta = (DataflowInput, DisplayName = "InsertionIndicesIn"))
	TArray<int32> InsertionIndicesIn;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "OriginIndicesOut"))
	TArray<int32> OriginIndicesOut;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "InsertionIndicesOut"))
	TArray<int32> InsertionIndicesOut;
	
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	float Radius = float(1);

	FGenerateOriginInsertionNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&OriginIndicesIn);
		RegisterInputConnection(&InsertionIndicesIn);
		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&OriginIndicesOut);
		RegisterOutputConnection(&InsertionIndicesOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowFlesh))
struct FIsolateComponentNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FIsolateComponentNode, "IsolateComponent", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	//typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bDeleteHiddenFaces = false;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString TargetComponentIndex = "";

	FIsolateComponentNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowFlesh))
struct FGetSurfaceIndicesNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetSurfaceIndicesNode, "GetSurfaceIndices", "Flesh", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowInput, DisplayName = "GeometryGroupGuidsIn"))
	TArray<FString> GeometryGroupGuidsIn;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "SurfaceIndicesOut"))
	TArray<int32> SurfaceIndicesOut;

	FGetSurfaceIndicesNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&GeometryGroupGuidsIn);
		RegisterOutputConnection(&SurfaceIndicesOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

namespace Dataflow
{
	void RegisterChaosFleshEngineAssetNodes();
}

