// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class IDetailsView;

namespace UE::MLDeformer
{
	class FMLDeformerEditorToolkit;

	struct FMLDeformerVizSettingsTabSummoner : public FWorkflowTabFactory
	{
	public:
		static const FName TabID;

		FMLDeformerVizSettingsTabSummoner(const TSharedRef<FMLDeformerEditorToolkit>& InEditor);
		virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
		virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

	protected:
		TWeakPtr<FMLDeformerEditorToolkit> Editor = nullptr;
		TSharedPtr<IDetailsView> DetailsView = nullptr;
	};
}	// namespace UE::MLDeformer
