// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageContext.h"
#include "LiveLinkMessages.h"
#include "LiveLinkProvider.h"
#include "MessageEndpoint.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Subject that the application has told us about
struct FTrackedSubject
{
	// Ref skeleton to go with transform data
	FLiveLinkRefSkeleton RefSkeleton;

	// Bone transform data
	TArray<FTransform> Transforms;

	// Curve data
	TArray<FLiveLinkCurveElement> Curves;

	// MetaData for subject
	FLiveLinkMetaData MetaData;

	// Incrementing time (application time) for interpolation purposes
	double Time;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

struct FLiveLinkProvider : public ILiveLinkProvider
{
private:
	const FString ProviderName;
	const FString MachineName;

	TSharedPtr<class FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Lock to stop multiple threads accessing the CurrentPreset at the same time */
	mutable FCriticalSection CriticalSection;

	// Array of our current connections
	TArray<struct FTrackedAddress> ConnectedAddresses;

	// Cache of our current subject state
	TArray<struct FTrackedStaticData> StaticDatas;
	TArray<struct FTrackedFrameData> FrameDatas;
	TMap<FName, FTrackedSubject> Subjects;

	// Delegate to notify interested parties when the client sources have changed
	FLiveLinkProviderConnectionStatusChanged OnConnectionStatusChanged;

	void CreateMessageEndpoint(struct FMessageEndpointBuilder& EndpointBuilder);

	//Message bus message handlers
	void HandlePingMessage(const FLiveLinkPingMessage& Message,
						   const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleConnectMessage(const FLiveLinkConnectMessage& Message,
							  const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message,
						 const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	// End message bus message handlers

	// Validate our current connections
	void ValidateConnections();
	
	FTrackedSubject& GetTrackedSubject(const FName& SubjectName);

	void SendSubject(FName SubjectName, const struct FTrackedSubject& Subject);

	void SendSubjectFrame(FName SubjectName, const struct FTrackedSubject& Subject);

	// Get the cached data for the named subject
	FTrackedStaticData* GetLastSubjectStaticData(const FName& SubjectName);

	FTrackedFrameData* GetLastSubjectFrameData(const FName& SubjectName);

	void SetLastSubjectStaticData(FName SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData);

	void SetLastSubjectFrameData(FName SubjectName, FLiveLinkFrameDataStruct&& FrameData);

	// Clear a existing track subject
	void ClearTrackedSubject(const FName& SubjectName);

	void SendClearSubjectToConnections(FName SubjectName);

	void GetConnectedAddresses(TArray<FMessageAddress>& Addresses);

protected:
	template<typename MessageType>
	void SendMessage(MessageType* Message)
	{
		if (!Message)
		{
			return;
		}

		TArray<FMessageAddress> Addresses;
		GetConnectedAddresses(Addresses);
		if (Addresses.Num() != 0)
		{
			MessageEndpoint->Send(Message, Addresses);
		}
	}

	template<typename MessageType>
	void SendMessage(MessageType* Message, const FMessageAddress& Address)
	{
		if (!Message || !Address.IsValid())
		{
			return;
		}

		MessageEndpoint->Send(Message, Address);
	}

	template<typename MessageType>
	void Subscribe()
	{
		if (MessageEndpoint.IsValid())
		{
			MessageEndpoint->Subscribe<MessageType>();
		}
	}

	const FString& GetProviderName() const
	{
		return ProviderName;
	}
	
	const FString& GetMachineName() const
	{
		return MachineName;
	}

public:
	FLiveLinkProvider(const FString& InProviderName);

	FLiveLinkProvider(const FString& InProviderName, struct FMessageEndpointBuilder&& EndpointBuilder);

	virtual ~FLiveLinkProvider();

	virtual void UpdateSubject(const FName& SubjectName, const TArray<FName>& BoneNames, const TArray<int32>& BoneParents) override;

	virtual bool UpdateSubjectStaticData(const FName SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData) override;

	virtual void ClearSubject(const FName& SubjectName);

	virtual void RemoveSubject(const FName SubjectName) override;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual void UpdateSubjectFrame(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FLiveLinkCurveElement>& CurveData, double Time) override;

	virtual void UpdateSubjectFrame(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FLiveLinkCurveElement>& CurveData,
									const FLiveLinkMetaData& MetaData, double Time) override;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual bool UpdateSubjectFrameData(const FName SubjectName, FLiveLinkFrameDataStruct&& FrameData);

	virtual bool HasConnection() const override;

	virtual FDelegateHandle RegisterConnStatusChangedHandle(const FLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) override;

	virtual void UnregisterConnStatusChangedHandle(FDelegateHandle Handle);
};
