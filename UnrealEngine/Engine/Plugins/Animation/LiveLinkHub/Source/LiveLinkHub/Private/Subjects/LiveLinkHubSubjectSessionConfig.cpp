// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubSubjectSessionConfig.h"

#include "Async/Async.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"


void FLiveLinkHubSubjectSessionConfig::Initialize()
{
	FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	constexpr bool bIncludeDisabledSubject = true;
	constexpr bool bIncludeVirtualSubject = true;

	for (const FLiveLinkSubjectKey& SubjectKey : LiveLinkClient.GetSubjects(bIncludeDisabledSubject, bIncludeVirtualSubject))
	{
		FLiveLinkHubSubjectProxy SubjectSettings;
		SubjectSettings.Initialize(SubjectKey, LiveLinkClient.GetSourceType(SubjectKey.Source).ToString());

		SubjectProxies.Add(SubjectKey, MoveTemp(SubjectSettings));
	}
}

TOptional<FLiveLinkHubSubjectProxy> FLiveLinkHubSubjectSessionConfig::GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const
{
	TOptional<FLiveLinkHubSubjectProxy> Settings;

    if (const FLiveLinkHubSubjectProxy* SettingsPtr = SubjectProxies.Find(InSubject))
    {
    	Settings = *SettingsPtr;
    }

    return Settings;
}

void FLiveLinkHubSubjectSessionConfig::RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName)
{
	if (FLiveLinkHubSubjectProxy* Proxy = SubjectProxies.Find(SubjectKey))
	{
		Proxy->SetOutboundName(NewName);
	}
}

void FLiveLinkHubSubjectProxy::Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource)
{
	SubjectName = InSubjectKey.SubjectName.Name.ToString();
	SubjectKey = InSubjectKey;
	OutboundName = InSubjectKey.SubjectName.Name.ToString();
	Source = MoveTemp(InSource);
}

FName FLiveLinkHubSubjectProxy::GetOutboundName() const
{
	if (bPendingOutboundNameChange)
	{
		return PreviousOutboundName;
	}

	return *OutboundName;
}

void FLiveLinkHubSubjectProxy::SetOutboundName(FName NewName)
{
	PreviousOutboundName = *OutboundName;
	OutboundName = *NewName.ToString();

	NotifyRename();
}

void FLiveLinkHubSubjectProxy::NotifyRename()
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	if (TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHubModule.GetLiveLinkProvider())
	{
		if (PreviousOutboundName != *OutboundName)
		{
			bPendingOutboundNameChange = true;

			Provider->SendClearSubjectToConnections(PreviousOutboundName);

			// Re-send the last static data with the new name.
			TPair<UClass*, FLiveLinkStaticDataStruct*> StaticData = Provider->GetLastSubjectStaticDataStruct(PreviousOutboundName);
			if (StaticData.Key && StaticData.Value)
			{
				FLiveLinkStaticDataStruct StaticDataCopy;
				StaticDataCopy.InitializeWith(*StaticData.Value);

				Provider->UpdateSubjectStaticData(*OutboundName, StaticData.Key, MoveTemp(StaticDataCopy));
			}

			// Then clear the old static data entry in the provider.
			Provider->RemoveSubject(PreviousOutboundName);

			bPendingOutboundNameChange = false;
			PreviousOutboundName = *OutboundName;
		}
	}
}
