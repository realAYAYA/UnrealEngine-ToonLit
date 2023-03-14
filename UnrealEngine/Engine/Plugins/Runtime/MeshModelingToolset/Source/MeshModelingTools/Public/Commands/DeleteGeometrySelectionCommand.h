// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selection/SelectionEditInteractiveCommand.h"
#include "DeleteGeometrySelectionCommand.generated.h"


/**
 * UDeleteGeometrySelectionCommand deletes the geometric elements identified by the Selection.
 * Currently only supports mesh selections (Triangle and Polygroup types)
 * Deletes selected faces, or faces connected to selected edges, or faces connected to selected vertices.
 */
UCLASS()
class MESHMODELINGTOOLS_API UDeleteGeometrySelectionCommand : public UGeometrySelectionEditCommand
{
	GENERATED_BODY()
public:

	virtual FText GetCommandShortString() const override;

	virtual bool CanExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments) override;
	virtual void ExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments) override;
};