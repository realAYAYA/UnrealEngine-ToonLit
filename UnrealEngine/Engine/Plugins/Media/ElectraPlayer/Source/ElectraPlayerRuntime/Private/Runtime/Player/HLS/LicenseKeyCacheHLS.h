// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Player/Manifest.h"
#include "ManifestBuilderHLS.h"

namespace Electra
{
class IPlayerSessionServices;


class ILicenseKeyCacheHLS
{
public:
	static ILicenseKeyCacheHLS* Create(IPlayerSessionServices* SessionServices);

	virtual ~ILicenseKeyCacheHLS() = default;

	virtual TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetLicenseKeyFor(const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo) = 0;

	virtual void AddLicenseKey(const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& LicenseKey, const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo, const FTimeValue& ExpiresAtUTC) = 0;
};


} // namespace Electra

