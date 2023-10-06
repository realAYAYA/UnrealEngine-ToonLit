// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Topo/Model.h"

#include "CADKernel/Core/EntityGeom.h"
#include "CADKernel/Topo/Body.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/Topo/TopologicalVertex.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Topo/TopologyReport.h"
#endif

namespace UE::CADKernel
{

void FModel::AddEntity(TSharedRef<FTopologicalEntity> Entity)
{
	switch (Entity->GetEntityType())
	{
	case EEntity::Body:
		Add(StaticCastSharedRef<FBody>(Entity));
		break;
	default:
		break;
	}
}

bool FModel::Contains(TSharedPtr<FTopologicalEntity> Entity)
{
	switch(Entity->GetEntityType())
	{
	case EEntity::Body:
		return Bodies.Find(StaticCastSharedPtr<FBody>(Entity)) != INDEX_NONE;
	default:
		return false;
	}
	return false;
}

void FModel::PrintBodyAndShellCount()
{
	int32 NbBody = 0;
	int32 NbShell = 0;

	TArray<TSharedPtr<FBody>> NewBodies;
	for (TSharedPtr<FBody> Body : Bodies)
	{
		NbShell += Body->GetShells().Num();
		NbBody++;
	}
	FMessage::Printf(Log, TEXT("Body count %d shell count %d \n"), NbBody, NbShell);
}

int32 FModel::FaceCount() const
{
	int32 FaceCount = 0;
	for (const TSharedPtr<FBody>& Body : Bodies)
	{
		FaceCount += Body->FaceCount();
	}
	return FaceCount;
}

void FModel::RemoveEmptyBodies()
{
	TArray<TSharedPtr<FBody>> NewBodies;
	NewBodies.Reserve(Bodies.Num());

	for (const TSharedPtr<FBody>& Body : Bodies)
	{
		if (!Body->IsDeleted() && Body->ShellCount())
		{
			NewBodies.Add(Body);
		}
		else
		{
			Body->Delete();
			Body->ResetHost();
		}
	}

	Bodies = MoveTemp(NewBodies);
}

void FModel::GetFaces(TArray<FTopologicalFace*>& OutFaces)
{
	for (const TSharedPtr<FBody>& Body : Bodies)
	{
		Body->GetFaces(OutFaces);
	}
}

void FModel::PropagateBodyOrientation()
{
	for (TSharedPtr<FBody>& Body : Bodies)
	{
		Body->PropagateBodyOrientation();
	}
}

void FModel::CompleteMetaData()
{
	for (TSharedPtr<FBody>& Body : Bodies)
	{
		Body->CompleteMetaData();
	}
}

struct FBodyShell
{
	TSharedPtr<FBody> Body;
	TSharedPtr<FShell> Shell;

	FBodyShell(TSharedPtr<FBody> InBody, TSharedPtr<FShell> InShell)
	: Body(InBody)
	, Shell(InShell)
	{
	}

};

void FModel::CheckTopology() 
{
	TArray<FBodyShell> IsolatedBodies;
	IsolatedBodies.Reserve(Bodies.Num()*2);

	int32 ShellCount = 0;

	for (TSharedPtr<FBody> Body : Bodies)
	{
		for (TSharedPtr<FShell> Shell : Body->GetShells())
		{
			ShellCount++;
			TArray<FFaceSubset> SubShells;
			Shell->CheckTopology(SubShells);

			if (SubShells.Num() == 1 )
			{
				if (Shell->FaceCount() < 3 )
				{
					IsolatedBodies.Emplace(Body, Shell);
				}
				else
				{
					if (SubShells[0].BorderEdgeCount > 0 || SubShells[0].NonManifoldEdgeCount > 0)
					{
#ifdef CORETECHBRIDGE_DEBUG
						FMessage::Printf(Log, TEXT("Body %d shell %d CADId %d is opened and has %d faces "), Body->GetKioId(), Shell->GetKioId(), Shell->GetId(), Shell->FaceCount());
#else
						FMessage::Printf(Log, TEXT("Body %d shell %d is opened and has %d faces "), Body->GetId(), Shell->GetId(), Shell->FaceCount());
#endif
						FMessage::Printf(Log, TEXT("and has %d border edges and %d nonManifold edges\n"), SubShells[0].BorderEdgeCount, SubShells[0].NonManifoldEdgeCount);
					}
					else
					{
#ifdef CORETECHBRIDGE_DEBUG
						FMessage::Printf(Log, TEXT("Body %d shell %d CADId %d is closed and has %d faces\n"), Body->GetKioId(), Shell->GetKioId(), Shell->GetId(), Shell->FaceCount());
#else
						FMessage::Printf(Log, TEXT("Body %d shell %d is closed and has %d faces\n"), Body->GetId(), Shell->GetId(), Shell->FaceCount());
#endif
					}
				}
			}
			else
			{
#ifdef CORETECHBRIDGE_DEBUG
				FMessage::Printf(Log, TEXT("Body %d shell %d CADId %d has %d subshells\n"), Body->GetKioId(), Shell->GetKioId(), Shell->GetId(), SubShells.Num());
#else
				FMessage::Printf(Log, TEXT("Body %d shell %d has %d subshells\n"), Body->GetId(), Shell->GetId(), SubShells.Num());
#endif
for (const FFaceSubset& Subset : SubShells)
				{
					FMessage::Printf(Log, TEXT("     - Subshell of %d faces %d border edges and %d nonManifold edges\n"), Subset. Faces.Num(), Subset.BorderEdgeCount, Subset.NonManifoldEdgeCount);
				}
			}
		}
	}
}

#ifdef CADKERNEL_DEV
void FModel::FillTopologyReport(FTopologyReport& Report) const
{
	for (TSharedPtr<FBody> Body : Bodies)
	{
		Body->FillTopologyReport(Report);
	}

	Report.ResetMarkers();
}
#endif

void FModel::Orient()
{
	for (TSharedPtr<FBody> Body : Bodies)
	{
		Body->Orient();
	}
}

}