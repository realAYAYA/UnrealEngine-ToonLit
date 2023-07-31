// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OSCLog.h"
#include "OSCPacket.h"
#include "OSCStream.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "OSCMessage.generated.h"


USTRUCT(BlueprintType)
struct OSC_API FOSCMessage
{
	GENERATED_USTRUCT_BODY()

	FOSCMessage();
	FOSCMessage(const TSharedPtr<IOSCPacket>& InPacket);
	~FOSCMessage();

	void SetPacket(TSharedPtr<IOSCPacket>& InPacket);
	const TSharedPtr<IOSCPacket>& GetPacket() const;

	bool SetAddress(const FOSCAddress& InAddress);
	const FOSCAddress& GetAddress() const;

private:
	TSharedPtr<IOSCPacket> Packet;
};