// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayout.h"

#include "Editor.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "TimerManager.h"


namespace UE::DMX::Private
{ 
	void SDMXControlConsoleEditorLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
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
