// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBundleDescriptor)

UContentBundleDescriptor::UContentBundleDescriptor(const FObjectInitializer& ObjectInitializer)
	: DebugColor(FColor::Black)
{
}

bool UContentBundleDescriptor::IsValid() const
{
	return Guid.IsValid()
		&& !DisplayName.IsEmpty()
		&& !PackageRoot.IsEmpty();
}

FString UContentBundleDescriptor::GetContentBundleCompactString(const FGuid& InContentBundleID)
{
	UE_CLOG(!InContentBundleID.IsValid(), LogContentBundle, Log, TEXT("Called UContentBundleDescriptor::GetContentBundleCompactString with an invalid Content Bundle guid."))
	return FString::Printf(TEXT("%X"), GetTypeHash(InContentBundleID));
}

#if WITH_EDITOR
void UContentBundleDescriptor::InitializeObject(const FString& InContentBundleName, const FString& InPackageRoot)
{
	Guid = FGuid::NewGuid();
	DisplayName = InContentBundleName;
	PackageRoot = InPackageRoot;
	InitDebugColor();
}

void UContentBundleDescriptor::PostLoad()
{
	InitDebugColor();

	Super::PostLoad();
}

void UContentBundleDescriptor::InitDebugColor()
{
	// If not set, generate a color based on guid
	if (DebugColor == FColor::Black)
	{
		DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(Guid));
	}
}
#endif
