// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selection/SelectionEditInteractiveCommand.h"
#include "DisconnectGeometrySelectionCommand.generated.h"


/**
 * UDisconnectGeometrySelectionCommand disconnects the geometric elements identified by the Selection.
 * Currently only supports mesh selections (Triangle and Polygroup types)
 * Disconnects selected faces, or faces connected to selected edges, or faces connected to selected vertices.
 */
UCLASS()
class MESHMODELINGTOOLS_API UDisconnectGeometrySelectionCommand : public UGeometrySelectionEditCommand
{
	GENERATED_BODY()
public:

	virtual FText GetCommandShortString() const override;

	virtual bool CanExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments) override;
	virtual void ExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments, UInteractiveCommandResult** Result) override;
};