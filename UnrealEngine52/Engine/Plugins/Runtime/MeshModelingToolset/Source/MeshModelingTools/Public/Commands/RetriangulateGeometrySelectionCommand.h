// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selection/SelectionEditInteractiveCommand.h"
#include "RetriangulateGeometrySelectionCommand.generated.h"


/**
 * URetriangulateGeometrySelectionCommand 
 */
UCLASS()
class MESHMODELINGTOOLS_API URetriangulateGeometrySelectionCommand : public UGeometrySelectionEditCommand
{
	GENERATED_BODY()
public:

	virtual bool AllowEmptySelection() const override { return true; }
	virtual FText GetCommandShortString() const override;

	virtual bool CanExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments) override;
	virtual void ExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments, UInteractiveCommandResult** Result) override;
};