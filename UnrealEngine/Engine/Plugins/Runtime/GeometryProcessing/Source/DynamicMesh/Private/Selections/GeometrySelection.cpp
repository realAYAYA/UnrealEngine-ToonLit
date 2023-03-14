// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selections/GeometrySelection.h"
#include "DynamicMesh/DynamicMesh3.h"

using namespace UE::Geometry;

void FGeometrySelectionEditor::Initialize(FGeometrySelection* TargetSelectionIn)
{
	check(TargetSelectionIn != nullptr);
	TargetSelection = TargetSelectionIn;
}

void FGeometrySelectionEditor::ClearSelection(FGeometrySelectionDelta& DeltaOut)
{
	for (uint64 ID : TargetSelection->Selection)
	{
		DeltaOut.Removed.Add(ID);
	}

	TargetSelection->Reset();
}


bool FGeometrySelectionEditor::IsSelected(uint64 ID) const
{
	return TargetSelection->Selection.Contains(ID);
}

bool FGeometrySelectionEditor::Replace(const FGeometrySelection& NewSelection, FGeometrySelectionDelta& DeltaOut)
{
	// currently hard-enforcing this...maybe we can allow switches? 
	if (ensure(TargetSelection->ElementType == NewSelection.ElementType) == false ||
		ensure(TargetSelection->TopologyType == NewSelection.TopologyType) == false)
	{
		return false;
	}

	for (uint64 ID : TargetSelection->Selection)
	{
		DeltaOut.Removed.Add(ID);
	}
	TargetSelection->Selection.Reset();

	for (uint64 ID : NewSelection.Selection)
	{
		TargetSelection->Selection.Add(ID);
		DeltaOut.Added.Add(ID);
	}

	return true;
}