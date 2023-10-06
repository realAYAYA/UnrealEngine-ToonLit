// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/UVToolAction.h"

#include "ContextObjectStore.h"
#include "Selection/UVToolSelectionAPI.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVToolAction)

#define LOCTEXT_NAMESPACE "UUVToolAction"

using namespace UE::Geometry;

void UUVToolAction::Setup(UInteractiveToolManager* ToolManagerIn)
{
	ToolManager = ToolManagerIn;

	UContextObjectStore* ContextStore = GetToolManager()->GetContextObjectStore();
	SelectionAPI = ContextStore->FindContext<UUVToolSelectionAPI>();
	checkSlow(SelectionAPI);
	EmitChangeAPI = ContextStore->FindContext<UUVToolEmitChangeAPI>();
}

void UUVToolAction::Shutdown()
{
	SelectionAPI = nullptr;
	EmitChangeAPI = nullptr;
}

#undef LOCTEXT_NAMESPACE
