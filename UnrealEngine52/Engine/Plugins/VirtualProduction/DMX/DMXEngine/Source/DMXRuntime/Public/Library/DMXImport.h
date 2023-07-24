// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DMXImport.generated.h"

namespace DMXImport
{
    template <typename EnumType> static EnumType GetEnumValueFromString(const FString& String)
    {
        return static_cast<EnumType>(StaticEnum<EnumType>()->GetValueByName(FName(*String)));
    }

    DMXRUNTIME_API FDMXColorCIE ParseColorCIE(const FString& InColor);

    DMXRUNTIME_API FMatrix ParseMatrix(FString&& InMatrixStr);
};

USTRUCT(BlueprintType)
struct FDMXColorCIE
{
    GENERATED_BODY()

        FDMXColorCIE()
        : X(0.f)
        , Y(0.f)
        , YY(0)
    {}

    UPROPERTY(EditAnywhere, Category = Color, meta = (ClampMin = "0", ClampMax = "1.0"))
    float X;

    UPROPERTY(EditAnywhere, Category = Color, meta = (ClampMin = "0", ClampMax = "1.0"))
    float Y;

    UPROPERTY(EditAnywhere, Category = Color, meta = (ClampMin = "0", ClampMax = "255"))
    uint8 YY;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportFixtureType
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportAttributeDefinitions
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportWheels
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportPhysicalDescriptions
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportModels
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGeometries
    : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportDMXModes
    : public UObject
{
    GENERATED_BODY()

};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportProtocols
    : public UObject
{
    GENERATED_BODY()

};

UCLASS(BlueprintType, Blueprintable, abstract)
class DMXRUNTIME_API UDMXImport
	: public UObject
{
	GENERATED_BODY()

public:
	template <typename TType>
	TType* CreateNewObject()
	{
        FName NewName = MakeUniqueObjectName(this, TType::StaticClass());
        return NewObject<TType>(this, TType::StaticClass(), NewName, RF_Public);
	}

public:
	UPROPERTY(VisibleAnywhere, Category = "Fixture Type", meta = (EditInline))
	TObjectPtr<UDMXImportFixtureType> FixtureType;

    UPROPERTY(VisibleAnywhere, Category = "Attribute Definitions", meta = (EditInline))
    TObjectPtr<UDMXImportAttributeDefinitions> AttributeDefinitions;

    UPROPERTY(VisibleAnywhere, Category = "Wheels", meta = (EditInline))
    TObjectPtr<UDMXImportWheels> Wheels;

    UPROPERTY(VisibleAnywhere, Category = "Physical Descriptions", meta = (EditInline))
    TObjectPtr<UDMXImportPhysicalDescriptions> PhysicalDescriptions;

    UPROPERTY(VisibleAnywhere, Category = "Models", meta = (EditInline))
    TObjectPtr<UDMXImportModels> Models;

    UPROPERTY(VisibleAnywhere, Category = "Geometries", meta = (EditInline))
    TObjectPtr<UDMXImportGeometries> Geometries;

    UPROPERTY(VisibleAnywhere, Category = "DMXModes", meta = (EditInline))
    TObjectPtr<UDMXImportDMXModes> DMXModes;

    UPROPERTY(VisibleAnywhere, Category = "Protocols", meta = (EditInline))
    TObjectPtr<UDMXImportProtocols> Protocols;
};
