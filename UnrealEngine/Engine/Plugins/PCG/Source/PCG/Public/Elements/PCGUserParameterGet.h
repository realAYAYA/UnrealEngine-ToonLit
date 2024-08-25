// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGUserParameterGet.generated.h"

/**
* Getter for user parameters defined in PCGGraph, by the user.
* Will pick up the value from the graph instance.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGUserParameterGetSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGUserParameterGetSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	FGuid PropertyGuid;

	UPROPERTY()
	FName PropertyName;

	/** If the property is a struct/object supported by metadata, this option can be toggled to force extracting all (compatible) properties contained in this property. Automatically true if unsupported by metadata. For now, only supports direct child properties (and not deeper). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bForceObjectAndStructExtraction = false;
	
	void UpdatePropertyName(FName InNewName);

	//~Begin UPCGSettings interface
	virtual bool RequiresDataFromPreTask() const override { return true; }
#if WITH_EDITOR
	virtual bool CanUserEditTitle() const override { return false; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName("GetGraphParameter"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGUserParameterGetSettings", "NodeTitle", "Get Graph Parameter"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GraphParameters; }
#endif
	//~End UPCGSettings interface
};

class FPCGUserParameterGetElement : public IPCGElement
{
public:
	// Graph parameters access is not thread safe
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
