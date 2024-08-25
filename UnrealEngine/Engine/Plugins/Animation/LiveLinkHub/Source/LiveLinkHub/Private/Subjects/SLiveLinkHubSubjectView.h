// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Delegates/DelegateCombinations.h"
#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "LiveLinkHubSubjectModel.h"
#include "LiveLinkTypes.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

DECLARE_DELEGATE_TwoParams(FOnRenameLiveLinkHubSubject, const FLiveLinkSubjectKey& /*SubjectKey*/, FName /*NewName*/);

/**
 * Provides the UI that displays information about a livelink hub subject.
 */
class SLiveLinkHubSubjectView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLiveLinkHubSubjectView) {}
	SLATE_EVENT(FOnRenameLiveLinkHubSubject, OnRenameSubject)
	SLATE_END_ARGS()

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs, const TSharedRef<ILiveLinkHubSubjectModel>& InSubjectModel)
	{
		SubjectModel = InSubjectModel;
		OnRenameSubjectDelegate = InArgs._OnRenameSubject;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = NAME_None;
		DetailsViewArgs.bShowCustomFilterOption = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = false;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FStructureDetailsViewArgs StructureDetailsArgs;

		SubjectData = MakeShared<TStructOnScope<FLiveLinkHubSubjectProxy>>();

		SettingsDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsArgs, SubjectData);
		SettingsDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SLiveLinkHubSubjectView::OnSubjectPropertyModified);

		ChildSlot
		[
			SettingsDetailsView->GetWidget().ToSharedRef()
		];
	}

	/** Clear the subject details. */
	void ClearSubjectDetails()
	{
		SubjectData->Reset();
		SettingsDetailsView->SetStructureData(nullptr);
	}

	/** Set the subject to be displayed in the details view. */
	void SetSubject(const FLiveLinkSubjectKey& InSubjectKey)
	{
		SubjectKey = InSubjectKey;

		if (TOptional<FLiveLinkHubSubjectProxy> SubjectProxy = SubjectModel->GetSubjectConfig(InSubjectKey))
		{
			const FLiveLinkHubSubjectProxy& Proxy = *SubjectProxy;
			SubjectData->InitializeAs<FLiveLinkHubSubjectProxy>(*SubjectProxy);
			SettingsDetailsView->SetStructureData(SubjectData);
		}
	}

	/** Get the outbound name for a subject. */
	FString GetSubjectOutboundName(const FLiveLinkSubjectKey& InSubjectKey) const
	{
		if (TOptional<FLiveLinkHubSubjectProxy> SubjectProxy = SubjectModel->GetSubjectConfig(InSubjectKey))
		{
			return SubjectProxy->GetOutboundName().ToString();
		}
		return InSubjectKey.SubjectName.ToString();
	}

	/** Handler called when a subject property is modified, used to trigger a rename on the session's subject config. */
	void OnSubjectPropertyModified(const FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (SubjectData && PropertyChangedEvent.GetPropertyName() == FLiveLinkHubSubjectProxy::GetOutboundNamePropertyName())
		{
			if (FLiveLinkHubSubjectProxy* Proxy = SubjectData->Get())
			{
				OnRenameSubjectDelegate.ExecuteIfBound(SubjectKey, Proxy->GetOutboundName());
			}
		}
	}

private:
	/** Details for the selected subject. */
	TSharedPtr<IStructureDetailsView> SettingsDetailsView;
	/** Subject being shown. */
	FLiveLinkSubjectKey SubjectKey;
	/** Model that holds the data for a given subject. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
	/** Struct on scope used to display subject data in the structure details view. */
	TSharedPtr<TStructOnScope<FLiveLinkHubSubjectProxy>> SubjectData;
	/** Delegate called when the outbound name is changed by the user. */
	FOnRenameLiveLinkHubSubject OnRenameSubjectDelegate;
};
