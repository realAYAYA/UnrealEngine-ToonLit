// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VCamConnectionStructs.h"

#include "Algo/AllOf.h"
#include "VCamComponent.h"

DEFINE_LOG_CATEGORY(LogVCamConnection);

bool FVCamConnection::IsConnected() const
{
	// If we have a valid modifier then we can assume we have a successful connection
	return IsValid(ConnectedModifier);
}

bool FVCamConnection::AttemptConnection(UVCamComponent* VCamComponent)
{
	bool bIsValidModifierConnection = false;
		UVCamModifier* Modifier = VCamComponent->GetModifierByName(ConnectionTargetSettings.TargetModifierName);
		FVCamModifierConnectionPoint* ConnectionPoint = nullptr;
		if (Modifier)
		{			
			const bool bImplementsAllRequiredInterfaces = Algo::AllOf(RequiredInterfaces, [Modifier](const TSubclassOf<UInterface>& Interface)
			{
				return Modifier->GetClass()->ImplementsInterface(Interface);
			});

			if (bImplementsAllRequiredInterfaces)
			{
				ConnectionPoint = Modifier->ConnectionPoints.Find(ConnectionTargetSettings.TargetConnectionPoint);
				if (ConnectionPoint)
				{
					// The Connection Point is considered valid if either we don't require an action or the required ActionType matches the ActionType inside the Connection Point
					const bool bConnectionPointHasRequiredActionType = !bRequiresInputAction || (ConnectionPoint->AssociatedAction && ConnectionPoint->AssociatedAction->ValueType == ActionType);
					if (bConnectionPointHasRequiredActionType)
					{
						bIsValidModifierConnection = true;
					}
					else
					{
						UE_LOG(LogVCamConnection, Warning, TEXT("Connection Failed: Connection Point with name %s does not provide an action of the required type. Expected %s but found %s"),
							*ConnectionTargetSettings.TargetConnectionPoint.ToString(),
							*UEnum::GetValueAsString(ActionType),
							ConnectionPoint->AssociatedAction ? *UEnum::GetValueAsString(ConnectionPoint->AssociatedAction->ValueType) : TEXT("None"));
					}
				}
				else
				{
					UE_LOG(LogVCamConnection, Warning, TEXT("Connection Failed: Failed to find Connection Point with name %s in Modifier %s"),
						*ConnectionTargetSettings.TargetConnectionPoint.ToString(),
						*ConnectionTargetSettings.TargetModifierName.ToString());
				}
			}
			else
			{
				UE_LOG(LogVCamConnection, Warning, TEXT("Connection Failed: Modifier with name %s does not implement all required interfaces for this connection."), *ConnectionTargetSettings.TargetModifierName.ToString());
			}
		}
		else
		{
			UE_LOG(LogVCamConnection, Warning, TEXT("Connection Failed: Failed to find Modifier with name %s in VCam Component"), *ConnectionTargetSettings.TargetModifierName.ToString());
		}

		ConnectedModifier = bIsValidModifierConnection ? Modifier : nullptr;
		ConnectedAction = bIsValidModifierConnection ? ConnectionPoint->AssociatedAction : nullptr;

	return bIsValidModifierConnection;
}

void FVCamConnection::ResetConnection()
{
	ConnectedModifier = nullptr;
	ConnectedAction = nullptr;
}
