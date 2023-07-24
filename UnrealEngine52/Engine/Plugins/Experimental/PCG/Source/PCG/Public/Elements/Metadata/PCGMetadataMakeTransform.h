// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataMakeTransform.generated.h"

namespace PCGMetadataTransformConstants
{
	const FName Translation = TEXT("Translation");
	const FName Rotation = TEXT("Rotation");
	const FName Scale = TEXT("Scale");
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataMakeTransformSettings : public UPCGMetadataSettingsBase
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	//~End UPCGSettings interface
#endif

	FPCGAttributePropertySelector GetInputSource(uint32 Index) const override;

	virtual FName GetInputPinLabel(uint32 Index) const override;
	virtual uint32 GetInputPinNum() const override;

	virtual bool IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const override;
	virtual uint16 GetOutputType(uint16 InputTypeId) const override;

protected:
	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FPCGAttributePropertySelector InputSource1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FPCGAttributePropertySelector InputSource2;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FPCGAttributePropertySelector InputSource3;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName Input1AttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	FName Input2AttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	FName Input3AttributeName_DEPRECATED = NAME_None;
#endif
};

class FPCGMetadataMakeTransformElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
