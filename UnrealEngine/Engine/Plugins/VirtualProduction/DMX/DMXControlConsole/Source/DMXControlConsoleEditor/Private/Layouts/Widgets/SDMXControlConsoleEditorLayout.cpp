// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayout.h"

#include "Editor.h"
#include "TimerManager.h"


namespace UE::DMXControlConsoleEditor::Layout::Private
{ 
	void SDMXControlConsoleEditorLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout)
	{
		EditorLayout = InLayout;
	}

	void SDMXControlConsoleEditorLayout::RequestRefresh()
	{
		if (!RefreshLayoutTimerHandle.IsValid() && CanRefresh())
		{
			RefreshLayoutTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXControlConsoleEditorLayout::Refresh));
		}
	}

	void SDMXControlConsoleEditorLayout::Refresh()
	{
		RefreshLayoutTimerHandle.Invalidate();

		OnLayoutElementAdded();
		OnLayoutElementRemoved();
	}
}
