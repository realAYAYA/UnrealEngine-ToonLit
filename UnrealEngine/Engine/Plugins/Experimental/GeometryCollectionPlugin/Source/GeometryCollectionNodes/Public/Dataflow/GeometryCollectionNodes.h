// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollectionNodes.generated.h"


class FGeometryCollection;

USTRUCT()
struct FGetCollectionAssetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetCollectionAssetDataflowNode, "GetCollectionAsset", "GeometryCollection", "")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	mutable FManagedArrayCollection Output;

	FGetCollectionAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Output);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
	Description for this node
*/
USTRUCT()
struct FExampleCollectionEditDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExampleCollectionEditDataflowNode, "ExampleCollectionEdit", "GeometryCollection", "")

public:
	typedef FManagedArrayCollection DataType;

	/** Description for this parameter */
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (EditCondition = "false", EditConditionHides));
	float Scale = 1.0;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", Passthrough = "Collection"))
	FManagedArrayCollection Collection;

	FExampleCollectionEditDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Collection, &Collection);
		RegisterInputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FSetCollectionAssetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetCollectionAssetDataflowNode, "SetCollectionAsset", "GeometryCollection", "")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	FSetCollectionAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FResetGeometryCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FResetGeometryCollectionDataflowNode, "ResetGeometryCollection", "GeometryCollection", "")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	FResetGeometryCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


USTRUCT()
struct FPrintStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FPrintStringDataflowNode, "PrintString", "Development", "")

public:
	UPROPERTY(EditAnywhere, Category = "Print");
	bool PrintToScreen = true;

	UPROPERTY(EditAnywhere, Category = "Print");
	bool PrintToLog = true;

	UPROPERTY(EditAnywhere, Category = "Print");
	FColor Color = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Print");
	float Duration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Print", meta = (DataflowInput, DisplayName = "String"));
	FString String = FString("");

	FPrintStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


USTRUCT()
struct FLogStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FLogStringDataflowNode, "LogString", "Development", "")

public:
	UPROPERTY(EditAnywhere, Category = "Print");
	bool PrintToLog = true;

	UPROPERTY(EditAnywhere, Category = "Print", meta = (DataflowInput, DisplayName = "String"));
	FString String = FString("");

	FLogStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


USTRUCT()
struct FMakeLiteralStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralStringDataflowNode, "MakeLiteralString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String");
	FString Value = FString("");

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String;

	FMakeLiteralStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FBoundingBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoundingBoxDataflowNode, "BoundingBox", "Utilities|Box", "")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "BoundingBox"))
	FBox BoundingBox = FBox(ForceInit);

	FBoundingBoxDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&BoundingBox);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FExpandBoundingBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExpandBoundingBoxDataflowNode, "ExpandBoundingBox", "Utilities|Box", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "BoundingBox"))
	FBox BoundingBox = FBox(ForceInit);;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Min"))
	FVector Min = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Max"))
	FVector Max = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Center"))
	FVector Center = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "HalfExtents"))
	FVector HalfExtents = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Volume"))
	float Volume = 0.0;

	FExpandBoundingBoxDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&BoundingBox);
		RegisterOutputConnection(&Min);
		RegisterOutputConnection(&Max);
		RegisterOutputConnection(&Center);
		RegisterOutputConnection(&HalfExtents);
		RegisterOutputConnection(&Volume);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FVectorToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVectorToStringDataflowNode, "VectorToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String = FString("");

	FVectorToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Vector);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FFloatToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatToStringDataflowNode, "FloatToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput, DisplayName = "Float"))
	float Float = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String = FString("");

	FFloatToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Float);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FMakePointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakePointsDataflowNode, "MakePoints", "Generators|Point", "")

public:
	UPROPERTY(EditAnywhere, Category = "Point")
	TArray<FVector> Point;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Points"))
	TArray<FVector> Points;

	FMakePointsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Points);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

UENUM(BlueprintType)
enum class EMakeBoxDataTypeEnum : uint8
{
	Dataflow_MakeBox_DataType_MinMax UMETA(DisplayName = "Min/Max"),
	Dataflow_MakeBox_DataType_CenterSize UMETA(DisplayName = "Center/Size"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

USTRUCT()
struct FMakeBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeBoxDataflowNode, "MakeBox", "Generators|Box", "")

public:
	UPROPERTY(EditAnywhere, Category = "Box", meta = (DisplayName = "Input Data Type"));
	EMakeBoxDataTypeEnum DataType = EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax;

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, DisplayName = "Min", EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Min = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, DisplayName = "Max", EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Max = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, DisplayName = "Center", EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Center = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, DisplayName = "Size", EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Size = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Box"));
	FBox Box = FBox(ForceInit);

	FMakeBoxDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Min);
		RegisterInputConnection(&Max);
		RegisterInputConnection(&Center);
		RegisterInputConnection(&Size);
		RegisterOutputConnection(&Box);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FUniformScatterPointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FUniformScatterPointsDataflowNode, "UniformScatterPoints", "Generators|Point", "")

public:
	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Min Number Of Points"));
	int32 MinNumberOfPoints = 20;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Max Number Of Points"));
	int32 MaxNumberOfPoints = 20;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Random Seed"));
	float RandomSeed = -1.f;

	UPROPERTY(meta = (DataflowInput, DisplayName = "BoundingBox"))
		FBox BoundingBox = FBox(ForceInit);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Points"))
		TArray<FVector> Points;

	FUniformScatterPointsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&MinNumberOfPoints);
		RegisterInputConnection(&MaxNumberOfPoints);
		RegisterInputConnection(&RandomSeed);
		RegisterInputConnection(&BoundingBox);
		RegisterOutputConnection(&Points);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRadialScatterPointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FRadialScatterPointsDataflowNode, "RadialScatterPoints", "Generators|Point", "")

public:
	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Center"));
	FVector Center = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Normal"));
	FVector Normal = FVector(0.0, 0.0, 1.0);

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Radius", UIMin = 0.01f));
	float Radius = 50.f;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Angular Steps", UIMin = 1, UIMax = 50));
	int32 AngularSteps = 5;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Radial Steps", UIMin = 1, UIMax = 50));
	int32 RadialSteps = 5;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Angle Offset"));
	float AngleOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Variability", UIMin = 0.f));
	float Variability = 0.f;

	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (DataflowInput, DisplayName = "Random Seed"));
	float RandomSeed = -1.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Points"))
		TArray<FVector> Points;

	FRadialScatterPointsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Center);
		RegisterInputConnection(&Normal);
		RegisterInputConnection(&Radius);
		RegisterInputConnection(&AngularSteps);
		RegisterInputConnection(&RadialSteps);
		RegisterInputConnection(&AngleOffset);
		RegisterInputConnection(&Variability);
		RegisterInputConnection(&RandomSeed);
		RegisterOutputConnection(&Points);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


//
// GridScatterPoints
//



USTRUCT()
struct FMakeLiteralFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralFloatDataflowNode, "MakeLiteralFloat", "Math|Float", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float");
	float Value = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float Float = 0.f;

	FMakeLiteralFloatDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FMakeLiteralIntDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralIntDataflowNode, "MakeLiteralInt", "Math|Int", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int");
	int32 Value = 0;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Int"))
	int32 Int = 0;

	FMakeLiteralIntDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Int);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FMakeLiteralBoolDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralBoolDataflowNode, "MakeLiteralBool", "Math|Boolean", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool");
	bool Value = false;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Bool"))
	bool Bool = false;

	FMakeLiteralBoolDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Bool);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FMakeLiteralVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralVectorDataflowNode, "MakeLiteralVector", "Math|Vector", "")

public:
	UPROPERTY(EditAnywhere, Category = "Vector");
	FVector Value = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	FMakeLiteralVectorDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Vector);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FIntToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FIntToStringDataflowNode, "IntToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int", meta = (DataflowInput, DisplayName = "Int"))
	int32 Int = 0;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String = FString("");

	FIntToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Int);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FBoolToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoolToStringDataflowNode, "BoolToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool", meta = (DataflowInput, DisplayName = "Bool"))
	bool Bool = false;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String = FString("");

	FBoolToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Bool);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FExpandVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExpandVectorDataflowNode, "ExpandVector", "Utilities|Vector", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "X"))
	float X = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Y"))
	float Y = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Z"))
	float Z = 0.f;


	FExpandVectorDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Vector);
		RegisterOutputConnection(&X);
		RegisterOutputConnection(&Y);
		RegisterOutputConnection(&Z);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FIntToFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FIntToFloatDataflowNode, "IntToFloat", "Math|Conversions", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int", meta = (DataflowInput, DisplayName = "Int"))
	int32 Int = 0;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float Float = 0.f;

	FIntToFloatDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Int);
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FVoronoiFractureDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVoronoiFractureDataflowNode, "VoronoiFracture", "Fracture", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Points"))
	TArray<FVector> Points;

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "RandomSeed"));
	float RandomSeed = -1.f;

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "ChanceToFracture", UIMin = 0.f, UIMax = 1.f));
	float ChanceToFracture = 1.f;

	UPROPERTY(EditAnywhere, Category = "Fracture");
	bool GroupFracture = true;

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "Grout", UIMin = 0.f));
	float Grout = 0.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Amplitude", UIMin = 0.f));
	float Amplitude = 0.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Frequency", UIMin = 0.00001f));
	float Frequency = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Persistence", UIMin = 0.f));
	float Persistence = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Lacunarity", UIMin = 0.f));
	float Lacunarity = 2.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "OctaveNumber", UIMin = 0.f));
	int32 OctaveNumber = 4;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "PointSpacing", UIMin = 0.f));
	float PointSpacing = 10.f;

	UPROPERTY(EditAnywhere, Category = "Collision");
	bool AddSamplesForCollision = false;

	UPROPERTY(EditAnywhere, Category = "Collision", meta = (DataflowInput, DisplayName = "CollisionSampleSpacing", UIMin = 0.f));
	float CollisionSampleSpacing = 50.f;

	FVoronoiFractureDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&Points);
		RegisterInputConnection(&RandomSeed);
		RegisterInputConnection(&ChanceToFracture);
		RegisterInputConnection(&Grout);
		RegisterInputConnection(&Amplitude);
		RegisterInputConnection(&Frequency);
		RegisterInputConnection(&Persistence);
		RegisterInputConnection(&Lacunarity);
		RegisterInputConnection(&OctaveNumber);
		RegisterInputConnection(&PointSpacing);
		RegisterInputConnection(&CollisionSampleSpacing);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FStringAppendDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FStringAppendDataflowNode, "StringAppend", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String", meta = (DataflowInput, DisplayName = "String1"))
	FString String1 = FString("");

	UPROPERTY(EditAnywhere, Category = "String", meta = (DataflowInput, DisplayName = "String2"))
	FString String2 = FString("");

	UPROPERTY(meta = (DataflowOutput, DisplayName = "String"))
	FString String = FString("");

	FStringAppendDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String1);
		RegisterInputConnection(&String2);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRandomFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FRandomFloatDataflowNode, "RandomFloat", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool Deterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, DisplayName = "RandomSeed", EditCondition = "Deterministic"))
	float RandomSeed = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float Float = 0.f;

	FRandomFloatDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RandomSeed = FMath::FRandRange(-1e5, 1e5);
		RegisterInputConnection(&RandomSeed);
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRandomFloatInRangeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FRandomFloatInRangeDataflowNode, "RandomFloatInRange", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool Deterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, DisplayName = "RandomSeed", EditCondition = "Deterministic"))
	float RandomSeed = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput, DisplayName = "Min"))
	float Min = 0.f;

	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput, DisplayName = "Max"))
	float Max = 1.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float Float = 0.f;

	FRandomFloatInRangeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RandomSeed = FMath::FRandRange(-1e5, 1e5);
		RegisterInputConnection(&RandomSeed);
		RegisterInputConnection(&Min);
		RegisterInputConnection(&Max);
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRandomUnitVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomUnitVectorDataflowNode, "RandomUnitVector", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool Deterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, DisplayName = "RandomSeed", EditCondition = "Deterministic"))
	float RandomSeed = 0.f;
	
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	FRandomUnitVectorDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RandomSeed = FMath::FRandRange(-1e5, 1e5);
		RegisterInputConnection(&RandomSeed);
		RegisterOutputConnection(&Vector);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRandomUnitVectorInConeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomUnitVectorInConeDataflowNode, "RandomUnitVectorInCone", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool Deterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, DisplayName = "RandomSeed", EditCondition = "Deterministic"))
	float RandomSeed = 0.f;

	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput, DisplayName = "ConeDirection"))
	FVector ConeDirection = FVector(0.0, 0.0, 1.0);

	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput, DisplayName = "ConeHalfAngle"))
	float ConeHalfAngle = PI / 4.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	FRandomUnitVectorInConeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RandomSeed = FMath::FRandRange(-1e5, 1e5);
		RegisterInputConnection(&RandomSeed);
		RegisterInputConnection(&ConeDirection);
		RegisterInputConnection(&ConeHalfAngle);
		RegisterOutputConnection(&Vector);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FRadiansToDegreesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRadiansToDegreesDataflowNode, "RadiansToDegrees", "Math|Trigonometry", "")

public:
	UPROPERTY(EditAnywhere, Category = "Radians", meta = (DataflowInput, DisplayName = "Radians"))
	float Radians = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Degrees"))
	float Degrees = 0.f;

	FRadiansToDegreesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Radians);
		RegisterOutputConnection(&Degrees);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FDegreesToRadiansDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDegreesToRadiansDataflowNode, "DegreesToRadians", "Math|Trigonometry", "")

public:
	UPROPERTY(EditAnywhere, Category = "Degrees", meta = (DataflowInput, DisplayName = "Degrees"))
	float Degrees = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Radians"))
	float Radians = 0.f;

	FDegreesToRadiansDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Degrees);
		RegisterOutputConnection(&Radians);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FExplodedViewDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExplodedViewDataflowNode, "ExplodedView", "Fracture|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Scale", meta = (DataflowInput, DisplayName = "UniformScale"))
	float UniformScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Scale", meta = (DataflowInput, DisplayName = "Scale"))
	FVector Scale = FVector(1.0);

	FExplodedViewDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&UniformScale);
		RegisterInputConnection(&Scale);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

private:
	// todo(chaos) this is a copy of a function in FractureEditorModeToolkit, we should move this to a common place  
	static bool GetValidGeoCenter(FGeometryCollection* Collection, const TManagedArray<int32>& TransformToGeometryIndex, const TArray<FTransform>& Transforms, const TManagedArray<TSet<int32>>& Children, const TManagedArray<FBox>& BoundingBox, int32 TransformIndex, FVector& OutGeoCenter);

};

USTRUCT()
struct FCreateNonOverlappingConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateNonOverlappingConvexHullsDataflowNode, "CreateNonOverlappingConvexHulls", "Fracture|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, DisplayName = "CanRemoveFraction", UIMin = 0.01f, UIMax = 1.f))
	float CanRemoveFraction = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, DisplayName = "CanExceedFraction", UIMin = 0.f))
	float CanExceedFraction = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, DisplayName = "SimplificationDistanceThreshold", UIMin = 0.f))
	float SimplificationDistanceThreshold = 10.f;

	FCreateNonOverlappingConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&CanRemoveFraction);
		RegisterInputConnection(&CanExceedFraction);
		RegisterInputConnection(&SimplificationDistanceThreshold);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FPlaneCutterDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FPlaneCutterDataflowNode, "PlaneCutter", "Fracture", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowInput, DisplayName = "BoundingBox"))
	FBox BoundingBox = FBox(ForceInit);

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "NumPlanes", UIMin = 1))
	int32 NumPlanes = 1;

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "RandomSeed"))
	float RandomSeed = -1.f;

	UPROPERTY(EditAnywhere, Category = "Fracture", meta = (DataflowInput, DisplayName = "Grout", UIMin = 0.f))
	float Grout = 0.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Amplitude", UIMin = 0.f));
	float Amplitude = 0.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Frequency", UIMin = 0.00001f));
	float Frequency = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Persistence", UIMin = 0.f));
	float Persistence = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "Lacunarity", UIMin = 0.f));
	float Lacunarity = 2.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "OctaveNumber", UIMin = 0.f));
	int32 OctaveNumber = 4;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DataflowInput, DisplayName = "PointSpacing", UIMin = 0.f));
	float PointSpacing = 10.f;

	UPROPERTY(EditAnywhere, Category = "Collision");
	bool AddSamplesForCollision = false;

	UPROPERTY(EditAnywhere, Category = "Collision", meta = (DataflowInput, DisplayName = "CollisionSampleSpacing", UIMin = 0.f));
	float CollisionSampleSpacing = 50.f;

	FPlaneCutterDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&BoundingBox);
		RegisterInputConnection(&NumPlanes);
		RegisterInputConnection(&RandomSeed);
		RegisterInputConnection(&Grout);
		RegisterInputConnection(&Amplitude);
		RegisterInputConnection(&Frequency);
		RegisterInputConnection(&Persistence);
		RegisterInputConnection(&Lacunarity);
		RegisterInputConnection(&OctaveNumber);
		RegisterInputConnection(&PointSpacing);
		RegisterInputConnection(&CollisionSampleSpacing);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FHashStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FHashStringDataflowNode, "HashString", "Utilities|String", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "String"))
	FString String = FString("");

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Hash"))
	int32 Hash = 0;


	FHashStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
		RegisterOutputConnection(&Hash);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

USTRUCT()
struct FHashVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FHashVectorDataflowNode, "HashVector", "Utilities|Vector", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Hash"))
	int32 Hash = 0;


	FHashVectorDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Vector);
		RegisterOutputConnection(&Hash);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

UENUM(BlueprintType)
enum class EFloatToIntFunctionEnum : uint8
{
	Dataflow_FloatToInt_Function_Floor UMETA(DisplayName = "Floor()"),
	Dataflow_FloatToInt_Function_Ceil UMETA(DisplayName = "Ceil()"),
	Dataflow_FloatToInt_Function_Round UMETA(DisplayName = "Round()"),
	Dataflow_FloatToInt_Function_Truncate UMETA(DisplayName = "Truncate()"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

USTRUCT()
struct FFloatToIntDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FFloatToIntDataflowNode, "FloatToInt", "Math|Conversions", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DisplayName = "Function"));
	EFloatToIntFunctionEnum Function = EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Round;

	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput, DisplayName = "Float"))
	float Float = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Int"))
	int32 Int = 0;

	FFloatToIntDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Float);
		RegisterOutputConnection(&Int);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

UENUM(BlueprintType)
enum class EMathConstantsEnum : uint8
{
	Dataflow_MathConstants_Pi UMETA(DisplayName = "Pi"),
	Dataflow_MathConstants_HalfPi UMETA(DisplayName = "HalfPi"),
	Dataflow_MathConstants_TwoPi UMETA(DisplayName = "TwoPi"),
	Dataflow_MathConstants_FourPi UMETA(DisplayName = "FourPi"),
	Dataflow_MathConstants_InvPi UMETA(DisplayName = "InvPi"),
	Dataflow_MathConstants_InvTwoPi UMETA(DisplayName = "InvTwoPi"),
	Dataflow_MathConstants_Sqrt2 UMETA(DisplayName = "Sqrt2"),
	Dataflow_MathConstants_InvSqrt2 UMETA(DisplayName = "InvSqrt2"),
	Dataflow_MathConstants_Sqrt3 UMETA(DisplayName = "Sqrt3"),
	Dataflow_MathConstants_InvSqrt3 UMETA(DisplayName = "InvSqrt3"),
	Dataflow_FloatToInt_Function_E UMETA(DisplayName = "e"),
	Dataflow_FloatToInt_Function_Gamma UMETA(DisplayName = "Gamma"),
	Dataflow_FloatToInt_Function_GoldenRatio UMETA(DisplayName = "GoldenRatio"),
	Dataflow_FloatToInt_Function_ZeroTolerance UMETA(DisplayName = "ZeroTolerance"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

USTRUCT()
struct FMathConstantsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMathConstantsDataflowNode, "MathConstants", "Math|Utilities", "")

public:
	UPROPERTY(EditAnywhere, Category = "Constants", meta = (DisplayName = "Constant"));
	EMathConstantsEnum Constant = EMathConstantsEnum::Dataflow_MathConstants_Pi;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float Float = 0;

	FMathConstantsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


namespace Dataflow
{
	void GeometryCollectionEngineAssetNodes();


}

