// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusExecutionDomain.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusExecutionDomain)


FString FOptimusExecutionDomain::AsExpression() const
{
	if (Type==EOptimusExecutionDomainType::DomainName)
	{
		return Name.ToString();
	}

	return Expression;
}

bool FOptimusExecutionDomain::IsDefined() const
{
	if (Type == EOptimusExecutionDomainType::DomainName)
	{
		return !Name.IsNone();
	}

	return !Expression.TrimStartAndEnd().IsEmpty();
}
