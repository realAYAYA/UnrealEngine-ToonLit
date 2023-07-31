// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_UpdateVirtualSubjectDataBase.h"
#include "K2Node_UpdateVirtualSubjectDataTyped.generated.h"

UCLASS()
class LIVELINKGRAPHNODE_API UK2Node_UpdateVirtualSubjectStaticData : public UK2Node_UpdateVirtualSubjectDataBase
{
	GENERATED_BODY()

public:

	//~ Begin UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

protected:

	//~ Begin UK2Node_UpdateVirtualSubjectDataBase interface
	virtual UScriptStruct* GetStructTypeFromRole(ULiveLinkRole* Role) const override;
	virtual FName GetUpdateFunctionName() const override;
	virtual FText GetStructPinName() const override;
	virtual void AddPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* UpdateVirtualSubjectDataFunction) const override {}
	//~ End UK2Node_UpdateVirtualSubjectDataBase interface
};

UCLASS()
class LIVELINKGRAPHNODE_API UK2Node_UpdateVirtualSubjectFrameData : public UK2Node_UpdateVirtualSubjectDataBase
{
	GENERATED_BODY()

public:

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

protected:

	//~ Begin UK2Node_UpdateVirtualSubjectDataBase interface
	virtual UScriptStruct* GetStructTypeFromRole(ULiveLinkRole* Role) const override;
	virtual FName GetUpdateFunctionName() const override;
	virtual FText GetStructPinName() const override;
	virtual void AddPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* UpdateVirtualSubjectDataFunction) const override;
	//~ End UK2Node_UpdateVirtualSubjectDataBase interface

	/** Returns the Timestamp pin  */
	UEdGraphPin* GetTimestampFramePin() const;

private:

	/** Name of the pin to enable/disable timestamping */
	static const FName LiveLinkTimestampFramePinName;
};