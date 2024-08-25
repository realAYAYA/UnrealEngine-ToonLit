// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaLevelViewport.h"
#include "AvaViewportPostProcessManager.h"
#include "AvaViewportSettings.h"
#include "ViewportClient/AvaLevelViewportClient.h"
#include "Visualizers/IAvaViewportBoundingBoxVisualizer.h"
#include "Visualizers/IAvaViewportPostProcessVisualizer.h"

void SAvaLevelViewport::ExecuteToggleChildActorLock()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	ViewportClient->SetChildActorsLocked(!ViewportClient->AreChildActorsLocked());
}

bool SAvaLevelViewport::IsPostProcessNoneEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::None;
}

bool SAvaLevelViewport::CanTogglePostProcessNone() const
{
	return GetAvaLevelViewportClient().IsValid();
}

void SAvaLevelViewport::ExecuteTogglePostProcessNone()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::None);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessBackgroundEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::Background;
}

bool SAvaLevelViewport::CanTogglePostProcessBackground() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessBackgroundEnabled())
	{
		return true;
	}

	if (TSharedPtr<IAvaViewportPostProcessVisualizer> BackgroundVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::Background))
	{
		return BackgroundVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessBackground()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::Background);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessChannelRedEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::RedChannel;
}

bool SAvaLevelViewport::CanTogglePostProcessChannelRed() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessChannelRedEnabled())
	{
		return true;
	}

	if (TSharedPtr<IAvaViewportPostProcessVisualizer> ChannelRedVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::RedChannel))
	{
		return ChannelRedVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessChannelRed()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::RedChannel);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessChannelGreenEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::GreenChannel;
}

bool SAvaLevelViewport::CanTogglePostProcessChannelGreen() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessChannelGreenEnabled())
	{
		return true;
	}

	if (TSharedPtr<IAvaViewportPostProcessVisualizer> ChannelGreenVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::GreenChannel))
	{
		return ChannelGreenVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessChannelGreen()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::GreenChannel);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessChannelBlueEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::BlueChannel;
}

bool SAvaLevelViewport::CanTogglePostProcessChannelBlue() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessChannelBlueEnabled())
	{
		return true;
	}

	if (TSharedPtr<IAvaViewportPostProcessVisualizer> ChannelBlueVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::BlueChannel))
	{
		return ChannelBlueVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessChannelBlue()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::BlueChannel);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessChannelAlphaEnabled() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::AlphaChannel;
}

bool SAvaLevelViewport::CanTogglePostProcessChannelAlpha() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessChannelAlphaEnabled())
	{
		return true;
	}

	if (TSharedPtr<IAvaViewportPostProcessVisualizer> ChannelAlphaVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::AlphaChannel))
	{
		return ChannelAlphaVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessChannelAlpha()
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (!ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();

	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::AlphaChannel);
	ViewportClient->Invalidate();

	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::IsPostProcessCheckerboardEnabled() const
{
	const TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid() || !ViewportClient->GetPostProcessManager().IsValid())
	{
		// Make sure none is toggled on if there's an error.
		return true;
	}

	return ViewportClient->GetPostProcessManager()->GetType() == EAvaViewportPostProcessType::Checkerboard;
}

bool SAvaLevelViewport::CanTogglePostProcessCheckerboard() const
{
	const TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid() || !ViewportClient->GetPostProcessManager().IsValid())
	{
		return false;
	}

	if (IsPostProcessCheckerboardEnabled())
	{
		return true;
	}

	if (const TSharedPtr<IAvaViewportPostProcessVisualizer> CheckerboardVisualizer = ViewportClient->GetPostProcessManager()->GetVisualizer(EAvaViewportPostProcessType::Checkerboard))
	{
		return CheckerboardVisualizer->CanActivate(/* bInSilent */ true);
	}

	return false;
}

void SAvaLevelViewport::ExecuteTogglePostProcessCheckerboard()
{
	const TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid() || !ViewportClient->GetPostProcessManager().IsValid())
	{
		return;
	}

	BeginPostProcessInfoTransaction();
	ViewportClient->GetPostProcessManager()->SetType(EAvaViewportPostProcessType::Checkerboard);
	ViewportClient->Invalidate();
	EndPostProcessInfoTransaction();
}

bool SAvaLevelViewport::CanToggleOverlay() const
{
	return true;
}

void SAvaLevelViewport::ExecuteToggleOverlay()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bEnableViewportOverlay = !AvaViewportSettings->bEnableViewportOverlay;
		AvaViewportSettings->SaveConfig();
		ApplySettings(AvaViewportSettings);
	}
}

bool SAvaLevelViewport::CanToggleSafeFrames() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay;
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleSafeFrames()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bSafeFramesEnabled = !AvaViewportSettings->bSafeFramesEnabled;
		AvaViewportSettings->SaveConfig();
		ApplySettings(AvaViewportSettings);
	}
}

bool SAvaLevelViewport::CanToggleBoundingBox() const
{
	TSharedPtr<FAvaLevelViewportClient> ViewportClient = GetAvaLevelViewportClient();

	if (!ViewportClient.IsValid())
	{
		return false;
	}

	return ViewportClient->GetBoundingBoxVisualizer()->GetOptimizationState() != EAvaViewportBoundingBoxOptimizationState::RenderNothing;
}

void SAvaLevelViewport::ExecuteToggleBoundingBox()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bEnableBoundingBoxes = !AvaViewportSettings->bEnableBoundingBoxes;
		AvaViewportSettings->SaveConfig();
		ApplySettings(AvaViewportSettings);
	}
}

bool SAvaLevelViewport::CanToggleGrid() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay;
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleGrid()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bGridEnabled = !AvaViewportSettings->bGridEnabled;
		AvaViewportSettings->SaveConfig();
		ApplySettings(AvaViewportSettings);
	}
}

bool SAvaLevelViewport::CanToggleGridAlwaysVisible() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGridEnabled;
	}

	return false;
}

bool SAvaLevelViewport::IsGridAlwaysVisible() const
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bGridAlwaysVisible;
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleGridAlwaysVisible()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bGridAlwaysVisible = !AvaViewportSettings->bGridAlwaysVisible;
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanIncreaseGridSize() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGridEnabled;
	}

	return false;
}

void SAvaLevelViewport::ExecuteIncreaseGridSize()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->GridSize = FMath::Min(AvaViewportSettings->GridSize + 1, 256);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanDecreaseGridSize() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGridEnabled;
	}

	return false;
}

void SAvaLevelViewport::ExecuteDecreaseGridSize()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->GridSize = FMath::Max(AvaViewportSettings->GridSize - 1, 1);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanChangeGridSize() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGridEnabled;
	}

	return false;
}

void SAvaLevelViewport::ExecuteSetGridSize(int32 InNewSize, bool bInCommit)
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->GridSize = FMath::Clamp(InNewSize, 1, 256);

		if (bInCommit)
		{
			AvaViewportSettings->SaveConfig();
		}
	}
}

bool SAvaLevelViewport::CanToggleSnapping() const
{
	return true;
}

void SAvaLevelViewport::ExecuteToggleSnapping()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->SetSnapState(AvaViewportSettings->GetSnapState() ^ EAvaViewportSnapState::Global);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanToggleGridSnapping() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return EnumHasAllFlags(AvaViewportSettings->GetSnapState(), EAvaViewportSnapState::Global);
	}

	return false;
}

bool SAvaLevelViewport::IsGridSnappingEnabled() const
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->HasSnapState(EAvaViewportSnapState::Grid);
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleGridSnapping()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->SetSnapState(AvaViewportSettings->GetSnapState() ^ EAvaViewportSnapState::Grid);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanToggleScreenSnapping() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return EnumHasAllFlags(AvaViewportSettings->GetSnapState(), EAvaViewportSnapState::Global);
	}

	return false;
}

bool SAvaLevelViewport::IsScreenSnappingEnabled() const
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->HasSnapState(EAvaViewportSnapState::Screen);
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleScreenSnapping()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->SetSnapState(AvaViewportSettings->GetSnapState() ^ EAvaViewportSnapState::Screen);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanToggleActorSnapping() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return EnumHasAllFlags(AvaViewportSettings->GetSnapState(), EAvaViewportSnapState::Global);
	}

	return false;
}

bool SAvaLevelViewport::IsActorSnappingEnabled() const
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->HasSnapState(EAvaViewportSnapState::Actor);
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleActorSnapping()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->SetSnapState(AvaViewportSettings->GetSnapState() ^ EAvaViewportSnapState::Actor);
		AvaViewportSettings->SaveConfig();
	}
}

bool SAvaLevelViewport::CanToggleGuides() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay;
	}

	return false;
}

void SAvaLevelViewport::ExecuteToggleGuides()
{
	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bGuidesEnabled = !AvaViewportSettings->bGuidesEnabled;
		AvaViewportSettings->SaveConfig();
		ApplySettings(AvaViewportSettings);
	}
}

bool SAvaLevelViewport::CanAddHorizontalGuide() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGuidesEnabled;
	}

	return false;
}

void SAvaLevelViewport::ExecuteAddHorizontalGuide()
{
	AddGuide(EOrientation::Orient_Horizontal, 0.5f);
}

bool SAvaLevelViewport::CanAddVerticalGuide() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay && AvaViewportSettings->bGuidesEnabled;
	}

	return false;
}

void SAvaLevelViewport::ExecuteAddVerticalGuide()
{
	AddGuide(EOrientation::Orient_Vertical, 0.5f);
}
