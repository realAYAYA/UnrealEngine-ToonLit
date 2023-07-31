// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Topo/Body.h"

#include "CADKernel/Topo/Model.h"
#include "CADKernel/Topo/Shell.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Topo/TopologyReport.h"
#endif

namespace UE::CADKernel
{

void FBody::AddShell(TSharedRef<FShell> Shell)
{
	Shells.Add(Shell);
	Shell->SetHost(this);
}

void FBody::RemoveEmptyShell(FModel& Model)
{
	TArray<TSharedPtr<FShell>> NewShells;
	NewShells.Reserve(Shells.Num());
	for (TSharedPtr<FShell> Shell : Shells)
	{
		if (Shell->FaceCount() > 0)
		{
			NewShells.Emplace(Shell);
		}
		else
		{
			Shell->Delete();
		}
	}
	if (NewShells.IsEmpty())
	{
		Delete();
		Model.Remove(this);
	}
	else
	{
		Swap(NewShells, Shells);
	}
}

void FBody::Remove(const FTopologicalShapeEntity* ShellToRemove)
{
	if (!ShellToRemove)
	{
		return;
	}

	int32 Index = Shells.IndexOfByPredicate([&](const TSharedPtr<FShell>& Shell) { return (Shell.Get() == ShellToRemove); });
	Shells.RemoveAt(Index);
}

#ifdef CADKERNEL_DEV
FInfoEntity& FBody::GetInfo(FInfoEntity& Info) const
{
	return FTopologicalShapeEntity::GetInfo(Info).Add(TEXT("Shells"), Shells);
}

void FBody::FillTopologyReport(FTopologyReport& Report) const
{
	Report.Add(this);
	for (TSharedPtr<FShell> Shell : Shells)
	{
		Shell->FillTopologyReport(Report);
	}
}
#endif

}
