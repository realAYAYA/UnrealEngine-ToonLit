// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"

#include "HAL/ThreadSafeBool.h"
#include "MessageEndpoint.h"

class ULiveLinkRole;
class ULiveLinkSourceSettings;
struct FMessageEndpointBuilder;

class ILiveLinkClient;
struct FLiveLinkPongMessage;
struct FLiveLinkSubjectDataMessage;
struct FLiveLinkSubjectFrameMessage;
struct FLiveLinkHeartbeatMessage;
struct FLiveLinkClearSubject;

class LIVELINK_API FLiveLinkMessageBusSource : public ILiveLinkSource
{
public:
	/** Text description for a valid source. */
	static FText ValidSourceStatus();
	/** Text description for an invalid source. */
	static FText InvalidSourceStatus();
	/** Text description for a source that has timed out. */
	static FText TimeoutSourceStatus();

	FLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset);

	//~ Begin ILiveLinkSource interface
	virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) override;
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual void Update() override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override { return SourceType; }
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override;

	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;
	//~ End ILiveLinkSource interface

protected:
	// Returns the source name to uniquely identify it among the FLiveLinkMessageBusSource classes
	virtual const FName& GetSourceName() const;
	// This lets child classes the opportunity to add custom message handlers to the endpoint builder
	virtual void InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder);
	bool IsMessageEndpointConnected() const { return ConnectionAddress.IsValid() && MessageEndpoint.IsValid() && MessageEndpoint->IsConnected(); }

	// Initialize the static data and send it to the clients
	virtual void InitializeAndPushStaticData_AnyThread(FName SubjectName,
													   TSubclassOf<ULiveLinkRole> SubjectRole,
													   const FLiveLinkSubjectKey& SubjectKey,
													   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													   UScriptStruct* MessageTypeInfo);
	// Initialize the frame data and send it to the clients
	virtual void InitializeAndPushFrameData_AnyThread(FName SubjectName,
													  const FLiveLinkSubjectKey& SubjectKey,
													  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													  UScriptStruct* MessageTypeInfo);

	// Allows derived classes to provide their own timeout duration before a source is removed because the heartbeat timeout was hit
	virtual double GetDeadSourceTimeout() const;

	// Send the static data to the clients
	void PushClientSubjectStaticData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData);
	// Send the frame data to the clients
	void PushClientSubjectFrameData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, FLiveLinkFrameDataStruct&& FrameData);

	// Send connect message to the provider and start the heartbeat emitter
	virtual void SendConnectMessage();

	// Send a message through the endpoint
	template<typename MessageType>
	void SendMessage(MessageType* Message)
	{
		if (!Message || !IsMessageEndpointConnected())
		{
			return;
		}

		MessageEndpoint->Send(Message, ConnectionAddress);
	}

	// Start the heartbeat emitter for this connection
	void StartHeartbeatEmitter();
protected:
	// Message bus endpoint responsible for communication with the livelink provider
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	// Connection address of the livelink provider 
	FMessageAddress ConnectionAddress;

	// Current Validity of Source
	FThreadSafeBool bIsValid;

private:
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> CreateAndInitializeMessageEndpoint();

	//~ Message bus message handlers
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	void HandleSubjectData(const FLiveLinkSubjectDataMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleSubjectFrame(const FLiveLinkSubjectFrameMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleClearSubject(const FLiveLinkClearSubject& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void InternalHandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	//~ End Message bus message handlers

	// Threadsafe update of the last active time
	FORCEINLINE void UpdateConnectionLastActive();

	ILiveLinkClient* Client;

	// Our identifier in LiveLink
	FGuid SourceGuid;

	// List of the roles available when the bus was opened
	TArray<TWeakObjectPtr<ULiveLinkRole>> RoleInstances;

	FText SourceType;
	FText SourceMachineName;

	// Time we last received anything 
	double ConnectionLastActive;

	// Critical section to allow for threadsafe updating of the connection time
	FCriticalSection ConnectionLastActiveSection;

	// Offset between sender's machine engine time and receiver's machine engine time
	double MachineTimeOffset;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "LiveLinkRole.h"
#endif
