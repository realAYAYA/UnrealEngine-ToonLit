// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangePipelineHelper.h"

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InterchangePipelineBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

namespace UE::Interchange::PipelineHelper
{
	void ShowModalDialog(TSharedRef<SInterchangeBaseConflictWidget> ConflictWidget, const FText& Title, const FVector2D& WindowSize)
	{
		TSharedPtr<SWindow> ParentWindow = FGlobalTabmanager::Get()->GetRootWindow();

		const float FbxImportWindowWidth = WindowSize.X > 150.0f ? WindowSize.X : 150.0f;
		const float FbxImportWindowHeight = WindowSize.Y > 50.0f ? WindowSize.Y : 50.0f;
		FVector2D FbxImportWindowSize = FVector2D(FbxImportWindowWidth, FbxImportWindowHeight); // Max window size it can get based on current slate

		FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
		FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
		FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

		float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
		FbxImportWindowSize *= ScaleFactor;

		FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - FbxImportWindowSize) / 2.0f) / ScaleFactor;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.SizingRule(ESizingRule::UserSized)
			.AutoCenter(EAutoCenter::None)
			.ClientSize(FbxImportWindowSize)
			.ScreenPosition(WindowPosition);

		ConflictWidget->SetWidgetWindow(Window);
		Window->SetContent
		(
			ConflictWidget
		);

		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}
}
