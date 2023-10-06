// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMRerouteNode.generated.h"

/**
 * A reroute node is used to visually improve the 
 * data flow within a Graph. Reroutes are purely 
 * cosmetic and have no impact on the resulting
 * VM whatsoever. Reroutes can furthermore be
 * displayed as full nodes or as small circles.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMRerouteNode : public URigVMNode
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMRerouteNode();

	// Override of node title
	virtual FString GetNodeTitle() const override;

	virtual FLinearColor GetNodeColor() const override;

	// Has no source connections
	bool IsLiteral() const;

private:

	static const FString RerouteName;
	static const FString ValueName;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend class FRigVMParserAST;
	friend class FRigVMDeveloperModule;
};

