// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"

#include "OSCMessage.h"
#include "OSCBundle.h"

#include "OSCClient.generated.h"


/** Interface for internal networking implementation.  See UOSCClient for details */
class OSC_API IOSCClientProxy
{
public:
	virtual ~IOSCClientProxy() { }

	virtual void GetSendIPAddress(FString& InIPAddress, int32& Port) const = 0;
	virtual bool SetSendIPAddress(const FString& InIPAddress, const int32 Port) = 0;

	virtual bool IsActive() const = 0;

	virtual void SendMessage(FOSCMessage& Message) = 0;
	virtual void SendBundle(FOSCBundle& Bundle) = 0;

	virtual void Stop() = 0;
};

UCLASS(BlueprintType)
class OSC_API UOSCClient : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void Connect();

	bool IsActive() const;

	/** Gets the OSC Client IP address and port. */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC")
	void GetSendIPAddress(UPARAM(ref) FString& IPAddress, UPARAM(ref) int32& Port);

	/** Sets the OSC Client IP address and port. Returns whether
	  * address and port was successfully set. */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC")
	bool SetSendIPAddress(const FString& IPAddress, const int32 Port);

	/** Send OSC message to  a specific address. */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC")
	void SendOSCMessage(UPARAM(ref) FOSCMessage& Message);

	/** Send OSC Bundle over the network. */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC")
	void SendOSCBundle(UPARAM(ref) FOSCBundle& Bundle);

protected:
	void BeginDestroy() override;

	/** Stop and tidy up network socket. */
	void Stop();
	
	/** Pointer to internal implementation of client proxy */
	TUniquePtr<IOSCClientProxy> ClientProxy;
};
