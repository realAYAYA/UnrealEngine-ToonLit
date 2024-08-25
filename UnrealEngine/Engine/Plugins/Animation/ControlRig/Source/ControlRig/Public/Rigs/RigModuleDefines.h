// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyElements.h"
#include "RigModuleDefines.generated.h"

USTRUCT(BlueprintType)
struct FModularRigSettings
{
	GENERATED_BODY()

	// Whether or not to autoresolve secondary connectors once the primary connector is resolved
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ModularRig)
	bool bAutoResolve = true;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleIdentifier
{
	GENERATED_BODY()
	
	FRigModuleIdentifier()
		: Name()
		, Type(TEXT("Module"))
	{}

	// The name of the module used to find it in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Name;

	// The kind of module this is (for example "Arm")
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Type;

	bool IsValid() const { return !Name.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleConnector
{
	GENERATED_BODY()
	
	FRigModuleConnector()
	{}

	bool operator==(const FRigModuleConnector& Other) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FRigConnectorSettings Settings;
	
	bool IsPrimary() const { return Settings.Type == EConnectorType::Primary; }
	bool IsSecondary() const { return Settings.Type == EConnectorType::Secondary; }
	bool IsOptional() const { return IsSecondary() && Settings.bOptional; }
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleSettings
{
	GENERATED_BODY()
	
	FRigModuleSettings()
	{}

	bool IsValidModule(bool bRequireExposedConnectors = true) const
	{
		return
			Identifier.IsValid() &&
			(!bRequireExposedConnectors || !ExposedConnectors.IsEmpty());
	}

	const FRigModuleConnector* FindPrimaryConnector() const
	{
		return ExposedConnectors.FindByPredicate([](const FRigModuleConnector& Connector)
		{
			return Connector.IsPrimary();
		});
	}

	// The identifier used to retrieve the module in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module)
	FRigModuleIdentifier Identifier;

	// The icon used for the module in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module,  meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath Icon;

	// The category of the module
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module)
	FString Category;

	// The keywords of the module
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module)
	FString Keywords;

	// The description of the module
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module, meta = (MultiLine = true))
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = Module)
	TArray<FRigModuleConnector> ExposedConnectors;
};


USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleDescription
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Module)
	FSoftObjectPath Path;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Module)
	FRigModuleSettings Settings;
};