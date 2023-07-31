// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

/**
 *
 */
class FCustomizableObjectDebuggerCommands : public TCommands<FCustomizableObjectDebuggerCommands>
{

public:
	FCustomizableObjectDebuggerCommands();

	TSharedPtr< FUICommandInfo > GenerateMutableGraph;
	TSharedPtr< FUICommandInfo > CompileMutableCode;
	TSharedPtr< FUICommandInfo > CompileOptions_EnableTextureCompression;
	TSharedPtr< FUICommandInfo > CompileOptions_UseParallelCompilation;
	TSharedPtr< FUICommandInfo > CompileOptions_UseDiskCompilation;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

};

