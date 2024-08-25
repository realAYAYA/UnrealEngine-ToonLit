// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncRemote.h"
#include "UnsyncCore.h"

namespace unsync {

EProtocolFlavor
protocol_flavor_from_string(const char* Str)
{
	if (!strcmp(Str, "jupiter"))
	{
		return EProtocolFlavor::Jupiter;
	}
	if (!strcmp(Str, "unsync"))
	{
		return EProtocolFlavor::Unsync;
	}
	else
	{
		return EProtocolFlavor::Unknown;
	}
}

const char*
ToString(EProtocolFlavor Protocol)
{
	switch (Protocol)
	{
		default:
		case EProtocolFlavor::Unknown:
			return "Unknown";
		case EProtocolFlavor::Unsync:
			return "UNSYNC";
		case EProtocolFlavor::Jupiter:
			return "Jupiter";
	}
}

static bool
IsValidUrlCharacter(char C)
{
	const char AllowedSpecials[] = ":/?#[]@!$&'()*+,;=-_.~";
	if ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9'))
	{
		return true;
	}
	for (char AllowedC : AllowedSpecials)
	{
		if (C == AllowedC)
		{
			return true;
		}
	}
	return false;
}

static bool
IsValidUrl(std::string_view Url)
{
	for (char C : Url)
	{
		if (!IsValidUrlCharacter(C))
		{
			return false;
		}
	}
	return true;
}

TResult<FRemoteDesc>
FRemoteDesc::FromUrl(std::string_view Url)
{
	if (!IsValidUrl(Url))
	{
		return AppError("Invalid URL");
	}

	FRemoteDesc Result;

	const size_t SchemePos	 = Url.find("://");
	const bool	 bHaveScheme = SchemePos != std::string::npos;

	std::string_view Scheme = bHaveScheme ? Url.substr(0, SchemePos) : std::string_view();

	bool bUseTls = false;

	std::string_view HostAddress = bHaveScheme ? Url.substr(SchemePos + 3) : Url;

	ETransportProtocol Transport = ETransportProtocol::Unsync;

	if (Scheme.ends_with("https"))
	{
		bUseTls	  = true;
		Transport = ETransportProtocol::Http;
	}
	else if (Scheme.ends_with("http"))
	{
		bUseTls	  = false;
		Transport = ETransportProtocol::Http;
	}
	else if (Scheme.ends_with("tls"))
	{
		bUseTls = true;
		if (Scheme.starts_with("unsync"))
		{
			Transport = ETransportProtocol::Unsync;
		}
		else
		{
			return AppError("Invalid transport scheme");
		}
	}

	switch (Transport)
	{
		default:
		case ETransportProtocol::Unsync:
			Result.Protocol = EProtocolFlavor::Unsync;
			break;
		case ETransportProtocol::Http:
			Result.Protocol = EProtocolFlavor::Jupiter;	 // TODO: UNSYNC-over-HTTP could also be supported
			break;
	}

	size_t NamespacePos = HostAddress.find_last_of('#');
	if (NamespacePos != std::string::npos)
	{
		Result.StorageNamespace = HostAddress.substr(NamespacePos + 1);
		HostAddress				= HostAddress.substr(0, NamespacePos);
	}

	uint16 HostPort = 0;
	size_t PortPos	= HostAddress.find_first_of(':');

	size_t RequestPos = HostAddress.find_first_of('/');
	if (RequestPos != std::string::npos)
	{
		Result.RequestPath = HostAddress.substr(RequestPos + 1);
		HostAddress		   = HostAddress.substr(0, RequestPos);
	}

	if (PortPos == std::string::npos)
	{
		switch (Transport)
		{
			default:
			case ETransportProtocol::Unsync:
				HostPort = UNSYNC_DEFAULT_PORT;
				break;
			case ETransportProtocol::Http:
				HostPort = bUseTls ? 443 : 80;
				break;
		}
	}
	else
	{
		int ParsedHostPort = std::atoi(&HostAddress[PortPos + 1]);
		if (ParsedHostPort > 0 && ParsedHostPort < 65536)
		{
			HostPort = uint16(ParsedHostPort);
		}
		HostAddress = HostAddress.substr(0, PortPos);
	}

	if (HostPort == 0)
	{
		return AppError("Invalid host port");  // TODO: extract the port substring
	}

	Result.Host.Address = HostAddress;
	Result.Host.Port	= HostPort;

	Result.bTlsEnable			 = bUseTls;
	Result.bTlsVerifyCertificate = bUseTls;
	Result.bTlsVerifySubject	 = bUseTls;

	return ResultOk(Result);
}

FTlsClientSettings
FRemoteDesc::GetTlsClientSettings() const
{
	FTlsClientSettings Result = {};
	if (bTlsEnable)
	{
		Result.Subject			  = GetTlsSubject();
		Result.bVerifyCertificate = bTlsVerifyCertificate;
		Result.bVerifySubject	  = bTlsVerifySubject;
		if (TlsCacert)
		{
			Result.CACert = TlsCacert->View();
		}
	}
	else
	{
		Result.bVerifyCertificate = false;
		Result.bVerifySubject	  = false;
	}
	return Result;
}

void
TestParseRemote()
{
	{
		UNSYNC_ASSERT(FRemoteDesc::FromUrl("bad url").IsError());
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("example.com");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == UNSYNC_DEFAULT_PORT);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Unsync);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("http://example.com");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 80);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("https://example.com");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 443);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("http://example.com:1234");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("https://example.com:1234");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("jupiter+http://example.com:1234");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("jupiter+https://example.com");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 443);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("jupiter+https://example.com#test.namespace");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 443);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
		UNSYNC_ASSERT(ParseResult->StorageNamespace == "test.namespace");
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("jupiter+https://example.com:1234#test.namespace");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
		UNSYNC_ASSERT(ParseResult->StorageNamespace == "test.namespace");
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("example.com:1234#test.namespace");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Unsync);
		UNSYNC_ASSERT(ParseResult->StorageNamespace == "test.namespace");
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("unsync://example.com:1234");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Unsync);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("unsync+tls://example.com:1234");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Unsync);
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("unsync+tls://example.com:invalid_port");
		UNSYNC_ASSERT(ParseResult.IsError());
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("unsync+tls://example.com:1234/request/path#namespace");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == true);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 1234);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Unsync);
		UNSYNC_ASSERT(ParseResult->StorageNamespace == "namespace");
		UNSYNC_ASSERT(ParseResult->RequestPath == "request/path");
	}

	{
		TResult<FRemoteDesc> ParseResult = FRemoteDesc::FromUrl("http://example.com/request/path#namespace");
		UNSYNC_ASSERT(ParseResult.IsOk());
		UNSYNC_ASSERT(ParseResult->bTlsEnable == false);
		UNSYNC_ASSERT(ParseResult->Host.Address == "example.com");
		UNSYNC_ASSERT(ParseResult->Host.Port == 80);
		UNSYNC_ASSERT(ParseResult->Protocol == EProtocolFlavor::Jupiter);
		UNSYNC_ASSERT(ParseResult->StorageNamespace == "namespace");
		UNSYNC_ASSERT(ParseResult->RequestPath == "request/path");
	}
}

}  // namespace unsync
