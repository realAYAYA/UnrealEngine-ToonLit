// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutomationWindowStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"

FName FAutomationWindowStyle::StyleName("AutomationWindowStyle");
TUniquePtr<FAutomationWindowStyle> FAutomationWindowStyle::Inst(nullptr);

const FName& FAutomationWindowStyle::GetStyleSetName() const
{
	return StyleName;
}

const FAutomationWindowStyle& FAutomationWindowStyle::Get()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FAutomationWindowStyle>(new FAutomationWindowStyle);
	}
	return *(Inst.Get());
}

void FAutomationWindowStyle::Shutdown()
{
	Inst.Reset();
}

FAutomationWindowStyle::FAutomationWindowStyle()
	: FSlateStyleSet(StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	{
		FTextBlockStyle NormalText = FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");

		Set("Automation.Header", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", 12))
			.SetColorAndOpacity(FStyleColors::ForegroundHeader));

		Set("Automation.Normal", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", 9))
			.SetColorAndOpacity(FStyleColors::Foreground));

		Set("Automation.Warning", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", 9))
			.SetColorAndOpacity(FStyleColors::Warning));

		Set("Automation.Error", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", 9))
			.SetColorAndOpacity(FStyleColors::Error));

		Set("Automation.ReportHeader", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", 10))
			.SetColorAndOpacity(FStyleColors::ForegroundHeader));

		//state of individual tests
		Set("Automation.Success", new IMAGE_BRUSH("Automation/Success", Icon16x16));
		Set("Automation.Warning", new IMAGE_BRUSH("Automation/Warning", Icon16x16));
		Set("Automation.Fail", new IMAGE_BRUSH("Automation/Fail", Icon16x16));
		Set("Automation.InProcess", new IMAGE_BRUSH("Automation/InProcess", Icon16x16));
		Set("Automation.NotRun", new IMAGE_BRUSH("Automation/NotRun", Icon16x16, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f)));
		Set("Automation.Skipped", new IMAGE_BRUSH("Automation/NoSessionWarning", Icon16x16));
		Set("Automation.ParticipantsWarning", new IMAGE_BRUSH("Automation/ParticipantsWarning", Icon16x16));
		Set("Automation.Participant", new IMAGE_BRUSH("Automation/Participant", Icon16x16));

		//status as a regression test or not
		Set("Automation.SmokeTest", new IMAGE_BRUSH("Automation/SmokeTest", Icon16x16));
		Set("Automation.SmokeTestParent", new IMAGE_BRUSH("Automation/SmokeTestParent", Icon16x16));

		//run icons
		Set("AutomationWindow.RunTests", new IMAGE_BRUSH("Automation/RunTests", Icon40x40));
		Set("AutomationWindow.RefreshTests", new IMAGE_BRUSH("Automation/RefreshTests", Icon40x40));
		Set("AutomationWindow.FindWorkers", new IMAGE_BRUSH("Automation/RefreshWorkers", Icon40x40));
		Set("AutomationWindow.StopTests", new IMAGE_BRUSH("Automation/StopTests", Icon40x40));
		Set("AutomationWindow.RunTests.Small", new IMAGE_BRUSH("Automation/RunTests", Icon20x20));
		Set("AutomationWindow.RefreshTests.Small", new IMAGE_BRUSH("Automation/RefreshTests", Icon20x20));
		Set("AutomationWindow.FindWorkers.Small", new IMAGE_BRUSH("Automation/RefreshWorkers", Icon20x20));
		Set("AutomationWindow.StopTests.Small", new IMAGE_BRUSH("Automation/StopTests", Icon20x20));

		//filter icons
		Set("AutomationWindow.ErrorFilter", new IMAGE_BRUSH("Automation/ErrorFilter", Icon40x40));
		Set("AutomationWindow.WarningFilter", new IMAGE_BRUSH("Automation/WarningFilter", Icon40x40));
		Set("AutomationWindow.SmokeTestFilter", new IMAGE_BRUSH("Automation/SmokeTestFilter", Icon40x40));
		Set("AutomationWindow.DeveloperDirectoryContent", new IMAGE_BRUSH("Automation/DeveloperDirectoryContent", Icon40x40));
		Set("AutomationWindow.ExcludedTestsFilter", new IMAGE_BRUSH("Automation/ExcludedTestsFilter", Icon40x40));
		Set("AutomationWindow.ErrorFilter.Small", new IMAGE_BRUSH("Automation/ErrorFilter", Icon20x20));
		Set("AutomationWindow.WarningFilter.Small", new IMAGE_BRUSH("Automation/WarningFilter", Icon20x20));
		Set("AutomationWindow.SmokeTestFilter.Small", new IMAGE_BRUSH("Automation/SmokeTestFilter", Icon20x20));
		Set("AutomationWindow.DeveloperDirectoryContent.Small", new IMAGE_BRUSH("Automation/DeveloperDirectoryContent", Icon20x20));
		Set("AutomationWindow.TrackHistory", new IMAGE_BRUSH("Automation/TrackTestHistory", Icon40x40));

		//device group settings
		Set("AutomationWindow.GroupSettings", new IMAGE_BRUSH("Automation/Groups", Icon40x40));
		Set("AutomationWindow.GroupSettings.Small", new IMAGE_BRUSH("Automation/Groups", Icon20x20));

		//test backgrounds
		Set("AutomationWindow.GameGroupBorder", new BOX_BRUSH("Automation/GameGroupBorder", FMargin(4.0f / 16.0f)));
		Set("AutomationWindow.EditorGroupBorder", new BOX_BRUSH("Automation/EditorGroupBorder", FMargin(4.0f / 16.0f)));
	}
	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAutomationWindowStyle::~FAutomationWindowStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
