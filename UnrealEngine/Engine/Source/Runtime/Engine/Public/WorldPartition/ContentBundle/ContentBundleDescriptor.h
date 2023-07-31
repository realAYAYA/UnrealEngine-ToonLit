// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "ContentBundleDescriptor.generated.h"

class UActorDescContainer;
class AWorldDataLayers;

UCLASS()
class ENGINE_API UContentBundleDescriptor : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	const FString& GetDisplayName() const;
	const FString& GetPackageRoot() const { return PackageRoot; }
	const FGuid& GetGuid() const { return Guid; }

#if WITH_EDITOR
	void SetDisplayName(const FString& InName) { DisplayName = InName; }
	void SetPackageRoot(const FString& InPackageRoot) { PackageRoot = InPackageRoot; }
#endif

	bool IsValid() const;

private:
	UPROPERTY(EditAnywhere, Category = BaseInformation)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, Category = BaseInformation, AdvancedDisplay)
	FGuid Guid;

	UPROPERTY(VisibleAnywhere, Category = BaseInformation, AdvancedDisplay)
	FString PackageRoot;
};