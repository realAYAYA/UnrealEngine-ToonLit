// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorTransformProxy.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"


#define LOCTEXT_NAMESPACE "UEditorTransformProxy"

FTransform UEditorTransformProxy::GetTransform() const
{
	if (FEditorViewportClient* ViewportClient = GLevelEditorModeTools().GetFocusedViewportClient())
	{
		FVector Location = ViewportClient->GetWidgetLocation();
		FMatrix RotMatrix = ViewportClient->GetWidgetCoordSystem();
		return FTransform(FQuat(RotMatrix), Location, FVector::OneVector);
	}
	else
	{
		return FTransform::Identity;
	}
}

void UEditorTransformProxy::InputTranslateDelta(const FVector& InDeltaTranslate, EAxisList::Type InAxisList)
{
	if (FEditorViewportClient* ViewportClient = GLevelEditorModeTools().GetFocusedViewportClient())
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
	if (FEditorViewportClient* ViewportClient = GLevelEditorModeTools().GetFocusedViewportClient())
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
	if (FEditorViewportClient* ViewportClient = GLevelEditorModeTools().GetFocusedViewportClient())
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

#undef LOCTEXT_NAMESPACE
