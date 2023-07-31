// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "XmppJingle/XmppJingle.h"
#include "XmppChat.h"
#include "Containers/Ticker.h"

#if WITH_XMPP_JINGLE

/**
 * Xmpp chat implementation using webrtc lib tasks/signals
 */
class FXmppChatJingle
	: public IXmppChat
	, public sigslot::has_slots<>
	, public FTSTickerObjectBase
{
public:

	// IXmppChat

	virtual bool SendChat(const FXmppUserJid& RecipientId, const FString& Message) override;
	virtual FOnXmppChatReceived& OnReceiveChat() override { return OnXmppChatReceivedDelegate; }

	// FTSTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FXmppChatJingle

	FXmppChatJingle(class FXmppConnectionJingle& InConnection);
	virtual ~FXmppChatJingle();

private:

	/** callback on pump thread when new chat has been received */
	void OnSignalChatReceived(const class FXmppChatMessageJingle& Chat);

	// called on pump thread
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);

	// completion delegates
	FOnXmppChatReceived OnXmppChatReceivedDelegate;

	/** task used to receive stanzas of type=chat from xmpp pump thread */
	class FXmppChatReceiveTask* ChatRcvTask;
	/** list of incoming chat messages */
	TQueue<class FXmppChatMessage*> ReceivedChatQueue;

	/** task used to send stanzas of type=chat via xmpp pump thread */
	class FXmppChatSendTask* ChatSendTask;
	/** list of outgoing chat messages */
	TQueue<class FXmppChatMessageJingle*> SendChatQueue;

	/** Number of chat messages received in a given interval */
	int32 NumReceivedChat;
	/** Number of chat messages sent in a given interval */
	int32 NumSentChat;

	class FXmppConnectionJingle& Connection;
	friend class FXmppConnectionJingle; 
};

#endif //WITH_XMPP_JINGLE
