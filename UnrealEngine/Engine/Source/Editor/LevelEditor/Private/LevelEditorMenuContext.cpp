// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorMenuContext.h"

#include "Elements/Framework/TypedElementRegistry.h"

FScriptTypedElementHandle ULevelEditorContextMenuContext::GetScriptHitProxyElement()
{
	return UTypedElementRegistry::GetInstance()->CreateScriptHandle(HitProxyElement.GetId());
}