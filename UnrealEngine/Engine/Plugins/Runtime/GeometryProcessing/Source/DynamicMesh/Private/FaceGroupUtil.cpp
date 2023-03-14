// Copyright Epic Games, Inc. All Rights Reserved.

#include "FaceGroupUtil.h"

using namespace UE::Geometry;

void FaceGroupUtil::SetGroupID(FDynamicMesh3& Mesh, int32 to)
{
	if (Mesh.HasTriangleGroups() == false)
	{
		return;
	}
	for (int32 tid : Mesh.TriangleIndicesItr())
	{
		Mesh.SetTriangleGroup(tid, to);
	}
}


void FaceGroupUtil::SetGroupID(FDynamicMesh3& Mesh, const TArrayView<const int>& triangles, int32 to)
{
	if (Mesh.HasTriangleGroups() == false)
	{
		return;
	}
	for (int32 tid : triangles)
	{
		Mesh.SetTriangleGroup(tid, to);
	}
}


void FaceGroupUtil::SetGroupToGroup(FDynamicMesh3& Mesh, int32 from, int32 to)
{
	if (Mesh.HasTriangleGroups() == false)
	{
		return;
	}

	int32 NT = Mesh.MaxTriangleID();
	for (int32 tid = 0; tid < NT; ++tid)
	{
		if (Mesh.IsTriangle(tid))
		{
			int32 gid = Mesh.GetTriangleGroup(tid);
			if (gid == from)
			{
				Mesh.SetTriangleGroup(tid, to);
			}
		}
	}
}


bool FaceGroupUtil::HasMultipleGroups(const FDynamicMesh3& Mesh)
{
	if (Mesh.HasTriangleGroups())
	{
		int32 CurGroupID = -1;
		int32 GroupsCounter = 0;
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			int32 GroupID = Mesh.GetTriangleGroup(tid);
			if (GroupID != CurGroupID)
			{
				CurGroupID = GroupID;
				if (GroupsCounter++ > 1)
				{
					return true;
				}
			}
		}
	}
	return false;
}


void FaceGroupUtil::FindAllGroups(const FDynamicMesh3& Mesh, TSet<int32>& GroupsOut)
{
	if (Mesh.HasTriangleGroups())
	{
		int32 NT = Mesh.MaxTriangleID();
		for (int32 tid = 0; tid < NT; ++tid)
		{
			if (Mesh.IsTriangle(tid))
			{
				int32 gid = Mesh.GetTriangleGroup(tid);
				GroupsOut.Add(gid);
			}
		}
	}
}


void FaceGroupUtil::CountAllGroups(const FDynamicMesh3& Mesh, TArray<int32>& GroupCountsOut)
{
	GroupCountsOut.SetNum(Mesh.MaxGroupID());

	if (Mesh.HasTriangleGroups())
	{
		int32 NT = Mesh.MaxTriangleID();
		for (int32 tid = 0; tid < NT; ++tid)
		{
			if (Mesh.IsTriangle(tid))
			{
				int32 gid = Mesh.GetTriangleGroup(tid);
				GroupCountsOut[gid]++;
			}
		}
	}
}


void FaceGroupUtil::FindTriangleSetsByGroup(const FDynamicMesh3& Mesh, TArray<TArray<int32>>& GroupTrisOut, int32 IgnoreGID)
{
	if (!Mesh.HasTriangleGroups())
	{
		return;
	}

	// find # of groups and triangle count for each
	TArray<int32> Counts;
	CountAllGroups(Mesh, Counts);
	TArray<int32> GroupIDs;
	for (int32 CountIdx = 0; CountIdx < Counts.Num(); CountIdx++) 
	{
		int32 Count = Counts[CountIdx];
		if (CountIdx != IgnoreGID && Count > 0)
		{
			GroupIDs.Add(CountIdx);
		}
	}
	TArray<int32> GroupMap; 
	GroupMap.SetNum(Mesh.MaxGroupID());

	// allocate sets
	GroupTrisOut.SetNum(GroupIDs.Num());
	for (int32 i = 0; i < GroupIDs.Num(); ++i)
	{
		int32 GID = GroupIDs[i];
		GroupTrisOut[i].Reserve(Counts[GID]);
		GroupMap[GID] = i;
	}

	// accumulate triangles
	int32 NT = Mesh.MaxTriangleID();
	for (int32 tid = 0; tid < NT; ++tid)
	{
		if (Mesh.IsTriangle(tid))
		{
			int32 GID = Mesh.GetTriangleGroup(tid);
			int32 i = GroupMap[GID];
			if (i >= 0)
			{
				GroupTrisOut[i].Add(tid);
			}
		}
	}
}



bool FaceGroupUtil::FindTrianglesByGroup(FDynamicMesh3& Mesh, int32 FindGroupID, TArray<int32>& TrianglesOut)
{
	int32 NumAdded = 0;
	TArray<int32> tris;
	if (Mesh.HasTriangleGroups() == false)
	{
		return false;
	}
	for (int32 tid : Mesh.TriangleIndicesItr())
	{
		if (Mesh.GetTriangleGroup(tid) == FindGroupID)
		{
			TrianglesOut.Add(tid);
			NumAdded++;
		}
	}
	return (NumAdded > 0);
}


void FaceGroupUtil::SeparateMeshByGroups(FDynamicMesh3& Mesh, TArray<FDynamicMesh3>& SplitMeshes)
{
	FDynamicMeshEditor::SplitMesh(&Mesh, SplitMeshes, [&Mesh](int32 TID)
	{
		return Mesh.GetTriangleGroup(TID);
	});
}


void FaceGroupUtil::SeparateMeshByGroups(FDynamicMesh3& Mesh, TArray<FDynamicMesh3>& SplitMeshes, TArray<int32>& GroupIDs)
{
	// build split meshes
	SeparateMeshByGroups(Mesh, SplitMeshes);

	// build array of per-mesh group id
	GroupIDs.Reset();
	for (const FDynamicMesh3& M : SplitMeshes)
	{
		check(M.TriangleCount() > 0); // SplitMesh should never add an empty mesh
		GroupIDs.Add(M.GetTriangleGroup(0));
	}
}