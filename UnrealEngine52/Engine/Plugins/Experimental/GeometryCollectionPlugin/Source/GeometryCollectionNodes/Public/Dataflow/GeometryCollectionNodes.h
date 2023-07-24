// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"
#include "Dataflow/DataflowSelection.h"
#include "Math/MathFwd.h"
#include "DynamicMesh/DynamicMesh3.h"

#include "GeometryCollectionNodes.generated.h"


class FGeometryCollection;
class UGeometryCollection;
class UStaticMesh;
class UMaterial;


/**
 *
 * Description for this node
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetCollectionFromAssetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetCollectionFromAssetDataflowNode, "GetCollectionFromAsset", "GeometryCollection|Asset", "")

public:
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (DataflowInput, DisplayName = "Asset"))
	TObjectPtr<UGeometryCollection> CollectionAsset;

	UPROPERTY(meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	FGetCollectionFromAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&CollectionAsset);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FAppendCollectionAssetsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAppendCollectionAssetsDataflowNode, "AppendCollections", "GeometryCollection", "")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection1"))
	FManagedArrayCollection Collection1;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection"))
	FManagedArrayCollection Collection2;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "GeometryGroupIndicesOut1"))
	TArray<FString> GeometryGroupGuidsOut1;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "GeometryGroupIndicesOut2"))
		TArray<FString> GeometryGroupGuidsOut2;

	FAppendCollectionAssetsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection1);
		RegisterInputConnection(&Collection2);
		RegisterOutputConnection(&Collection1, &Collection1);
		RegisterOutputConnection(&GeometryGroupGuidsOut1);
		RegisterOutputConnection(&GeometryGroupGuidsOut2);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FPrintStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FPrintStringDataflowNode, "PrintString", "Development", "")

public:
	UPROPERTY(EditAnywhere, Category = "Print");
	bool bPrintToScreen = true;

	UPROPERTY(EditAnywhere, Category = "Print");
	bool bPrintToLog = true;

	UPROPERTY(EditAnywhere, Category = "Print");
	FColor Color = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Print");
	float Duration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Print", meta = (DataflowInput));
	FString String = FString("");

	FPrintStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FLogStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FLogStringDataflowNode, "LogString", "Development", "")

public:
	UPROPERTY(EditAnywhere, Category = "Print");
	bool bPrintToLog = true;

	UPROPERTY(EditAnywhere, Category = "Print", meta = (DataflowInput));
	FString String = FString("");

	FLogStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeLiteralStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralStringDataflowNode, "MakeLiteralString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String");
	FString Value = FString("");

	UPROPERTY(meta = (DataflowOutput))
	FString String;

	FMakeLiteralStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FBoundingBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoundingBoxDataflowNode, "BoundingBox", "Utilities|Box", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FBox"), "BoundingBox")

public:
	UPROPERTY(meta = (DataflowInput))
	FManagedArrayCollection Collection;

	UPROPERTY(meta = (DataflowOutput))
	FBox BoundingBox = FBox(ForceInit);

	FBoundingBoxDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&BoundingBox);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FExpandBoundingBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExpandBoundingBoxDataflowNode, "ExpandBoundingBox", "Utilities|Box", "")

public:
	UPROPERTY(meta = (DataflowInput))
	FBox BoundingBox = FBox(ForceInit);;

	UPROPERTY(meta = (DataflowOutput))
	FVector Min = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
	FVector Max = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
	FVector Center = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
	FVector HalfExtents = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FVectorToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVectorToStringDataflowNode, "VectorToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput))
	FVector Vector = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
	FString String = FString("");

	FVectorToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Vector);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FFloatToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatToStringDataflowNode, "FloatToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput))
	float Float = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	FString String = FString("");

	FFloatToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Float);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakePointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakePointsDataflowNode, "MakePoints", "Generators|Point", "")

public:
	UPROPERTY(EditAnywhere, Category = "Point")
	TArray<FVector> Point;

	UPROPERTY(meta = (DataflowOutput))
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

/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeBoxDataflowNode, "MakeBox", "Generators|Box", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FBox"), "Box")

public:
	UPROPERTY(EditAnywhere, Category = "Box", meta = (DisplayName = "Input Data Type"));
	EMakeBoxDataTypeEnum DataType = EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax;

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Min = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Max = FVector(10.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Center = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Size = FVector(10.0);

	UPROPERTY(meta = (DataflowOutput));
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


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeSphereDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeSphereDataflowNode, "MakeSphere", "Generators|Sphere", "")

public:
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DataflowInput));
	FVector Center = FVector(0.f);

	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DataflowInput));
	float Radius = 10.f;

	UPROPERTY(meta = (DataflowOutput));
	FSphere Sphere = FSphere(ForceInit);

	FMakeSphereDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Center);
		RegisterInputConnection(&Radius);
		RegisterOutputConnection(&Sphere);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeLiteralFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralFloatDataflowNode, "MakeLiteralFloat", "Math|Float", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float");
	float Value = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	float Float = 0.f;

	FMakeLiteralFloatDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Description for this node
 *
 */
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

/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeLiteralBoolDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralBoolDataflowNode, "MakeLiteralBool", "Math|Boolean", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool");
	bool Value = false;

	UPROPERTY(meta = (DataflowOutput))
	bool Bool = false;

	FMakeLiteralBoolDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Bool);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * 
 *
 */
USTRUCT()
struct FMakeLiteralVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralVectorDataflowNode, "MakeLiteralVector", "Math|Vector", "")

public:
	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float X = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float Y = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float Z = float(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	FMakeLiteralVectorDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&X);
		RegisterInputConnection(&Y);
		RegisterInputConnection(&Z);
		RegisterOutputConnection(&Vector);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts an Int to a String
 *
 */
USTRUCT()
struct FIntToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FIntToStringDataflowNode, "IntToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int", meta = (DataflowInput))
	int32 Int = 0;

	UPROPERTY(meta = (DataflowOutput))
	FString String = FString("");

	FIntToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Int);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a Bool to a String in a form of ("true", "false")
 *
 */
USTRUCT()
struct FBoolToStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoolToStringDataflowNode, "BoolToString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool", meta = (DataflowInput))
	bool Bool = false;

	UPROPERTY(meta = (DataflowOutput))
	FString String = FString("");

	FBoolToStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Bool);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Expands a Vector into X, Y, Z components
 *
 */
USTRUCT()
struct FExpandVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FExpandVectorDataflowNode, "ExpandVector", "Utilities|Vector", "")

public:
	UPROPERTY(meta = (DataflowInput))
	FVector Vector = FVector(0.0);

	UPROPERTY(meta = (DataflowOutput))
	float X = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	float Y = 0.f;

	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Converts an Int to a Float
 *
 */
USTRUCT()
struct FIntToFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FIntToFloatDataflowNode, "IntToFloat", "Math|Conversions", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int", meta = (DataflowInput))
	int32 Int = 0;

	UPROPERTY(meta = (DataflowOutput))
	float Float = 0.f;

	FIntToFloatDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Int);
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Concatenates two strings together to make a new string
 *
 */
USTRUCT()
struct FStringAppendDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FStringAppendDataflowNode, "StringAppend", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String", meta = (DataflowInput))
	FString String1 = FString("");

	UPROPERTY(EditAnywhere, Category = "String", meta = (DataflowInput))
	FString String2 = FString("");

	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Generates a random float
 *
 */
USTRUCT()
struct FRandomFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomFloatDataflowNode, "RandomFloat", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, EditCondition = "Deterministic"))
	float RandomSeed = 0.f;

	UPROPERTY(meta = (DataflowOutput))
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

/**
 *
 * Generates a random float between Min and Max
 *
 */
USTRUCT()
struct FRandomFloatInRangeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomFloatInRangeDataflowNode, "RandomFloatInRange", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, EditCondition = "Deterministic"))
	float RandomSeed = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput))
	float Min = 0.f;

	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput))
	float Max = 1.f;

	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Returns a random vector with length of 1
 *
 */
USTRUCT()
struct FRandomUnitVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomUnitVectorDataflowNode, "RandomUnitVector", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, EditCondition = "Deterministic"))
	float RandomSeed = 0.f;
	
	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution 
 *
 */
USTRUCT()
struct FRandomUnitVectorInConeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRandomUnitVectorInConeDataflowNode, "RandomUnitVectorInCone", "Math|Random", "")

public:
	UPROPERTY(EditAnywhere, Category = "Seed")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, Category = "Seed", meta = (DataflowInput, EditCondition = "Deterministic"))
	float RandomSeed = 0.f;

	/** The base "center" direction of the cone */
	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput))
	FVector ConeDirection = FVector(0.0, 0.0, 1.0);

	/** The half-angle of the cone (from ConeDir to edge), in degrees */
	UPROPERTY(EditAnywhere, Category = "Random", meta = (DataflowInput))
	float ConeHalfAngle = PI / 4.f;

	UPROPERTY(meta = (DataflowOutput))
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


/**
 *
 * Converts radians to degrees
 *
 */
USTRUCT()
struct FRadiansToDegreesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRadiansToDegreesDataflowNode, "RadiansToDegrees", "Math|Trigonometry", "")

public:
	UPROPERTY(EditAnywhere, Category = "Radians", meta = (DataflowInput))
	float Radians = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	float Degrees = 0.f;

	FRadiansToDegreesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Radians);
		RegisterOutputConnection(&Degrees);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts degrees to radians
 *
 */
USTRUCT()
struct FDegreesToRadiansDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDegreesToRadiansDataflowNode, "DegreesToRadians", "Math|Trigonometry", "")

public:
	UPROPERTY(EditAnywhere, Category = "Degrees", meta = (DataflowInput))
	float Degrees = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	float Radians = 0.f;

	FDegreesToRadiansDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Degrees);
		RegisterOutputConnection(&Radians);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Generates a hash value from a string
 *
 */
USTRUCT()
struct FHashStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FHashStringDataflowNode, "HashString", "Utilities|String", "")

public:
	/** String to hash */
	UPROPERTY(meta = (DataflowInput))
	FString String = FString("");

	/** Generated hash value */
	UPROPERTY(meta = (DataflowOutput))
	int32 Hash = 0;

	FHashStringDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&String);
		RegisterOutputConnection(&Hash);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Generates a hash value from a vector
 *
 */
USTRUCT()
struct FHashVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FHashVectorDataflowNode, "HashVector", "Utilities|Vector", "")

public:
	/** Vector to hash */
	UPROPERTY(meta = (DataflowInput))
	FVector Vector = FVector(0.0);

	/** Generated hash value */
	UPROPERTY(meta = (DataflowOutput))
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

/**
 *
 * Converts a Float to Int using the specified method
 *
 */
USTRUCT()
struct FFloatToIntDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatToIntDataflowNode, "FloatToInt", "Math|Conversions", "")

public:
	/** Method to convert */
	UPROPERTY(EditAnywhere, Category = "Float");
	EFloatToIntFunctionEnum Function = EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Round;

	/** Float value to convert */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput))
	float Float = 0.f;

	/** Int output */
	UPROPERTY(meta = (DataflowOutput))
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
enum class EFloatArrayToIntArrayFunctionEnum : uint8
{
	Dataflow_FloatToInt_Function_Floor UMETA(DisplayName = "Floor()"),
	Dataflow_FloatToInt_Function_Ceil UMETA(DisplayName = "Ceil()"),
	Dataflow_FloatToInt_Function_Round UMETA(DisplayName = "Round()"),
	Dataflow_FloatToInt_Function_Truncate UMETA(DisplayName = "Truncate()"),
	Dataflow_FloatToInt_NonZeroToIndex UMETA(DisplayName = "Non-zero to Index"),
	Dataflow_FloatToInt_ZeroToIndex UMETA(DisplayName = "Zero to Index"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Converts a Float array to Int array using the specified method.
 *
 */
USTRUCT()
struct FFloatArrayToIntArrayDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FFloatArrayToIntArrayDataflowNode, "FloatArrayToIntArray", "Math|Conversions", "")

public:
	/** Conversion method:
	* Floor takes the floor of each input float value - 1.1 turns into 1.
	* Ceil takes the ceil - 1.1 turns into 2.
	* Round rounds to the nearest integer - 1.1 turns into 1.
	* Tuncate trucates like a type cast - 1.1 turns into 1.
	* Non-zero to Index appends the index of all non-zero values to the output array.
	* Zero to Index appends the index of all zero values to the output array.
	*/
	UPROPERTY(EditAnywhere, Category = "Float");
	EFloatArrayToIntArrayFunctionEnum Function = EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_NonZeroToIndex;

	/** Float array value to convert */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput))
	TArray<float> FloatArray;

	/** Int array output */
	UPROPERTY(meta = (DataflowOutput))
	TArray<int32> IntArray;

	FFloatArrayToIntArrayDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&FloatArray);
		RegisterOutputConnection(&IntArray);
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

/**
 *
 * Offers a selection of Math constants
 *
 */
USTRUCT()
struct FMathConstantsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMathConstantsDataflowNode, "MathConstants", "Math|Utilities", "")

public:
	/** Math constant to output */
	UPROPERTY(EditAnywhere, Category = "Constants");
	EMathConstantsEnum Constant = EMathConstantsEnum::Dataflow_MathConstants_Pi;

	/** Selected Math constant */
	UPROPERTY(meta = (DataflowOutput))
	float Float = 0;

	FMathConstantsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Returns the specified element from an array
 *
 */
USTRUCT()
struct FGetArrayElementDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetArrayElementDataflowNode, "GetArrayElement", "Utilities|Array", "")

public:
	/** Element index */
	UPROPERTY(EditAnywhere, Category = "Index");
	int32 Index = 0;

	/** Array to get the element from */
	UPROPERTY(meta = (DataflowInput))
	TArray<FVector> Points;

	/** Specified element */
	UPROPERTY(meta = (DataflowOutput))
	FVector Point = FVector(0.0);

	FGetArrayElementDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterInputConnection(&Index);
		RegisterOutputConnection(&Point);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Returns the number of elements in an array
 *
 */
USTRUCT()
struct FGetNumArrayElementsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetNumArrayElementsDataflowNode, "GetNumArrayElements", "Utilities|Array", "")

public:
	/** FVector array input */
	UPROPERTY(meta = (DataflowInput, DisplayName = "VectorArray"))
	TArray<FVector> Points;

	/** FVector3f array input */
	UPROPERTY(meta = (DataflowInput, DisplayName = "Vector3fArray"))
	TArray<FVector3f> Vector3fArray;

	/** Number of elements in the array */
	UPROPERTY(meta = (DataflowOutput))
	int32 NumElements = 0;

	FGetNumArrayElementsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterInputConnection(&Vector3fArray);
		RegisterOutputConnection(&NumElements);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Gets BoundingBoxes of pieces from a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetBoundingBoxesFromCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetBoundingBoxesFromCollectionDataflowNode, "GetBoundingBoxesFromCollection", "GeometryCollection|Utilities", "")

public:
	/** Input Collection */
	UPROPERTY(meta = (DataflowInput))
	FManagedArrayCollection Collection;

	/** The BoundingBoxes will be output for the bones selected in the TransformSelection  */
	UPROPERTY(meta = (DataflowInput, DisplayName = "TransformSelection"))
	FDataflowTransformSelection TransformSelection;

	/** Output BoundingBoxes */
	UPROPERTY(meta = (DataflowOutput))
	TArray<FBox> BoundingBoxes;

	FGetBoundingBoxesFromCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&TransformSelection);
		RegisterOutputConnection(&BoundingBoxes);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Gets centroids of pieces from a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetCentroidsFromCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetCentroidsFromCollectionDataflowNode, "GetCentroidsFromCollection", "GeometryCollection|Utilities", "")

public:
	/** Input Collection */
	UPROPERTY(meta = (DataflowInput))
	FManagedArrayCollection Collection;

	/** The centroids will be output for the bones selected in the TransformSelection  */
	UPROPERTY(meta = (DataflowInput, DisplayName = "TransformSelection"))
	FDataflowTransformSelection TransformSelection;

	/** Output centroids */
	UPROPERTY(meta = (DataflowOutput))
	TArray<FVector> Centroids = TArray<FVector>();

	FGetCentroidsFromCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&TransformSelection);
		RegisterOutputConnection(&Centroids);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class ERotationOrderEnum : uint8
{
	Dataflow_RotationOrder_XYZ UMETA(DisplayName = "XYZ"),
	Dataflow_RotationOrder_YZX UMETA(DisplayName = "YZX"),
	Dataflow_RotationOrder_ZXY UMETA(DisplayName = "ZXY"),
	Dataflow_RotationOrder_XZY UMETA(DisplayName = "XZY"),
	Dataflow_RotationOrder_YXZ UMETA(DisplayName = "YXZ"),
	Dataflow_RotationOrder_ZYX UMETA(DisplayName = "ZYX"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Transforms a Collection
 *
 */
USTRUCT()
struct FTransformCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FTransformCollectionDataflowNode, "TransformCollection", "Math|Transform", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	/** Output mesh */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Translation */
	UPROPERTY(EditAnywhere, Category = "Transform");
	FVector Translate = FVector(0.0);

	/** Rotation Order */
	UPROPERTY(EditAnywhere, Category = "Transform");
	ERotationOrderEnum RotationOrder = ERotationOrderEnum::Dataflow_RotationOrder_XYZ;

	/** Rotation */
	UPROPERTY(EditAnywhere, Category = "Transform");
	FVector Rotate = FVector(0.0);

	/** Scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (UIMin = 0.001f));
	FVector Scale = FVector(1.0);

	/** Shear */
//	UPROPERTY(EditAnywhere, Category = "Transform");
//	FVector Shear = FVector(0.0);

	/** Uniform scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (UIMin = 0.001f));
	float UniformScale = 1.f;

	/** Pivot for the rotation */
	UPROPERTY(EditAnywhere, Category = "Pivot");
	FVector RotatePivot = FVector(0.0);

	/** Pivot for the scale */
	UPROPERTY(EditAnywhere, Category = "Pivot");
	FVector ScalePivot = FVector(0.0);

	/** Invert the transformation */
	UPROPERTY(EditAnywhere, Category = "General");
	bool bInvertTransformation = false;

	FTransformCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Bake transforms in Collection
 *
 */
USTRUCT()
struct FBakeTransformsInCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBakeTransformsInCollectionDataflowNode, "BakeTransformsInCollection", "Math|Transform", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	/** Collection to bake transforms in */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	FBakeTransformsInCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Transforms a mesh
 *
 */
USTRUCT()
struct FTransformMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FTransformMeshDataflowNode, "TransformMesh", "Math|Transform", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** Output mesh */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Mesh", DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Translation */
	UPROPERTY(EditAnywhere, Category = "Transform");
	FVector Translate = FVector(0.0);

	/** Rotation Order */
	UPROPERTY(EditAnywhere, Category = "Transform");
	ERotationOrderEnum RotationOrder = ERotationOrderEnum::Dataflow_RotationOrder_XYZ;

	/** Rotation */
	UPROPERTY(EditAnywhere, Category = "Transform");
	FVector Rotate = FVector(0.0);

	/** Scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (UIMin = 0.001f));
	FVector Scale = FVector(1.0);

	/** Shear */
//	UPROPERTY(EditAnywhere, Category = "Transform");
//	FVector Shear = FVector(0.0);

	/** Uniform scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (UIMin = 0.001f));
	float UniformScale = 1.f;

	/** Pivot for the rotation */
	UPROPERTY(EditAnywhere, Category = "Pivot");
	FVector RotatePivot = FVector(0.0);

	/** Pivot for the scale */
	UPROPERTY(EditAnywhere, Category = "Pivot");
	FVector ScalePivot = FVector(0.0);

	/** Invert the transformation */
	UPROPERTY(EditAnywhere, Category = "General");
	bool bInvertTransformation = false;

	FTransformMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&Mesh, &Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class ECompareOperationEnum : uint8
{
	Dataflow_Compare_Equal UMETA(DisplayName = "=="),
	Dataflow_Compare_Smaller UMETA(DisplayName = "<"),
	Dataflow_Compare_SmallerOrEqual UMETA(DisplayName = "<="),
	Dataflow_Compare_Greater UMETA(DisplayName = ">"),
	Dataflow_Compare_GreaterOrEqual UMETA(DisplayName = ">="),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Comparison between integers
 *
 */
USTRUCT()
struct FCompareIntDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCompareIntDataflowNode, "CompareInt", "Math|Int", "")

public:
	/** Comparison operation */
	UPROPERTY(EditAnywhere, Category = "Compare");
	ECompareOperationEnum Operation = ECompareOperationEnum::Dataflow_Compare_Equal;

	/** Int input */
	UPROPERTY(EditAnywhere, Category = "Compare");
	int32 IntA = 0;

	/** Int input */
	UPROPERTY(EditAnywhere, Category = "Compare");
	int32 IntB = 0;

	/** Boolean result of the comparison */
	UPROPERTY(meta = (DataflowOutput));
	bool Result = false;

	FCompareIntDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&IntA);
		RegisterInputConnection(&IntB);
		RegisterOutputConnection(&Result);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Branch between two inputs based on boolean condition
 *
 */
USTRUCT()
struct FBranchDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBranchDataflowNode, "Branch", "Utilities|FlowControl", "")

public:
	/** Mesh input */
	UPROPERTY(meta = (DataflowInput))
	TObjectPtr<UDynamicMesh> MeshA;

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput))
	TObjectPtr<UDynamicMesh> MeshB;

	/** If true, Output = MeshA, otherwise Output = MeshB */
	UPROPERTY(EditAnywhere, Category = "Branch");
	bool bCondition = false;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FBranchDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&MeshA);
		RegisterInputConnection(&MeshB);
		RegisterInputConnection(&bCondition);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Collects grooup and attribute information from the Collection and outputs it into a formatted string
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetSchemaDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetSchemaDataflowNode, "GetSchema", "GeometryCollection|Utilities", "")

public:
	/** GeometryCollection  for the information */
	UPROPERTY(meta = (DataflowInput))
	FManagedArrayCollection Collection;

	/** Formatted string containing the groups and attributes */
	UPROPERTY(meta = (DataflowOutput))
	FString String;

	FGetSchemaDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


USTRUCT(meta = (DataflowGeometryCollection))
struct FRemoveOnBreakDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FRemoveOnBreakDataflowNode, "RemoveOnBreak", "GeometryCollection|Utilities", "")

public:
	/** Collection to set theremoval data on */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** selection to apply the data on ( if not specified the entire collection will be set ) */
	UPROPERTY(meta = (DataflowInput, DisplayName = "TransformSelection"))
	FDataflowTransformSelection TransformSelection;

	/** Whether or not to enable the removal on the selection */
	UPROPERTY(EditAnywhere, Category = "Removal", meta = (DataflowInput))
	bool bEnabledRemoval = true;

	/** How long after the break the removal will start ( Min / Max ) */
	UPROPERTY(EditAnywhere, Category = "Removal", meta = (DataflowInput))
	FVector2f PostBreakTimer{0.0, 0.0};

	/** How long removal will last ( Min / Max ) */
	UPROPERTY(EditAnywhere, Category = "Removal", meta = (DataflowInput))
	FVector2f RemovalTimer{0.0, 1.0};

	/** If applied to a cluster this will cause the cluster to crumble upon removal, otherwise will have no effect */
	UPROPERTY(EditAnywhere, Category = "Removal", meta = (DataflowInput))
	bool bClusterCrumbling = false;

	FRemoveOnBreakDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&TransformSelection);
		RegisterInputConnection(&bEnabledRemoval);
		RegisterInputConnection(&PostBreakTimer);
		RegisterInputConnection(&RemovalTimer);
		RegisterInputConnection(&bClusterCrumbling);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class EAnchorStateEnum : uint8
{
	Dataflow_AnchorState_Anchored UMETA(DisplayName = "Anchored"),
	Dataflow_AnchorState_NotAnchored UMETA(DisplayName = "Not Anchored"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Sets the anchored state on the selected bones in a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FSetAnchorStateDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetAnchorStateDataflowNode, "SetAnchorState", "GeometryCollection|Utilities", "")

public:
	/** What anchor state to set on selected bones */
	UPROPERTY(EditAnywhere, Category = "Anchoring");
	EAnchorStateEnum AnchorState = EAnchorStateEnum::Dataflow_AnchorState_Anchored;

	/** If true, sets the non selected bones to opposite anchor state */
	UPROPERTY(EditAnywhere, Category = "Anchoring")
	bool bSetNotSelectedBonesToOppositeState = false;

	/** GeometryCollection to set anchor state on */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Bone selection for setting the state on */
	UPROPERTY(meta = (DataflowInput, DisplayName = "TransformSelection", DataflowIntrinsic))
	FDataflowTransformSelection TransformSelection;

	FSetAnchorStateDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&TransformSelection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class EProximityMethodEnum : uint8
{
	/** Precise proximity mode looks for geometry with touching vertices or touching, coplanar, opposite - facing triangles.This works well with geometry fractured using our fracture tools. */
	Dataflow_ProximityMethod_Precise UMETA(DisplayName = "Precise"),
	/** Convex Hull proximity mode looks for geometry with overlapping convex hulls(with an optional offset) */
	Dataflow_ProximityMethod_ConvexHull UMETA(DisplayName = "ConvexHull"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Update the proximity (contact) graph for the bones in a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FProximityDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FProximityDataflowNode, "Proximity", "GeometryCollection|Utilities", "")

public:
	/** Which method to use to decide whether a given piece of geometry is in proximity with another */
	UPROPERTY(EditAnywhere, Category = "Proximity");
	EProximityMethodEnum ProximityMethod = EProximityMethodEnum::Dataflow_ProximityMethod_Precise;

	/** If hull-based proximity detection is enabled, amount to expand hulls when searching for overlapping neighbors */
	UPROPERTY(EditAnywhere, Category = "Proximity", meta = (ClampMin = "0", EditCondition = "ProximityMethod == EProximityMethodEnum::Dataflow_ProximityMethod_ConvexHull"))
	float DistanceThreshold = 1;

	// If greater than zero, proximity will be additionally filtered by a 'contact' threshold, in cm, to exclude grazing / corner proximity
	UPROPERTY(EditAnywhere, Category = "Proximity", meta = (ClampMin = "0"))
	float ContactThreshold = 0;

	/** Whether to automatically transform the proximity graph into a connection graph to be used for simulation */
	UPROPERTY(EditAnywhere, Category = "Proximity")
	bool bUseAsConnectionGraph = false;

	/** GeometryCollection to update the proximity graph on */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	FProximityDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Sets pivot for Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCollectionSetPivotDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCollectionSetPivotDataflowNode, "SetPivot", "GeometryCollection|Utilities", "")

public:
	/** Collection for the pivot change */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Pivot transform */
	UPROPERTY(EditAnywhere, Category = "Pivot", meta = (DataflowInput))
	FTransform Transform;

	FCollectionSetPivotDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&Transform);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class EStandardGroupNameEnum : uint8
{
	Dataflow_EStandardGroupNameEnum_Transform	UMETA(DisplayName = "Transform", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Geometry	UMETA(DisplayName = "Geometry", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Faces		UMETA(DisplayName = "Faces", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Vertices	UMETA(DisplayName = "Vertices", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Material	UMETA(DisplayName = "Material", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Breaking	UMETA(DisplayName = "Breaking", ToolTip = ""),
	Dataflow_EStandardGroupNameEnum_Custom		UMETA(DisplayName = "Custom", ToolTip = ""),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ECustomAttributeTypeEnum : uint8
{
	Dataflow_CustomAttributeType_UInt8				UMETA(DisplayName = "UInt8", ToolTip = "Data type: UInt8"),
	Dataflow_CustomAttributeType_Int32				UMETA(DisplayName = "Int32", ToolTip = "Data type: Int32"),
	Dataflow_CustomAttributeType_Float				UMETA(DisplayName = "Float", ToolTip = "Data type: Float"),
	Dataflow_CustomAttributeType_Double				UMETA(DisplayName = "Double", ToolTip = "Data type: Double"),
	Dataflow_CustomAttributeType_Bool				UMETA(DisplayName = "Bool", ToolTip = "Data type: Bool"),
	Dataflow_CustomAttributeType_String				UMETA(DisplayName = "String", ToolTip = "Data type: FString"),
	Dataflow_CustomAttributeType_Vector2f			UMETA(DisplayName = "Vector2D", ToolTip = "Data type: FVector2f"),
	Dataflow_CustomAttributeType_Vector3f			UMETA(DisplayName = "Vector", ToolTip = "Data type: FVector3f"),
	Dataflow_CustomAttributeType_Vector3d			UMETA(DisplayName = "Vector3d", ToolTip = "Data type: FVector3d"),
	Dataflow_CustomAttributeType_Vector4f			UMETA(DisplayName = "Vector4f", ToolTip = "Data type: FVector4f"),
	Dataflow_CustomAttributeType_LinearColor		UMETA(DisplayName = "LinearColor", ToolTip = "Data type: FLinearColor"),
	Dataflow_CustomAttributeType_Transform			UMETA(DisplayName = "Transform", ToolTip = "Data type: FTransform"),
	Dataflow_CustomAttributeType_Quat4f				UMETA(DisplayName = "Quat", ToolTip = "Data type: FQuat4f"),
	Dataflow_CustomAttributeType_Box				UMETA(DisplayName = "Box", ToolTip = "Data type: FBox"),
	Dataflow_CustomAttributeType_Guid				UMETA(DisplayName = "Guid", ToolTip = "Data type: FGuid"),
	Dataflow_CustomAttributeType_Int32Set			UMETA(DisplayName = "IntArray", ToolTip = "Data type: TSet<int32>"),
	Dataflow_CustomAttributeType_Int32Array			UMETA(DisplayName = "Int32Array", ToolTip = "Data type: TArray<int32>"),
	Dataflow_CustomAttributeType_IntVector			UMETA(DisplayName = "IntVector", ToolTip = "Data type: FIntVector"),
	Dataflow_CustomAttributeType_IntVector2			UMETA(DisplayName = "IntVector2", ToolTip = "Data type: FIntVector2"),
	Dataflow_CustomAttributeType_IntVector4			UMETA(DisplayName = "IntVector4", ToolTip = "Data type: FIntVector4"),
	Dataflow_CustomAttributeType_IntVector2Array	UMETA(DisplayName = "IntVector2Array", ToolTip = "Data type: TArray<FIntVector2>"),
	Dataflow_CustomAttributeType_FloatArray			UMETA(DisplayName = "FloatArray", ToolTip = "Data type: TArray<float>"),
	Dataflow_CustomAttributeType_Vector2fArray		UMETA(DisplayName = "Vector2DArray", ToolTip = "Data type: TArray<FVector2f>"),
	Dataflow_CustomAttributeType_FVector3fArray		UMETA(DisplayName = "FVectorArray", ToolTip = "Data type: TArray<FVector3f>"),

	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Adds custom attribute to Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FAddCustomCollectionAttributeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAddCustomCollectionAttributeDataflowNode, "AddCustomCollectionAttribute", "GeometryCollection|Utilities", "")

public:
	/** Collection for the custom attribute */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Standard group names */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Group"))
	EStandardGroupNameEnum GroupName = EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Transform;

	/** User specified group name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Custom Group", EditCondition = "GroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom"))
	FString CustomGroupName = FString("");

	/** Attribute name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Attribute"))
	FString AttrName = FString("");

	/** Attribute type */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Attribute Type"));
	ECustomAttributeTypeEnum CustomAttributeType = ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Float;

	/** Number of elements for the attribute */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Number of Elements"));
	int32 NumElements = 0;

	FAddCustomCollectionAttributeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&NumElements);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Returns number of elements in a group in a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetNumElementsInCollectionGroupDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetNumElementsInCollectionGroupDataflowNode, "GetNumElementsInCollectionGroup", "GeometryCollection|Utilities", "")

public:
	/** Collection for the custom attribute */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Standard group names */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Group"))
	EStandardGroupNameEnum GroupName = EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Transform;

	/** User specified group name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Custom Group", EditCondition = "GroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom"))
	FString CustomGroupName = FString("");

	/** Number of elements for the attribute */
	UPROPERTY(meta = (DataflowOutput));
	int32 NumElements = 0;

	FGetNumElementsInCollectionGroupDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&NumElements);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Get attribute data from a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetCollectionAttributeDataTypedDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetCollectionAttributeDataTypedDataflowNode, "GetCollectionAttributeDataTyped", "GeometryCollection|Utilities", "")

public:
	/** Collection for the custom attribute */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Standard group names */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Group"))
	EStandardGroupNameEnum GroupName = EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Transform;

	/** User specified group name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Custom Group", EditCondition = "GroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom"))
	FString CustomGroupName = FString("");

	/** Attribute name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Attribute"))
	FString AttrName = FString("");

	/** Bool type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<bool> BoolAttributeData;

	/** Float type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<float> FloatAttributeData;

	/** Float type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<double> DoubleAttributeData;

	/** Int type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<int32> Int32AttributeData;

	/** Int type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<FString> StringAttributeData;

	/** Vector3f type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<FVector3f> Vector3fAttributeData;

	/** Vector3d type attribute data */
	UPROPERTY(meta = (DataflowOutput));
	TArray<FVector3d> Vector3dAttributeData;

	FGetCollectionAttributeDataTypedDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&BoolAttributeData);
		RegisterOutputConnection(&FloatAttributeData);
		RegisterOutputConnection(&DoubleAttributeData);
		RegisterOutputConnection(&Int32AttributeData);
		RegisterOutputConnection(&StringAttributeData);
		RegisterOutputConnection(&Vector3fAttributeData);
		RegisterOutputConnection(&Vector3dAttributeData);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Set attribute data in a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FSetCollectionAttributeDataTypedDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetCollectionAttributeDataTypedDataflowNode, "SetCollectionAttributeDataTyped", "GeometryCollection|Utilities", "")

public:
	/** Collection for the custom attribute */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Standard group names */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Group"))
	EStandardGroupNameEnum GroupName = EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Transform;

	/** User specified group name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Custom Group", EditCondition = "GroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom"))
	FString CustomGroupName = FString("");

	/** Attribute name */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Attribute"))
	FString AttrName = FString("");

	/** Bool type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<bool> BoolAttributeData;

	/** Float type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<float> FloatAttributeData;

	/** Float type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<double> DoubleAttributeData;

	/** Int type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<int32> Int32AttributeData;

	/** Int type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<FString> StringAttributeData;

	/** Vector3f type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<FVector3f> Vector3fAttributeData;

	/** Vector3d type attribute data */
	UPROPERTY(meta = (DataflowInput));
	TArray<FVector3d> Vector3dAttributeData;

	FSetCollectionAttributeDataTypedDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&BoolAttributeData);
		RegisterInputConnection(&FloatAttributeData);
		RegisterInputConnection(&DoubleAttributeData);
		RegisterInputConnection(&Int32AttributeData);
		RegisterInputConnection(&StringAttributeData);
		RegisterInputConnection(&Vector3fAttributeData);
		RegisterInputConnection(&Vector3dAttributeData);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a TArray<bool> to a FDataflowFaceSelection
 *
 */
USTRUCT()
struct FBoolArrayToFaceSelectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoolArrayToFaceSelectionDataflowNode, "BoolArrayToFaceSelection", "Utilities|Array", "")

public:
	/** TArray<bool> data */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic));
	TArray<bool> BoolAttributeData;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "FaceSelection"))
	FDataflowFaceSelection FaceSelection;

	FBoolArrayToFaceSelectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&BoolAttributeData);
		RegisterOutputConnection(&FaceSelection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a TArray<float> to a FDataflowVertexSelection
 *
 */
USTRUCT()
struct FFloatArrayToVertexSelectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatArrayToVertexSelectionDataflowNode, "FloatArrayToVertexSelection", "Utilities|Array", "")

public:
	/** TArray<floatl> array */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic));
	TArray<float> FloatArray;

	/** Comparison operation */
	UPROPERTY(EditAnywhere, Category = "Compare");
	ECompareOperationEnum Operation = ECompareOperationEnum::Dataflow_Compare_Greater;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Compare")
	float Threshold = 0.f;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "VertexSelection"))
	FDataflowVertexSelection VertexSelection;

	FFloatArrayToVertexSelectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&FloatArray);
		RegisterOutputConnection(&VertexSelection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * 
 *
 */
USTRUCT()
struct FSetVertexColorInCollectionFromVertexSelectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetVertexColorInCollectionFromVertexSelectionDataflowNode, "SetVertexColorInCollectionFromVertexSelection", "Collection|Utilities", "")

public:
	/** Collection */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/**  */
	UPROPERTY(meta = (DataflowInput, DisplayName = "VertexSelection", DataflowIntrinsic))
	FDataflowVertexSelection VertexSelection;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Color")
	FLinearColor SelectedColor = FLinearColor(FColor::Yellow);

	/**  */
	UPROPERTY(EditAnywhere, Category = "Color", meta = (DisplayName = "NonSelected Color"))
	FLinearColor NonSelectedColor = FLinearColor(FColor::Blue);

	FSetVertexColorInCollectionFromVertexSelectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&VertexSelection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 *
 *
 */
USTRUCT()
struct FSelectionToVertexListDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FSelectionToVertexListDataflowNode, "SelectionToVertexList", "Selection|Utility", "")

public:

	/**  */
	UPROPERTY(meta = (DataflowInput, DisplayName = "VertexSelection", DataflowIntrinsic))
	FDataflowVertexSelection VertexSelection;

	/**  */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "VertexList"))
	TArray<int32> VertexList;


	FSelectionToVertexListDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&VertexSelection);
		RegisterOutputConnection(&VertexList);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeTransformDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeTransformDataflowNode, "MakeTransform", "Generators|Transform", "")

public:

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Translation"));
	FVector InTranslation = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Rotation"));
	FVector InRotation = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Scale"));
	FVector InScale = FVector(1, 1, 1);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Transform"));
	FTransform OutTransform = FTransform::Identity;

	FMakeTransformDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InTranslation);
		RegisterInputConnection(&InRotation);
		RegisterInputConnection(&InScale);
		RegisterOutputConnection(&OutTransform);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 *
 *
 */
USTRUCT()
struct FSetVertexColorInCollectionFromFloatArrayDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetVertexColorInCollectionFromFloatArrayDataflowNode, "SetVertexColorInCollectionFromFloatArray", "Collection|Utilities", "")

public:
	/** Collection */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/**  */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<float> FloatArray;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Color")
	float Scale = 1.f;

	FSetVertexColorInCollectionFromFloatArrayDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&FloatArray);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Normalize the selected float data in a FloatArray
 *
 */
USTRUCT()
struct FFloatArrayNormalizeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatArrayNormalizeDataflowNode, "FloatArrayNormalize", "Math|Float", "")

public:
	/** Input VectorArray */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<float> InFloatArray;

	/** Selection for the operation */
	UPROPERTY(meta = (DataflowInput))
	FDataflowVertexSelection Selection;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Field", meta = (DataflowInput))
	float MinRange = 0.f;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Field", meta = (DataflowInput))
	float MaxRange = 1.f;

	/**  */
	UPROPERTY(meta = (DataflowOutput))
	TArray<float> OutFloatArray;

	FFloatArrayNormalizeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InFloatArray);
		RegisterInputConnection(&Selection);
		RegisterInputConnection(&MinRange);
		RegisterInputConnection(&MaxRange);
		RegisterOutputConnection(&OutFloatArray);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Normalize all the selected vectors in a VectorArray
 *
 */
USTRUCT()
struct FVectorArrayNormalizeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVectorArrayNormalizeDataflowNode, "VectorArrayNormalize", "Math|Vector", "")

public:
	/** Input VectorArray */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<FVector> InVectorArray;

	/** Selection for the operation */
	UPROPERTY(meta = (DataflowInput))
	FDataflowVertexSelection Selection;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Field", meta = (DataflowInput))
	float Magnitude = 1.f;

	/**  */
	UPROPERTY(meta = (DataflowOutput))
	TArray<FVector> OutVectorArray;

	FVectorArrayNormalizeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InVectorArray);
		RegisterInputConnection(&Selection);
		RegisterInputConnection(&Magnitude);
		RegisterOutputConnection(&OutVectorArray);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMultiplyTransformDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMultiplyTransformDataflowNode, "MultiplyTransform", "Math|Transform", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "Left Transform"));
	FTransform InLeftTransform = FTransform::Identity;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Right Transform"));
	FTransform InRightTransform = FTransform::Identity;

	UPROPERTY(meta = ( DataflowOutput, DisplayName = "Out Transform"));
	FTransform OutTransform = FTransform::Identity;

	FMultiplyTransformDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InLeftTransform);
		RegisterInputConnection(&InRightTransform);
		RegisterOutputConnection(&OutTransform);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Invert a transform.
 *
 */
USTRUCT()
struct FInvertTransformDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FInvertTransformDataflowNode, "InvertTransform", "Math|Transform", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "InTransform"));
	FTransform InTransform = FTransform::Identity;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "OutTransform"));
	FTransform OutTransform = FTransform::Identity;

	FInvertTransformDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InTransform);
		RegisterOutputConnection(&OutTransform);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT()
struct FUnionIntArraysDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
		DATAFLOW_NODE_DEFINE_INTERNAL(FUnionIntArraysDataflowNode, "UnionIntArrays", "Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "InArray1"));
	TArray<int32> InArray1;

	UPROPERTY(meta = (DataflowInput, DisplayName = "InArray2"));
	TArray<int32> InArray2;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "OutArray", DataflowPassthrough = "InArray1"));
	TArray<int32> OutArray;

	FUnionIntArraysDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InArray1);
		RegisterInputConnection(&InArray2);
		RegisterOutputConnection(&OutArray, &InArray1);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 *
 *
 */
USTRUCT()
struct FMakeQuaternionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeQuaternionDataflowNode, "MakeQuaternion", "Math|Vector", "")

public:
	UPROPERTY(EditAnywhere, Category = "Quaternion ", meta = (DataflowInput));
	float X = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float Y = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float Z = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float W = float(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Quaternion"))
	FQuat Quaternion = FQuat(ForceInitToZero);

	FMakeQuaternionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&X);
		RegisterInputConnection(&Y);
		RegisterInputConnection(&Z);
		RegisterInputConnection(&W);
		RegisterOutputConnection(&Quaternion);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


namespace Dataflow
{
	void GeometryCollectionEngineNodes();
}
