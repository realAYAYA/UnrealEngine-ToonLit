// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemHoloLens.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"

#include "BSDSockets/IPAddressBSD.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include "Iphlpapi.h"
THIRD_PARTY_INCLUDES_END
#include "Microsoft/HideMicrosoftPlatformTypes.h"


FSocketSubsystemHoloLens* FSocketSubsystemHoloLens::SocketSingleton = nullptr;


FName CreateSocketSubsystem(FSocketSubsystemModule& SocketSubsystemModule)
{
	FName SubsystemName(TEXT("HoloLens"));

	// Create and register our singleton factory with the main online subsystem for easy access
	FSocketSubsystemHoloLens* SocketSubsystem = FSocketSubsystemHoloLens::Create();
	FString Error;

	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);

		return SubsystemName;
	}

	FSocketSubsystemHoloLens::Destroy();

	return NAME_None;
}


void DestroySocketSubsystem(FSocketSubsystemModule& SocketSubsystemModule)
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("HoloLens")));
	FSocketSubsystemHoloLens::Destroy();
}


/* FSocketSubsystemHoloLens interface
*****************************************************************************/

FSocketSubsystemHoloLens* FSocketSubsystemHoloLens::Create()
{
	if (SocketSingleton == nullptr)
	{
		SocketSingleton = new FSocketSubsystemHoloLens();
	}

	return SocketSingleton;
}

void FSocketSubsystemHoloLens::Destroy()
{
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}


/* FSocketSubsystemBSD overrides
*****************************************************************************/

FSocket* FSocketSubsystemHoloLens::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType)
{
	FSocketBSD* NewSocket = static_cast<FSocketBSD*>(FSocketSubsystemBSD::CreateSocket(SocketType, SocketDescription, ProtocolType));

	if (NewSocket != nullptr)
	{
		NewSocket->SetIPv6Only(false);
	}
	else	
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}


bool FSocketSubsystemHoloLens::Init(FString& Error)
{
	bool bSuccess = false;

	if (bTriedToInit == false)
	{
		bTriedToInit = true;
		WSADATA WSAData;

		// initialize WSA
		int32 Code = WSAStartup(0x0101, &WSAData);

		if (Code == 0)
		{
			bSuccess = true;
			UE_LOG(LogInit, Log, TEXT("%s: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i"),
				GetSocketAPIName(),
				WSAData.wVersion >> 8, WSAData.wVersion & 0xFF,
				WSAData.wHighVersion >> 8, WSAData.wHighVersion & 0xFF,
				WSAData.iMaxSockets, WSAData.iMaxUdpDg);
		}
		else
		{
			Error = FString::Printf(TEXT("WSAStartup failed (%s)"), GetSocketError(TranslateErrorCode(Code)));
		}
	}

	return bSuccess;
}


void FSocketSubsystemHoloLens::Shutdown(void)
{
	WSACleanup();
}


ESocketErrors FSocketSubsystemHoloLens::GetLastErrorCode()
{
	return TranslateErrorCode(WSAGetLastError());
}

ESocketErrors FSocketSubsystemHoloLens::TranslateErrorCode(int32 Code)
{
	// handle the generic -1 error
	if (Code == SOCKET_ERROR)
	{
		return GetLastErrorCode();
	}

	switch (Code)
	{
	case 0: return SE_NO_ERROR;
	case ERROR_INVALID_HANDLE: return SE_ECONNRESET; // invalid socket handle catch
	case WSAEINTR: return SE_EINTR;
	case WSAEBADF: return SE_EBADF;
	case WSAEACCES: return SE_EACCES;
	case WSAEFAULT: return SE_EFAULT;
	case WSAEINVAL: return SE_EINVAL;
	case WSAEMFILE: return SE_EMFILE;
	case WSAEWOULDBLOCK: return SE_EWOULDBLOCK;
	case WSAEINPROGRESS: return SE_EINPROGRESS;
	case WSAEALREADY: return SE_EALREADY;
	case WSAENOTSOCK: return SE_ENOTSOCK;
	case WSAEDESTADDRREQ: return SE_EDESTADDRREQ;
	case WSAEMSGSIZE: return SE_EMSGSIZE;
	case WSAEPROTOTYPE: return SE_EPROTOTYPE;
	case WSAENOPROTOOPT: return SE_ENOPROTOOPT;
	case WSAEPROTONOSUPPORT: return SE_EPROTONOSUPPORT;
	case WSAESOCKTNOSUPPORT: return SE_ESOCKTNOSUPPORT;
	case WSAEOPNOTSUPP: return SE_EOPNOTSUPP;
	case WSAEPFNOSUPPORT: return SE_EPFNOSUPPORT;
	case WSAEAFNOSUPPORT: return SE_EAFNOSUPPORT;
	case WSAEADDRINUSE: return SE_EADDRINUSE;
	case WSAEADDRNOTAVAIL: return SE_EADDRNOTAVAIL;
	case WSAENETDOWN: return SE_ENETDOWN;
	case WSAENETUNREACH: return SE_ENETUNREACH;
	case WSAENETRESET: return SE_ENETRESET;
	case WSAECONNABORTED: return SE_ECONNABORTED;
	case WSAECONNRESET: return SE_ECONNRESET;
	case WSAENOBUFS: return SE_ENOBUFS;
	case WSAEISCONN: return SE_EISCONN;
	case WSAENOTCONN: return SE_ENOTCONN;
	case WSAESHUTDOWN: return SE_ESHUTDOWN;
	case WSAETOOMANYREFS: return SE_ETOOMANYREFS;
	case WSAETIMEDOUT: return SE_ETIMEDOUT;
	case WSAECONNREFUSED: return SE_ECONNREFUSED;
	case WSAELOOP: return SE_ELOOP;
	case WSAENAMETOOLONG: return SE_ENAMETOOLONG;
	case WSAEHOSTDOWN: return SE_EHOSTDOWN;
	case WSAEHOSTUNREACH: return SE_EHOSTUNREACH;
	case WSAENOTEMPTY: return SE_ENOTEMPTY;
	case WSAEPROCLIM: return SE_EPROCLIM;
	case WSAEUSERS: return SE_EUSERS;
	case WSAEDQUOT: return SE_EDQUOT;
	case WSAESTALE: return SE_ESTALE;
	case WSAEREMOTE: return SE_EREMOTE;
	case WSAEDISCON: return SE_EDISCON;
	case WSASYSNOTREADY: return SE_SYSNOTREADY;
	case WSAVERNOTSUPPORTED: return SE_VERNOTSUPPORTED;
	case WSANOTINITIALISED: return SE_NOTINITIALISED;
	case WSAHOST_NOT_FOUND: return SE_HOST_NOT_FOUND;
	case WSATRY_AGAIN: return SE_TRY_AGAIN;
	case WSANO_RECOVERY: return SE_NO_RECOVERY;
	case WSANO_DATA: return SE_NO_DATA;
		// case : return SE_UDP_ERR_PORT_UNREACH; //@TODO Find it's replacement
	}

	UE_LOG(LogSockets, Warning, TEXT("Unhandled socket error! Error Code: %d"), Code);
	check(0);

	return SE_NO_ERROR;
}


bool FSocketSubsystemHoloLens::HasNetworkDevice()
{
	return true;
}

const TCHAR* FSocketSubsystemHoloLens::GetSocketAPIName() const
{
	return TEXT("WinSock");
}

