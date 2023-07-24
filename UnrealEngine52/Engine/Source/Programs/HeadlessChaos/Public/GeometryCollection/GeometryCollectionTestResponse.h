// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace GeometryCollectionTest
{
	class ExampleResponse
	{
	public:
		ExampleResponse() : ErrorFlag(false) {}
		virtual ~ExampleResponse() {}
		virtual void ExpectTrue(bool, FString Reason = "");
		virtual bool HasError() { return ErrorFlag; }
		TArray<FString> Reasons;
		bool ErrorFlag;
	};

}