// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VCamWidget.h"

#include "LogVCamCore.h"
#include "VCamComponent.h"

UVCamWidget::UVCamWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// We have two events so C++ lambdas can be used since dynamic delegates do not support it.
	OnPostConnectionsReinitializedDelegate.AddLambda([this]()
	{
		OnPostConnectionsReinitializedDelegate_Blueprint.Broadcast();
	});
}

void UVCamWidget::NativePreConstruct()
{
	// If we were created by a widget dynamically, let's use their VCamComponent so ReinitializeConnections will work
	if (UVCamWidget* Outer = GetTypedOuter<UVCamWidget>(); !VCamComponent && Outer && Outer->VCamComponent)
	{
		VCamComponent = Outer->VCamComponent;
	}
	
	Super::NativePreConstruct();
}

void UVCamWidget::NativeDestruct()
{
	Super::NativeDestruct();
	
	if (VCamComponent)
	{
		VCamComponent->UnregisterObjectForInput(this);
	}
}

void UVCamWidget::InitializeConnections(UVCamComponent* VCam)
{
	VCamComponent = VCam;

	// Register for input with the VCam Component if desired
	if (VCamComponent && bRegisterForInput)
	{
		VCamComponent->RegisterObjectForInput(this);
		VCamComponent->AddInputMappingContext(InputMappingContext, InputContextPriority);
	}

	OnInitializeConnections(VCam);
	ReinitializeConnections();
}

bool UVCamWidget::ReinitializeConnections()
{
	if (!IsValid(VCamComponent))
	{
		UE_LOG(LogVCamCore, Error, TEXT("ReinitializeConnections failed because VCamComponet is not set (for widget %s of class %s)"), *GetPathName(), *GetClass()->GetPathName());
		return false;
	}
	
	// Iteratively call AttemptConnection on each connection within the widget and notify the result via OnConnectionUpdated
	for (TPair<FName, FVCamConnection>& Connection : Connections)
	{
		const FName& ConnectionName = Connection.Key;
		FVCamConnection& VCamConnection = Connection.Value;

		bool bDidConnectSuccessfully= false;
		if (VCamConnection.ConnectionTargetSettings.HasValidSettings())
		{
			bDidConnectSuccessfully = VCamConnection.AttemptConnection(VCamComponent);

			if (!bDidConnectSuccessfully)
			{
				UE_LOG(LogVCamConnection, Warning, TEXT("Widget %s: Failed to create for VCam Connection with Connection Name: %s"), *GetName(), *ConnectionName.ToString());	
			}
		}
		else
		{
			VCamConnection.ResetConnection();
		}
		
		OnConnectionUpdated(ConnectionName, bDidConnectSuccessfully, VCamConnection.ConnectionTargetSettings.TargetConnectionPoint, VCamConnection.ConnectedModifier, VCamConnection.ConnectedAction);
	}

	PostConnectionsInitialized();
	OnPostConnectionsReinitializedDelegate.Broadcast();
	return true;
}

void UVCamWidget::UpdateConnectionTargets(const TMap<FName, FVCamConnectionTargetSettings>& NewConnectionTargets, const bool bReinitializeOnSuccessfulUpdate, EConnectionUpdateResult& Result)
{
	bool bConnectionsUpdated = false;
	for (const TPair<FName, FVCamConnectionTargetSettings>& NewConnectionTarget : NewConnectionTargets)
	{
		const FName& ConnectionName = NewConnectionTarget.Key;
		const FVCamConnectionTargetSettings& TargetSettings = NewConnectionTarget.Value;

		if (FVCamConnection* CurrentConnection = Connections.Find(ConnectionName))
		{
			if (CurrentConnection->ConnectionTargetSettings == TargetSettings)
			{
				continue;
			}

			CurrentConnection->ConnectionTargetSettings = TargetSettings;
			bConnectionsUpdated = true;
		}
	}

	if (bConnectionsUpdated)
	{
		Result = EConnectionUpdateResult::DidUpdateConnections;
		if (bReinitializeOnSuccessfulUpdate)
		{
			ReinitializeConnections();
		}
	}
	else
	{
		Result = EConnectionUpdateResult::NoConnectionsUpdated;
	}
}

void UVCamWidget::OnInitializeConnections_Implementation(UVCamComponent* VCam)
{
}
