// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaViewportPostProcessManager.h"

#include "AvaTypeSharedPointer.h"
#include "AvaViewportDataSubsystem.h"
#include "AvaVisibleArea.h"
#include "Interaction/AvaCameraZoomController.h"
#include "Viewport/Interaction/AvaViewportPostProcessInfo.h"
#include "Viewport/Interaction/IAvaViewportDataProvider.h"
#include "Viewport/Interaction/IAvaViewportDataProxy.h"
#include "ViewportClient/IAvaViewportClient.h"
#include "Visualizers/AvaViewportBackgroundVisualizer.h"
#include "Visualizers/AvaViewportChannelVisualizer.h"
#include "Visualizers/AvaViewportCheckerboardVisualizer.h"

FAvaViewportPostProcessManager::FAvaViewportPostProcessManager(TSharedRef<IAvaViewportClient> InAvaViewportClient)
{
	AvaViewportClientWeak = InAvaViewportClient;

	Visualizers.Emplace(EAvaViewportPostProcessType::Background,   MakeShared<FAvaViewportBackgroundVisualizer>(InAvaViewportClient));
	Visualizers.Emplace(EAvaViewportPostProcessType::RedChannel,   MakeShared<FAvaViewportChannelVisualizer>(InAvaViewportClient, EAvaViewportPostProcessType::RedChannel));
	Visualizers.Emplace(EAvaViewportPostProcessType::GreenChannel, MakeShared<FAvaViewportChannelVisualizer>(InAvaViewportClient, EAvaViewportPostProcessType::GreenChannel));
	Visualizers.Emplace(EAvaViewportPostProcessType::BlueChannel,  MakeShared<FAvaViewportChannelVisualizer>(InAvaViewportClient, EAvaViewportPostProcessType::BlueChannel));
	Visualizers.Emplace(EAvaViewportPostProcessType::AlphaChannel, MakeShared<FAvaViewportChannelVisualizer>(InAvaViewportClient, EAvaViewportPostProcessType::AlphaChannel));
	Visualizers.Emplace(EAvaViewportPostProcessType::Checkerboard, MakeShared<FAvaViewportCheckerboardVisualizer>(InAvaViewportClient));
}

FAvaViewportPostProcessInfo* FAvaViewportPostProcessManager::GetPostProcessInfo() const
{
	TSharedPtr<IAvaViewportClient> AvaViewportClient = AvaViewportClientWeak.Pin();

	if (!AvaViewportClient.IsValid())
	{
		return nullptr;
	}

	UAvaViewportDataSubsystem* DataSubsystem = UAvaViewportDataSubsystem::Get(AvaViewportClient->GetViewportWorld());

	if (!DataSubsystem)
	{
		return nullptr;
	}

	if (FAvaViewportData* Data = DataSubsystem->GetData())
	{
		return &Data->PostProcessInfo;
	}

	return nullptr;
}

void FAvaViewportPostProcessManager::UpdateSceneView(FSceneView* InSceneView)
{
	TSharedPtr<IAvaViewportClient> AvaViewportClient = AvaViewportClientWeak.Pin();

	if (!AvaViewportClient.IsValid())
	{
		return;
	}

	TSharedPtr<FAvaViewportPostProcessVisualizer> Visualizer = UE::AvaCore::CastSharedPtr<FAvaViewportPostProcessVisualizer>(GetActiveVisualizer());

	if (!Visualizer.IsValid())
	{
		return;
	}

	FVector2f PanOffset = FVector2f::ZeroVector;

	if (TSharedPtr<FAvaCameraZoomController> ZoomController = AvaViewportClient->GetZoomController())
	{
		PanOffset = ZoomController->GetPanOffsetFraction() * AvaViewportClient->GetViewportSize() * -1.f;
	}

	Visualizer->UpdateForViewport(
		AvaViewportClient->GetZoomedVisibleArea(),
		AvaViewportClient->GetViewportWidgetSize(),
		PanOffset
	);

	Visualizer->ApplyToSceneView(InSceneView);
}

void FAvaViewportPostProcessManager::LoadPostProcessInfo()
{
	TSharedPtr<FAvaViewportPostProcessVisualizer> Visualizer = UE::AvaCore::CastSharedPtr<FAvaViewportPostProcessVisualizer>(GetActiveVisualizer());

	if (!Visualizer.IsValid())
	{
		return;
	}

	Visualizer->LoadPostProcessInfo();
}

EAvaViewportPostProcessType FAvaViewportPostProcessManager::GetType() const
{
	if (FAvaViewportPostProcessInfo* PostProcessInfo = GetPostProcessInfo())
	{
		return PostProcessInfo->Type;
	}

	return EAvaViewportPostProcessType::None;
}

void FAvaViewportPostProcessManager::SetType(EAvaViewportPostProcessType InType)
{
	FAvaViewportPostProcessInfo* PostProcessInfo = GetPostProcessInfo();

	if (!PostProcessInfo)
	{
		return;
	}

	if (PostProcessInfo->Type == InType)
	{
		return;
	}

	TSharedPtr<FAvaViewportPostProcessVisualizer> NewVisualizer = UE::AvaCore::CastSharedPtr<FAvaViewportPostProcessVisualizer>(GetVisualizer(InType));

	if (NewVisualizer.IsValid() && !NewVisualizer->CanActivate(/* bInSilent */ false))
	{
		return;
	}

	if (TSharedPtr<FAvaViewportPostProcessVisualizer> CurrentVisualizer = UE::AvaCore::CastSharedPtr<FAvaViewportPostProcessVisualizer>(GetActiveVisualizer()))
	{
		CurrentVisualizer->OnDeactivate();
	}
	
	PostProcessInfo->Type = InType;

	if (NewVisualizer.IsValid())
	{
		NewVisualizer->OnActivate();
	}
}

float FAvaViewportPostProcessManager::GetOpacity()
{
	if (FAvaViewportPostProcessInfo* PostProcessInfo = GetPostProcessInfo())
	{
		return PostProcessInfo->Opacity;
	}

	return 1.f;
}

void FAvaViewportPostProcessManager::SetOpacity(float InOpacity)
{
	if (FAvaViewportPostProcessInfo* PostProcessInfo = GetPostProcessInfo())
	{
		PostProcessInfo->Opacity = InOpacity;
		LoadPostProcessInfo();
	}
}

TSharedPtr<IAvaViewportPostProcessVisualizer> FAvaViewportPostProcessManager::GetVisualizer(EAvaViewportPostProcessType InType) const
{
	if (const TSharedPtr<IAvaViewportPostProcessVisualizer>* VisualizerPtr = Visualizers.Find(InType))
	{
		return *VisualizerPtr;
	}

	return nullptr;
}

TSharedPtr<IAvaViewportPostProcessVisualizer> FAvaViewportPostProcessManager::GetActiveVisualizer() const
{
	if (FAvaViewportPostProcessInfo* PostProcessInfo = GetPostProcessInfo())
	{
		return GetVisualizer(PostProcessInfo->Type);
	}

	return nullptr;
}
	