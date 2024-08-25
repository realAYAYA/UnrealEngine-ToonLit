// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubModule.h"
#include "LiveLinkTypes.h"
#include "Modules/ModuleManager.h"
#include "Session/LiveLinkHubSession.h"
#include "SLiveLinkHubSubjectView.h"

/** Controller responsible for handling the hub's subjects and creating the subject view. */
class FLiveLinkHubSubjectController
{
public:
	FLiveLinkHubSubjectController()
	{
		SubjectModel = MakeShared<FLiveLinkHubSubjectModel>();

		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		LiveLinkHubModule.GetSessionManager()->OnActiveSessionChanged().AddRaw(this, &FLiveLinkHubSubjectController::OnActiveSessionChanged);
	}

	~FLiveLinkHubSubjectController()
	{
		if (const FLiveLinkHubModule* LiveLinkHubModule = FModuleManager::Get().GetModulePtr<FLiveLinkHubModule>("LiveLinkHub"))
		{
			if (const TSharedPtr<ILiveLinkHubSessionManager> SessionManager = LiveLinkHubModule->GetSessionManager())
			{
				SessionManager->OnActiveSessionChanged().RemoveAll(this);
			}
		}
	}

	/** Create the widget for displaying a subject's settings. */
	TSharedRef<SWidget> MakeSubjectView()
	{
		return SAssignNew(SubjectsView, SLiveLinkHubSubjectView, SubjectModel.ToSharedRef())
			.OnRenameSubject_Raw(this, &FLiveLinkHubSubjectController::OnSubjectRenamed);
	}

	/** Set the displayed subject in the subject view. */
	void SetSubject(const FLiveLinkSubjectKey& Subject)
	{
		SubjectsView->SetSubject(Subject);
	}

	/** Handle modifying the session config for the specified subject. */
	void OnSubjectRenamed(const FLiveLinkSubjectKey& SubjectKey, FName NewName)
	{
		if (TSharedPtr<ILiveLinkHubSessionManager> SessionManager = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub()->GetSessionManager())
		{
			SessionManager->GetCurrentSession()->RenameSubject(SubjectKey, NewName);
		}
	}

	/** Handle updating the subject details when the session has been swapped out for a different one. */
	void OnActiveSessionChanged(const TSharedRef<ILiveLinkHubSession>& ActiveSession)
	{
		if (SubjectsView)
		{
			SubjectsView->ClearSubjectDetails();
		}
	}

private:
	/** View widget for the selected subject. */
	TSharedPtr<SLiveLinkHubSubjectView> SubjectsView;
	/** Model responsible for the subjects data. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
};
