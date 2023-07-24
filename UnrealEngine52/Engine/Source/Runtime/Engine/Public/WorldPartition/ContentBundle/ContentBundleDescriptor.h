// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "ContentBundleDescriptor.generated.h"

class UActorDescContainer;
class AWorldDataLayers;
struct FColor;

UCLASS()
class ENGINE_API UContentBundleDescriptor : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	const FString& GetDisplayName() const { return DisplayName; }
	const FColor& GetDebugColor() const { return DebugColor; }
	const FString& GetPackageRoot() const { return PackageRoot; }
	const FGuid& GetGuid() const { return Guid; }

#if WITH_EDITOR
	void InitializeObject(const FString& InContentBundleName, const FString& InPackageRoot);
	void SetDisplayName(const FString& InName) { DisplayName = InName; }
	void SetPackageRoot(const FString& InPackageRoot) { PackageRoot = InPackageRoot; }
	void SetDebugColor(const FColor& InDebugColor) { DebugColor = InDebugColor; }

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif

	bool IsValid() const;

	// Helper method that returns a compact string for a given content bundle ID
	static FString GetContentBundleCompactString(const FGuid& InContentBundleID);

private:
	void InitDebugColor();

	UPROPERTY(EditAnywhere, Category = BaseInformation)
	FString DisplayName;

	UPROPERTY(EditAnywhere, Category = BaseInformation)
	FColor DebugColor;

	UPROPERTY(VisibleAnywhere, Category = BaseInformation, AdvancedDisplay)
	FGuid Guid;

	UPROPERTY(VisibleAnywhere, Category = BaseInformation, AdvancedDisplay)
	FString PackageRoot;
};