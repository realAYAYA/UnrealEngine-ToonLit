// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

//////////////////////////////////////////////////////////////////////
//

class FConversationEditorCommonCommands : public TCommands<FConversationEditorCommonCommands>
{
public:
	FConversationEditorCommonCommands();

	TSharedPtr<FUICommandInfo> SearchConversation;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

//////////////////////////////////////////////////////////////////////
//

class FConversationDebuggerCommands : public TCommands<FConversationDebuggerCommands>
{
public:
	FConversationDebuggerCommands();

	TSharedPtr<FUICommandInfo> BackInto;
	TSharedPtr<FUICommandInfo> BackOver;
	TSharedPtr<FUICommandInfo> ForwardInto;
	TSharedPtr<FUICommandInfo> ForwardOver;
	TSharedPtr<FUICommandInfo> StepOut;

	TSharedPtr<FUICommandInfo> PausePlaySession;
	TSharedPtr<FUICommandInfo> ResumePlaySession;
	TSharedPtr<FUICommandInfo> StopPlaySession;

	TSharedPtr<FUICommandInfo> CurrentValues;
	TSharedPtr<FUICommandInfo> SavedValues;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

