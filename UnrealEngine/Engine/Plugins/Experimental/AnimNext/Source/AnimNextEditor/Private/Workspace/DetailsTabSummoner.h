// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class IDetailsView;

namespace UE::AnimNext::Editor
{
	class FWorkspaceEditor;
}

namespace UE::AnimNext::Editor
{

DECLARE_DELEGATE_OneParam(FOnDetailsViewCreated, TSharedRef<IDetailsView>);

class FDetailsTabSummoner : public FWorkflowTabFactory
{
public:
	FDetailsTabSummoner(TSharedPtr<FWorkspaceEditor> InHostingApp, FOnDetailsViewCreated InOnDetailsViewCreated);

	TSharedPtr<IDetailsView> GetDetailsView() const
	{
		return DetailsView;
	}

private:
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

	TSharedPtr<IDetailsView> DetailsView;
};

}