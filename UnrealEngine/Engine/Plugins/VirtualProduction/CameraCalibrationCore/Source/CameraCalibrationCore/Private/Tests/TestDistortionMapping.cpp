// Copyright Epic Games, Inc. All Rights Reserved.


#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

#include "CameraCalibrationSubsystem.h"
#include "CameraCalibrationTypes.h"
#include "Engine/Engine.h"
#include "LensFile.h"
#include "Misc/AutomationTest.h"
#include "SphericalLensDistortionModelHandler.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestCameraCalibrationCore, "Plugin.CameraCalibrationCore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


namespace CameraCalibrationTestUtil
{
	UWorld* GetFirstWorld()
	{
		//Get first valid world
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				if(Context.WorldType == EWorldType::Editor)
				{
					return Context.World();
				}
			}
			else
			{
				if(Context.World() != nullptr)
				{
					return Context.World();
				}
			}
#else
			if(Context.World() != nullptr)
			{
				return Context.World();
			}
#endif
		}

		return nullptr;
	}


	template<typename T>
	void TestEvaluationResult(FAutomationTestBase& Test, TConstArrayView<T> InExpected, TConstArrayView<T> InResult)
	{
		for(int32 Index = 0; Index < InResult.Num(); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("Parameter[%d] should be equal to %0.2f"), Index, InExpected[Index]), InResult[Index], InExpected[Index]);
		}	
	}

	template<typename T>
	void  TestDualCurveEvaluationResult(FAutomationTestBase& Test, TConstArrayView<T> InResult, T InBlendingFactor, TConstArrayView<T> InLerpSource1, TConstArrayView<T> InLerpSource2, TConstArrayView<T> InLerpSource3, TConstArrayView<T> InLerpSource4)
	{
		for(int32 Index = 0; Index < InResult.Num(); ++Index)
		{
			const T CurveOneBlending = FMath::Lerp(InLerpSource1[Index], InLerpSource2[Index], InBlendingFactor);
			const T CurveTwoBlending = FMath::Lerp(InLerpSource3[Index], InLerpSource4[Index], InBlendingFactor);
			const T ExpectedValue = FMath::Lerp(CurveOneBlending, CurveTwoBlending, InBlendingFactor);
			Test.TestEqual(FString::Printf(TEXT("Parameter[%d] should be equal to %0.2f"), Index, ExpectedValue), InResult[Index], ExpectedValue);
		}
	}

	template<typename T>
	void TestSingleCurveEvaluationResult(FAutomationTestBase& Test, TConstArrayView<T> InResult, T InBlendingFactor, TConstArrayView<T> InLerpSource1, TConstArrayView<T> InLerpSource2)
	{
		for(int32 Index = 0; Index < InResult.Num(); ++Index)
		{
			const T ExpectedValue = FMath::Lerp(InLerpSource1[Index], InLerpSource2[Index], InBlendingFactor);
			Test.TestEqual(FString::Printf(TEXT("Parameter[%d] should be equal to %0.2f"), Index, ExpectedValue), InResult[Index], ExpectedValue);
		}
	}

	void TestDistortionParameterCurveBlending(FAutomationTestBase& Test)
	{
		UWorld* ValidWorld = GetFirstWorld();
		if(ValidWorld == nullptr)
		{
			return;
		}
		
		// Create LensFile container
		const TCHAR* LensFileName = TEXT("AutomationTestLensFile");
		ULensFile* LensFile = NewObject<ULensFile>(GetTransientPackage(), LensFileName);


		const TCHAR* HandlerName = TEXT("AutomationTestLensHandler");
		USphericalLensDistortionModelHandler* ProducedLensDistortionHandler = NewObject<USphericalLensDistortionModelHandler>(ValidWorld, HandlerName, RF_Transient);
		ensure(ProducedLensDistortionHandler);
		if(ProducedLensDistortionHandler == nullptr)
		{
			return;
		}
		
		struct FLensData
		{
			FLensData() = default;
			FLensData(float InFocus, float InZoom, FDistortionInfo InDistortionInfo, FImageCenterInfo InImageCenter, FFocalLengthInfo InFocalLength)
				: Focus(InFocus)
				, Zoom(InZoom)
				, Distortion(InDistortionInfo)
				, ImageCenter(InImageCenter)
				, FocalLength(InFocalLength)
			{}
			
			float Focus;
			float Zoom;
			FDistortionInfo Distortion;
			FImageCenterInfo ImageCenter;
			FFocalLengthInfo FocalLength;
		};

		constexpr int32 InputCount = 4;
		FLensData InputData[InputCount] =
		{
			  FLensData(0.0f, 0.0f, {{TArray<float>({1.0f, -1.0f, 0.0f, 0.0f, 0.0f})}}, {FVector2D(8.0f, 7.0f)}, {FVector2D(3.0f, 2.0f)})
			, FLensData(0.0f, 1.0f, {{TArray<float>({2.0f, 0.0f, 0.0f, 0.0f, 1.0f})}},  {FVector2D(9.0f, 8.0f)}, {FVector2D(5.0f, 3.0f)})
			, FLensData(1.0f, 0.0f, {{TArray<float>({0.0f, 1.0f, 0.0f, 0.0f, -2.0f})}}, {FVector2D(2.0f, 4.0f)}, {FVector2D(4.0f, 6.0f)})
			, FLensData(1.0f, 1.0f, {{TArray<float>({1.0f, 2.0f, 0.0f, 0.0f, 0.0f})}},  {FVector2D(4.0f, 7.0f)}, {FVector2D(9.0f, 8.0f)})
		};

		for(int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
		{
			//straight tests
			const FLensData& Input = InputData[InputIndex];
			LensFile->AddDistortionPoint(Input.Focus, Input.Zoom, Input.Distortion, Input.FocalLength);
			LensFile->AddImageCenterPoint(Input.Focus, Input.Zoom, Input.ImageCenter);
		}

		const FLensData& Input0 = InputData[0];
		const FLensData& Input1 = InputData[1];
		const FLensData& Input2 = InputData[2];
		const FLensData& Input3 = InputData[3];
		FLensData EvaluatedData;
		LensFile->EvaluateImageCenterParameters(Input0.Focus, Input0.Zoom, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input0.Focus, Input0.Zoom, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input0.Focus, Input0.Zoom, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input0.Focus, Input0.Zoom, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y});
		TestEvaluationResult<float>(Test, Input0.Distortion.Parameters, EvaluatedData.Distortion.Parameters);
		TestEvaluationResult<float>(Test, Input0.Distortion.Parameters, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y});

		LensFile->EvaluateImageCenterParameters(Input1.Focus, Input1.Zoom, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input1.Focus, Input1.Zoom, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input1.Focus, Input1.Zoom, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input1.Focus, Input1.Zoom, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y}, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y}, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y});
		TestEvaluationResult<float>(Test, Input1.Distortion.Parameters, EvaluatedData.Distortion.Parameters);
		TestEvaluationResult<float>(Test, Input1.Distortion.Parameters, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y});

		
		LensFile->EvaluateImageCenterParameters(Input2.Focus, Input2.Zoom, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input2.Focus, Input2.Zoom, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input2.Focus, Input2.Zoom, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input2.Focus, Input2.Zoom, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y}, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y}, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y});
		TestEvaluationResult<float>(Test, Input2.Distortion.Parameters, EvaluatedData.Distortion.Parameters);
		TestEvaluationResult<float>(Test, Input2.Distortion.Parameters, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y});

		LensFile->EvaluateImageCenterParameters(Input3.Focus, Input3.Zoom, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input3.Focus, Input3.Zoom, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input3.Focus, Input3.Zoom, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input3.Focus, Input3.Zoom, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y}, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y}, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y});
		TestEvaluationResult<float>(Test, Input3.Distortion.Parameters, EvaluatedData.Distortion.Parameters);
		TestEvaluationResult<float>(Test, Input3.Distortion.Parameters, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters);
		TestEvaluationResult<FVector2D::FReal>(Test, {Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y});
		TestEvaluationResult<FVector2D::FReal>(Test, {Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y}, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y});

		//Sin
		constexpr FVector2D::FReal BlendFactor = 0.25f;	
		LensFile->EvaluateImageCenterParameters(Input0.Focus, 0.25f, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input0.Focus, 0.25f, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input0.Focus, 0.25f, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input0.Focus, 0.25f, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y}, BlendFactor, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y});
		TestSingleCurveEvaluationResult<float>(Test, EvaluatedData.Distortion.Parameters, BlendFactor, Input0.Distortion.Parameters, Input1.Distortion.Parameters);
		TestSingleCurveEvaluationResult<float>(Test, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters, BlendFactor, Input0.Distortion.Parameters, Input1.Distortion.Parameters);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y}, BlendFactor, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y});

		
		LensFile->EvaluateImageCenterParameters(Input2.Focus, 0.25f, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(Input2.Focus, 0.25f, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(Input2.Focus, 0.25f, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(Input2.Focus, 0.25f, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y}, {Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y}, BlendFactor, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y}, {Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y});
		TestSingleCurveEvaluationResult<float>(Test, EvaluatedData.Distortion.Parameters, BlendFactor, Input2.Distortion.Parameters, Input3.Distortion.Parameters);
		TestSingleCurveEvaluationResult<float>(Test, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters, BlendFactor, Input2.Distortion.Parameters, Input3.Distortion.Parameters);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y}, {Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y}, BlendFactor, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y}, {Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y});

		LensFile->EvaluateImageCenterParameters(0.25f, Input0.Zoom, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(0.25f, Input0.Zoom, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(0.25f, Input0.Zoom, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(0.25f, Input0.Zoom, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y}, BlendFactor, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y});
		TestSingleCurveEvaluationResult<float>(Test, EvaluatedData.Distortion.Parameters, BlendFactor, Input0.Distortion.Parameters, Input2.Distortion.Parameters);
		TestSingleCurveEvaluationResult<float>(Test, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters, BlendFactor, Input0.Distortion.Parameters, Input2.Distortion.Parameters);
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y}, BlendFactor, {Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y}, {Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y});
		TestSingleCurveEvaluationResult<FVector2D::FReal>(Test, {ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y}, BlendFactor, {Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y}, {Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y});
		
		LensFile->EvaluateImageCenterParameters(0.25f, 0.25f, EvaluatedData.ImageCenter);
		LensFile->EvaluateDistortionParameters(0.25f, 0.25f, EvaluatedData.Distortion);
		LensFile->EvaluateFocalLength(0.25f, 0.25f, EvaluatedData.FocalLength);
		LensFile->EvaluateDistortionData(0.25f, 0.25f, LensFile->LensInfo.SensorDimensions, ProducedLensDistortionHandler);
		TestDualCurveEvaluationResult<FVector2D::FReal>(Test, { EvaluatedData.ImageCenter.PrincipalPoint.X, EvaluatedData.ImageCenter.PrincipalPoint.Y }, BlendFactor, { Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y }, { Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y }, { Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y }, { Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y });
		TestDualCurveEvaluationResult<FVector2D::FReal>(Test, { EvaluatedData.FocalLength.FxFy.X, EvaluatedData.FocalLength.FxFy.Y }, BlendFactor, { Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y }, { Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y }, { Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y }, { Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y });
		TestDualCurveEvaluationResult<float>(Test, EvaluatedData.Distortion.Parameters, BlendFactor, Input0.Distortion.Parameters, Input1.Distortion.Parameters, Input2.Distortion.Parameters, Input3.Distortion.Parameters);
		TestDualCurveEvaluationResult<float>(Test, ProducedLensDistortionHandler->GetCurrentDistortionState().DistortionInfo.Parameters, BlendFactor, Input0.Distortion.Parameters, Input1.Distortion.Parameters, Input2.Distortion.Parameters, Input3.Distortion.Parameters);
		TestDualCurveEvaluationResult<FVector2D::FReal>(Test, { ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.X, ProducedLensDistortionHandler->GetCurrentDistortionState().ImageCenter.PrincipalPoint.Y }, BlendFactor, { Input0.ImageCenter.PrincipalPoint.X, Input0.ImageCenter.PrincipalPoint.Y }, { Input1.ImageCenter.PrincipalPoint.X, Input1.ImageCenter.PrincipalPoint.Y }, { Input2.ImageCenter.PrincipalPoint.X, Input2.ImageCenter.PrincipalPoint.Y }, { Input3.ImageCenter.PrincipalPoint.X, Input3.ImageCenter.PrincipalPoint.Y });
		TestDualCurveEvaluationResult<FVector2D::FReal>(Test, { ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.X, ProducedLensDistortionHandler->GetCurrentDistortionState().FocalLengthInfo.FxFy.Y }, BlendFactor, { Input0.FocalLength.FxFy.X, Input0.FocalLength.FxFy.Y }, { Input1.FocalLength.FxFy.X, Input1.FocalLength.FxFy.Y }, { Input2.FocalLength.FxFy.X, Input2.FocalLength.FxFy.Y }, { Input3.FocalLength.FxFy.X, Input3.FocalLength.FxFy.Y });
	}

	void TestLensFileAddPoints(FAutomationTestBase& Test)
	{
		// Create LensFile container
		const TCHAR* LensFileName = TEXT("AddPointsTestLensFile");
		ULensFile* LensFile = NewObject<ULensFile>(GetTransientPackage(), LensFileName);

		FDistortionInfo TestDistortionParams;
		TestDistortionParams.Parameters = TArray<float>({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });

		FFocalLengthInfo TestFocalLength;
		TestFocalLength.FxFy = FVector2D(1.0f, 1.777f);

		TArray<FDistortionFocusPoint>& FocusPoints = LensFile->DistortionTable.GetFocusPoints();

		// Clear the LensFile's distortion table and confirm that the FocusPoints array is empty
		LensFile->ClearAll();
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 0);

		// Add a single point at F=0 Z=0 and confirm that FocusPoints array has one curve, and that curve has one ZoomPoint
		LensFile->AddDistortionPoint(0.0f, 0.0f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 1);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 1);

		// Add a point at F=0 Z=0.5 and confirm that FocusPoints array has one curve, and that curve has two ZoomPoints
		LensFile->AddDistortionPoint(0.0f, 0.5f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 1);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 2);

		// Attempt to add a duplicate point at F=0 Z=0.5 and confirm that FocusPoints array has one curve, and that curve has two ZoomPoints
		LensFile->AddDistortionPoint(0.0f, 0.5f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 1);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 2);

		// Test tolerance when adding a new zoom point
		LensFile->AddDistortionPoint(0.0f, 0.49f, TestDistortionParams, TestFocalLength);
		LensFile->AddDistortionPoint(0.0f, 0.4999f, TestDistortionParams, TestFocalLength);
		LensFile->AddDistortionPoint(0.0f, 0.5001f, TestDistortionParams, TestFocalLength);
		LensFile->AddDistortionPoint(0.0f, 0.51f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 1);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 4);

		// Add two points at F=1 Z=0 and F=1 Z=0.5 and confirm that FocusPoints array has two curves, and that each curve has two ZoomPoints
		LensFile->AddDistortionPoint(1.0f, 0.0f, TestDistortionParams, TestFocalLength);
		LensFile->AddDistortionPoint(1.0f, 0.5f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 2);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 4);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[1].ZoomPoints.Num(), 2);

		// Test sorting when adding a new focus point
		LensFile->AddDistortionPoint(0.5f, 0.0f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 3);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 4);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[1].ZoomPoints.Num(), 1);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[2].ZoomPoints.Num(), 2);

		// Test tolerance when adding focus points with slight differences in value
		LensFile->AddDistortionPoint(0.5001f, 0.25f, TestDistortionParams, TestFocalLength);
		LensFile->AddDistortionPoint(0.4999f, 0.5f, TestDistortionParams, TestFocalLength);
		Test.TestEqual(FString::Printf(TEXT("Num Focus Points")), FocusPoints.Num(), 3);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[0].ZoomPoints.Num(), 4);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[1].ZoomPoints.Num(), 3);
		Test.TestEqual(FString::Printf(TEXT("Num Zoom Points")), FocusPoints[2].ZoomPoints.Num(), 2);

		// Finally, test final state of each focus curve to ensure proper values and sorting
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[0]")), FocusPoints[0].ZoomPoints[0].Zoom, 0.0f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[1]")), FocusPoints[0].ZoomPoints[1].Zoom, 0.49f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[2]")), FocusPoints[0].ZoomPoints[2].Zoom, 0.5f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[3]")), FocusPoints[0].ZoomPoints[3].Zoom, 0.51f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[0]")), FocusPoints[1].ZoomPoints[0].Zoom, 0.0f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[1]")), FocusPoints[1].ZoomPoints[1].Zoom, 0.25f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[2]")), FocusPoints[1].ZoomPoints[2].Zoom, 0.5f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[0]")), FocusPoints[2].ZoomPoints[0].Zoom, 0.0f);
		Test.TestEqual(FString::Printf(TEXT("FocusPoints[0], ZoomPoints[1]")), FocusPoints[2].ZoomPoints[1].Zoom, 0.5f);
	}

	template <bool Const, typename Class, typename FuncType>
    struct TClassFunPtrType;
    
    template <typename Class, typename RetType, typename... ArgTypes>
    struct TClassFunPtrType<false, Class, RetType(ArgTypes...)>
    {
    	typedef RetType (Class::* Type)(ArgTypes...);
    };
    
    template <typename Class, typename RetType, typename... ArgTypes>
    struct TClassFunPtrType<true, Class, RetType(ArgTypes...)>
    {
    	typedef RetType (Class::* Type)(ArgTypes...) const;
    };

	template <typename FPointType>
	void TestRemoveSinglePoint(FAutomationTestBase& Test, ULensFile* InLensFile, ELensDataCategory InDataCategory,
	                       typename TClassFunPtrType<false, ULensFile, void(float, float, const FPointType&)>::Type
	                       InFunc)
	{
		UEnum* Enum = StaticEnum<ELensDataCategory>();
		const FString EnumString = Enum ? Enum->GetNameByIndex(static_cast<uint8>(InDataCategory)).ToString() : "";
		
		Test.AddInfo(FString::Printf(TEXT("Remove %s"), *EnumString));
		const FPointType PointInfo;
		const float FocusValue = 0.12f;
		const float ZoomValue = 0.91f;
		(InLensFile->*InFunc)(FocusValue, ZoomValue, PointInfo);
		InLensFile->RemoveZoomPoint(InDataCategory, FocusValue, ZoomValue);
		const int32 PointsNum = InLensFile->GetTotalPointNum(InDataCategory);
		Test.TestEqual(TEXT("Num Zoom Points"), PointsNum, 0);
	}

	void TestLensFileRemovePoints(FAutomationTestBase& Test)
	{
		constexpr auto LensFileName = TEXT("RemovePointsTestLensFile");
		ULensFile* LensFile = NewObject<ULensFile>(GetTransientPackage(), LensFileName);

		Test.AddInfo(TEXT("Remove ELensDataCategory::Focus"));
		{
			constexpr float FocusKeyTimeValue = 0.12f;
			constexpr float FocusKeyMappingValue = 0.91f;
			LensFile->EncodersTable.Focus.AddKey(FocusKeyTimeValue, FocusKeyMappingValue);
			Test.TestEqual(TEXT("Time Value Should be the same"), LensFile->EncodersTable.GetFocusInput(0), FocusKeyTimeValue);
			LensFile->EncodersTable.RemoveFocusPoint(FocusKeyTimeValue);
			Test.TestEqual(TEXT("Num Focus Points"), LensFile->EncodersTable.GetNumFocusPoints(), 0);
		}

		Test.AddInfo(TEXT("Remove ELensDataCategory::Iris"));
		{
			constexpr float IrisKeyTimeValue = 0.12f;
			constexpr float IrisKeyMappingValue = 0.91f;
			LensFile->EncodersTable.Iris.AddKey(IrisKeyTimeValue, IrisKeyMappingValue);
			Test.TestEqual(TEXT("Time Value should be the same"), LensFile->EncodersTable.GetIrisInput(0), IrisKeyTimeValue);
			LensFile->EncodersTable.RemoveIrisPoint(IrisKeyTimeValue);
			Test.TestEqual(TEXT("Num Iris Points"), LensFile->EncodersTable.GetNumIrisPoints(), 0);
		}
		
		Test.AddInfo(TEXT("Remove all point ELensDataCategory::Zoom"));
		{
			const FFocalLengthInfo FocalLengthInfo;
			const float FocusValue = 0.12f;
			const float ZoomValue1 = 0.191f;
			const float ZoomValue2 = 0.91f;
			LensFile->AddFocalLengthPoint(FocusValue, ZoomValue1, FocalLengthInfo);
			LensFile->AddFocalLengthPoint(FocusValue, ZoomValue2, FocalLengthInfo);
			LensFile->RemoveFocusPoint(ELensDataCategory::Zoom, FocusValue);
			const int32 PointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num Zoom Points"), PointsNum, 0);
		}

		Test.AddInfo(TEXT("Remove ELensDataCategory::Distortion"));
		{
			const FFocalLengthInfo FocalLengthInfo;
			const FDistortionInfo DistortionPoint;
			const float FocusValue = 0.12f;
			const float ZoomValue = 0.191f;
			LensFile->AddDistortionPoint(FocusValue, ZoomValue, DistortionPoint, FocalLengthInfo);
			LensFile->RemoveZoomPoint(ELensDataCategory::Distortion, FocusValue, ZoomValue);
			const int32 DistortionPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Distortion);
			int32 FocalLengthPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num Distortion Points"), DistortionPointsNum, 0);
			Test.TestEqual(TEXT("Num FocalLength Points"), FocalLengthPointsNum, 1);
			LensFile->RemoveZoomPoint(ELensDataCategory::Zoom, FocusValue, ZoomValue);
			FocalLengthPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num FocalLength Points"), FocalLengthPointsNum, 0);
		}

		Test.AddInfo(TEXT("Remove ELensDataCategory::Distortion, add focal length before add distortion point"));
		{
			const FFocalLengthInfo FocalLengthInfo;
			const FDistortionInfo DistortionPoint;
			const float FocusValue = 0.12f;
			const float ZoomValue = 0.191f;
			LensFile->AddFocalLengthPoint(FocusValue, ZoomValue, FocalLengthInfo);
			LensFile->AddDistortionPoint(FocusValue, ZoomValue, DistortionPoint, FocalLengthInfo);
			int32 FocalLengthPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num FocalLength Points"), FocalLengthPointsNum, 1);
			LensFile->RemoveZoomPoint(ELensDataCategory::Distortion, FocusValue, ZoomValue);
			const int32 DistortionPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Distortion);
			FocalLengthPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num Distortion Points"), DistortionPointsNum, 0);
			Test.TestEqual(TEXT("Num FocalLength Points"), FocalLengthPointsNum, 1);
			LensFile->RemoveZoomPoint(ELensDataCategory::Zoom, FocusValue, ZoomValue);
			FocalLengthPointsNum = LensFile->GetTotalPointNum(ELensDataCategory::Zoom);
			Test.TestEqual(TEXT("Num FocalLength Points"), FocalLengthPointsNum, 0);
		}

		TestRemoveSinglePoint<FFocalLengthInfo>(Test, LensFile, ELensDataCategory::Zoom, &ULensFile::AddFocalLengthPoint);
		TestRemoveSinglePoint<FImageCenterInfo>(Test, LensFile, ELensDataCategory::ImageCenter, &ULensFile::AddImageCenterPoint);
		TestRemoveSinglePoint<FNodalPointOffset>(Test, LensFile, ELensDataCategory::NodalOffset, &ULensFile::AddNodalOffsetPoint);
		TestRemoveSinglePoint<FSTMapInfo>(Test, LensFile, ELensDataCategory::STMap, &ULensFile::AddSTMapPoint);
	}
}


bool FTestCameraCalibrationCore::RunTest(const FString& Parameters)
{
	CameraCalibrationTestUtil::TestDistortionParameterCurveBlending(*this);
	CameraCalibrationTestUtil::TestLensFileAddPoints(*this);
	CameraCalibrationTestUtil::TestLensFileRemovePoints(*this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS



