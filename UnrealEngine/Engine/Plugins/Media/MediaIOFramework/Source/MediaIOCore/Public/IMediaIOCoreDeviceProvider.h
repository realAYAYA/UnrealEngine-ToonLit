// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaIOCoreDefinitions.h"

class UMediaSource;

class MEDIAIOCORE_API IMediaIOCoreDeviceProvider
{
public:
	virtual FName GetFName() = 0;

	virtual TArray<FMediaIOConnection> GetConnections() const = 0;
	virtual TArray<FMediaIOConfiguration> GetConfigurations() const = 0;
	virtual TArray<FMediaIOConfiguration> GetConfigurations(bool bAllowInput, bool bAllowOutput) const = 0;
	virtual TArray<FMediaIODevice> GetDevices() const = 0;
	virtual TArray<FMediaIOMode> GetModes(const FMediaIODevice& InDevice, bool bInOutput) const = 0;
	virtual TArray<FMediaIOInputConfiguration> GetInputConfigurations() const = 0;
	virtual TArray<FMediaIOOutputConfiguration> GetOutputConfigurations() const = 0;
	virtual TArray<FMediaIOVideoTimecodeConfiguration> GetTimecodeConfigurations() const = 0;

	virtual FMediaIOConfiguration GetDefaultConfiguration() const = 0;
	virtual FMediaIOMode GetDefaultMode() const = 0;
	virtual FMediaIOInputConfiguration GetDefaultInputConfiguration() const = 0;
	virtual FMediaIOOutputConfiguration GetDefaultOutputConfiguration() const = 0;
	virtual FMediaIOVideoTimecodeConfiguration GetDefaultTimecodeConfiguration() const = 0;

	virtual UMediaSource* CreateMediaSource(const FMediaIOConfiguration& InConfiguration,
		UObject* Outer) const { return nullptr; }

	virtual FText ToText(const FMediaIOConfiguration& InConfiguration, bool bInIsAutoDetected = false) const;
	virtual FText ToText(const FMediaIOConnection& InConnection) const;
	virtual FText ToText(const FMediaIODevice& InDevice) const;
	virtual FText ToText(const FMediaIOMode& InMode) const;
	virtual FText ToText(const FMediaIOInputConfiguration& InMode) const;
	virtual FText ToText(const FMediaIOOutputConfiguration& InMode) const;

	static FText GetTransportName(EMediaIOTransportType InLinkType, EMediaIOQuadLinkTransportType InQuadLinkType);

#if WITH_EDITOR
	virtual bool ShowInputTransportInSelector() const { return true; }
	virtual bool ShowOutputTransportInSelector() const { return true; }
	virtual bool ShowInputKeyInSelector() const { return true; }
	virtual bool ShowOutputKeyInSelector() const { return true; }
	virtual bool ShowReferenceInSelector() const { return true; }
#endif
};
