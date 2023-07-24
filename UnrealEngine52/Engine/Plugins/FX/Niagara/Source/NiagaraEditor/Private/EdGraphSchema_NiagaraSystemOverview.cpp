// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraphSchema_NiagaraSystemOverview.h"
#include "EdGraphNode_Comment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EdGraphSchema_NiagaraSystemOverview)

#define LOCTEXT_NAMESPACE "NiagaraSchema"

int32 UEdGraphSchema_NiagaraSystemOverview::CurrentCacheRefreshID = 0;

UEdGraphSchema_NiagaraSystemOverview::UEdGraphSchema_NiagaraSystemOverview(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEdGraphSchema_NiagaraSystemOverview::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	//@TODO System Overview: enable/disable emitter, solo, etc. 
}

bool UEdGraphSchema_NiagaraSystemOverview::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return InVisualizationCacheID != CurrentCacheRefreshID;
}

int32 UEdGraphSchema_NiagaraSystemOverview::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UEdGraphSchema_NiagaraSystemOverview::ForceVisualizationCacheClear() const
{
	CurrentCacheRefreshID++;
}

#undef LOCTEXT_NAMESPACE

