// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

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
#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override;
	//~End UPCGSettings interface
#endif

	virtual FName GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const override;

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
	FName Input1AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input2AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input3AttributeName = NAME_None;
};

class FPCGMetadataMakeTransformElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
