// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCascadePreviewViewport.h"

#include "Cascade.h"
#include "CascadeParticleSystemComponent.h"
#include "CascadePreviewViewportClient.h"
#include "Containers/EnumAsByte.h"
#include "Delegates/Delegate.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Math/BoxSphereBounds.h"
#include "Math/UnrealMathSSE.h"
#include "Particles/ParticleSystemComponent.h"
#include "PreviewScene.h"
#include "RHIDefinitions.h"
#include "SCascadePreviewToolbar.h"
#include "Slate/SceneViewport.h"
#include "SlotBase.h"
#include "Types/SlateEnums.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SOverlay.h"

class SWidget;
struct FGeometry;



void SCascadePreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	JustTicked = true;
}


SCascadePreviewViewport::~SCascadePreviewViewport()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	Editor->OnPreviewFeatureLevelChanged().Remove(PreviewFeatureLevelChangedHandle);

	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = NULL;
	}
}

void SCascadePreviewViewport::Construct(const FArguments& InArgs)
{
	CascadePtr = InArgs._Cascade;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	// Restore last used feature level
	if (ViewportClient.IsValid())
	{
		UWorld* World = ViewportClient->GetPreviewScene().GetWorld();
		if (World != nullptr)
		{
			World->ChangeFeatureLevel(GWorld->GetFeatureLevel());
		}
	}

	// Use a delegate to inform the attached world of feature level changes.
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	PreviewFeatureLevelChangedHandle = Editor->OnPreviewFeatureLevelChanged().AddLambda([this](ERHIFeatureLevel::Type NewFeatureLevel)
		{
			if(ViewportClient.IsValid())
			{
				UWorld* World = ViewportClient->GetPreviewScene().GetWorld();
				if (World != nullptr)
				{
					World->ChangeFeatureLevel(NewFeatureLevel);
				}
			}
		});
}

void SCascadePreviewViewport::RefreshViewport()
{
	SceneViewport->Invalidate();
}

bool SCascadePreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible();
}

TSharedPtr<FSceneViewport> SCascadePreviewViewport::GetViewport() const
{
	return SceneViewport;
}

TSharedPtr<FCascadeEdPreviewViewportClient> SCascadePreviewViewport::GetViewportClient() const
{
	return ViewportClient;
}

TSharedPtr<SViewport> SCascadePreviewViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

TSharedRef<FEditorViewportClient> SCascadePreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FCascadeEdPreviewViewportClient(CascadePtr, SharedThis(this)));

	ViewportClient->bSetListenerPosition = false;

	ViewportClient->SetRealtime(true);
	ViewportClient->VisibilityDelegate.BindSP(this, &SCascadePreviewViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SCascadePreviewViewport::MakeViewportToolbar()
{
	return
	SNew(SCascadePreviewViewportToolBar)
		.CascadePtr(CascadePtr)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}


void SCascadePreviewViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// this widget will display the current viewed feature level
	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			BuildFeatureLevelWidget()
		];
}

void SCascadePreviewViewport::OnFocusViewportToSelection()
{
	UParticleSystemComponent* Component = CascadePtr.Pin()->GetParticleSystemComponent();

	if( Component )
	{
		ViewportClient->FocusViewportOnBox( Component->Bounds.GetBox() );
	}
}
