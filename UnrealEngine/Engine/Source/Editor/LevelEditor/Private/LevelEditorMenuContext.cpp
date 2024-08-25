// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorMenuContext.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "SLevelEditor.h"

TWeakPtr<ILevelEditor> ULevelEditorMenuContext::GetLevelEditor() const
{
	return LevelEditor;
}

FScriptTypedElementHandle ULevelEditorContextMenuContext::GetScriptHitProxyElement()
{
	return UTypedElementRegistry::GetInstance()->CreateScriptHandle(HitProxyElement.GetId());
}
