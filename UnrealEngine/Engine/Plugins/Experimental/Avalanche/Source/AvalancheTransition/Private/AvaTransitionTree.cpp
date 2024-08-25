// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTree.h"

FAvaTagHandle UAvaTransitionTree::GetTransitionLayer() const
{
	return TransitionLayer;
}

void UAvaTransitionTree::SetTransitionLayer(FAvaTagHandle InTransitionLayer)
{
	TransitionLayer = InTransitionLayer;
}

bool UAvaTransitionTree::IsEnabled() const
{
	return bEnabled;
}

void UAvaTransitionTree::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}
