// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataBreakTransform.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGMetadataBreakTransformSettings : public UPCGMetadataSettingsBase
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface
	// 
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
#endif
	

	virtual FPCGAttributePropertyInputSelector GetInputSource(uint32 Index) const override;

	virtual FName GetOutputPinLabel(uint32 Index) const override;
	virtual uint32 GetResultNum() const override;

	virtual bool IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const override;
	virtual FName GetOutputAttributeName(FName BaseName, uint32 Index) const override;

	virtual bool HasDifferentOutputTypes() const override;
	virtual TArray<uint16> GetAllOutputTypes() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName InputAttributeName_DEPRECATED = NAME_None;
#endif
};

class FPCGMetadataBreakTransformElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(PCGMetadataOps::FOperationData& OperationData) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Elements/Metadata/PCGMetadataMakeTransform.h"
#endif
