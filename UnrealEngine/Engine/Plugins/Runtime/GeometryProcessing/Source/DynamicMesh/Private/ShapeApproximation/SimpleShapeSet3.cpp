// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShapeApproximation/SimpleShapeSet3.h"

#include "MeshQueries.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Intersection/ContainmentQueries3.h"

using namespace UE::Geometry;


// FSimpleShapeElementKey identifies an element of a FSimpleShapeSet3d, via the Type
// of that element, and the Index into the per-element-type arrays.
struct FSimpleShapeElementKey
{
	// type of element
	ESimpleShapeType Type;
	// index of this element in per-type element lists inside a FSimpleShapeSet3d
	int32 Index;

	// volume of the element, used for sorting/etc
	double Volume;

	FSimpleShapeElementKey() = default;

	FSimpleShapeElementKey(ESimpleShapeType TypeIn, int32 IndexIn, double VolumeIn)
		: Type(TypeIn), Index(IndexIn), Volume(VolumeIn)
	{}
};



static void FilterContained(const FSimpleShapeSet3d& Geometry, const FSphereShape3d& Sphere, const TArray<FSimpleShapeElementKey>& Elements, int32 k, TArray<bool>& RemovedInOut)
{
	int32 N = Elements.Num();
	for (int32 j = k + 1; j < N; ++j)
	{
		if (RemovedInOut[j] == false)
		{
			bool bContained = false;
			int32 ElemIdx = Elements[j].Index;
			if (Elements[j].Type == ESimpleShapeType::Sphere)
			{
				bContained = UE::Geometry::IsInside<double>(Sphere.Sphere, Geometry.Spheres[ElemIdx].Sphere);
			}
			else if (Elements[j].Type == ESimpleShapeType::Box)
			{
				bContained = UE::Geometry::IsInside<double>(Sphere.Sphere, Geometry.Boxes[ElemIdx].Box);
			}
			else if (Elements[j].Type == ESimpleShapeType::Capsule)
			{
				bContained = UE::Geometry::IsInside<double>(Sphere.Sphere, Geometry.Capsules[ElemIdx].Capsule);
			}
			else if (Elements[j].Type == ESimpleShapeType::Convex)
			{
				bContained = UE::Geometry::IsInside(Sphere.Sphere, Geometry.Convexes[ElemIdx].Mesh.VerticesItr());
			}
			else if (Elements[j].Type == ESimpleShapeType::LevelSet)
			{
				bContained = UE::Geometry::IsInside(Sphere.Sphere, Geometry.LevelSets[ElemIdx].GetGridBox());
			}
			else
			{
				ensure(false);		// not implemented yet?
			}

			if (bContained)
			{
				RemovedInOut[j] = true;
			}
		}
	}
}




static void FilterContained(const FSimpleShapeSet3d& Geometry, const UE::Geometry::FCapsuleShape3d& Capsule, const TArray<FSimpleShapeElementKey>& Elements, int32 k, TArray<bool>& RemovedInOut)
{
	int32 N = Elements.Num();
	for (int32 j = k + 1; j < N; ++j)
	{
		if (RemovedInOut[j] == false)
		{
			bool bContained = false;
			int32 ElemIdx = Elements[j].Index;
			if (Elements[j].Type == ESimpleShapeType::Sphere)
			{
				bContained = UE::Geometry::IsInside<double>(Capsule.Capsule, Geometry.Spheres[ElemIdx].Sphere);
			}
			else if (Elements[j].Type == ESimpleShapeType::Box)
			{
				bContained = UE::Geometry::IsInside<double>(Capsule.Capsule, Geometry.Boxes[ElemIdx].Box);
			}
			else if (Elements[j].Type == ESimpleShapeType::Capsule)
			{
				bContained = UE::Geometry::IsInside<double>(Capsule.Capsule, Geometry.Capsules[ElemIdx].Capsule);
			}
			else if (Elements[j].Type == ESimpleShapeType::Convex)
			{
				bContained = UE::Geometry::IsInside(Capsule.Capsule, Geometry.Convexes[ElemIdx].Mesh.VerticesItr());
			}
			else if (Elements[j].Type == ESimpleShapeType::LevelSet)
			{
				bContained = UE::Geometry::IsInside(Capsule.Capsule, Geometry.LevelSets[ElemIdx].GetGridBox());
			}
			else
			{
				ensure(false);
			}

			if (bContained)
			{
				RemovedInOut[j] = true;
			}
		}
	}
}




static void FilterContained(const FSimpleShapeSet3d& Geometry, const FBoxShape3d& Box, const TArray<FSimpleShapeElementKey>& Elements, int32 k, TArray<bool>& RemovedInOut)
{
	int32 N = Elements.Num();
	for (int32 j = k + 1; j < N; ++j)
	{
		if (RemovedInOut[j] == false)
		{
			bool bContained = false;
			int32 ElemIdx = Elements[j].Index;
			if (Elements[j].Type == ESimpleShapeType::Sphere)
			{
				bContained = UE::Geometry::IsInside<double>(Box.Box, Geometry.Spheres[ElemIdx].Sphere);
			}
			else if (Elements[j].Type == ESimpleShapeType::Box)
			{
				bContained = UE::Geometry::IsInside<double>(Box.Box, Geometry.Boxes[ElemIdx].Box);
			}
			else if (Elements[j].Type == ESimpleShapeType::Capsule)
			{
				bContained = UE::Geometry::IsInside<double>(Box.Box, Geometry.Capsules[ElemIdx].Capsule);
			}
			else if (Elements[j].Type == ESimpleShapeType::Convex)
			{
				bContained = UE::Geometry::IsInside(Box.Box, Geometry.Convexes[ElemIdx].Mesh.VerticesItr());
			}
			else if (Elements[j].Type == ESimpleShapeType::LevelSet)
			{
				bContained = UE::Geometry::IsInside(Box.Box, Geometry.LevelSets[ElemIdx].GetGridBox());
			}
			else
			{
				ensure(false);
			}

			if (bContained)
			{
				RemovedInOut[j] = true;
			}
		}
	}
}





static void FilterContained(const FSimpleShapeSet3d& Geometry, const FConvexShape3d& Convex, const TArray<FSimpleShapeElementKey>& Elements, int32 k, TArray<bool>& RemovedInOut)
{
	TArray<FHalfspace3d> Planes;
	for (int32 tid : Convex.Mesh.TriangleIndicesItr())
	{
		FVector3d Normal, Centroid; double Area;
		Convex.Mesh.GetTriInfo(tid, Normal, Area, Centroid);
		Planes.Add(FHalfspace3d(Normal, Centroid));
	}

	int32 N = Elements.Num();
	for (int32 j = k + 1; j < N; ++j)
	{
		if (RemovedInOut[j] == false)
		{
			bool bContained = false;
			int32 ElemIdx = Elements[j].Index;
			if (Elements[j].Type == ESimpleShapeType::Sphere)
			{
				bContained = UE::Geometry::IsInsideHull<double>(Planes, Geometry.Spheres[ElemIdx].Sphere);
			}
			else if (Elements[j].Type == ESimpleShapeType::Box)
			{
				bContained = UE::Geometry::IsInsideHull<double>(Planes, Geometry.Boxes[ElemIdx].Box);
			}
			else if (Elements[j].Type == ESimpleShapeType::Capsule)
			{
				bContained = UE::Geometry::IsInsideHull<double>(Planes, Geometry.Capsules[ElemIdx].Capsule);
			}
			else if (Elements[j].Type == ESimpleShapeType::Convex)
			{
				bContained = UE::Geometry::IsInsideHull<double>(Planes, Geometry.Convexes[ElemIdx].Mesh.VerticesItr());
			}
			else if (Elements[j].Type == ESimpleShapeType::LevelSet)
			{
				bContained = UE::Geometry::IsInsideHull<double>(Planes, Geometry.LevelSets[ElemIdx].GetGridBox());
			}
			else
			{
				ensure(false);
			}

			if (bContained)
			{
				RemovedInOut[j] = true;
			}
		}
	}
}


static void FilterContained(const FSimpleShapeSet3d& Geometry, FLevelSetShape3d& LevelSetShape, const TArray<FSimpleShapeElementKey>& Elements, int32 k, TArray<bool>& RemovedInOut)
{
	TTriLinearGridInterpolant<FDenseGrid3f> GridInterp(&LevelSetShape.Grid, FVector3d::ZeroVector, LevelSetShape.CellSize, LevelSetShape.Grid.GetDimensions());

	int32 N = Elements.Num();
	for (int32 j = k + 1; j < N; ++j)
	{
		if (RemovedInOut[j] == false)
		{
			bool bContained = false;
			int32 ElemIdx = Elements[j].Index;
			if (Elements[j].Type == ESimpleShapeType::Sphere)
			{
				bContained = UE::Geometry::IsInside(GridInterp, LevelSetShape.GridTransform, Geometry.Spheres[ElemIdx].Sphere);
			}
			else if (Elements[j].Type == ESimpleShapeType::Box)
			{
				bContained = UE::Geometry::IsInside(GridInterp, LevelSetShape.GridTransform, Geometry.Boxes[ElemIdx].Box);
			}
			else if (Elements[j].Type == ESimpleShapeType::Capsule)
			{
				bContained = UE::Geometry::IsInside(GridInterp, LevelSetShape.GridTransform, Geometry.Capsules[ElemIdx].Capsule);
			}
			else if (Elements[j].Type == ESimpleShapeType::Convex)
			{
				bContained = UE::Geometry::IsInside<double>(GridInterp, LevelSetShape.GridTransform, Geometry.Convexes[ElemIdx].Mesh.VerticesItr());
			}
			else if (Elements[j].Type == ESimpleShapeType::LevelSet)
			{
				bContained = UE::Geometry::IsInside(GridInterp, LevelSetShape.GridTransform, Geometry.LevelSets[ElemIdx].GetGridBox());
			}
			else
			{
				ensure(false);
			}

			if (bContained)
			{
				RemovedInOut[j] = true;
			}
		}
	}
}


static void GetElementsList(FSimpleShapeSet3d& GeometrySet, TArray<FSimpleShapeElementKey>& Elements)
{
	for (int32 k = 0; k < GeometrySet.Spheres.Num(); ++k)
	{
		Elements.Add(FSimpleShapeElementKey(ESimpleShapeType::Sphere, k, GeometrySet.Spheres[k].Sphere.Volume()));
	}
	for (int32 k = 0; k < GeometrySet.Boxes.Num(); ++k)
	{
		Elements.Add(FSimpleShapeElementKey(ESimpleShapeType::Box, k, GeometrySet.Boxes[k].Box.Volume()));
	}
	for (int32 k = 0; k < GeometrySet.Capsules.Num(); ++k)
	{
		Elements.Add(FSimpleShapeElementKey(ESimpleShapeType::Capsule, k, GeometrySet.Capsules[k].Capsule.Volume()));
	}
	for (int32 k = 0; k < GeometrySet.Convexes.Num(); ++k)
	{
		FVector2d VolArea = TMeshQueries<FDynamicMesh3>::GetVolumeArea(GeometrySet.Convexes[k].Mesh);
		Elements.Add(FSimpleShapeElementKey(ESimpleShapeType::Convex, k, VolArea.X));
	}
	for (int32 k = 0; k < GeometrySet.LevelSets.Num(); ++k)
	{
		Elements.Add(FSimpleShapeElementKey(ESimpleShapeType::LevelSet, k, GeometrySet.LevelSets[k].GetGridBox().Volume()));
	}

}

static void GetElementsSortedByDecreasing(FSimpleShapeSet3d& GeometrySet, TArray<FSimpleShapeElementKey>& Elements)
{
	GetElementsList(GeometrySet, Elements);
	// sort by decreasing volume
	Elements.Sort([](const FSimpleShapeElementKey& A, const FSimpleShapeElementKey& B) { return A.Volume > B.Volume; });
}


void FSimpleShapeSet3d::RemoveContainedGeometry()
{
	TArray<FSimpleShapeElementKey> Elements;
	GetElementsSortedByDecreasing(*this, Elements);

	int32 N = Elements.Num();
	TArray<bool> Removed;
	Removed.Init(false, N);

	// remove contained elements
	for (int32 k = 0; k < N; ++k)
	{
		if (Removed[k]) continue;

		ESimpleShapeType ElemType = Elements[k].Type;
		int32 ElemIdx = (int32)Elements[k].Index;

		if (ElemType == ESimpleShapeType::Sphere)
		{
			FilterContained(*this, Spheres[ElemIdx], Elements, k, Removed);
		}
		else if (ElemType == ESimpleShapeType::Capsule)
		{
			FilterContained(*this, Capsules[ElemIdx], Elements, k, Removed);
		}
		else if (ElemType == ESimpleShapeType::Box)
		{
			FilterContained(*this, Boxes[ElemIdx], Elements, k, Removed);
		}
		else if (ElemType == ESimpleShapeType::Convex)
		{
			FilterContained(*this, Convexes[ElemIdx], Elements, k, Removed);
		}
		else if (ElemType == ESimpleShapeType::LevelSet)
		{
			// Pass in zero here instead of k because we don't currently have a good volume value for level set objects
			FilterContained(*this, LevelSets[ElemIdx], Elements, 0, Removed);
		}
		else
		{
			ensure(false);
		}
	}


	// build a new shape set
	FSimpleShapeSet3d NewSet;
	for (int32 k = 0; k < N; ++k)
	{
		if (Removed[k] == false)
		{
			ESimpleShapeType ElemType = Elements[k].Type;
			int32 ElemIdx = Elements[k].Index;

			switch (ElemType)
			{
			case ESimpleShapeType::Sphere:
				NewSet.Spheres.Add(Spheres[ElemIdx]);
				break;
			case ESimpleShapeType::Box:
				NewSet.Boxes.Add(Boxes[ElemIdx]);
				break;
			case ESimpleShapeType::Capsule:
				NewSet.Capsules.Add(Capsules[ElemIdx]);
				break;
			case ESimpleShapeType::Convex:
				NewSet.Convexes.Add(Convexes[ElemIdx]);		// todo movetemp here...
				break;
			case ESimpleShapeType::LevelSet:
				NewSet.LevelSets.Add(LevelSets[ElemIdx]);		// todo movetemp here...
				break;
			}
		}
	}

	// replace our lists with new set
	Spheres = MoveTemp(NewSet.Spheres);
	Boxes = MoveTemp(NewSet.Boxes);
	Capsules = MoveTemp(NewSet.Capsules);
	Convexes = MoveTemp(NewSet.Convexes);
	LevelSets = MoveTemp(NewSet.LevelSets);
}


static void TransformSphereShape(FSphereShape3d& SphereShape, const FTransform3d& Transform)
{
	double RadiusScale = Transform.GetScale3D().Length() / FVector3d::One().Length();
	SphereShape.Sphere.Center = Transform.TransformPosition(SphereShape.Sphere.Center);
	SphereShape.Sphere.Radius *= RadiusScale;
}
static void TransformBoxShape(FBoxShape3d& BoxShape, const FTransform3d& Transform)
{
	FVector3d CornerVec = BoxShape.Box.Frame.PointAt(BoxShape.Box.Extents) - BoxShape.Box.Frame.Origin;
	BoxShape.Box.Frame.Transform(Transform);
	CornerVec = Transform.TransformVector(CornerVec);
	BoxShape.Box.Extents.X = CornerVec.Dot(BoxShape.Box.AxisX());
	BoxShape.Box.Extents.Y = CornerVec.Dot(BoxShape.Box.AxisY());
	BoxShape.Box.Extents.Z = CornerVec.Dot(BoxShape.Box.AxisZ());
}
static void TransformCapsuleShape(UE::Geometry::FCapsuleShape3d& CapsuleShape, const FTransform3d& Transform)
{
	FVector3d P0 = Transform.TransformPosition(CapsuleShape.Capsule.Segment.StartPoint());
	FVector3d P1 = Transform.TransformPosition(CapsuleShape.Capsule.Segment.EndPoint());

	CapsuleShape.Capsule.Segment.Center = 0.5 * (P0 + P1);
	CapsuleShape.Capsule.Segment.Direction = (P1 - P0);
	CapsuleShape.Capsule.Segment.Extent = Normalize(CapsuleShape.Capsule.Segment.Direction) * 0.5;

	double CurRadius = CapsuleShape.Capsule.Radius;
	FFrame3d CapsuleFrame(CapsuleShape.Capsule.Segment.Center, CapsuleShape.Capsule.Segment.Direction);
	FVector3d SideVec = CapsuleFrame.PointAt(FVector3d(CurRadius, CurRadius, 0)) - CapsuleFrame.Origin;
	FVector3d NewSideVec = Transform.TransformVector(SideVec);
	double RadiusScale = NewSideVec.Length() / SideVec.Length();
	CapsuleShape.Capsule.Radius *= RadiusScale;
}

static void TransformLevelSetShape(UE::Geometry::FLevelSetShape3d& LevelSetShape, const FTransform3d& Transform)
{
	const FTransform3d Sub(-0.5 * LevelSetShape.CellSize * FVector3d::One());
	const FTransform3d Add(0.5 * LevelSetShape.CellSize * FVector3d::One());
	LevelSetShape.GridTransform = LevelSetShape.GridTransform * Sub * Transform * Add;
}



static void TransformSphereShape(FSphereShape3d& SphereShape, const TArray<FTransform3d>& TransformSequence)
{
	for (const FTransform3d& XForm : TransformSequence)
	{
		SphereShape.Sphere.Center = XForm.TransformPosition(SphereShape.Sphere.Center);
		double RadiusScale = XForm.GetScale3D().Length() / FVector3d::One().Length();
		SphereShape.Sphere.Radius *= RadiusScale;
	}
}
static void TransformBoxShape(FBoxShape3d& BoxShape, const TArray<FTransform3d>& TransformSequence)
{
	FVector3d CornerVec = BoxShape.Box.Frame.PointAt(BoxShape.Box.Extents) - BoxShape.Box.Frame.Origin;
	for (const FTransform3d& XForm : TransformSequence)
	{
		BoxShape.Box.Frame.Transform(XForm);
		CornerVec = XForm.TransformVector(CornerVec);
	}
	BoxShape.Box.Extents.X = CornerVec.Dot(BoxShape.Box.AxisX());
	BoxShape.Box.Extents.Y = CornerVec.Dot(BoxShape.Box.AxisY());
	BoxShape.Box.Extents.Z = CornerVec.Dot(BoxShape.Box.AxisZ());
}
static void TransformCapsuleShape(UE::Geometry::FCapsuleShape3d& CapsuleShape, const TArray<FTransform3d>& TransformSequence)
{
	FVector3d P0 = CapsuleShape.Capsule.Segment.StartPoint();
	FVector3d P1 = CapsuleShape.Capsule.Segment.EndPoint();

	double CurRadius = CapsuleShape.Capsule.Radius;
	FFrame3d CapsuleFrame(CapsuleShape.Capsule.Segment.Center, CapsuleShape.Capsule.Segment.Direction);
	FVector3d InitialSideVec = CapsuleFrame.PointAt(FVector3d(CurRadius, CurRadius, 0)) - CapsuleFrame.Origin;
	FVector3d NewSideVec = InitialSideVec;

	for (const FTransform3d& XForm : TransformSequence)
	{
		P0 = XForm.TransformPosition(P0);
		P1 = XForm.TransformPosition(P1);
		NewSideVec = XForm.TransformVector(NewSideVec);
	}

	CapsuleShape.Capsule.Segment.Center = 0.5 * (P0 + P1);
	CapsuleShape.Capsule.Segment.Direction = (P1 - P0);
	CapsuleShape.Capsule.Segment.Extent = Normalize(CapsuleShape.Capsule.Segment.Direction) * 0.5;
	double RadiusScale = NewSideVec.Length() / InitialSideVec.Length();
	CapsuleShape.Capsule.Radius *= RadiusScale;
}

static void TransformLevelSetShape(UE::Geometry::FLevelSetShape3d& LevelSetShape, const TArray<FTransform3d>& TransformSequence)
{
	const FTransform3d Sub(-0.5 * LevelSetShape.CellSize * FVector3d::One());
	const FTransform3d Add(0.5 * LevelSetShape.CellSize * FVector3d::One());

	LevelSetShape.GridTransform = LevelSetShape.GridTransform * Sub;

	for (const FTransform3d& XForm : TransformSequence)
	{
		LevelSetShape.GridTransform = LevelSetShape.GridTransform * XForm;
	}

	LevelSetShape.GridTransform = LevelSetShape.GridTransform * Add;
}


void FSimpleShapeSet3d::Append(const FSimpleShapeSet3d& OtherShapeSet)
{
	for (FSphereShape3d SphereShape : OtherShapeSet.Spheres)
	{
		Spheres.Add(SphereShape);
	}

	for (FBoxShape3d BoxShape : OtherShapeSet.Boxes)
	{
		Boxes.Add(BoxShape);
	}

	for (UE::Geometry::FCapsuleShape3d CapsuleShape : OtherShapeSet.Capsules)
	{
		Capsules.Add(CapsuleShape);
	}

	Convexes.Reserve(Convexes.Num() + OtherShapeSet.Convexes.Num());
	for (const FConvexShape3d& ConvexShape : OtherShapeSet.Convexes)
	{
		Convexes.Add(ConvexShape);
	}

	LevelSets.Reserve(LevelSets.Num() + OtherShapeSet.LevelSets.Num());
	for (const FLevelSetShape3d& LevelSetShape : OtherShapeSet.LevelSets)
	{
		LevelSets.Add(LevelSetShape);
	}
}


void FSimpleShapeSet3d::Append(const FSimpleShapeSet3d& OtherShapeSet, const FTransform3d& Transform)
{
	for (FSphereShape3d SphereShape : OtherShapeSet.Spheres)
	{
		TransformSphereShape(SphereShape, Transform);
		Spheres.Add(SphereShape);
	}

	for (FBoxShape3d BoxShape : OtherShapeSet.Boxes)
	{
		TransformBoxShape(BoxShape, Transform);
		Boxes.Add(BoxShape);
	}

	for (UE::Geometry::FCapsuleShape3d CapsuleShape : OtherShapeSet.Capsules)
	{
		TransformCapsuleShape(CapsuleShape, Transform);
		Capsules.Add(CapsuleShape);
	}

	Convexes.Reserve(Convexes.Num() + OtherShapeSet.Convexes.Num());
	for (const FConvexShape3d& ConvexShape : OtherShapeSet.Convexes)
	{
		Convexes.Add(ConvexShape);
		MeshTransforms::ApplyTransform(Convexes.Last().Mesh, Transform, true);
	}

	LevelSets.Reserve(LevelSets.Num() + OtherShapeSet.LevelSets.Num());
	for (const FLevelSetShape3d& LevelSetShape : OtherShapeSet.LevelSets)
	{
		LevelSets.Add(LevelSetShape);
		TransformLevelSetShape(LevelSets.Last(), Transform);
	}
}



void FSimpleShapeSet3d::Append(const FSimpleShapeSet3d& OtherShapeSet, const TArray<FTransform3d>& TransformSequence)
{
	for (FSphereShape3d SphereShape : OtherShapeSet.Spheres)
	{
		TransformSphereShape(SphereShape, TransformSequence);
		Spheres.Add(SphereShape);
	}

	for (FBoxShape3d BoxShape : OtherShapeSet.Boxes)
	{
		TransformBoxShape(BoxShape, TransformSequence);
		Boxes.Add(BoxShape);
	}

	for (UE::Geometry::FCapsuleShape3d CapsuleShape : OtherShapeSet.Capsules)
	{
		TransformCapsuleShape(CapsuleShape, TransformSequence);
		Capsules.Add(CapsuleShape);
	}

	Convexes.Reserve(Convexes.Num() + OtherShapeSet.Convexes.Num());
	for (const FConvexShape3d& ConvexShape : OtherShapeSet.Convexes)
	{
		Convexes.Add(ConvexShape);
		for (const FTransform3d& XForm : TransformSequence)
		{
			MeshTransforms::ApplyTransform(Convexes.Last().Mesh, XForm, true);
		}
	}

	LevelSets.Reserve(LevelSets.Num() + OtherShapeSet.LevelSets.Num());
	for (const FLevelSetShape3d& LevelSetShape : OtherShapeSet.LevelSets)
	{
		LevelSets.Add(LevelSetShape);
		TransformLevelSetShape(LevelSets.Last(), TransformSequence);
	}
}



void FSimpleShapeSet3d::FilterByVolume(int32 MaximumCount)
{
	TArray<FSimpleShapeElementKey> Elements;
	GetElementsSortedByDecreasing(*this, Elements);
	if (Elements.Num() <= MaximumCount)
	{
		return;
	}

	FSimpleShapeSet3d NewSet;
	for (int32 k = 0; k < MaximumCount; ++k)
	{
		ESimpleShapeType ElemType = Elements[k].Type;
		int32 ElemIdx = Elements[k].Index;

		switch (ElemType)
		{
		case ESimpleShapeType::Sphere:
			NewSet.Spheres.Add(Spheres[ElemIdx]);
			break;
		case ESimpleShapeType::Box:
			NewSet.Boxes.Add(Boxes[ElemIdx]);
			break;
		case ESimpleShapeType::Capsule:
			NewSet.Capsules.Add(Capsules[ElemIdx]);
			break;
		case ESimpleShapeType::Convex:
			NewSet.Convexes.Add(Convexes[ElemIdx]);		// todo movetemp here...
			break;
		case ESimpleShapeType::LevelSet:
			NewSet.LevelSets.Add(LevelSets[ElemIdx]);		// todo movetemp here...
			break;
		}
	}

	Spheres = MoveTemp(NewSet.Spheres);
	Boxes = MoveTemp(NewSet.Boxes);
	Capsules = MoveTemp(NewSet.Capsules);
	Convexes = MoveTemp(NewSet.Convexes);
	LevelSets = MoveTemp(NewSet.LevelSets);
}



void FSimpleShapeSet3d::ApplyTransform(const FTransform3d& Transform)
{
	for (FSphereShape3d& SphereShape : Spheres)
	{
		TransformSphereShape(SphereShape, Transform);
	}

	for (FBoxShape3d& BoxShape : Boxes)
	{
		TransformBoxShape(BoxShape, Transform);
	}

	for (UE::Geometry::FCapsuleShape3d& CapsuleShape : Capsules)
	{
		TransformCapsuleShape(CapsuleShape, Transform);
	}

	for (FConvexShape3d& ConvexShape : Convexes)
	{
		MeshTransforms::ApplyTransform(ConvexShape.Mesh, Transform, true);
	}

	for (FLevelSetShape3d& LevelSetShape : LevelSets)
	{
		TransformLevelSetShape(LevelSetShape, Transform);
	}
}
