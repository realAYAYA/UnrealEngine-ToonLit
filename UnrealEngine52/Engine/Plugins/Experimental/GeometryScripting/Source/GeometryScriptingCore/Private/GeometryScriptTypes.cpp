// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryBase.h"



FGeometryScriptDebugMessage UE::Geometry::MakeScriptError(EGeometryScriptErrorType ErrorTypeIn, const FText& MessageIn)
{
	UE_LOG(LogGeometry, Warning, TEXT("GeometryScriptError: %s"), *MessageIn.ToString() );

	return FGeometryScriptDebugMessage{ EGeometryScriptDebugMessageType::ErrorMessage, ErrorTypeIn, MessageIn };
}

FGeometryScriptDebugMessage UE::Geometry::MakeScriptWarning(EGeometryScriptErrorType WarningTypeIn, const FText& MessageIn)
{
	UE_LOG(LogGeometry, Warning, TEXT("GeometryScriptWarning: %s"), *MessageIn.ToString() );

	return FGeometryScriptDebugMessage{ EGeometryScriptDebugMessageType::WarningMessage, WarningTypeIn, MessageIn };
}






void UE::Geometry::AppendError(UGeometryScriptDebug* Debug, EGeometryScriptErrorType ErrorTypeIn, const FText& MessageIn)
{
	FGeometryScriptDebugMessage Result = MakeScriptError(ErrorTypeIn, MessageIn);
	if (Debug != nullptr)
	{
		Debug->Append(Result);
	}
}

void UE::Geometry::AppendWarning(UGeometryScriptDebug* Debug, EGeometryScriptErrorType WarningTypeIn, const FText& MessageIn)
{
	FGeometryScriptDebugMessage Result = MakeScriptWarning(WarningTypeIn, MessageIn);
	if (Debug != nullptr)
	{
		Debug->Append(Result);
	}
}

void UE::Geometry::AppendError(TArray<FGeometryScriptDebugMessage>* DebugMessages, EGeometryScriptErrorType ErrorType, const FText& Message)
{
	FGeometryScriptDebugMessage Result = MakeScriptError(ErrorType, Message);
	if (DebugMessages != nullptr)
	{
		DebugMessages->Add(Result);
	}
}

void UE::Geometry::AppendWarning(TArray<FGeometryScriptDebugMessage>* DebugMessages, EGeometryScriptErrorType WarningType, const FText& Message)
{
	FGeometryScriptDebugMessage Result = MakeScriptWarning(WarningType, Message);
	if (DebugMessages != nullptr)
	{
		DebugMessages->Add(Result);
	}
}
