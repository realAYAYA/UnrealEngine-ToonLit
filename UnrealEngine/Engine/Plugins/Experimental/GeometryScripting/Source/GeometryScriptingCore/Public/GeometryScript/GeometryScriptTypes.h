// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryBase.h"
#include "GeometryScriptTypes.generated.h"

PREDECLARE_GEOMETRY(class FDynamicMesh3);
PREDECLARE_GEOMETRY(template<typename MeshType> class TMeshAABBTree3);
PREDECLARE_GEOMETRY(template<typename MeshType> class TFastWindingTree);
PREDECLARE_GEOMETRY(typedef TMeshAABBTree3<FDynamicMesh3> FDynamicMeshAABBTree3);


UENUM(BlueprintType)
enum EGeometryScriptOutcomePins
{
	Failure,
	Success
};

UENUM(BlueprintType)
enum EGeometryScriptSearchOutcomePins
{
	Found,
	NotFound
};

UENUM(BlueprintType)
enum EGeometryScriptContainmentOutcomePins
{
	Inside, 
	Outside
};


UENUM(BlueprintType)
enum class EGeometryScriptLODType : uint8
{
	MaxAvailable,
	HiResSourceModel,
	SourceModel,
	RenderData
};


UENUM(BlueprintType)
enum class EGeometryScriptAxis : uint8
{
	X = 0,
	Y = 1,
	Z = 2
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMeshReadLOD
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = LOD)
	EGeometryScriptLODType LODType = EGeometryScriptLODType::MaxAvailable;

	UPROPERTY(BlueprintReadWrite, Category = LOD)
	int32 LODIndex = 0;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMeshWriteLOD
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = LOD)
	bool bWriteHiResSource = false;

	UPROPERTY(BlueprintReadWrite, Category = LOD)
	int32 LODIndex = 0;
};





//
// Triangles
//

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptTriangle
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector Vector0 = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector Vector1 = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector Vector2 = FVector::ZeroVector;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptTrianglePoint
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Options)
	bool bValid = false;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	int TriangleID = -1;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector BaryCoords = FVector::ZeroVector;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptUVTriangle
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector2D UV0 = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector2D UV1 = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FVector2D UV2 = FVector2D::ZeroVector;
};




//
// Colors
//

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptColorFlags
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Color)
	bool bRed = true;

	UPROPERTY(BlueprintReadWrite, Category = Color)
	bool bGreen = true;

	UPROPERTY(BlueprintReadWrite, Category = Color)
	bool bBlue = true;

	UPROPERTY(BlueprintReadWrite, Category = Color)
	bool bAlpha = true;

	bool AllSet() const { return bRed && bGreen && bBlue && bAlpha; }
};



//
// Polygroups
//

/**
 * FGeometryScriptGroupLayer identifies a Polygroup Layer of a Mesh.
 * The Default Layer always exists, Extended layers may or may not exist.
 */
USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptGroupLayer
{
	GENERATED_BODY()
public:
	/** If true,the default/standard PolyGroup Layer is used */
	UPROPERTY(BlueprintReadWrite, Category = LOD)
	bool bDefaultLayer = true;

	/** Index of an extended PolyGroup Layer (which may or may not exist on any given Mesh) */
	UPROPERTY(BlueprintReadWrite, Category = LOD)
	int ExtendedLayerIndex = 0;
};


//
// List Types
//

// By default structs exposed to Python will use a per-UPROPERTY comparison. When this doesn't give correct results
// (e.g., for structs with no properties, which will compare equal in all cases because there are no properties to
// compare), it is necessary to define explicity equality operators and add the following WithIdenticalViaEquality trait.
// Note that users can write blueprints/python scripts which pass these lists to many function calls so we would like to
// avoid copying them (very slow if they have millions of elements) so we defined the equality operations using pointer
// equality but this is potentially confusing if users expect that different lists with the same elements compare equal

UENUM(BlueprintType)
enum class EGeometryScriptIndexType : uint8
{
	// Index lists of Any type are compatible with any other index list type
	Any,
	Triangle,
	Vertex,
	MaterialID,
	PolygroupID
};

USTRUCT(BlueprintType, meta = (DisplayName = "Index List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptIndexList
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = IndexList)
	EGeometryScriptIndexType IndexType = EGeometryScriptIndexType::Any;
public:
	TSharedPtr<TArray<int>> List;

	void Reset(EGeometryScriptIndexType TargetIndexType)
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<int>>();
		}
		List->Reset();
		IndexType = TargetIndexType;
	}

	bool IsCompatibleWith(EGeometryScriptIndexType OtherType) const
	{
		return IndexType == OtherType || IndexType == EGeometryScriptIndexType::Any;
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptIndexList& Other) const
	{
		return IndexType == Other.IndexType && List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptIndexList& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptIndexList> : public TStructOpsTypeTraitsBase2<FGeometryScriptIndexList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};


USTRUCT(BlueprintType, meta = (DisplayName = "Triangle List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptTriangleList
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<FIntVector>> List;

	void Reset()
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<FIntVector>>();
		}
		List->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptTriangleList& Other) const
	{
		return List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptTriangleList& Other) const
	{
		return List.Get() != Other.List.Get(); 
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptTriangleList> : public TStructOpsTypeTraitsBase2<FGeometryScriptTriangleList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};


USTRUCT(BlueprintType, meta = (DisplayName = "Scalar List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptScalarList
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<double>> List;

	void Reset()
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<double>>();
		}
		List->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptScalarList& Other) const
	{
		return List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptScalarList& Other) const
	{
		return List.Get() != Other.List.Get(); 
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptScalarList> : public TStructOpsTypeTraitsBase2<FGeometryScriptScalarList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};



USTRUCT(BlueprintType, meta = (DisplayName = "Vector List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptVectorList
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<FVector>> List;

	void Reset()
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<FVector>>();
		}
		List->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptVectorList& Other) const
	{
		return List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptVectorList& Other) const
	{
		return List.Get() != Other.List.Get(); 
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptVectorList> : public TStructOpsTypeTraitsBase2<FGeometryScriptVectorList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};


USTRUCT(BlueprintType, meta = (DisplayName = "UV List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptUVList
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<FVector2D>> List;

	void Reset()
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<FVector2D>>();
		}
		List->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptUVList& Other) const
	{
		return List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptUVList& Other) const
	{
		return List.Get() != Other.List.Get(); 
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptUVList> : public TStructOpsTypeTraitsBase2<FGeometryScriptUVList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};



USTRUCT(BlueprintType, meta = (DisplayName = "Color List"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptColorList
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<FLinearColor>> List;

	void Reset()
	{
		if (List.IsValid() == false)
		{
			List = MakeShared<TArray<FLinearColor>>();
		}
		List->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptColorList& Other) const
	{
		return List.Get() == Other.List.Get(); 
	}
	bool operator!=(const FGeometryScriptColorList& Other) const
	{
		return List.Get() != Other.List.Get(); 
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptColorList> : public TStructOpsTypeTraitsBase2<FGeometryScriptColorList>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};


USTRUCT(BlueprintType, meta = (DisplayName = "PolyPath"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPolyPath
{
	GENERATED_BODY()
public:
	TSharedPtr<TArray<FVector>> Path;

	UPROPERTY(EditAnywhere, Category = "Options")
	bool bClosedLoop = false;

	void Reset()
	{
		if (!Path.IsValid())
		{
			Path = MakeShared<TArray<FVector>>();
		}
		Path->Reset();
	}

	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptPolyPath& Other) const
	{
		return bClosedLoop == Other.bClosedLoop && Path.Get() == Other.Path.Get();
	}
	bool operator!=(const FGeometryScriptPolyPath& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptPolyPath> : public TStructOpsTypeTraitsBase2<FGeometryScriptPolyPath>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};




//
// Spatial data structures
//


USTRUCT(BlueprintType, meta = (DisplayName = "DynamicMesh BVH Cache"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptDynamicMeshBVH
{
	GENERATED_BODY()
public:

	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3> Spatial;
	TSharedPtr<UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3>> FWNTree;
	
	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGeometryScriptDynamicMeshBVH& Other) const
	{
		return Spatial.Get() == Other.Spatial.Get() && FWNTree.Get() == Other.FWNTree.Get();
	}
	bool operator!=(const FGeometryScriptDynamicMeshBVH& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FGeometryScriptDynamicMeshBVH> : public TStructOpsTypeTraitsBase2<FGeometryScriptDynamicMeshBVH>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};




//
// Errors/Debugging
//



UENUM(BlueprintType)
enum class EGeometryScriptDebugMessageType : uint8
{
	ErrorMessage,
	WarningMessage
};


UENUM(BlueprintType)
enum class EGeometryScriptErrorType : uint8
{
	// warning: must only append members!
	NoError,
	UnknownError,
	InvalidInputs,
	OperationFailed
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptDebugMessage
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Options)
	EGeometryScriptDebugMessageType MessageType = EGeometryScriptDebugMessageType::ErrorMessage;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	EGeometryScriptErrorType ErrorType = EGeometryScriptErrorType::UnknownError;

	UPROPERTY(BlueprintReadWrite, Category = Options)
	FText Message = FText();
};


UCLASS(BlueprintType, meta = (TestMetadata))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptDebug : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Messages)
	TArray<FGeometryScriptDebugMessage> Messages;


	void Append(const FGeometryScriptDebugMessage& MessageIn)
	{
		Messages.Add(MessageIn);
	}
};



namespace UE
{
namespace Geometry
{
	GEOMETRYSCRIPTINGCORE_API FGeometryScriptDebugMessage MakeScriptError(EGeometryScriptErrorType ErrorTypeIn, const FText& MessageIn);
	GEOMETRYSCRIPTINGCORE_API FGeometryScriptDebugMessage MakeScriptWarning(EGeometryScriptErrorType WarningTypeIn, const FText& MessageIn);


	GEOMETRYSCRIPTINGCORE_API void AppendError(UGeometryScriptDebug* Debug, EGeometryScriptErrorType ErrorTypeIn, const FText& MessageIn);
	GEOMETRYSCRIPTINGCORE_API void AppendWarning(UGeometryScriptDebug* Debug, EGeometryScriptErrorType WarningTypeIn, const FText& MessageIn);

	/**
	 * These variants of AppendError/Warning are for direct write to a debug message array.
	 * This may be useful in cases where a function is async and needs to accumulate
	 * debug messages for later collation on a game thread.
	 */
	GEOMETRYSCRIPTINGCORE_API void AppendError(TArray<FGeometryScriptDebugMessage>* DebugMessages, EGeometryScriptErrorType ErrorTypeIn, const FText& MessageIn);
	GEOMETRYSCRIPTINGCORE_API void AppendWarning(TArray<FGeometryScriptDebugMessage>* DebugMessages, EGeometryScriptErrorType WarningTypeIn, const FText& MessageIn);
}
}