// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "LiveLinkTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkHubSubjectSessionConfig.generated.h"

/** Holds information and config for a subject for the duration of a livelink hub session.  */
USTRUCT()
struct FLiveLinkHubSubjectProxy
{
public:
	GENERATED_BODY()

	/** Initialize from a subject key. */
	void Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource);

	/** Get the outbound name for this subject, allows  */
	FName GetOutboundName() const;

	/** Change the outbound name for this subject proxy. */
	void SetOutboundName(FName NewName);

	/** Get the outbound name property name. */
	static FName GetOutboundNamePropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FLiveLinkHubSubjectProxy, OutboundName);
	}

private:
	/** Notify clients that the old subject should be deleted and replaced with a new one with the updated name. */
	void NotifyRename();

private:
	/** Name of  the subject, */
	UPROPERTY(VisibleAnywhere, Category = "Subject Details")
	FString SubjectName;

	/** Name override that will be transmitted to clients instead of the subject name. */
	UPROPERTY(EditAnywhere, Category = "Subject Details")
	FString OutboundName;

	/** Source that contains the subject. */
	UPROPERTY(VisibleAnywhere, Category = "Subject Details")
	FString Source;

	/** SubjectKey for this subject, */
	UPROPERTY()
	FLiveLinkSubjectKey SubjectKey;

	/**
	 * If this is set, then the outbound name is currently undergoing a rename,
	 * If this is not set, then the OutboundName is committed.
	 */
	bool bPendingOutboundNameChange = false;

	/* Previous outbound name to be used for noticing clients to remove this entry from their subject list. */
	FName PreviousOutboundName;
};

/** Config pertaining to livelink hub subjects for a given session. */
USTRUCT()
struct FLiveLinkHubSubjectSessionConfig
{
public:
	GENERATED_BODY()

	void Initialize();

	/** Get the config for a livelinkhub subject. */
	TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const;

	/** Change the outbound name of a subject for the current session. */
	void RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName);

private:
	/** Settings for subjects displayed in the livelink hub. */
	UPROPERTY()
	TMap<FLiveLinkSubjectKey, FLiveLinkHubSubjectProxy> SubjectProxies;

	friend class FLiveLinkHubSession;
};
