// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RemoteControlProtocolDMX.h"
#include "UObject/Object.h"

#include "RemoteControlProtocolDMXSettings.generated.h"

/**
 * DMX Remote Control Settings
 */
UCLASS(Config = Engine, DefaultConfig)
class REMOTECONTROLPROTOCOLDMX_API URemoteControlProtocolDMXSettings : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	UFUNCTION(BlueprintGetter)
	FGuid GetOrCreateDefaultInputPortId();

	/** Returns a delegate broadcast whenever the Remote Control Protocol DMX Settings changed */
	static FSimpleMulticastDelegate& GetOnRemoteControlProtocolDMXSettingsChanged();

	static FName GetDefaultInputPortIdPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(URemoteControlProtocolDMXSettings, DefaultInputPortId); }

private:
	/** DMX Default Device */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Meta = (BlueprintGetter = GetOrCreateDefaultInputPortId, AllowPrivateAccess = true), Category = Mapping)
	FGuid DefaultInputPortId;

	static FSimpleMulticastDelegate OnRemoteControlProtocolDMXSettingsChangedDelegate;
};
