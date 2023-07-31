// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBundleDescriptor)

UContentBundleDescriptor::UContentBundleDescriptor(const FObjectInitializer& ObjectInitializer)
	:Guid(FGuid::NewGuid())
{

}

const FString& UContentBundleDescriptor::GetDisplayName() const
{
	return DisplayName;
}

bool UContentBundleDescriptor::IsValid() const
{
	return Guid.IsValid()
		&& !DisplayName.IsEmpty()
		&& !PackageRoot.IsEmpty();
}
