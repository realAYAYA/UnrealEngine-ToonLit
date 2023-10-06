// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"

#include "DatasmithCloImportOptions.generated.h"

USTRUCT(BlueprintType)
struct FCloOptions
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	bool bGenerateClothStaticMesh = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	bool bGeneratePerPanel2DMesh = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	bool bGenerateClothActor = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	bool bPerPanelMaterial = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	bool bSkipPanelsWithoutPhysicsProperties = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo")
	float Scale = 1.0f;
};


UCLASS(BlueprintType, config = EditorPerProjectUserSettings)
class DATASMITHCLOTRANSLATOR_API UDatasmithCloImportOptions : public UDatasmithOptionsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Clo", meta = (ShowOnlyInnerProperties))
	FCloOptions Options;

#if WITH_EDITOR
	bool CanEditChange(const FProperty* InProperty) const override
	{
		if (!Super::CanEditChange(InProperty))
		{
			return false;
		}

		FName PropertyFName = InProperty->GetFName();
		if (PropertyFName == GET_MEMBER_NAME_CHECKED(FCloOptions, bPerPanelMaterial))
		{
			return Options.bGenerateClothStaticMesh;
		}

		return true;
	}
#endif //WITH_EDITOR
};

