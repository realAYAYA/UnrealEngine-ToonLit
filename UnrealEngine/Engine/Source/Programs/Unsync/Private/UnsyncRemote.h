// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncError.h"
#include "UnsyncSocket.h"

#include <memory>
#include <string>

namespace unsync {

class FBuffer;

static constexpr uint16 UNSYNC_DEFAULT_PORT = 53841;

enum class EProtocolFlavor
{
	Unknown,
	Unsync,
	Jupiter,
};

enum class ETransportProtocol
{
	Http,
	Unsync,
};

EProtocolFlavor ProtocolFlavorFromString(const char* Str);
const char*		ToString(EProtocolFlavor Protocol);

struct FRemoteDesc
{
	EProtocolFlavor Protocol = EProtocolFlavor::Unknown;

	std::string HostAddress;
	uint16		HostPort = 0;

	std::string RequestPath;
	std::string StorageNamespace;
	std::string StorageBucket = "unsync";  // TODO: override via command line
	std::string HttpHeaders;

	bool					 bTlsEnable			   = true;	// Prefer TLS, if supported by protocol and remote server
	bool					 bTlsVerifyCertificate = true;	// Disabling this allows self-signed certificates
	std::string				 TlsSubject;					// Use host by default
	std::shared_ptr<FBuffer> TlsCacert;	 // Custom CA to use for server certificate validation (system root CA is used by default)

	uint32 MaxConnections = 8;	// Limit on concurrent connections to this server

	bool IsValid() const { return Protocol != EProtocolFlavor::Unknown && !HostAddress.empty() && HostPort != 0; }

	static TResult<FRemoteDesc> FromUrl(std::string_view Url);

	FTlsClientSettings GetTlsClientSettings() const;
};

}  // namespace unsync
