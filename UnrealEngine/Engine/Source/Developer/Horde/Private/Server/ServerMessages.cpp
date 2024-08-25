// Copyright Epic Games, Inc. All Rights Reserved.

#include "Server/ServerMessages.h"

FGetServerInfoResponse::FGetServerInfoResponse()
{
}

FGetServerInfoResponse::~FGetServerInfoResponse()
{
}

void FGetServerInfoResponse::Serialize(FJsonSerializerBase& Serializer, bool bFlatObject)
{
	JSON_SERIALIZE("ServerVersion", ServerVersion);
	JSON_SERIALIZE("AgentVersion", AgentVersion);
	JSON_SERIALIZE("OsDescription", OsDescription);
}

// -------------------------------------

FGetAuthConfigResponse::FGetAuthConfigResponse()
{
}

FGetAuthConfigResponse::~FGetAuthConfigResponse()
{
}

void FGetAuthConfigResponse::Serialize(FJsonSerializerBase& Serializer, bool bFlatObject)
{
	JSON_SERIALIZE("Method", Method);
	JSON_SERIALIZE("ServerUrl", ServerUrl);
	JSON_SERIALIZE("ClientId", ClientId);
	JSON_SERIALIZE_ARRAY("LocalRedirectUrls", LocalRedirectUrls);
}

