// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Views/DashboardViewFactory.h"
#include "Widgets/SNullWidget.h"
// #include "LevelEditor.h"
// #include "SLevelViewport.h"

#define LOCTEXT_NAMESPACE "AudioInsights"


namespace UE::Audio::Insights
{
	class FViewportDashboardViewFactory : public IDashboardViewFactory
	{
	public:
		virtual FName GetName() const override
		{
			return "Viewport";
		}

		virtual FText GetDisplayName() const override
		{
			return LOCTEXT("AudioDashboard_ViewportTab_DisplayName", "Viewport");
		}

		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override
		{
			return EDefaultDashboardTabStack::Viewport;
		}

		virtual FSlateIcon GetIcon() const override
		{
			return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Viewport");
		}

		virtual TSharedRef<SWidget> MakeWidget() override
		{
			// TODO: This test currently hijacks the main level viewport which is not ideal.
			// Find way to make our own instance and bind to tab here.
			// FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			// TSharedPtr<SLevelViewport> LevelViewport = LevelEditor.GetFirstActiveLevelViewport();
			// ViewportContent = LevelViewport->AsShared();

			return SNullWidget::NullWidget;
		}
	};
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
