// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VCamUIFunctionLibrary.h"

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


