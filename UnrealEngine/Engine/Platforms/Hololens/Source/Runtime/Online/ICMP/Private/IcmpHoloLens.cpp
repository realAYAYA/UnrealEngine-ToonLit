// Copyright Epic Games, Inc. All Rights Reserved.
#include "Icmp.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "WinSock2.h"
#undef _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Microsoft/HideMicrosoftPlatformTypes.h"

uint16 NtoHS(uint16 val)
{
	return ntohs(val);
}

uint16 HtoNS(uint16 val)
{
	return htons(val);
}

uint32 NtoHL(uint32 val)
{
	return ntohl(val);
}

uint32 HtoNL(uint32 val)
{
	return htonl(val);
}
