// Copyright Epic Games, Inc. All Rights Reserved.


#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Async/ParallelFor.h"

using namespace UE::Geometry;

void MeshTransforms::Translate(FDynamicMesh3& Mesh, const FVector3d& Translation)
{
	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int vid) 
	{
		if (Mesh.IsVertex(vid))
		{
			Mesh.SetVertex(vid, Mesh.GetVertex(vid) + Translation);
		}
	});
}


void MeshTransforms::Scale(FDynamicMesh3& Mesh, const FVector3d& Scale, const FVector3d& Origin, bool bReverseOrientationIfNeeded)
{
	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int32 vid)
	{
		if (Mesh.IsVertex(vid))
		{
			Mesh.SetVertex(vid, (Mesh.GetVertex(vid) - Origin) * Scale + Origin);
		}
	});
	if (bReverseOrientationIfNeeded && Scale.X * Scale.Y * Scale.Z < 0)
	{
		Mesh.ReverseOrientation(false);
	}
}



void MeshTransforms::WorldToFrameCoords(FDynamicMesh3& Mesh, const FFrame3d& Frame)
{
	bool bVertexNormals = Mesh.HasVertexNormals();

	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int vid)
	{
		if (Mesh.IsVertex(vid))
		{
			FVector3d Position = Mesh.GetVertex(vid);
			Mesh.SetVertex(vid, Frame.ToFramePoint(Position));

			if (bVertexNormals)
			{
				FVector3f Normal = Mesh.GetVertexNormal(vid);
				Mesh.SetVertexNormal(vid, (FVector3f)Frame.ToFrameVector((FVector3d)Normal));
			}
		}
	});

	if (Mesh.HasAttributes())
	{
		FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		int NumNormals = Normals->MaxElementID();
		ParallelFor(NumNormals, [&](int elemid)
		{
			if (Normals->IsElement(elemid))
			{
				FVector3f Normal = Normals->GetElement(elemid);
				Normals->SetElement(elemid, (FVector3f)Frame.ToFrameVector((FVector3d)Normal));
			}
		});
	}
}





void MeshTransforms::FrameCoordsToWorld(FDynamicMesh3& Mesh, const FFrame3d& Frame)
{
	bool bVertexNormals = Mesh.HasVertexNormals();

	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int vid)
	{
		if (Mesh.IsVertex(vid))
		{
			FVector3d Position = Mesh.GetVertex(vid);
			Mesh.SetVertex(vid, Frame.FromFramePoint(Position));

			if (bVertexNormals)
			{
				FVector3f Normal = Mesh.GetVertexNormal(vid);
				Mesh.SetVertexNormal(vid, (FVector3f)Frame.FromFrameVector((FVector3d)Normal));
			}
		}
	});

	if (Mesh.HasAttributes())
	{
		FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		int NumNormals = Normals->MaxElementID();
		ParallelFor(NumNormals, [&](int elemid)
		{
			if (Normals->IsElement(elemid))
			{
				FVector3f Normal = Normals->GetElement(elemid);
				Normals->SetElement(elemid, (FVector3f)Frame.FromFrameVector((FVector3d)Normal));
			}
		});
	}
}




void MeshTransforms::ApplyTransform(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform, bool bReverseOrientationIfNeeded)
{
	bool bVertexNormals = Mesh.HasVertexNormals();

	for (int vid : Mesh.VertexIndicesItr())
	{
		FVector3d Position = Mesh.GetVertex(vid);
		Position = Transform.TransformPosition(Position);
		Mesh.SetVertex(vid, Position);

		if (bVertexNormals)
		{
			FVector3f Normal = Mesh.GetVertexNormal(vid);
			Normal = (FVector3f)Transform.TransformNormal((FVector3d)Normal);
			Mesh.SetVertexNormal(vid, Normalized(Normal));
		}
	}
	if (Mesh.HasAttributes())
	{
		FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		for (int elemid : Normals->ElementIndicesItr())
		{
			FVector3f Normal = Normals->GetElement(elemid);
			Normal = (FVector3f)Transform.TransformNormal((FVector3d)Normal);
			Normals->SetElement(elemid, Normalized(Normal));
		}
	}

	if (bReverseOrientationIfNeeded && Transform.GetDeterminant() < 0)
	{
		Mesh.ReverseOrientation(false);
	}
}



void MeshTransforms::ApplyTransformInverse(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform, bool bReverseOrientationIfNeeded)
{
	bool bVertexNormals = Mesh.HasVertexNormals();

	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int vid)
	{
		if (Mesh.IsVertex(vid)) 
		{
			FVector3d Position = Mesh.GetVertex(vid);
			Position = Transform.InverseTransformPosition(Position);
			Mesh.SetVertex(vid, Position);

			if (bVertexNormals)
			{
				FVector3f Normal = Mesh.GetVertexNormal(vid);
				Normal = (FVector3f)Transform.InverseTransformNormal((FVector3d)Normal);
				Mesh.SetVertexNormal(vid, Normalized(Normal));
			}
		}
	});

	if (Mesh.HasAttributes())
	{
		FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		if (Normals)
		{
			int NumNormals = Normals->MaxElementID();
			ParallelFor(NumNormals, [&](int elemid)
			{
				if (Normals->IsElement(elemid))
				{
					FVector3f Normal = Normals->GetElement(elemid);
					Normal = (FVector3f)Transform.InverseTransformNormal((FVector3d)Normal);
					Normals->SetElement(elemid, Normalized(Normal));
				}
			});
		}
	}

	if (bReverseOrientationIfNeeded && Transform.GetDeterminant() < 0)
	{
		Mesh.ReverseOrientation(false);
	}
}


void MeshTransforms::ReverseOrientationIfNeeded(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform)
{
	if (Transform.GetDeterminant() < 0)
	{
		Mesh.ReverseOrientation(false);
	}
}



void MeshTransforms::ApplyTransform(FDynamicMesh3& Mesh,
	TFunctionRef<FVector3d(const FVector3d&)> PositionTransform,
	TFunctionRef<FVector3f(const FVector3f&)> NormalTransform)
{
	bool bVertexNormals = Mesh.HasVertexNormals();

	int NumVertices = Mesh.MaxVertexID();
	ParallelFor(NumVertices, [&](int vid)
	{
		if (Mesh.IsVertex(vid))
		{
			FVector3d Position = Mesh.GetVertex(vid);
			Position = PositionTransform(Position);
			Mesh.SetVertex(vid, Position);

			if (bVertexNormals)
			{
				FVector3f Normal = Mesh.GetVertexNormal(vid);
				Normal = NormalTransform(Normal);
				Mesh.SetVertexNormal(vid, Normalized(Normal));
			}
		}
	});

	if (Mesh.HasAttributes())
	{
		FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		if (Normals)
		{
			int NumNormals = Normals->MaxElementID();
			ParallelFor(NumNormals, [&](int elemid)
			{
				if (Normals->IsElement(elemid))
				{
					FVector3f Normal = Normals->GetElement(elemid);
					Normal = NormalTransform(Normal);
					Normals->SetElement(elemid, Normalized(Normal));
				}
			});
		}
	}
}
