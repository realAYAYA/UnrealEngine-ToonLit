// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VCamUIFunctionLibrary.h"

#include "UI/VCamWidget.h"

bool UVCamUIFunctionLibrary::IsConnected_VCamConnection(const FVCamConnection& Connection)
{
	return Connection.IsConnected();
}

FName UVCamUIFunctionLibrary::GetConnectionPointName_VCamConnection(const FVCamConnection& Connection)
{
	return Connection.ConnectionTargetSettings.TargetConnectionPoint;
}

UVCamModifier* UVCamUIFunctionLibrary::GetConnectedModifier_VCamConnection(const FVCamConnection& Connection)
{
	return Connection.ConnectedModifier;
}

UInputAction* UVCamUIFunctionLibrary::GetConnectedInputAction_VCamConnection(const FVCamConnection& Connection)
{
	return Connection.ConnectedAction;
}

bool UVCamUIFunctionLibrary::GetConnectionByName_VCamWidget(UVCamWidget* Widget, FName ConnectionId, FVCamConnection& OutConnection)
{
	if (!Widget)
	{
		return false;
	}

	if (const FVCamConnection* Connection = Widget->Connections.Find(ConnectionId))
	{
		OutConnection = *Connection;
		return true;
	}
	return false;
}

bool UVCamUIFunctionLibrary::IsConnected_VCamWidget(UVCamWidget* Widget, FName ConnectionId, bool& bOutIsConnected)
{
	FVCamConnection Connection;
	if (GetConnectionByName_VCamWidget(Widget, ConnectionId, Connection))
	{
		bOutIsConnected = IsConnected_VCamConnection(Connection);
		return true;
	}
	return false;
}

bool UVCamUIFunctionLibrary::GetConnectionPointName_VCamWidget(UVCamWidget* Widget, FName ConnectionId, FName& OutConnectionPointName)
{
	FVCamConnection Connection;
	if (GetConnectionByName_VCamWidget(Widget, ConnectionId, Connection))
	{
		OutConnectionPointName = GetConnectionPointName_VCamConnection(Connection);
		return true;
	}
	return false;
}

bool UVCamUIFunctionLibrary::GetConnectedModifier_VCamWidget(UVCamWidget* Widget, FName ConnectionId, UVCamModifier*& OutModifier)
{
	FVCamConnection Connection;
	if (GetConnectionByName_VCamWidget(Widget, ConnectionId, Connection))
	{
		OutModifier = GetConnectedModifier_VCamConnection(Connection);
		return true;
	}
	return false;
}

bool UVCamUIFunctionLibrary::GetConnectedInputAction_VCamWidget(UVCamWidget* Widget, FName ConnectionId, UInputAction*& OutInputAction)
{
	FVCamConnection Connection;
	if (GetConnectionByName_VCamWidget(Widget, ConnectionId, Connection))
	{
		OutInputAction = GetConnectedInputAction_VCamConnection(Connection);
		return true;
	}
	return false;
}


