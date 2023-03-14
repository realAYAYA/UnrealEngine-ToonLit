// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "Framework/Docking/TabManager.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Docking/SDockTab.h"
#include "SAudioMeter.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Colors/SColorBlock.h"

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound
{
	namespace Editor
	{
		namespace TabFactory
		{
			namespace Names
			{
				const FName Analyzers = "MetasoundEditor_Analyzers";
				const FName Details = "MetasoundEditor_Details";
				const FName GraphCanvas = "MetasoundEditor_GraphCanvas";
				const FName Members = "MetasoundEditor_Members";
				const FName Palette = "MetasoundEditor_Palette";
				const FName Interfaces = "MetasoundEditor_Interfaces";
			}

			TSharedRef<SDockTab> CreateAnalyzersTab(TSharedPtr<SWidget> InAnalyzerWidget, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::Analyzers);

				return SNew(SDockTab)
				.Label(LOCTEXT("MetasoundAnalyzersTitle", "Analyzers"))
				[
					InAnalyzerWidget.ToSharedRef()
				];
			}

			TSharedRef<SDockTab> CreateGraphCanvasTab(TSharedPtr<SWidget> GraphEditor, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::GraphCanvas);

				TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
					.Label(LOCTEXT("MetasoundGraphCanvasTitle", "Viewport"));

				if (GraphEditor.IsValid())
				{
					SpawnedTab->SetContent(GraphEditor.ToSharedRef());
				}

				return SpawnedTab;
			}

			TSharedRef<SDockTab> CreateDetailsTab(TSharedPtr<IDetailsView> DetailsView, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::Details);

				return SNew(SDockTab)
					.Label(LOCTEXT("MetaSoundDetailsTitle", "Details"))
					[
						DetailsView.ToSharedRef()
					];
			}

			TSharedRef<SDockTab> CreateMembersTab(TSharedPtr<SGraphActionMenu> GraphActionMenu, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::Members);

				TSharedRef<SDockTab> NewTab = SNew(SDockTab)
				.Label(LOCTEXT("GraphMembersMenulTitle", "Members"))
				[
					GraphActionMenu.ToSharedRef()
				];

				if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
				{
					NewTab->SetTabIcon(MetasoundStyle->GetBrush("MetasoundEditor.Metasound.Icon"));
				}

				return NewTab;
			}

			TSharedRef<SDockTab> CreatePaletteTab(TSharedPtr<SMetasoundPalette> Palette, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::Palette);

				return SNew(SDockTab)
					.Label(LOCTEXT("MetaSoundPaletteTitle", "Palette"))
					[
						Palette.ToSharedRef()
					];
			}

			TSharedRef<SDockTab> CreateInterfacesTab(TSharedPtr<IDetailsView> InInterfacesDetails, const FSpawnTabArgs& Args)
			{
				check(Args.GetTabId() == Names::Interfaces);

				return SNew(SDockTab)
					.Label(LOCTEXT("MetasoundInterfacesDetailsTitle", "Interfaces"))
					[
						InInterfacesDetails.ToSharedRef()
					];
			}
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE // MetasoundEditor
