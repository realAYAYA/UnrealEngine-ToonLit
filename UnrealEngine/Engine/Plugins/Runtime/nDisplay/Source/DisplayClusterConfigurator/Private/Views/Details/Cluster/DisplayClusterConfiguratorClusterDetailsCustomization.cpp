// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorClusterDetailsCustomization.h"

#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterRootActor.h"

#include "DetailLayoutBuilder.h"

void FDisplayClusterConfiguratorClusterDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& InLayoutBuilder)
{
	FDisplayClusterConfiguratorBaseDetailCustomization::CustomizeDetails(InLayoutBuilder);

	if (ToolkitPtr.IsValid())
	{
		check (RootActorPtr.IsValid());
		{
			const TSharedPtr<IPropertyHandle> PropertyHandle = InLayoutBuilder.AddObjectPropertyData(
				{ RootActorPtr->GetConfigData() },
				GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationData, Info));
			check(PropertyHandle->IsValidHandle());
			InLayoutBuilder.AddPropertyToCategory(PropertyHandle);
		}
		{
			const TSharedPtr<IPropertyHandle> PropertyHandle = InLayoutBuilder.AddObjectPropertyData(
				{ RootActorPtr->GetConfigData() },
				GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationData, Diagnostics));
			check(PropertyHandle->IsValidHandle());
			InLayoutBuilder.AddPropertyToCategory(PropertyHandle);
		}
	}
}
