// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleTypeFactory.h"

#include "WorldPartition/ContentBundle/ContentBundleClient.h"

TSharedPtr<FContentBundleClient> UContentBundleTypeFactory::CreateClient(const UContentBundleDescriptor* Descriptor, const FString& ClientDisplayName)
{
	return MakeShared<FContentBundleClient>(Descriptor, ClientDisplayName);
}