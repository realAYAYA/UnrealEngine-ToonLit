// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterSync/DisplayClusterClusterSyncClient.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncStrings.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Listener/DisplayClusterHelloMessageStrings.h"

#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Misc/ScopeLock.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterSyncClient::FDisplayClusterClusterSyncClient()
	: FDisplayClusterClusterSyncClient(FString("CLN_CS"))
{
}

FDisplayClusterClusterSyncClient::FDisplayClusterClusterSyncClient(const FString& InName)
	: FDisplayClusterClient(InName)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClient
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterSyncClient::Connect(const FString& Address, const uint16 Port, const uint32 ConnectRetriesAmount, const uint32 ConnectRetryDelay)
{
	// First, allow base class to perform connection
	if(!FDisplayClusterClient::Connect(Address, Port, ConnectRetriesAmount, ConnectRetryDelay))
	{
		return false;
	}

	// Prepare 'hello' message
	TSharedPtr<FDisplayClusterPacketInternal> HelloMsg = MakeShared<FDisplayClusterPacketInternal>(
		DisplayClusterHelloMessageStrings::Hello::Name,
		DisplayClusterClusterSyncStrings::TypeRequest,
		DisplayClusterClusterSyncStrings::ProtocolName
	);

	// Fill in the message with data
	const FString NodeId = GDisplayCluster->GetPrivateClusterMgr()->GetNodeId();
	HelloMsg->SetTextArg(DisplayClusterHelloMessageStrings::ArgumentsDefaultCategory, DisplayClusterHelloMessageStrings::Hello::ArgNodeId, NodeId);

	// Send message (no response awaiting)
	return SendPacket(HelloMsg);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolClusterSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterSyncClient::WaitForGameStart()
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::WaitForGameStart::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::WaitForGameStart);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::WaitForFrameStart()
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::WaitForFrameStart::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::WaitForFrameStart);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::WaitForFrameEnd()
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::WaitForFrameEnd::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::WaitForFrameEnd);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::GetTimeData::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::GetTimeData);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	// Extract sync data from response packet
	FString StrDeltaTime;
	if (!Response->GetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgDeltaTime, StrDeltaTime))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't extract parameter: %s"), DisplayClusterClusterSyncStrings::GetTimeData::ArgDeltaTime);
		return EDisplayClusterCommResult::WrongResponseData;
	}

	// Convert from hex string to float
	OutDeltaTime = DisplayClusterTypesConverter::template FromHexString<double>(StrDeltaTime);

	FString StrGameTime;
	if (!Response->GetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgGameTime, StrGameTime))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't extract parameter: %s"), DisplayClusterClusterSyncStrings::GetTimeData::ArgGameTime);
		return EDisplayClusterCommResult::WrongResponseData;
	}

	// Convert from hex string to float
	OutGameTime = DisplayClusterTypesConverter::template FromHexString<double>(StrGameTime);

	// Extract sync data from response packet
	bool bIsFrameTimeValid = false;
	if (!Response->GetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgIsFrameTimeValid, bIsFrameTimeValid))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't extract parameter: %s"), DisplayClusterClusterSyncStrings::GetTimeData::ArgIsFrameTimeValid);
		return EDisplayClusterCommResult::WrongResponseData;
	}

	if (bIsFrameTimeValid)
	{
		FQualifiedFrameTime NewFrameTime;
		if (!Response->GetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgFrameTime, NewFrameTime))
		{
			UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't extract parameter: %s"), DisplayClusterClusterSyncStrings::GetTimeData::ArgFrameTime);
			return EDisplayClusterCommResult::WrongResponseData;
		}

		OutFrameTime = NewFrameTime;
	}

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	static TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::GetObjectsData::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);
	
	Request->SetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetObjectsData::ArgSyncGroup, static_cast<uint8>(InSyncGroup));
	
	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::GetObjectsData);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	// Extract data from response packet
	OutObjectsData = Response->GetTextArgs(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory);

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents)
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::GetEventsData::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::GetEventsData);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	// Extract events data from response packet
	DisplayClusterNetworkDataConversion::JsonEventsFromInternalPacket(Response,   OutJsonEvents);
	DisplayClusterNetworkDataConversion::BinaryEventsFromInternalPacket(Response, OutBinaryEvents);

	return Response->GetCommResult();
}

EDisplayClusterCommResult FDisplayClusterClusterSyncClient::GetNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	static const TSharedPtr<FDisplayClusterPacketInternal> Request = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterClusterSyncStrings::GetNativeInputData::Name,
			DisplayClusterClusterSyncStrings::TypeRequest,
			DisplayClusterClusterSyncStrings::ProtocolName);

	TSharedPtr<FDisplayClusterPacketInternal> Response;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CS::GetNativeInputData);
		Response = SendRecvPacket(Request);
	}

	if (!Response)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Network error on '%s'"), *Request->GetName());
		return EDisplayClusterCommResult::NetworkError;
	}

	// Extract data from response packet
	OutNativeInputData = Response->GetTextArgs(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory);

	return Response->GetCommResult();
}
