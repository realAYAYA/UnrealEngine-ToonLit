// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMasterLockitFactory.h"

#include "LiveLinkMasterLockitSource.h"
#include "LiveLinkMasterLockitConnectionSettings.h"

#define LOCTEXT_NAMESPACE "LiveLinkMasterLockitSourceFactory"

ULiveLinkMasterLockitSourceFactory::FBuildCreationPanelDelegate ULiveLinkMasterLockitSourceFactory::OnBuildCreationPanel;

FText ULiveLinkMasterLockitSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "MasterLockit");
}

FText ULiveLinkMasterLockitSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to a MasterLockit metadata server");
}

TSharedPtr<SWidget> ULiveLinkMasterLockitSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	if (OnBuildCreationPanel.IsBound())
	{
		return OnBuildCreationPanel.Execute(this, InOnLiveLinkSourceCreated);
	}
	return TSharedPtr<SWidget>();
}

TSharedPtr<ILiveLinkSource> ULiveLinkMasterLockitSourceFactory::CreateSource(const FString& ConnectionString) const
{
	FLiveLinkMasterLockitConnectionSettings Settings;
	if (!ConnectionString.IsEmpty())
	{
		FLiveLinkMasterLockitConnectionSettings::StaticStruct()->ImportText(*ConnectionString, &Settings, nullptr, EPropertyPortFlags::PPF_None, nullptr, FLiveLinkMasterLockitConnectionSettings::StaticStruct()->GetName(), true);
	}
	return MakeShared<FLiveLinkMasterLockitSource>(Settings);
}

FString ULiveLinkMasterLockitSourceFactory::CreateConnectionString(const FLiveLinkMasterLockitConnectionSettings& Settings)
{
	FString ConnectionString;
	FLiveLinkMasterLockitConnectionSettings::StaticStruct()->ExportText(ConnectionString, &Settings, nullptr, nullptr, EPropertyPortFlags::PPF_None, nullptr, true);
	return ConnectionString;
}

#undef LOCTEXT_NAMESPACE