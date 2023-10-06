// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Framework/Commands/Commands.h"

class NIAGARAEDITOR_API FNiagaraHierarchyEditorCommands : public TCommands<FNiagaraHierarchyEditorCommands>
{
public:
	
	FNiagaraHierarchyEditorCommands()
		: TCommands<FNiagaraHierarchyEditorCommands>( TEXT("NiagaraHierarchyEditorCommands"), NSLOCTEXT("NiagaraHierarchyEditorCommands", "Hierarchy Editor Commands", "Hierarchy Editor Commands"), NAME_None, "NiagaraEditorStyle" )
	{
	}

	virtual ~FNiagaraHierarchyEditorCommands() override
	{
	}

	virtual void RegisterCommands() override;	
	
	TSharedPtr<FUICommandInfo> DeleteSection;
};
