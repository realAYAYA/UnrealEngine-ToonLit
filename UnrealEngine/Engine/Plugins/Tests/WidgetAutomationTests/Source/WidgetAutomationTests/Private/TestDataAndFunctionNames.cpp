// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestDataAndFunctionNames.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::SlateWidgetAutomationTest
{
	namespace FTestIONames
	{
		const FLazyName NAME_FailedToLoadJSON = "FailedToLoadJSON";
		const FLazyName NAME_TestFileNotFound = "TestFileNotFound";
		const FLazyName NAME_BadTestFile = "BadTestFile";
	};

	namespace FTestFunctionNames
	{
		const FLazyName NAME_OnPaint = "OnPaint";
		const FLazyName NAME_ComputeDesiredSize = "ComputeDesiredSize";
		const FLazyName NAME_OnFocusChanging = "OnFocusChanging";
		const FLazyName NAME_OnFocusLost = "OnFocusLost";
		const FLazyName NAME_OnFocusReceived = "OnFocusReceived";
	};
}
#endif