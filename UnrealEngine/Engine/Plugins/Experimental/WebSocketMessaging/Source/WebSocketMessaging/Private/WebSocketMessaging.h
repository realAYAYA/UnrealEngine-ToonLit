// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWebSocketMessaging, Log, All);

namespace WebSocketMessaging
{
	namespace Tag
	{
		static constexpr const TCHAR* Sender = TEXT("Sender");
		static constexpr const TCHAR* Recipients = TEXT("Recipients");
		static constexpr const TCHAR* Expiration = TEXT("Expiration");
		static constexpr const TCHAR* TimeSent = TEXT("TimeSent");
		static constexpr const TCHAR* Annotations = TEXT("Annotations");
		static constexpr const TCHAR* Scope = TEXT("Scope");
		static constexpr const TCHAR* MessageType = TEXT("MessageType");
		static constexpr const TCHAR* Message = TEXT("Message");
	}

	namespace Header
	{
		static constexpr const TCHAR* TransportId = TEXT("UE-MessageBus-TransportId");
	}
}

class FWebSocketMessagingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool HandleSettingsSaved();

	virtual void InitializeBridge();
	virtual void ShutdownBridge();

protected:
	/** Holds the message bridge if present. */
	TSharedPtr<class IMessageBridge, ESPMode::ThreadSafe> MessageBridge;
};
