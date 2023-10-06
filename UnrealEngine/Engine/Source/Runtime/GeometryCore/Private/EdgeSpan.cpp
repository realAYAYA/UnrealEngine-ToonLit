// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdgeSpan.h"

using namespace UE::Geometry;

void FEdgeSpan::Initialize(const FDynamicMesh3* mesh, const TArray<int>& vertices, const TArray<int> & edges, const TArray<int>* BowtieVerticesIn)
{
	Mesh = mesh;
	Vertices = vertices;
	Edges = edges;
	if (BowtieVerticesIn != nullptr)
	{
		BowtieVertices = *BowtieVerticesIn;
		bBowtiesCalculated = true;
	}
}



void FEdgeSpan::InitializeFromEdges(const TArray<int>& EdgesIn)
{
	check(Mesh != nullptr);
	Edges = EdgesIn;

	int NumEdges = Edges.Num();
	Vertices.SetNum(NumEdges + 1);

	FIndex2i start_ev = Mesh->GetEdgeV(Edges[0]);
	FIndex2i prev_ev = start_ev;
	if (NumEdges > 1)
	{
		for (int i = 1; i < Edges.Num(); ++i)
		{
			FIndex2i next_ev = Mesh->GetEdgeV(Edges[i]);
			Vertices[i] = IndexUtil::FindSharedEdgeVertex(prev_ev, next_ev);
			prev_ev = next_ev;
		}
		Vertices[0] = IndexUtil::FindEdgeOtherVertex(start_ev, Vertices[1]);
		Vertices[Vertices.Num() - 1] = IndexUtil::FindEdgeOtherVertex(prev_ev, Vertices[Vertices.Num() - 2]);
	}
	else
	{
		Vertices[0] = start_ev[0];
		Vertices[1] = start_ev[1];
	}
}




bool FEdgeSpan::InitializeFromVertices(const TArray<int>& VerticesIn, bool bAutoOrient)
{
	check(Mesh != nullptr);
	Vertices = VerticesIn;

	int NumVertices = Vertices.Num();
	Edges.SetNum(NumVertices - 1);
	for (int i = 0; i < NumVertices - 1; ++i)
	{
		int a = Vertices[i], b = Vertices[i + 1];
		Edges[i] = Mesh->FindEdge(a, b);
		if (Edges[i] == FDynamicMesh3::InvalidID)
		{
			checkf(false, TEXT("EdgeSpan.FromVertices: invalid edge [%d,%d]"), a, b);
			return false;
		}
	}

	if (bAutoOrient)
	{
		SetCorrectOrientation();
	}

	return true;
}


void FEdgeSpan::CalculateBowtieVertices()
{
	BowtieVertices.Reset();
	int NumVertices = Vertices.Num();
	for (int i = 0; i < NumVertices; ++i)
	{
		if (Mesh->IsBowtieVertex(Vertices[i]))
		{
			BowtieVertices.Add(Vertices[i]);
		}
	}
	bBowtiesCalculated = true;
}



FAxisAlignedBox3d FEdgeSpan::GetBounds() const
{
	FAxisAlignedBox3d box = FAxisAlignedBox3d::Empty();
	for (int i = 0; i < Vertices.Num(); ++i)
	{
		box.Contain(Mesh->GetVertex(Vertices[i]));
	}
	return box;
}



void FEdgeSpan::GetPolyline(FPolyline3d& PolylineOut) const
{
	PolylineOut.Clear();
	for (int i = 0; i < Vertices.Num(); ++i)
	{
		PolylineOut.AppendVertex(Mesh->GetVertex(Vertices[i]));
	}
}


bool FEdgeSpan::SetCorrectOrientation()
{
	int NumEdges = Edges.Num();
	for (int i = 0; i < NumEdges; ++i)
	{
		int eid = Edges[i];
		if (Mesh->IsBoundaryEdge(eid))
		{
			int a = Vertices[i], b = Vertices[i + 1];
			FIndex2i ev = Mesh->GetOrientedBoundaryEdgeV(eid);
			if (ev.A == b && ev.B == a)
			{
				Reverse();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}



bool FEdgeSpan::IsInternalspan() const
{
	int NumEdges = Edges.Num();
	for (int i = 0; i < NumEdges; ++i)
	{
		if (Mesh->IsBoundaryEdge(Edges[i]))
		{
			return false;
		}
	}
	return true;
}


bool FEdgeSpan::IsBoundaryspan(const FDynamicMesh3* TestMesh) const
{
	const FDynamicMesh3* UseMesh = (TestMesh != nullptr) ? TestMesh : Mesh;

	int NumEdges = Edges.Num();
	for (int i = 0; i < NumEdges; ++i)
	{
		if (UseMesh->IsBoundaryEdge(Edges[i]) == false)
		{
			return false;
		}
	}
	return true;
}



int FEdgeSpan::FindVertexIndex(int VertexID) const
{
	int N = Vertices.Num();
	for (int i = 0; i < N; ++i)
	{
		if (Vertices[i] == VertexID)
		{
			return i;
		}
	}
	return -1;
}



int FEdgeSpan::FindNearestVertexIndex(const FVector3d& QueryPoint) const
{
	int iNear = -1;
	double fNearSqr = TNumericLimits<double>::Max();
	int N = Vertices.Num();
	for (int i = 0; i < N; ++i)
	{
		FVector3d lv = Mesh->GetVertex(Vertices[i]);
		double d2 = DistanceSquared(QueryPoint, lv);
		if (d2 < fNearSqr)
		{
			fNearSqr = d2;
			iNear = i;
		}
	}
	return iNear;
}



bool FEdgeSpan::CheckValidity(EValidityCheckFailMode FailMode) const
{
	bool is_ok = true;
	TFunction<void(bool)> CheckOrFailF = [&](bool b)
	{
		is_ok = is_ok && b;
	};
	if (FailMode == EValidityCheckFailMode::Check)
	{
		CheckOrFailF = [&](bool b)
		{
			checkf(b, TEXT("FEdgeSpan::CheckValidity failed!"));
			is_ok = is_ok && b;
		};
	}
	else if (FailMode == EValidityCheckFailMode::Ensure)
	{
		CheckOrFailF = [&](bool b)
		{
			ensureMsgf(b, TEXT("FEdgeSpan::CheckValidity failed!"));
			is_ok = is_ok && b;
		};
	}


	CheckOrFailF(Vertices.Num() == (Edges.Num() + 1));
	for (int ei = 0; ei < Edges.Num(); ++ei)
	{
		FIndex2i ev = Mesh->GetEdgeV(Edges[ei]);
		CheckOrFailF(Mesh->IsVertex(ev.A));
		CheckOrFailF(Mesh->IsVertex(ev.B));
		CheckOrFailF(Mesh->FindEdge(ev.A, ev.B) != FDynamicMesh3::InvalidID);
		CheckOrFailF(Vertices[ei] == ev.A || Vertices[ei] == ev.B);
		CheckOrFailF(Vertices[ei + 1] == ev.A || Vertices[ei + 1] == ev.B);
	}
	for (int vi = 0; vi < Vertices.Num() - 1; ++vi)
	{
		int a = Vertices[vi], b = Vertices[vi + 1];
		CheckOrFailF(Mesh->IsVertex(a));
		CheckOrFailF(Mesh->IsVertex(b));
		CheckOrFailF(Mesh->FindEdge(a, b) != FDynamicMesh3::InvalidID);

		// @todo rewrite this test for span, has to handle endpoint vertices that only have one nbr
		if (vi < Vertices.Num() - 2) {
			int n = 0, edge_before_b = Edges[vi], edge_after_b = Edges[(vi + 1) % Vertices.Num()];
			for (int nbr_e : Mesh->VtxEdgesItr(b))
			{
				if (nbr_e == edge_before_b || nbr_e == edge_after_b)
				{
					n++;
				}
			}
			CheckOrFailF(n == 2);
		}
	}
	return is_ok;
}



void FEdgeSpan::VertexSpanToEdgeSpan(const FDynamicMesh3* Mesh, const TArray<int>& VertexSpan, TArray<int>& OutEdgeSpan)
{
	// @todo this function should be in a utility class?

	int NV = VertexSpan.Num();
	OutEdgeSpan.SetNum(NV - 1);
	for (int i = 0; i < NV - 1; ++i)
	{
		int v0 = VertexSpan[i];
		int v1 = VertexSpan[i + 1];
		OutEdgeSpan[i] = Mesh->FindEdge(v0, v1);
	}
}