// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorTransformProxy.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"


#define LOCTEXT_NAMESPACE "UEditorTransformProxy"

FTransform UEditorTransformProxy::GetTransform() const
{
	if (const FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		const FQuat Rotation(ViewportClient->GetWidgetCoordSystem());
		const FVector Location = ViewportClient->GetWidgetLocation();
		const FVector Scale(WeakContext->GetModeTools()->GetWidgetScale());
		return FTransform(Rotation, Location, Scale);
	}
	
	return FTransform::Identity;
}

void UEditorTransformProxy::InputTranslateDelta(const FVector& InDeltaTranslate, EAxisList::Type InAxisList)
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		FVector Translate = InDeltaTranslate;
		FRotator Rot = FRotator::ZeroRotator;
		FVector Scale = FVector::ZeroVector;

		// Set legacy widget axis temporarily because InputWidgetDelta branches/overrides may expect it
		ViewportClient->SetCurrentWidgetAxis(InAxisList);
		ViewportClient->InputWidgetDelta(ViewportClient->Viewport, InAxisList, Translate, Rot, Scale);
		ViewportClient->SetCurrentWidgetAxis(EAxisList::None);
	}
}

void UEditorTransformProxy::InputScaleDelta(const FVector& InDeltaScale, EAxisList::Type InAxisList)
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		FVector Translate = FVector::ZeroVector;
		FRotator Rot = FRotator::ZeroRotator;
		FVector Scale = InDeltaScale;

		// Set legacy widget axis temporarily because InputWidgetDelta validates the axis in some crashes and crashes if it is not set
		ViewportClient->SetCurrentWidgetAxis(InAxisList);
		ViewportClient->InputWidgetDelta(ViewportClient->Viewport, InAxisList, Translate, Rot, Scale);
		ViewportClient->SetCurrentWidgetAxis(EAxisList::None);
	}
}

void UEditorTransformProxy::InputRotateDelta(const FRotator& InDeltaRotate, EAxisList::Type InAxisList)
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		FVector Translate = FVector::ZeroVector;
		FRotator Rot = InDeltaRotate;
		FVector Scale = FVector::ZeroVector;

		// Set legacy widget axis temporarily because InputWidgetDelta branches/overrides may expect it
		ViewportClient->SetCurrentWidgetAxis(InAxisList);
		ViewportClient->InputWidgetDelta(ViewportClient->Viewport, InAxisList, Translate, Rot, Scale);
		ViewportClient->SetCurrentWidgetAxis(EAxisList::None);
	}
}

UEditorTransformProxy* UEditorTransformProxy::CreateNew(const UEditorTransformGizmoContextObject* InContext)
{
	UEditorTransformProxy* NewProxy = NewObject<UEditorTransformProxy>();
	NewProxy->WeakContext = InContext;
	return NewProxy;
}

FEditorViewportClient* UEditorTransformProxy::GetViewportClient() const
{
	if (WeakContext.IsValid() && WeakContext->GetModeTools())
	{
		return WeakContext->GetModeTools()->GetFocusedViewportClient();	
	}
	return GLevelEditorModeTools().GetFocusedViewportClient();
}

void UEditorTransformProxy::SetCurrentAxis(const EAxisList::Type InAxisList) const
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		ViewportClient->SetCurrentWidgetAxis(InAxisList);
	}
}

#undef LOCTEXT_NAMESPACE
