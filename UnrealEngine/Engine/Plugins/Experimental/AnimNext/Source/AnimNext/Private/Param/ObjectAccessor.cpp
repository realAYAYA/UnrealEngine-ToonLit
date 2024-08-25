// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectAccessor.h"
#include "Param/ClassProxy.h"
#include "EngineLogs.h"

namespace UE::AnimNext
{

FObjectAccessor::FObjectAccessor(FName InAccessorName, FObjectAccessorFunction&& InFunction, TSharedRef<FClassProxy> InClassProxy)
	: AccessorName(InAccessorName)
	, Function(MoveTemp(InFunction))
	, ClassProxy(InClassProxy)
{
	// Prefix class proxy names into RemappedParameters
	RemappedParameters.Reserve(ClassProxy->Parameters.Num());
	for(const FClassProxyParameter& ClassProxyParameter : ClassProxy->Parameters)
	{
		TStringBuilder<128> RemappedNameBuilder;
		AccessorName.AppendString(RemappedNameBuilder);
		RemappedNameBuilder.Append(TEXT("_"));
		ClassProxyParameter.ClassParameterName.AppendString(RemappedNameBuilder);

		FName RemappedName = RemappedNameBuilder.ToString();
		int32 Index = RemappedParameters.Add(RemappedName);
		
		RemappedParametersMap.Add(RemappedName, Index);
	}
}
	
}
