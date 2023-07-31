// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeResult.h"

#include "InterchangeFbxMessages.generated.h"


/**
 * Base class for FBX parser warnings
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultMeshWarning : public UInterchangeResultWarning
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString MeshName;
};

/**
 * Base class for FBX parser warnings
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultTextureWarning : public UInterchangeResultWarning
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString TextureName;
};

/**
 * Base class for FBX parser errors
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultMeshError : public UInterchangeResultError
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString MeshName;
};


/**
 * A generic class for FBX parser warnings, with no additional metadata, and where the text is specified by the user
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultMeshWarning_Generic : public UInterchangeResultMeshWarning
{
	GENERATED_BODY()

public:
	virtual FText GetText() const override;

	UPROPERTY()
	FText Text;
};


/**
 * A generic class for FBX parser errors, with no additional metadata, and where the text is specified by the user
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultMeshError_Generic : public UInterchangeResultMeshError
{
	GENERATED_BODY()

public:
	virtual FText GetText() const override;

	UPROPERTY()
	FText Text;
};


/**
 * 
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultMeshWarning_TooManyUVs : public UInterchangeResultMeshWarning
{
	GENERATED_BODY()

public:
	virtual FText GetText() const override;

	UPROPERTY()
	int32 ExcessUVs;
};

/**
 * A generic class for FBX parser warnings, with no additional metadata, and where the text is specified by the user
 */
UCLASS()
class INTERCHANGEMESSAGES_API UInterchangeResultTextureWarning_TextureFileDoNotExist : public UInterchangeResultTextureWarning
{
	GENERATED_BODY()

public:
	virtual FText GetText() const override;

	UPROPERTY()
	FText Text;

	UPROPERTY()
	FString MaterialName;
};