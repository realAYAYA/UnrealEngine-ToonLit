// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "OSCPacket.h"
#include "OSCBundle.generated.h"


USTRUCT(BlueprintType)
struct OSC_API FOSCBundle
{
	GENERATED_USTRUCT_BODY()

	FOSCBundle();
	FOSCBundle(const TSharedPtr<IOSCPacket>& InPacket);
	~FOSCBundle();

	void SetPacket(const TSharedPtr<IOSCPacket>& InPacket);
	const TSharedPtr<IOSCPacket>& GetPacket() const;

private:
	TSharedPtr<IOSCPacket> Packet;
};