// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTextureGraphError, Log, All);
struct FTextureGraphErrorReport
{
	int32 ErrorId;
	FString ErrorMsg;
	UObject* ReferenceObj = nullptr;
	
	FString GetFormattedMessage()
	{
		return FString::Format(TEXT("({0}) {1}"), { ErrorId, *ErrorMsg });
	}
};
using ErrorReportMap = TMap<int32, TArray<FTextureGraphErrorReport>>;
enum class ETextureGraphErrorType: int32
{
	UNSUPPORTED_TYPE,
	MISSING_REQUIRED_INPUT,
	RECURSIVE_CALL,
	UNSUPPORTED_MATERIAL,
	SUBGRAPH_INTERNAL_ERROR,
	NODE_WARNING
};
class TEXTUREGRAPHENGINE_API FTextureGraphErrorReporter
{
public:
	FTextureGraphErrorReporter()
	{
	}
	
	virtual ~FTextureGraphErrorReporter() { }
	
	virtual FTextureGraphErrorReport ReportLog(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr);

	virtual FTextureGraphErrorReport ReportWarning(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr);

	virtual FTextureGraphErrorReport ReportError(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr);
	
	virtual void Clear()
	{
		CompilationErrors.Empty();
	}
protected:
	ErrorReportMap CompilationErrors;

public:
	ErrorReportMap GetCompilationErrors() const
	{
		return CompilationErrors;
	}
};
