// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FConversationEditor;
class FConversationTreeEditor;
class FExtender;
class FToolBarBuilder;

class FConversationEditorToolbar : public TSharedFromThis<FConversationEditorToolbar>
{
public:
	FConversationEditorToolbar(TSharedPtr<FConversationEditor> InConversationEditor)
		: ConversationEditor(InConversationEditor) {}

	void AddDebuggerToolbar(TSharedPtr<FExtender> Extender);
	void AddConversationEditorToolbar(TSharedPtr<FExtender> Extender);

private:
	void FillDebuggerToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillConversationEditorToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FConversationEditor> ConversationEditor;
};
