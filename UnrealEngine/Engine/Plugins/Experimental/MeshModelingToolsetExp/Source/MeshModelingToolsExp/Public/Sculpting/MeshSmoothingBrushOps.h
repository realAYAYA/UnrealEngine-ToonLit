// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sculpting/MeshBrushOpBase.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MeshWeights.h"
#include "DynamicMesh/MeshNormals.h"
#include "Async/ParallelFor.h"
#include "MeshSmoothingBrushOps.generated.h"


UCLASS()
class MESHMODELINGTOOLSEXP_API UBaseSmoothBrushOpProps : public UMeshSculptBrushOpProps
{
	GENERATED_BODY()
public:
	virtual bool GetPreserveUVFlow() { return false; }
};


UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothBrushOpProps : public UBaseSmoothBrushOpProps
{
	GENERATED_BODY()
public:
	/** Strength of the Brush */
	UPROPERTY(EditAnywhere, Category = SmoothBrush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.5;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = SmoothBrush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5;

	/** If true, try to preserve the shape of the UV/3D mapping. This will limit Smoothing and Remeshing in some cases. */
	UPROPERTY(EditAnywhere, Category = SmoothBrush)
	bool bPreserveUVFlow = false;

	virtual float GetStrength() override { return Strength; }
	virtual void SetStrength(float NewStrength) override { Strength = FMathf::Clamp(NewStrength, 0.0f, 1.0f); }
	virtual float GetFalloff() override { return Falloff; }
	virtual bool GetPreserveUVFlow() override { return bPreserveUVFlow; }
};



UCLASS()
class MESHMODELINGTOOLSEXP_API USecondarySmoothBrushOpProps : public UBaseSmoothBrushOpProps
{
	GENERATED_BODY()
public:
	/** Strength of the Brush */
	UPROPERTY(EditAnywhere, Category = ShiftSmoothBrush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.5;

	/** Amount of falloff to apply */
	UPROPERTY(EditAnywhere, Category = ShiftSmoothBrush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5;

	/** If true, try to preserve the shape of the UV/3D mapping. This will limit Smoothing and Remeshing in some cases. */
	UPROPERTY(EditAnywhere, Category = ShiftSmoothBrush)
	bool bPreserveUVFlow = false;

	virtual float GetStrength() override { return Strength; }
	virtual void SetStrength(float NewStrength) override { Strength = FMathf::Clamp(NewStrength, 0.0f, 1.0f); }
	virtual float GetFalloff() override { return Falloff; }
	virtual bool GetPreserveUVFlow() override { return bPreserveUVFlow; }
};



class FSmoothBrushOp : public FMeshSculptBrushOp
{

public:

	virtual void ApplyStamp(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& Vertices, TArray<FVector3d>& NewPositionsOut) override
	{
		const FVector3d& StampPos = Stamp.LocalFrame.Origin;
		bool bPreserveUVFlow = GetPropertySetAs<UBaseSmoothBrushOpProps>()->GetPreserveUVFlow();

		ParallelFor(Vertices.Num(), [&](int32 k)
		{
			int32 VertIdx = Vertices[k];

			FVector3d OrigPos = Mesh->GetVertex(VertIdx);

			double Falloff = GetFalloff().Evaluate(Stamp, OrigPos);

			FVector3d SmoothedPos = OrigPos;
			if (bPreserveUVFlow)
			{
				SmoothedPos = UE::Geometry::FMeshWeights::CotanCentroidSafe(*Mesh, VertIdx, 10.0);
			}
			else
			{
				SmoothedPos = UE::Geometry::FMeshWeights::UniformCentroid(*Mesh, VertIdx);
			}

			FVector3d NewPos = UE::Geometry::Lerp(OrigPos, SmoothedPos, Falloff * Stamp.Power);

			NewPositionsOut[k] = NewPos;
		});
	}
};





UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothFillBrushOpProps : public UBaseSmoothBrushOpProps
{
	GENERATED_BODY()
public:
	/** Strength of the Brush */
	UPROPERTY(EditAnywhere, Category = SmoothFillBrush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.5;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = SmoothFillBrush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5;

	/** If true, try to preserve the shape of the UV/3D mapping. This will limit Smoothing and Remeshing in some cases. */
	UPROPERTY(EditAnywhere, Category = SmoothFillBrush)
	bool bPreserveUVFlow = false;

	virtual float GetStrength() override { return Strength; }
	virtual void SetStrength(float NewStrength) override { Strength = FMathf::Clamp(NewStrength, 0.0f, 1.0f); }
	virtual float GetFalloff() override { return Falloff; }
	virtual bool GetPreserveUVFlow() override { return bPreserveUVFlow; }
};



class FSmoothFillBrushOp : public FMeshSculptBrushOp
{

public:

	virtual void ApplyStamp(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& Vertices, TArray<FVector3d>& NewPositionsOut) override
	{
		const FVector3d& StampPos = Stamp.LocalFrame.Origin;
		double Direction = Stamp.Direction;
		bool bPreserveUVFlow = GetPropertySetAs<UBaseSmoothBrushOpProps>()->GetPreserveUVFlow();

		ParallelFor(Vertices.Num(), [&](int32 k)
		{
			int32 VertIdx = Vertices[k];

			FVector3d OrigPos = Mesh->GetVertex(VertIdx);
			FVector3d Normal = UE::Geometry::FMeshNormals::ComputeVertexNormal(*Mesh, VertIdx);

			double Falloff = GetFalloff().Evaluate(Stamp, OrigPos);

			FVector3d SmoothedPos = OrigPos;
			if (bPreserveUVFlow)
			{
				SmoothedPos = UE::Geometry::FMeshWeights::CotanCentroidSafe(*Mesh, VertIdx, 10.0);
			}
			else
			{
				SmoothedPos = UE::Geometry::FMeshWeights::UniformCentroid(*Mesh, VertIdx);
			}

			FVector3d NewPos = UE::Geometry::Lerp(OrigPos, SmoothedPos, Falloff * Stamp.Power);

			if ((NewPos - OrigPos).Dot(Normal) * Direction < 0)
			{
				NewPositionsOut[k] = OrigPos;
			}
			else
			{
				NewPositionsOut[k] = NewPos;
			}
		});
	}
};





UCLASS()
class MESHMODELINGTOOLSEXP_API UFlattenBrushOpProps : public UMeshSculptBrushOpProps
{
	GENERATED_BODY()
public:
	/** Strength of the Brush */
	UPROPERTY(EditAnywhere, Category = FlattenBrush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.5;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = FlattenBrush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5;

	/** Depth of Brush into surface along surface normal */
	UPROPERTY(EditAnywhere, Category = FlattenBrush, meta = (UIMin = "-0.5", UIMax = "0.5", ClampMin = "-1.0", ClampMax = "1.0"))
	float Depth = 0;

	/** Control whether effect of brush should be limited to one side of the Plane  */
	UPROPERTY(EditAnywhere, Category = FlattenBrush)
	EPlaneBrushSideMode WhichSide = EPlaneBrushSideMode::BothSides;

	virtual float GetStrength() override { return Strength; }
	virtual void SetStrength(float NewStrength) override { Strength = FMathf::Clamp(NewStrength, 0.0f, 1.0f); }
	virtual float GetFalloff() override { return Falloff; }
	virtual float GetDepth() override { return Depth; }
};


class FFlattenBrushOp : public FMeshSculptBrushOp
{

public:
	double BrushSpeedTuning = 1.0;

	virtual void ApplyStamp(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& Vertices, TArray<FVector3d>& NewPositionsOut) override
	{
		static const double PlaneSigns[3] = { 0, -1, 1 };
		double PlaneSign = PlaneSigns[(int32)GetPropertySetAs<UFlattenBrushOpProps>()->WhichSide];

		const FVector3d& StampPos = Stamp.LocalFrame.Origin;

		double UseSpeed = Stamp.Power * Stamp.Radius * Stamp.DeltaTime * BrushSpeedTuning;
		const FFrame3d& FlattenPlane = Stamp.RegionPlane;
		FVector3d PlaneZ = FlattenPlane.Z();

		ParallelFor(Vertices.Num(), [&](int32 k)
		{
			int32 VertIdx = Vertices[k];
			FVector3d OrigPos = Mesh->GetVertex(VertIdx);
			FVector3d Normal = UE::Geometry::FMeshNormals::ComputeVertexNormal(*Mesh, VertIdx);
			FVector3d PlanePos = FlattenPlane.ToPlane(OrigPos, 2);
			FVector3d MoveDelta = PlanePos - OrigPos;

			double PlaneDot = MoveDelta.Dot(FlattenPlane.Z());
			FVector3d NewPos = OrigPos;
			if (PlaneDot * PlaneSign >= 0)
			{
				double Falloff = GetFalloff().Evaluate(Stamp, OrigPos);
				FVector3d MoveVec = Falloff * UseSpeed * MoveDelta;
				double MaxDist = UE::Geometry::Normalize(MoveDelta);
				NewPos = (MoveVec.SquaredLength() > MaxDist * MaxDist) ?
					PlanePos : (OrigPos + Falloff * MoveVec);
			}

			NewPositionsOut[k] = NewPos;
		});
	}


	virtual bool WantsStampRegionPlane() const
	{
		return true;
	}
};






UCLASS()
class MESHMODELINGTOOLSEXP_API UEraseBrushOpProps : public UMeshSculptBrushOpProps
{
	GENERATED_BODY()
public:
	/** Strength of the Brush */
	UPROPERTY(EditAnywhere, Category = EraseBrush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.5;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = EraseBrush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5;

	virtual float GetStrength() override { return Strength; }
	virtual void SetStrength(float NewStrength) override { Strength = FMathf::Clamp(NewStrength, 0.0f, 1.0f); }
	virtual float GetFalloff() override { return Falloff; }
};


class FEraseToBaseMeshBrushOp : public FMeshSculptBrushOp
{
public:
	double BrushSpeedTuning = 3.0;

	typedef TUniqueFunction<bool(int32, const FVector3d&, double, FVector3d&, FVector3d&)> NearestQueryFuncType;

	NearestQueryFuncType BaseMeshNearestQueryFunc;

	FEraseToBaseMeshBrushOp(NearestQueryFuncType QueryFunc)
	{
		BaseMeshNearestQueryFunc = MoveTemp(QueryFunc);
	}

	virtual ESculptBrushOpTargetType GetBrushTargetType() const override
	{
		return ESculptBrushOpTargetType::TargetMesh;
	}

	virtual void ApplyStamp(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& Vertices, TArray<FVector3d>& NewPositionsOut) override
	{
		double UsePower = Stamp.Power * Stamp.Radius * Stamp.DeltaTime * BrushSpeedTuning;
		double MaxOffset = Stamp.Radius;

		ParallelFor(Vertices.Num(), [&](int32 k)
		{
			int32 VertIdx = Vertices[k];
			FVector3d OrigPos = Mesh->GetVertex(VertIdx);

			FVector3d BasePos, BaseNormal;
			bool bFoundBasePos = BaseMeshNearestQueryFunc(VertIdx, OrigPos, 4.0 * Stamp.Radius, BasePos, BaseNormal);
			if (bFoundBasePos == false)
			{
				NewPositionsOut[k] = OrigPos;
			}
			else
			{
				FVector3d MoveVec = (BasePos - OrigPos); 
				double Falloff = GetFalloff().Evaluate(Stamp, OrigPos);
				double MoveDist = Falloff * UsePower;
				if (MoveVec.SquaredLength() < MoveDist * MoveDist)
				{
					NewPositionsOut[k] = BasePos;
				}
				else
				{
					UE::Geometry::Normalize(MoveVec);
					NewPositionsOut[k] = OrigPos + MoveDist * MoveVec;
				}
			}
		});
	}

};
