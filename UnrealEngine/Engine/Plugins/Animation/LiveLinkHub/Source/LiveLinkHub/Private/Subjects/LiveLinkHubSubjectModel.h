// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubModule.h"
#include "LiveLinkHubSubjectSessionConfig.h"
#include "LiveLinkTypes.h"
#include "Session/LiveLinkHubSession.h"


/** Subject Model used by the view to access the subject settings. */
class ILiveLinkHubSubjectModel
{
public:
	virtual ~ILiveLinkHubSubjectModel() = default;

	/** Get the settings for a livelinkhub subject. */
	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const = 0;
};

/** Implementation of the ILiveLinkHubSubjectModel. */
class FLiveLinkHubSubjectModel : public ILiveLinkHubSubjectModel
{
public:
	/** Get the settings for a livelinkhub subject. */
	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const override
	{
		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		if (const TSharedPtr<ILiveLinkHubSession> Session = LiveLinkHubModule.GetSessionManager()->GetCurrentSession())
		{
			return Session->GetSubjectConfig(InSubject);
		}
		return {};
	}
};
