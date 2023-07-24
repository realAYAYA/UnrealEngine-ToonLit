// Copyright Epic Games, Inc. All Rights Reserved.

#include "CorrectivesEditorTabSummoner.h"
#include "SCorrectivesViewer.h"
#include "PoseCorrectivesEditorToolkit.h"
#include "PoseCorrectivesEditorController.h"
#include "IDocumentation.h"


#define LOCTEXT_NAMESPACE "CorrectivesEditorTabSummoner"

const FName FCorrectivesEditorTabSummoner::TabID(TEXT("CorrectivesEditorTabSummoner"));

FCorrectivesEditorTabSummoner::FCorrectivesEditorTabSummoner(const TSharedRef<FPoseCorrectivesEditorToolkit>& InPoseCorrectivesToolkit)
	: FWorkflowTabFactory(TabID, InPoseCorrectivesToolkit)
	, PoseCorrectivesToolkit(InPoseCorrectivesToolkit)
{
	bIsSingleton = true; // only allow a single instance of this tab
	
	TabLabel = LOCTEXT("CorrectivesEditor_TabLabel", "Correctives");
	TabIcon = FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Correctives.TabIcon");

	ViewMenuDescription = LOCTEXT("CorrectivesEditor_ViewMenu_Desc", "Correctives");
	ViewMenuTooltip = LOCTEXT("CorrectivesEditor_ViewMenu_ToolTip", "Show the Pose Correctives Tab");
}

TSharedPtr<SToolTip> FCorrectivesEditorTabSummoner::CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const
{
	return  IDocumentation::Get()->CreateToolTip(LOCTEXT("PoseCorrectivesTooltip", "The Pose Correctives tab lets you view the list of correctives."), NULL, TEXT("Shared/Editors/Persona"), TEXT("PoseCorrectives_Window"));
}

TSharedRef<SWidget> FCorrectivesEditorTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	const TSharedRef<IPersonaPreviewScene>& PreviewScene = PoseCorrectivesToolkit.Pin()->GetPersonaToolkit()->GetPreviewScene();
	return SNew(SCorrectivesViewer, PoseCorrectivesToolkit.Pin()->GetController(), PreviewScene);
}

#undef LOCTEXT_NAMESPACE 
