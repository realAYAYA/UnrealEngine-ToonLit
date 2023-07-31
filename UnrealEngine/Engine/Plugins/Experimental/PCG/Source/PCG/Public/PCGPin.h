// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGCommon.h"

#include "PCGPin.generated.h"

class UPCGNode;
class UPCGEdge;

USTRUCT(BlueprintType)
struct PCG_API FPCGPinProperties
{
	GENERATED_BODY()

	FPCGPinProperties() = default;
	explicit FPCGPinProperties(const FName& InLabel, EPCGDataType InAllowedTypes = EPCGDataType::Any, bool bInAllowMultipleConnections = true);

	bool operator==(const FPCGPinProperties& Other) const;

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Label = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGDataType AllowedTypes = EPCGDataType::Any;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bAllowMultipleConnections = true;
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGPin : public UObject
{
	GENERATED_BODY()

public:
	UPCGPin(const FObjectInitializer& ObjectInitializer);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	UPROPERTY()
	TObjectPtr<UPCGNode> Node = nullptr;

	UPROPERTY()
	FName Label_DEPRECATED = NAME_None;

	UPROPERTY(TextExportTransient)
	TArray<TObjectPtr<UPCGEdge>> Edges;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGPinProperties Properties;

	bool IsCompatible(const UPCGPin* OtherPin) const;
	bool CanConnect(const UPCGPin* OtherPin) const;
	bool AddEdgeTo(UPCGPin* OtherPin);
	bool BreakEdgeTo(UPCGPin* OtherPin);
	bool BreakAllEdges();
	bool BreakAllIncompatibleEdges();
	bool IsConnected() const;
	int32 EdgeCount() const;
};