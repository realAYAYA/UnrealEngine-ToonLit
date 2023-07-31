// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Misc/AutomationTest.h"

#include "ImgMediaMipMapInfo.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImgMediaTests, "System.Plugins.ImgMedia.TileSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImgMediaTests::RunTest(const FString& Parameters)
{
	{
		// Single continuous region
		FImgMediaTileSelection Selection = FImgMediaTileSelection(10, 10);
		for (int32 Ty = 3; Ty < 7; Ty++)
		{
			for (int32 Tx = 1; Tx < 6; Tx++)
			{
				Selection.SetVisible(Tx, Ty);
			}
		}

		FIntRect R0 = Selection.GetVisibleRegion();
		FIntRect R1 = Selection.GetVisibleRegions()[0];
		AddErrorIfFalse(R0 == R1, TEXT("FImgMediaTests: Mismatched tile regions."));
	}

	{
		//Two regions on both sides
		FImgMediaTileSelection Selection = FImgMediaTileSelection(10, 10);
		for (int32 Ty = 0; Ty < 10; Ty++)
		{
			for (int32 Tx = 0; Tx < 2; Tx++)
			{
				Selection.SetVisible(Tx, Ty);
			}

			for (int32 Tx = 8; Tx < 10; Tx++)
			{
				Selection.SetVisible(Tx, Ty);
			}
		}
		TArray<FIntRect> Result = Selection.GetVisibleRegions();

		bool bExpectedNum = Result.Num() == 2;
		if (bExpectedNum)
		{
			AddErrorIfFalse(Result[0] == FIntRect(FIntPoint(0, 0), FIntPoint(2, 10)), TEXT("FImgMediaTests: Mismatched tile regions."));
			AddErrorIfFalse(Result[1] == FIntRect(FIntPoint(8, 0), FIntPoint(10, 10)), TEXT("FImgMediaTests: Mismatched tile regions."));
		}
		else
		{
			AddError(TEXT("FImgMediaTests: Expected 2 regions."));
		}
	}

	{
		//Each row has a different length, resulting in one region for each row with the current algo.
		FImgMediaTileSelection Selection = FImgMediaTileSelection(10, 10);
		for (int32 Ty = 0; Ty < 10; Ty++)
		{
			for (int32 Tx = 0; Tx < Ty + 1; Tx++)
			{
				Selection.SetVisible(Tx, Ty);
			}
		}

		TArray<FIntRect> Result = Selection.GetVisibleRegions();

		AddErrorIfFalse(Result.Num() == 10, TEXT("FImgMediaTests: Expected 10 regions."));
	}

	{
		//Worst case: checkerboard pattern where each tile becomes a region.
		FImgMediaTileSelection Selection = FImgMediaTileSelection(4, 4);
		Selection.SetVisible(0, 0); Selection.SetVisible(2, 0);
		Selection.SetVisible(1, 1); Selection.SetVisible(3, 1);
		Selection.SetVisible(0, 2); Selection.SetVisible(2, 2);
		Selection.SetVisible(1, 3); Selection.SetVisible(3, 3);

		TArray<FIntRect> Result = Selection.GetVisibleRegions();

		AddErrorIfFalse(Result.Num() == 8, TEXT("FImgMediaTests: Expected 8 regions."));
	}

	return !HasAnyErrors();
}

#endif
