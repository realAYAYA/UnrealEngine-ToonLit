// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensFile.h"

#include "Algo/MaxElement.h"
#include "CalibratedMapProcessor.h"
#include "CameraCalibrationCoreLog.h"
#include "CameraCalibrationSettings.h"
#include "CameraCalibrationSubsystem.h"
#include "CineCameraComponent.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LensDistortionModelHandlerBase.h"
#include "LensFileRendering.h"
#include "LensInterpolationUtils.h"
#include "Curves/CurveEvaluation.h"
#include "Models/SphericalLensModel.h"
#include "Tables/BaseLensTable.h"
#include "Tables/LensTableUtils.h"

namespace LensFileUtils
{
	UTextureRenderTarget2D* CreateDisplacementMapRenderTarget(UObject* Outer, const FIntPoint DisplacementMapResolution)
	{
		check(Outer);

		UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(Outer, MakeUniqueObjectName(Outer, UTextureRenderTarget2D::StaticClass(), TEXT("LensDisplacementMap")), RF_Public);
		NewRenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RG16f;
		NewRenderTarget2D->ClearColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);
		NewRenderTarget2D->AddressX = TA_Clamp;
		NewRenderTarget2D->AddressY = TA_Clamp;
		NewRenderTarget2D->bAutoGenerateMips = false;
		NewRenderTarget2D->bCanCreateUAV = true;
		NewRenderTarget2D->InitAutoFormat(DisplacementMapResolution.X, DisplacementMapResolution.Y);
		NewRenderTarget2D->UpdateResourceImmediate(true);

		//Flush RHI thread after creating texture render target to make sure that RHIUpdateTextureReference is executed before doing any rendering with it
		//This makes sure that Value->TextureReference.TextureReferenceRHI->GetReferencedTexture() is valid so that FUniformExpressionSet::FillUniformBuffer properly uses the texture for rendering, instead of using a fallback texture
		ENQUEUE_RENDER_COMMAND(FlushRHIThreadToUpdateTextureRenderTargetReference)(
			[](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			});

		return NewRenderTarget2D;
	}

	float EvalAtTwoPoints(const float EvalTime, const float Time0, const float Time1, const float Value0, const float Value1, const float Tangent0, const float Tangent1)
	{
		if (FMath::IsNearlyEqual(Time0, Time1))
		{
			return Value0;
		}

		constexpr float OneThird = 1.0f / 3.0f;

		const float CurveDiff = Time1 - Time0;
		const float CurveAlpha = (EvalTime - Time0) / CurveDiff;

		const float DeltaInput = Value1 - Value0;
		const float CurveDelta = DeltaInput / CurveDiff;
		const float CurveTan0 = Tangent0 * CurveDelta;
		const float CurveTan1 = Tangent1 * CurveDelta;

		const float P0 = Value0;
		const float P3 = Value1;
		const float P1 = P0 + (CurveTan0 * CurveDiff * OneThird);
		const float P2 = P3 - (CurveTan1 * CurveDiff * OneThird);
		return UE::Curves::BezierInterp(P0, P1, P2, P3, CurveAlpha);
	}

	void FindWeightsAndInterp(float InEvalTime, TConstArrayView<float> InTimes, TConstArrayView<float> InTangents, TOptional<float> LerpFactor, TConstArrayView<float> Inputs, float& Output)
	{
		const int32 CurveCount = InTimes.Num();
		check(CurveCount == 2 || CurveCount == 4);

		const int32 ResultCount = InTimes.Num() / 2;

		TArray<float, TInlineAllocator<4>> BezierResults;
		BezierResults.SetNumZeroed(ResultCount);

		for (int32 CurveIndex = 0; CurveIndex < InTimes.Num(); CurveIndex += 2)
		{
			BezierResults[CurveIndex / 2] = EvalAtTwoPoints(InEvalTime
				, InTimes[CurveIndex + 0]
				, InTimes[CurveIndex + 1]
				, Inputs[CurveIndex + 0]
				, Inputs[CurveIndex + 1]
				, InTangents[CurveIndex + 0]
				, InTangents[CurveIndex + 1]);
		}

		if (LerpFactor.IsSet())
		{
			check(BezierResults.Num() == 2);

			const float BlendFactor = LerpFactor.GetValue();
			Output = FMath::Lerp(BezierResults[0], BezierResults[1], BlendFactor);
		}
		else
		{
			check(BezierResults.Num() == 1);
			Output = BezierResults[0];
		}
	}

	void FindWeightsAndInterp(float InEvalTime, TConstArrayView<float> InTimes, TConstArrayView<float> InTangents, TOptional<float> LerpFactor, TConstArrayView<TConstArrayView<float>> Inputs, TArray<float>& Output)
	{
		const int32 CurveCount = InTimes.Num();
		check(CurveCount == 2 || CurveCount == 4);
		
		constexpr float OneThird = 1.0f / 3.0f;
		const int32 ResultCount = InTimes.Num() / 2;
		const int32 InputCount = Inputs[0].Num();
		
		TArray<TArray<float, TInlineAllocator<10>>, TInlineAllocator<4>> BezierResults;
		BezierResults.SetNumZeroed(ResultCount);
		for(TArray<float, TInlineAllocator<10>>& Result: BezierResults)
		{
			Result.SetNumZeroed(InputCount);
		}
        
		for(int32 CurveIndex = 0; CurveIndex < InTimes.Num(); CurveIndex += 2)
		{
			TArray<float, TInlineAllocator<10>>& ResultContainer = BezierResults[CurveIndex/2];

			TConstArrayView<float> Inputs0 = Inputs[CurveIndex+0];
			TConstArrayView<float> Inputs1 = Inputs[CurveIndex+1];

			for(int32 InputIndex = 0; InputIndex < Inputs0.Num(); ++InputIndex)
			{
				ResultContainer[InputIndex] = EvalAtTwoPoints(InEvalTime
					, InTimes[CurveIndex + 0]
					, InTimes[CurveIndex + 1]
					, Inputs0[InputIndex]
					, Inputs1[InputIndex]
					, InTangents[CurveIndex + 0]
					, InTangents[CurveIndex + 1]);
			}
		}

		if(LerpFactor.IsSet())
		{
			check(BezierResults.Num() == 2);

			const float BlendFactor = LerpFactor.GetValue();
			Output.SetNumUninitialized(InputCount);
			for(int32 InputIndex = 0; InputIndex < BezierResults[0].Num(); ++InputIndex)
			{
				Output[InputIndex] = FMath::Lerp(BezierResults[0][InputIndex], BezierResults[1][InputIndex], BlendFactor);
			}
		}
		else
		{
			check(BezierResults.Num() == 1);
			Output = MoveTemp(BezierResults[0]);
		}
	}
}

const TArray<FVector2D> ULensFile::UndistortedUVs =
{
	FVector2D(0.0f, 0.0f),
	FVector2D(0.5f, 0.0f),
	FVector2D(1.0f, 0.0f),
	FVector2D(1.0f, 0.5f),
	FVector2D(1.0f, 1.0f),
	FVector2D(0.5f, 1.0f),
	FVector2D(0.0f, 1.0f),
	FVector2D(0.0f, 0.5f)
};

ULensFile::ULensFile()
{
	LensInfo.LensModel = USphericalLensModel::StaticClass();
	
	if (!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		CalibratedMapProcessor = MakeUnique<FCalibratedMapProcessor>();
#if WITH_EDITOR
		UCameraCalibrationSettings* DefaultSettings = GetMutableDefault<UCameraCalibrationSettings>();
		DefaultSettings->OnDisplacementMapResolutionChanged().AddUObject(this, &ULensFile::UpdateDisplacementMapResolution);
		DefaultSettings->OnCalibrationInputToleranceChanged().AddUObject(this, &ULensFile::UpdateInputTolerance);

		UpdateInputTolerance(DefaultSettings->GetCalibrationInputTolerance());
#endif // WITH_EDITOR
	}
}

#if WITH_EDITOR

void ULensFile::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		FName ActiveMemberName;
		FProperty* ActiveMemberProperty = nullptr;
		if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode() && PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue())
		{
			ActiveMemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
			ActiveMemberName = ActiveMemberProperty->GetFName();
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FSTMapInfo, DistortionMap))
		{
			//When the distortion map (stmap) changes, flag associated derived data as dirty to update it
			if (ActiveMemberProperty)
			{
				//@todo Find out which map was changed and set it dirty
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FLensInfo, LensModel))
		{
			//LensModel has changed, clear distortion and focal length tables
			LensDataTableUtils::EmptyTable(DistortionTable);
			LensDataTableUtils::EmptyTable(FocalLengthTable);
		}
		else if (ActiveMemberName == GET_MEMBER_NAME_CHECKED(ULensFile, LensInfo))
		{
			//Make sure sensor dimensions have valid values
			LensInfo.SensorDimensions.X = FMath::Max(LensInfo.SensorDimensions.X, 1.0f);
			LensInfo.SensorDimensions.Y = FMath::Max(LensInfo.SensorDimensions.Y, 1.0f);
		}
	}
	
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

#endif //WITH_EDITOR

bool ULensFile::EvaluateDistortionParameters(float InFocus, float InZoom, FDistortionInfo& OutEvaluatedValue) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateDistortionParameters);

	if (DistortionTable.GetFocusPoints().Num() <= 0)
	{
		return false;
	}

	const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, DistortionTable.GetFocusPoints());
	const FDistortionFocusPoint& PreviousPoint = DistortionTable.GetFocusPoints()[Neighbors.PreviousIndex];
	const FDistortionFocusPoint& NextPoint = DistortionTable.GetFocusPoints()[Neighbors.NextIndex];
	if (Neighbors.NextIndex == Neighbors.PreviousIndex)
	{
		const LensDataTableUtils::FPointNeighbors ZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
		const FDistortionZoomPoint& PreviousZoomPoint = PreviousPoint.ZoomPoints[ZoomNeighbors.PreviousIndex];
		const FDistortionZoomPoint& NextZoomPoint = PreviousPoint.ZoomPoints[ZoomNeighbors.NextIndex];
		if (ZoomNeighbors.NextIndex == ZoomNeighbors.PreviousIndex)
		{
			//Exactly on one point
			OutEvaluatedValue = PreviousZoomPoint.DistortionInfo;
		}
		else
		{
			//Blend parameters following map blending curve 
			const FRichCurveKey Curve0Key0 = PreviousPoint.MapBlendingCurve.Keys[ZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve0Key1 = PreviousPoint.MapBlendingCurve.Keys[ZoomNeighbors.NextIndex];
			const float Times[2] = { Curve0Key0.Time, Curve0Key1.Time };
			const float Tangents[2] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent };
			const TConstArrayView<float> Parameters[2] = { PreviousZoomPoint.DistortionInfo.Parameters, NextZoomPoint.DistortionInfo.Parameters };
			LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, TOptional<float>(), Parameters, OutEvaluatedValue.Parameters);
		}
	}
	else
	{
		//Previous Focus two zoom points
		const LensDataTableUtils::FPointNeighbors PreviousFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
		const FDistortionZoomPoint& PreviousFocusPreviousZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.PreviousIndex];
		const FDistortionZoomPoint& PreviousFocusNextZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.NextIndex];

		//Next focus two zoom points
		const LensDataTableUtils::FPointNeighbors NextFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, NextPoint.ZoomPoints);
		const FDistortionZoomPoint NextFocusPreviousZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.PreviousIndex];
		const FDistortionZoomPoint& NextFocusNextZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.NextIndex];

		//Verify if we are dealing with one zoom point on each focus. If that's the case, we are doing simple lerp across the focus curves
		if (PreviousFocusZoomNeighbors.NextIndex == PreviousFocusZoomNeighbors.PreviousIndex
			&& NextFocusZoomNeighbors.NextIndex == NextFocusZoomNeighbors.PreviousIndex)
		{
			//Linear blend between each zoom point pair and then both results linearly according to focus
			const float FocusBlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, PreviousPoint.Focus, NextPoint.Focus);
			LensInterpolationUtils::Interpolate(FocusBlendFactor, &PreviousFocusPreviousZoomPoint.DistortionInfo, &NextFocusPreviousZoomPoint.DistortionInfo, &OutEvaluatedValue);
		}
		else
		{
			//Blend parameters following both zoom curves and lerp result using focus blend factor
			const float FocusBlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, PreviousPoint.Focus, NextPoint.Focus);
			const FRichCurveKey Curve0Key0 = PreviousPoint.MapBlendingCurve.Keys[PreviousFocusZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve0Key1 = PreviousPoint.MapBlendingCurve.Keys[PreviousFocusZoomNeighbors.NextIndex];
			const FRichCurveKey Curve1Key0 = NextPoint.MapBlendingCurve.Keys[NextFocusZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve1Key1 = NextPoint.MapBlendingCurve.Keys[NextFocusZoomNeighbors.NextIndex];

			const float Times[4] = { Curve0Key0.Time, Curve0Key1.Time, Curve1Key0.Time, Curve1Key1.Time };
			const float Tangents[4] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent, Curve1Key0.LeaveTangent, Curve1Key1.ArriveTangent };
			const TConstArrayView<float> Parameters[4] = { PreviousFocusPreviousZoomPoint.DistortionInfo.Parameters
														 , PreviousFocusNextZoomPoint.DistortionInfo.Parameters
														 , NextFocusPreviousZoomPoint.DistortionInfo.Parameters
														 , NextFocusNextZoomPoint.DistortionInfo.Parameters };
			LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, FocusBlendFactor, Parameters, OutEvaluatedValue.Parameters);
		}
	}

	return true;
}

bool ULensFile::EvaluateFocalLength(float InFocus, float InZoom, FFocalLengthInfo& OutEvaluatedValue) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateFocalLength);

	if (FocalLengthTable.FocusPoints.Num() <= 0)
	{
		return false;
	}

	if (FocalLengthTable.FocusPoints.Num() == 1)
	{
		OutEvaluatedValue.FxFy.X = FocalLengthTable.FocusPoints[0].Fx.Eval(InZoom);
		OutEvaluatedValue.FxFy.Y = FocalLengthTable.FocusPoints[0].Fy.Eval(InZoom);
		return true;
	}

	const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, FocalLengthTable.GetFocusPoints());
	if (Neighbors.NextIndex == Neighbors.PreviousIndex)
	{
		OutEvaluatedValue.FxFy.X = FocalLengthTable.FocusPoints[Neighbors.PreviousIndex].Fx.Eval(InZoom);
		OutEvaluatedValue.FxFy.Y = FocalLengthTable.FocusPoints[Neighbors.PreviousIndex].Fy.Eval(InZoom);
	}
	else
	{
		FVector2D PreviousValue;
		PreviousValue.X = FocalLengthTable.FocusPoints[Neighbors.PreviousIndex].Fx.Eval(InZoom);
		PreviousValue.Y = FocalLengthTable.FocusPoints[Neighbors.PreviousIndex].Fy.Eval(InZoom);
		FVector2D NextValue;
		NextValue.X = FocalLengthTable.FocusPoints[Neighbors.NextIndex].Fx.Eval(InZoom);
		NextValue.Y = FocalLengthTable.FocusPoints[Neighbors.NextIndex].Fy.Eval(InZoom);

		//Blend result between focus
		const float BlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, FocalLengthTable.FocusPoints[Neighbors.PreviousIndex].Focus, FocalLengthTable.FocusPoints[Neighbors.NextIndex].Focus);
		OutEvaluatedValue.FxFy = LensInterpolationUtils::BlendValue(BlendFactor, PreviousValue, NextValue);
	}

	return true;
}

bool ULensFile::EvaluateImageCenterParameters(float InFocus, float InZoom, FImageCenterInfo& OutEvaluatedValue) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateImageCenterParameters);

	if (ImageCenterTable.FocusPoints.Num() <= 0)
	{
		return false;
	}

	if (ImageCenterTable.FocusPoints.Num() == 1)
	{
		OutEvaluatedValue.PrincipalPoint.X = ImageCenterTable.FocusPoints[0].Cx.Eval(InZoom);
		OutEvaluatedValue.PrincipalPoint.Y = ImageCenterTable.FocusPoints[0].Cy.Eval(InZoom);
		return true;
	}

	const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, ImageCenterTable.GetFocusPoints());
	if (Neighbors.NextIndex == Neighbors.PreviousIndex)
	{
		OutEvaluatedValue.PrincipalPoint.X = ImageCenterTable.FocusPoints[Neighbors.PreviousIndex].Cx.Eval(InZoom);
		OutEvaluatedValue.PrincipalPoint.Y = ImageCenterTable.FocusPoints[Neighbors.PreviousIndex].Cy.Eval(InZoom);
	}
	else
	{
		FVector2D PreviousValue;
		PreviousValue.X = ImageCenterTable.FocusPoints[Neighbors.PreviousIndex].Cx.Eval(InZoom);
		PreviousValue.Y = ImageCenterTable.FocusPoints[Neighbors.PreviousIndex].Cy.Eval(InZoom);
		FVector2D NextValue;
		NextValue.X = ImageCenterTable.FocusPoints[Neighbors.NextIndex].Cx.Eval(InZoom);
		NextValue.Y = ImageCenterTable.FocusPoints[Neighbors.NextIndex].Cy.Eval(InZoom);

		//Blend result between focus
		const float BlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, ImageCenterTable.FocusPoints[Neighbors.PreviousIndex].Focus, ImageCenterTable.FocusPoints[Neighbors.NextIndex].Focus);
		OutEvaluatedValue.PrincipalPoint = LensInterpolationUtils::BlendValue(BlendFactor, PreviousValue, NextValue);
	}

	return true;
}

bool ULensFile::EvaluateDistortionData(float InFocus, float InZoom, FVector2D InFilmback, ULensDistortionModelHandlerBase* InLensHandler) const
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateDistortionData);

	if (InLensHandler == nullptr)
	{
		UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Can't evaluate LensFile '%s' - Invalid Lens Handler"), *GetName());
		return false;
	}
	
	if (InLensHandler->GetUndistortionDisplacementMap() == nullptr)
	{
		UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Can't evaluate LensFile '%s' - Invalid undistortion displacement map in LensHandler '%s'"), *GetName(), *InLensHandler->GetName());
		return false;
	}

	if (InLensHandler->GetDistortionDisplacementMap() == nullptr)
	{
		UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Can't evaluate LensFile '%s' - Invalid distortion displacement map in LensHandler '%s'"), *GetName(), *InLensHandler->GetName());
		return false;
	}

	if (LensInfo.LensModel == nullptr)
	{
		UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Can't evaluate LensFile '%s' - Invalid Lens Model"), *GetName());
		SetupNoDistortionOutput(InLensHandler);
		return false;
	}

	if (InLensHandler->IsModelSupported(LensInfo.LensModel) == false)
	{
		UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Can't evaluate LensFile '%s' - LensHandler '%s' doesn't support lens model '%s'"), *GetName(), *InLensHandler->GetName(), *LensInfo.LensModel.GetDefaultObject()->GetModelName().ToString());
		SetupNoDistortionOutput(InLensHandler);
		return false;
	}
	
	if(DataMode == ELensDataMode::Parameters)
	{
		return EvaluateDistortionForParameters(InFocus, InZoom, InFilmback, InLensHandler);
	}
	else
	{
		//Only other mode for now
		check(DataMode == ELensDataMode::STMap);

		return EvaluateDistortionForSTMaps(InFocus, InZoom, InFilmback, InLensHandler);
	}
}

float ULensFile::ComputeOverscan(const FDistortionData& DerivedData, FVector2D PrincipalPoint) const
{
	//Edge case if computed data hasn't came back yet
	if (UndistortedUVs.Num() != DerivedData.DistortedUVs.Num())
	{
		return 1.0f;
	}

	TArray<float, TInlineAllocator<8>> OverscanFactors;
	OverscanFactors.Reserve(UndistortedUVs.Num());
	for (int32 Index = 0; Index < UndistortedUVs.Num(); ++Index)
	{
		const FVector2D& UndistortedUV = UndistortedUVs[Index];
		const FVector2D& DistortedUV = DerivedData.DistortedUVs[Index] + (PrincipalPoint - FVector2D(0.5f, 0.5f)) * 2.0f;
		const float OverscanX = (UndistortedUV.X != 0.5f) ? (DistortedUV.X - 0.5f) / (UndistortedUV.X - 0.5f) : 1.0f;
		const float OverscanY = (UndistortedUV.Y != 0.5f) ? (DistortedUV.Y - 0.5f) / (UndistortedUV.Y - 0.5f) : 1.0f;
		OverscanFactors.Add(FMath::Max(OverscanX, OverscanY));
	}

	float* MaxOverscanFactor = Algo::MaxElement(OverscanFactors);
	const float FoundOverscan = MaxOverscanFactor ? *MaxOverscanFactor : 1.0f;
	
	return FoundOverscan;
}

void ULensFile::SetupNoDistortionOutput(ULensDistortionModelHandlerBase* LensHandler) const
{
	LensFileRendering::ClearDisplacementMap(LensHandler->GetUndistortionDisplacementMap());
	LensFileRendering::ClearDisplacementMap(LensHandler->GetDistortionDisplacementMap());
	LensHandler->SetOverscanFactor(1.0f);
}

bool ULensFile::EvaluateDistortionForParameters(float InFocus, float InZoom, FVector2D InFilmback, ULensDistortionModelHandlerBase* InLensHandler) const
{
	//ImageCenter is always required
	FImageCenterInfo ImageCenter;
	EvaluateImageCenterParameters(InFocus, InZoom, ImageCenter);

	//No distortion parameters case. Still process to have center shift
	if (DistortionTable.GetFocusPoints().Num() <= 0)
	{
		//Initialize all distortion parameters with 0
		FDistortionInfo DistortionPoint;
		DistortionPoint.Parameters.SetNumZeroed(LensInfo.LensModel.GetDefaultObject()->GetNumParameters());
		
		//Setup handler state based on evaluated parameters. If none were found, no distortion will be returned
		FLensDistortionState State;
		State.DistortionInfo.Parameters = MoveTemp(DistortionPoint.Parameters);

		//Evaluate Focal Length
		FFocalLengthInfo FocalLength;
		EvaluateFocalLength(InFocus, InZoom, FocalLength);
		const FVector2D FxFyScale = FVector2D(LensInfo.SensorDimensions.X / InFilmback.X, LensInfo.SensorDimensions.Y / InFilmback.Y);
		State.FocalLengthInfo.FxFy = FocalLength.FxFy * FxFyScale;
		State.ImageCenter.PrincipalPoint = ImageCenter.PrincipalPoint;

		//Updates handler state
		InLensHandler->SetDistortionState(State);

		InLensHandler->SetOverscanFactor(1.0f);

		//Draw displacement map associated with the new state
		InLensHandler->ProcessCurrentDistortion();
	}
	else
	{
		FLensDistortionState State;
		State.ImageCenter.PrincipalPoint = ImageCenter.PrincipalPoint;

		FDisplacementMapBlendingParams Params;
		float InterpolatedOverscanFactor = 1.0f;

		//Helper function to compute the current distortion state
		const auto ProcessDistortionState = [this, &State, InFilmback, InLensHandler](const FDistortionInfo& DistortionInfo, const FFocalLengthInfo& FocalLength, UTextureRenderTarget2D* UndistortionRenderTarget, UTextureRenderTarget2D* DistortionRenderTarget, float& OverscanFactor)
		{
			State.DistortionInfo.Parameters = DistortionInfo.Parameters;

			const FVector2D FxFyScale = FVector2D(LensInfo.SensorDimensions.X / InFilmback.X, LensInfo.SensorDimensions.Y / InFilmback.Y);
			State.FocalLengthInfo.FxFy = FocalLength.FxFy * FxFyScale;

			InLensHandler->SetDistortionState(State);
			InLensHandler->DrawUndistortionDisplacementMap(UndistortionRenderTarget);
			InLensHandler->DrawDistortionDisplacementMap(DistortionRenderTarget);

			OverscanFactor = InLensHandler->ComputeOverscanFactor();
		};

		//Find focuses in play
		const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, DistortionTable.GetFocusPoints());
		const FDistortionFocusPoint& PreviousPoint = DistortionTable.GetFocusPoints()[Neighbors.PreviousIndex];
		const FDistortionFocusPoint& NextPoint = DistortionTable.GetFocusPoints()[Neighbors.NextIndex];
		if (Neighbors.NextIndex == Neighbors.PreviousIndex)
		{
			const LensDataTableUtils::FPointNeighbors ZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
			const FDistortionZoomPoint& PreviousZoomPoint = PreviousPoint.ZoomPoints[ZoomNeighbors.PreviousIndex];
			const FDistortionZoomPoint& NextZoomPoint = NextPoint.ZoomPoints[ZoomNeighbors.NextIndex];

			//Get FocalLength points
			FFocalLengthInfo PreviousZoomPointFocalLength;
			FFocalLengthInfo NextZoomPointFocalLength;
			LensDataTableUtils::GetPointValue(PreviousPoint.Focus, PreviousZoomPoint.Zoom, FocalLengthTable.GetFocusPoints(), PreviousZoomPointFocalLength);
			LensDataTableUtils::GetPointValue(PreviousPoint.Focus, NextZoomPoint.Zoom, FocalLengthTable.GetFocusPoints(), NextZoomPointFocalLength);

			if (ZoomNeighbors.NextIndex == ZoomNeighbors.PreviousIndex)
			{
				//Exactly on one point
				Params.BlendType = EDisplacementMapBlendType::OneFocusOneZoom;

				ProcessDistortionState(PreviousZoomPoint.DistortionInfo, PreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[0], DistortionDisplacementMapHolders[0], InterpolatedOverscanFactor);
			}
			else
			{
				//Interpolate between two zoom points following the map blending curve
				Params.BlendType = EDisplacementMapBlendType::OneFocusTwoZoom;

				float BlendedOverscanFactors[2];
				ProcessDistortionState(PreviousZoomPoint.DistortionInfo, PreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[0], DistortionDisplacementMapHolders[0], BlendedOverscanFactors[0]);
				ProcessDistortionState(NextZoomPoint.DistortionInfo, NextZoomPointFocalLength, UndistortionDisplacementMapHolders[1], DistortionDisplacementMapHolders[1], BlendedOverscanFactors[1]);

				//Set displacement map blending parameters
				const TArray<FRichCurveKey>& PreviousPointsKeys = PreviousPoint.MapBlendingCurve.GetConstRefOfKeys();
				const FRichCurveKey Curve0Key0 = PreviousPointsKeys[ZoomNeighbors.PreviousIndex];
				const FRichCurveKey Curve0Key1 = PreviousPointsKeys[ZoomNeighbors.NextIndex];

				Params.EvalTime = InZoom;
				Params.Curve0Key0Time = Curve0Key0.Time;
				Params.Curve0Key1Time = Curve0Key1.Time;
				Params.Curve0Key0Tangent = Curve0Key0.LeaveTangent;
				Params.Curve0Key1Tangent = Curve0Key1.ArriveTangent;

				//Interpolate distortion parameters along the map blending curve
				const float Times[2] = { Curve0Key0.Time, Curve0Key1.Time };
				const float Tangents[2] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent };
				const TConstArrayView<float> Parameters[2] = { PreviousZoomPoint.DistortionInfo.Parameters, NextZoomPoint.DistortionInfo.Parameters };
				LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, TOptional<float>(), Parameters, State.DistortionInfo.Parameters);

				//Interpolate overscan factor along the map blending curve
				const float OverscanFactors[2] = { BlendedOverscanFactors[0], BlendedOverscanFactors[1] };
				LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, TOptional<float>(), OverscanFactors, InterpolatedOverscanFactor);
			}
		}
		else
		{
			//Previous Focus two zoom points
			const LensDataTableUtils::FPointNeighbors PreviousFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
			const FDistortionZoomPoint& PreviousFocusPreviousZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.PreviousIndex];
			const FDistortionZoomPoint& PreviousFocusNextZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.NextIndex];
			FFocalLengthInfo PreviousFocusPreviousZoomPointFocalLength;
			FFocalLengthInfo PreviousFocusNextZoomPointFocalLength;
			LensDataTableUtils::GetPointValue<FFocalLengthFocusPoint>(PreviousPoint.Focus, PreviousFocusPreviousZoomPoint.Zoom, FocalLengthTable.FocusPoints, PreviousFocusPreviousZoomPointFocalLength);
			LensDataTableUtils::GetPointValue<FFocalLengthFocusPoint>(PreviousPoint.Focus, PreviousFocusNextZoomPoint.Zoom, FocalLengthTable.FocusPoints, PreviousFocusNextZoomPointFocalLength);

			//Next focus two zoom points
			const LensDataTableUtils::FPointNeighbors NextFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, NextPoint.ZoomPoints);
			const FDistortionZoomPoint& NextFocusPreviousZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.PreviousIndex];
			const FDistortionZoomPoint& NextFocusNextZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.NextIndex];
			FFocalLengthInfo NextFocusPreviousZoomPointFocalLength;
			FFocalLengthInfo NextFocusNextZoomPointFocalLength;
			LensDataTableUtils::GetPointValue<FFocalLengthFocusPoint>(NextPoint.Focus, NextFocusPreviousZoomPoint.Zoom, FocalLengthTable.FocusPoints, NextFocusPreviousZoomPointFocalLength);
			LensDataTableUtils::GetPointValue<FFocalLengthFocusPoint>(NextPoint.Focus, NextFocusNextZoomPoint.Zoom, FocalLengthTable.FocusPoints, NextFocusNextZoomPointFocalLength);

			const float FocusBlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, PreviousPoint.Focus, NextPoint.Focus);
			Params.FocusBlendFactor = FocusBlendFactor;

			//Verify if we are dealing with one zoom point on each focus. If that's the case, we are doing simple lerp across the focus curves
			if(PreviousFocusZoomNeighbors.NextIndex == PreviousFocusZoomNeighbors.PreviousIndex
				&& NextFocusZoomNeighbors.NextIndex == NextFocusZoomNeighbors.PreviousIndex)
			{
				//Linearly interpolate between two focus curves
				Params.BlendType = EDisplacementMapBlendType::TwoFocusOneZoom;

				float BlendedOverscanFactors[2];
				ProcessDistortionState(PreviousFocusPreviousZoomPoint.DistortionInfo, PreviousFocusPreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[0], DistortionDisplacementMapHolders[0], BlendedOverscanFactors[0]);
				ProcessDistortionState(NextFocusPreviousZoomPoint.DistortionInfo, NextFocusPreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[1], DistortionDisplacementMapHolders[1], BlendedOverscanFactors[1]);

				//Linearly interpolate the distortion parameters
				LensInterpolationUtils::Interpolate(FocusBlendFactor, &PreviousFocusPreviousZoomPoint.DistortionInfo, &NextFocusPreviousZoomPoint.DistortionInfo, &State.DistortionInfo);

				//Linearly interpolate the overscan factor
				InterpolatedOverscanFactor = FMath::Lerp(BlendedOverscanFactors[0], BlendedOverscanFactors[1], FocusBlendFactor);
			}
			else
			{
				//Interpolate between two zoom points on each curve using the correct map blending curve, then linearly interpolate between the two focus points
				Params.BlendType = EDisplacementMapBlendType::TwoFocusTwoZoom;

				float BlendedOverscanFactors[4];
				ProcessDistortionState(PreviousFocusPreviousZoomPoint.DistortionInfo, PreviousFocusPreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[0], DistortionDisplacementMapHolders[0], BlendedOverscanFactors[0]);
				ProcessDistortionState(PreviousFocusNextZoomPoint.DistortionInfo, PreviousFocusNextZoomPointFocalLength, UndistortionDisplacementMapHolders[1], DistortionDisplacementMapHolders[1], BlendedOverscanFactors[1]);
				ProcessDistortionState(NextFocusPreviousZoomPoint.DistortionInfo, NextFocusPreviousZoomPointFocalLength, UndistortionDisplacementMapHolders[2], DistortionDisplacementMapHolders[2], BlendedOverscanFactors[2]);
				ProcessDistortionState(NextFocusNextZoomPoint.DistortionInfo, NextFocusNextZoomPointFocalLength, UndistortionDisplacementMapHolders[3], DistortionDisplacementMapHolders[3], BlendedOverscanFactors[3]);

				//Set displacement map blending parameters
				const TArray<FRichCurveKey>& Curve0Keys = PreviousPoint.MapBlendingCurve.GetConstRefOfKeys();
				const FRichCurveKey Curve0Key0 = Curve0Keys[PreviousFocusZoomNeighbors.PreviousIndex];
				const FRichCurveKey Curve0Key1 = Curve0Keys[PreviousFocusZoomNeighbors.NextIndex];

				const TArray<FRichCurveKey>& Curve1Keys = NextPoint.MapBlendingCurve.GetConstRefOfKeys();
				const FRichCurveKey Curve1Key0 = Curve1Keys[NextFocusZoomNeighbors.PreviousIndex];
				const FRichCurveKey Curve1Key1 = Curve1Keys[NextFocusZoomNeighbors.NextIndex];

				Params.EvalTime = InZoom;
				Params.Curve0Key0Time = Curve0Key0.Time;
				Params.Curve0Key1Time = Curve0Key1.Time;
				Params.Curve0Key0Tangent = Curve0Key0.LeaveTangent;
				Params.Curve0Key1Tangent = Curve0Key1.ArriveTangent;
				Params.Curve1Key0Time = Curve1Key0.Time;
				Params.Curve1Key1Time = Curve1Key1.Time;
				Params.Curve1Key0Tangent = Curve1Key0.LeaveTangent;
				Params.Curve1Key1Tangent = Curve1Key1.ArriveTangent;

				//Interpolate distortion parameters along the map blending curves, then linearly interpolate between focus points
				const float Times[4] = { Curve0Key0.Time, Curve0Key1.Time, Curve1Key0.Time, Curve1Key1.Time };
				const float Tangents[4] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent, Curve1Key0.LeaveTangent, Curve1Key1.ArriveTangent };
				const TConstArrayView<float> Parameters[4] = { PreviousFocusPreviousZoomPoint.DistortionInfo.Parameters
															 , PreviousFocusNextZoomPoint.DistortionInfo.Parameters
															 , NextFocusPreviousZoomPoint.DistortionInfo.Parameters
															 , NextFocusNextZoomPoint.DistortionInfo.Parameters};
				LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, FocusBlendFactor, Parameters, State.DistortionInfo.Parameters);

				//Interpolate overscan factor along the map blending curves, then linearly interpolate between focus points
				const float OverscanFactors[4] = { BlendedOverscanFactors[0], BlendedOverscanFactors[1], BlendedOverscanFactors[2], BlendedOverscanFactors[3] };
				LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, FocusBlendFactor, OverscanFactors, InterpolatedOverscanFactor);
			}
		}

		//Resulting displacement maps is the result of blending each individual maps together
		//Distortion state can also provide distortion parameters and FxFy that were used.
		//Instead of providing nothing (that accurately matches the blended map)
		//We fill it with blended parameters and FxFy

		FFocalLengthInfo FocalLength;
		EvaluateFocalLength(InFocus, InZoom, FocalLength);
		const FVector2D FxFyScale = FVector2D(LensInfo.SensorDimensions.X / InFilmback.X, LensInfo.SensorDimensions.Y / InFilmback.Y);
		State.FocalLengthInfo.FxFy = FocalLength.FxFy * FxFyScale;
		
		//Sets final blended distortion state
		InLensHandler->SetDistortionState(State);

		//Draw resulting undistortion displacement map for evaluation point
		LensFileRendering::DrawBlendedDisplacementMap(InLensHandler->GetUndistortionDisplacementMap()
			, Params
			, UndistortionDisplacementMapHolders[0]
			, UndistortionDisplacementMapHolders[1]
			, UndistortionDisplacementMapHolders[2]
			, UndistortionDisplacementMapHolders[3]);

		//Draw resulting distortion displacement map for evaluation point
		LensFileRendering::DrawBlendedDisplacementMap(InLensHandler->GetDistortionDisplacementMap()
			, Params
			, DistortionDisplacementMapHolders[0]
			, DistortionDisplacementMapHolders[1]
			, DistortionDisplacementMapHolders[2]
			, DistortionDisplacementMapHolders[3]);

		InLensHandler->SetOverscanFactor(InterpolatedOverscanFactor);
	}
	
	return true;
}

bool ULensFile::EvaluateDistortionForSTMaps(float InFocus, float InZoom, FVector2D InFilmback, ULensDistortionModelHandlerBase* InLensHandler) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateDistortionForSTMaps);

	if(DerivedDataInFlightCount > 0)
	{
		UE_LOG(LogCameraCalibrationCore, Verbose, TEXT("Can't evaluate LensFile '%s' - %d data points still being computed. Clearing render target for no distortion"), *GetName(), DerivedDataInFlightCount);
		SetupNoDistortionOutput(InLensHandler);
		return true;
	}

	if(STMapTable.FocusPoints.Num() <= 0)
	{
		UE_LOG(LogCameraCalibrationCore, Verbose, TEXT("Can't evaluate LensFile '%s' - No calibrated maps"), *GetName());
		SetupNoDistortionOutput(InLensHandler);
		return true;
	}

	if (((LensInfo.SensorDimensions.X + UE_DOUBLE_KINDA_SMALL_NUMBER) < InFilmback.X) || ((LensInfo.SensorDimensions.Y + UE_DOUBLE_KINDA_SMALL_NUMBER) < InFilmback.Y))
	{
		UE_LOG(LogCameraCalibrationCore, Verbose
			, TEXT("Can't evaluate LensFile '%s' - The filmback used to generate the calibrated ST Maps is smaller than" \
				"the filmback of the camera that distortion is being applied to. There is not enough distortion information available in the ST Maps.")
			, *GetName());
		SetupNoDistortionOutput(InLensHandler);
		return false;
	}

	FDisplacementMapBlendingParams Params;

	TArray<UTextureRenderTarget2D*, TInlineAllocator<4>> UndistortionMapSource;
	UndistortionMapSource.AddZeroed(4);
	TArray<UTextureRenderTarget2D*, TInlineAllocator<4>> DistortionMapSource;
	DistortionMapSource.AddZeroed(4);

	//When dealing with STMaps, FxFy was not a calibrated value. We can interpolate our curve directly for desired point
	FLensDistortionState State;

	const FVector2D FxFyScale = FVector2D(InFilmback.X / LensInfo.SensorDimensions.X, InFilmback.Y / LensInfo.SensorDimensions.Y);
	Params.FxFyScale = FxFyScale;

	FFocalLengthInfo FocalLength;
	EvaluateFocalLength(InFocus, InZoom, FocalLength);
	State.FocalLengthInfo.FxFy = FocalLength.FxFy * FxFyScale;

	FImageCenterInfo ImageCenter;
	EvaluateImageCenterParameters(InFocus, InZoom, ImageCenter);
	State.ImageCenter = ImageCenter;
	Params.PrincipalPoint = ImageCenter.PrincipalPoint;

	float InterpolatedOverscanFactor = 0.0f;

	//Find focuses in play
	const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, STMapTable.GetFocusPoints());
	const FSTMapFocusPoint& PreviousPoint = STMapTable.FocusPoints[Neighbors.PreviousIndex];
	const FSTMapFocusPoint& NextPoint = STMapTable.FocusPoints[Neighbors.NextIndex];
	if (Neighbors.NextIndex == Neighbors.PreviousIndex)
	{
		const LensDataTableUtils::FPointNeighbors ZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
		const FSTMapZoomPoint& PreviousZoomPoint = PreviousPoint.ZoomPoints[ZoomNeighbors.PreviousIndex];
		const FSTMapZoomPoint& NextZoomPoint = PreviousPoint.ZoomPoints[ZoomNeighbors.NextIndex];

		if (ZoomNeighbors.NextIndex == ZoomNeighbors.PreviousIndex)
		{
			//Exactly on one point
			Params.BlendType = EDisplacementMapBlendType::OneFocusOneZoom;

			UndistortionMapSource[0] = PreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[0] = PreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
 			InterpolatedOverscanFactor = ComputeOverscan(PreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
		}
		else
		{
			//Interpolate between two zoom points following the map blending curve
			Params.BlendType = EDisplacementMapBlendType::OneFocusTwoZoom;

			UndistortionMapSource[0] = PreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[0] = PreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			UndistortionMapSource[1] = NextZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[1] = NextZoomPoint.DerivedDistortionData.DistortionDisplacementMap;

			//Set displacement map blending parameters
			const TArray<FRichCurveKey>& PreviousPointsKeys = PreviousPoint.MapBlendingCurve.GetConstRefOfKeys();
			const FRichCurveKey Curve0Key0 = PreviousPointsKeys[ZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve0Key1 = PreviousPointsKeys[ZoomNeighbors.NextIndex];

			Params.EvalTime = InZoom;
			Params.Curve0Key0Time = Curve0Key0.Time;
			Params.Curve0Key1Time = Curve0Key1.Time;
			Params.Curve0Key0Tangent = Curve0Key0.LeaveTangent;
			Params.Curve0Key1Tangent = Curve0Key1.ArriveTangent;

			//Interpolate overscan factor along the map blending curve
			const float PreviousZoomPointOverscanFactor = ComputeOverscan(PreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			const float NextZoomPointOverscanFactor = ComputeOverscan(NextZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);

			const float Times[2] = { Curve0Key0.Time, Curve0Key1.Time };
			const float Tangents[2] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent };
			const float OverscanFactors[2] = { PreviousZoomPointOverscanFactor, NextZoomPointOverscanFactor };
			LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, TOptional<float>(), OverscanFactors, InterpolatedOverscanFactor);
		}
	}
	else
	{
		//Previous Focus two zoom points
		const LensDataTableUtils::FPointNeighbors PreviousFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, PreviousPoint.ZoomPoints);
		const FSTMapZoomPoint& PreviousFocusPreviousZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.PreviousIndex];
		const FSTMapZoomPoint& PreviousFocusNextZoomPoint = PreviousPoint.ZoomPoints[PreviousFocusZoomNeighbors.NextIndex];

		//Next focus two zoom points
		const LensDataTableUtils::FPointNeighbors NextFocusZoomNeighbors = LensDataTableUtils::FindZoomPoints(InZoom, NextPoint.ZoomPoints);
		const FSTMapZoomPoint NextFocusPreviousZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.PreviousIndex];
		const FSTMapZoomPoint& NextFocusNextZoomPoint = NextPoint.ZoomPoints[NextFocusZoomNeighbors.NextIndex];
		
		const float FocusBlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, PreviousPoint.Focus, NextPoint.Focus);
		Params.FocusBlendFactor = FocusBlendFactor;

		//Verify if we are dealing with one zoom point on each focus. If that's the case, we are doing simple lerp across the focus curves
		if(PreviousFocusZoomNeighbors.NextIndex == PreviousFocusZoomNeighbors.PreviousIndex
			&& NextFocusZoomNeighbors.NextIndex == NextFocusZoomNeighbors.PreviousIndex)
		{
			Params.BlendType = EDisplacementMapBlendType::TwoFocusOneZoom;

			UndistortionMapSource[0] = PreviousFocusPreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[0] = PreviousFocusPreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			UndistortionMapSource[1] = NextFocusPreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[1] = NextFocusPreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;

			//Linearly interpolate the overscan factor
			const float PreviousPointOverscanFactor = ComputeOverscan(PreviousFocusPreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			const float NextPointOverscanFactor = ComputeOverscan(NextFocusPreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			InterpolatedOverscanFactor = FMath::Lerp(PreviousPointOverscanFactor, NextPointOverscanFactor, FocusBlendFactor);
		}
		else
		{
			//Interpolate between two zoom points on each curve using the correct map blending curve, then linearly interpolate between the two focus points
			Params.BlendType = EDisplacementMapBlendType::TwoFocusTwoZoom;

			UndistortionMapSource[0] = PreviousFocusPreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[0] = PreviousFocusPreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			UndistortionMapSource[1] = PreviousFocusNextZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[1] = PreviousFocusNextZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			UndistortionMapSource[2] = NextFocusPreviousZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[2] = NextFocusPreviousZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			UndistortionMapSource[3] = NextFocusNextZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
			DistortionMapSource[3] = NextFocusNextZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
			
			//Set displacement map blending parameters
			const TArray<FRichCurveKey>& Curve0Keys = PreviousPoint.MapBlendingCurve.GetConstRefOfKeys();
			const FRichCurveKey Curve0Key0 = Curve0Keys[PreviousFocusZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve0Key1 = Curve0Keys[PreviousFocusZoomNeighbors.NextIndex];

			const TArray<FRichCurveKey>& Curve1Keys = NextPoint.MapBlendingCurve.GetConstRefOfKeys();
			const FRichCurveKey Curve1Key0 = Curve1Keys[NextFocusZoomNeighbors.PreviousIndex];
			const FRichCurveKey Curve1Key1 = Curve1Keys[NextFocusZoomNeighbors.NextIndex];

			Params.EvalTime = InZoom;
			Params.Curve0Key0Time = Curve0Key0.Time;
			Params.Curve0Key1Time = Curve0Key1.Time;
			Params.Curve0Key0Tangent = Curve0Key0.LeaveTangent;
			Params.Curve0Key1Tangent = Curve0Key1.ArriveTangent;
			Params.Curve1Key0Time = Curve1Key0.Time;
			Params.Curve1Key1Time = Curve1Key1.Time;
			Params.Curve1Key0Tangent = Curve1Key0.LeaveTangent;
			Params.Curve1Key1Tangent = Curve1Key1.ArriveTangent;

			//Interpolate overscan factor along the map blending curves, then linearly interpolate between focus points
			const float PreviousFocusPreviousZoomOverscanFactor = ComputeOverscan(PreviousFocusPreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			const float PreviousFocusNextZoomOverscanFactor = ComputeOverscan(PreviousFocusNextZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			const float NextFocusPreviousZoomOverscanFactor = ComputeOverscan(NextFocusPreviousZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);
			const float NextFocusNextZoomOverscanFactor = ComputeOverscan(NextFocusNextZoomPoint.DerivedDistortionData.DistortionData, ImageCenter.PrincipalPoint);

			const float Times[4] = { Curve0Key0.Time, Curve0Key1.Time, Curve1Key0.Time, Curve1Key1.Time };
			const float Tangents[4] = { Curve0Key0.LeaveTangent, Curve0Key1.ArriveTangent, Curve1Key0.LeaveTangent, Curve1Key1.ArriveTangent };
			const float OverscanFactors[4] = { PreviousFocusPreviousZoomOverscanFactor, PreviousFocusNextZoomOverscanFactor, NextFocusPreviousZoomOverscanFactor, NextFocusNextZoomOverscanFactor };
			LensFileUtils::FindWeightsAndInterp(InZoom, Times, Tangents, FocusBlendFactor, OverscanFactors, InterpolatedOverscanFactor);
		}
	}

	//Draw resulting undistortion displacement map for evaluation point
	LensFileRendering::DrawBlendedDisplacementMap(InLensHandler->GetUndistortionDisplacementMap()
		, Params
		, UndistortionMapSource[0]
		, UndistortionMapSource[1]
		, UndistortionMapSource[2]
		, UndistortionMapSource[3]);

	//Draw resulting distortion displacement map for evaluation point
	LensFileRendering::DrawBlendedDisplacementMap(InLensHandler->GetDistortionDisplacementMap()
		, Params
		, DistortionMapSource[0]
		, DistortionMapSource[1]
		, DistortionMapSource[2]
		, DistortionMapSource[3]);

			
	//Sets final blended distortion state
	InLensHandler->SetDistortionState(MoveTemp(State));
	InLensHandler->SetOverscanFactor(InterpolatedOverscanFactor);

	return true;
}

bool ULensFile::EvaluateNodalPointOffset(float InFocus, float InZoom, FNodalPointOffset& OutEvaluatedValue) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULensFile::EvaluateNodalPointOffset);

	if (NodalOffsetTable.GetFocusPoints().Num() <= 0)
	{
		return false;
	}

	const auto EvaluateNodalOffset = [this](int32 FocusIndex,float Zoom, FNodalPointOffset& OutNodalOffset)
	{
		FRotator RotationOffset;
		constexpr int32 RotationDimensions = 3;
		for(int32 AxisIndex = 0; AxisIndex < RotationDimensions; ++AxisIndex)
		{
			const EAxis::Type Axis = static_cast<EAxis::Type>(AxisIndex + 1);
			OutNodalOffset.LocationOffset.SetComponentForAxis(Axis, NodalOffsetTable.FocusPoints[FocusIndex].LocationOffset[AxisIndex].Eval(Zoom));
			RotationOffset.SetComponentForAxis(Axis, NodalOffsetTable.FocusPoints[FocusIndex].RotationOffset[AxisIndex].Eval(Zoom));
		}
		
		OutNodalOffset.RotationOffset = FQuat(RotationOffset);	
	};

	if (NodalOffsetTable.GetFocusPoints().Num() == 1)
	{
		EvaluateNodalOffset(0, InZoom, OutEvaluatedValue);
		return true;
	}
	
	const LensDataTableUtils::FPointNeighbors Neighbors = LensDataTableUtils::FindFocusPoints(InFocus, NodalOffsetTable.GetFocusPoints());
	if (Neighbors.NextIndex == Neighbors.PreviousIndex)
	{
		EvaluateNodalOffset(Neighbors.PreviousIndex, InZoom, OutEvaluatedValue);
	}
	else
	{
		FNodalPointOffset PreviousValue;
		EvaluateNodalOffset(Neighbors.PreviousIndex, InZoom, PreviousValue);
		FNodalPointOffset NextValue;
		EvaluateNodalOffset(Neighbors.NextIndex, InZoom, NextValue);
	
		//Blend result between focus
		const float BlendFactor = LensInterpolationUtils::GetBlendFactor(InFocus, NodalOffsetTable.FocusPoints[Neighbors.PreviousIndex].Focus, NodalOffsetTable.FocusPoints[Neighbors.NextIndex].Focus);
		LensInterpolationUtils::Interpolate(BlendFactor, &PreviousValue, &NextValue, &OutEvaluatedValue);
	}

	return true;
}

bool ULensFile::HasFocusEncoderMapping() const
{
	return EncodersTable.Focus.GetNumKeys() > 0;
}

float ULensFile::EvaluateNormalizedFocus(float InNormalizedValue) const
{
	return EncodersTable.Focus.Eval(InNormalizedValue);
}

bool ULensFile::HasIrisEncoderMapping() const
{
	return EncodersTable.Iris.GetNumKeys() > 0;
}

float ULensFile::EvaluateNormalizedIris(float InNormalizedValue) const
{
	return EncodersTable.Iris.Eval(InNormalizedValue);
}

void ULensFile::OnDistortionDerivedDataJobCompleted(const FDerivedDistortionDataJobOutput& JobOutput)
{
	//Keep track of jobs being processed
	--DerivedDataInFlightCount;

	if(FSTMapFocusPoint* FocusPoint = STMapTable.GetFocusPoint(JobOutput.Focus))
	{
		if(FSTMapZoomPoint* ZoomPoint = FocusPoint->GetZoomPoint(JobOutput.Zoom))
		{
			if (JobOutput.Result == EDerivedDistortionDataResult::Success)
			{
				ZoomPoint->DerivedDistortionData.DistortionData.DistortedUVs = JobOutput.EdgePointsDistortedUVs;	
			}
			else
			{
				UE_LOG(LogCameraCalibrationCore, Warning, TEXT("Could not derive distortion data for calibrated map point with Focus = '%0.2f' and Zoom = '%0.2f' on LensFile '%s'"), FocusPoint->Focus, ZoomPoint->Zoom, *GetName());
			}
		}
	}
}

bool ULensFile::IsCineCameraCompatible(const UCineCameraComponent* CineCameraComponent) const
{
	return true;
}

void ULensFile::UpdateInputTolerance(const float NewTolerance)
{
	InputTolerance = NewTolerance;
}

TArray<FDistortionPointInfo> ULensFile::GetDistortionPoints() const
{
	return LensDataTableUtils::GetAllPointsInfo<FDistortionPointInfo>(DistortionTable);
}

TArray<FFocalLengthPointInfo> ULensFile::GetFocalLengthPoints() const
{
	return LensDataTableUtils::GetAllPointsInfo<FFocalLengthPointInfo>(FocalLengthTable);
}

TArray<FSTMapPointInfo> ULensFile::GetSTMapPoints() const
{
	return LensDataTableUtils::GetAllPointsInfo<FSTMapPointInfo>(STMapTable);
}

TArray<FImageCenterPointInfo> ULensFile::GetImageCenterPoints() const
{
	return LensDataTableUtils::GetAllPointsInfo<FImageCenterPointInfo>(ImageCenterTable);
}

TArray<FNodalOffsetPointInfo> ULensFile::GetNodalOffsetPoints() const
{
	return LensDataTableUtils::GetAllPointsInfo<FNodalOffsetPointInfo>(NodalOffsetTable);
}

bool ULensFile::GetDistortionPoint(const float InFocus, const float InZoom, FDistortionInfo& OutDistortionInfo) const
{
	return DistortionTable.GetPoint(InFocus, InZoom, OutDistortionInfo);
}

bool ULensFile::GetFocalLengthPoint(const float InFocus, const float InZoom, FFocalLengthInfo& OutFocalLengthInfo) const
{
	return FocalLengthTable.GetPoint(InFocus, InZoom, OutFocalLengthInfo);
}

bool ULensFile::GetImageCenterPoint(const float InFocus, const float InZoom, FImageCenterInfo& OutImageCenterInfo) const
{
	return ImageCenterTable.GetPoint(InFocus, InZoom, OutImageCenterInfo);
}

bool ULensFile::GetNodalOffsetPoint(const float InFocus, const float InZoom, FNodalPointOffset& OutNodalPointOffset) const
{
	return NodalOffsetTable.GetPoint(InFocus, InZoom, OutNodalPointOffset);
}

bool ULensFile::GetSTMapPoint(const float InFocus, const float InZoom, FSTMapInfo& OutSTMapInfo) const
{
	return STMapTable.GetPoint(InFocus, InZoom, OutSTMapInfo);
}

void ULensFile::AddDistortionPoint(float NewFocus, float NewZoom, const FDistortionInfo& NewDistortionPoint, const FFocalLengthInfo& NewFocalLength)
{
	const bool bPointAdded = DistortionTable.AddPoint(NewFocus, NewZoom, NewDistortionPoint, InputTolerance, false);
	FocalLengthTable.AddPoint(NewFocus, NewZoom, NewFocalLength, InputTolerance, bPointAdded);
}

void ULensFile::AddFocalLengthPoint(float NewFocus, float NewZoom, const FFocalLengthInfo& NewFocalLength)
{
	FocalLengthTable.AddPoint(NewFocus, NewZoom, NewFocalLength, InputTolerance, false);
}

void ULensFile::AddImageCenterPoint(float NewFocus, float NewZoom, const FImageCenterInfo& NewImageCenterPoint)
{
	ImageCenterTable.AddPoint(NewFocus, NewZoom, NewImageCenterPoint, InputTolerance, false);
}

void ULensFile::AddNodalOffsetPoint(float NewFocus, float NewZoom, const FNodalPointOffset& NewNodalOffsetPoint)
{
	NodalOffsetTable.AddPoint(NewFocus, NewZoom, NewNodalOffsetPoint, InputTolerance, false);
}

void ULensFile::AddSTMapPoint(float NewFocus, float NewZoom, const FSTMapInfo& NewPoint)
{
	STMapTable.AddPoint(NewFocus, NewZoom, NewPoint, InputTolerance, false);
}

void ULensFile::RemoveFocusPoint(ELensDataCategory InDataCategory, float InFocus)
{
	switch(InDataCategory)
	{
		case ELensDataCategory::Distortion:
		{
			DistortionTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::ImageCenter:
		{
			ImageCenterTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::Zoom:
		{
			FocalLengthTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::STMap:
		{
			STMapTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::NodalOffset:
		{
			NodalOffsetTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::Focus:
		{
			EncodersTable.RemoveFocusPoint(InFocus);
			break;
		}
		case ELensDataCategory::Iris:
		{
			EncodersTable.RemoveIrisPoint(InFocus);
			break;
		}
		default:
		{}
	}
}

void ULensFile::RemoveZoomPoint(ELensDataCategory InDataCategory, float InFocus, float InZoom)
{
	switch(InDataCategory)
	{
		case ELensDataCategory::Distortion:
		{
			DistortionTable.RemoveZoomPoint(InFocus, InZoom);
			break;
		}
		case ELensDataCategory::ImageCenter:
		{
			ImageCenterTable.RemoveZoomPoint(InFocus, InZoom);
			break;
		}
		case ELensDataCategory::Zoom:
		{
			FocalLengthTable.RemoveZoomPoint(InFocus, InZoom);
			break;
		}
		case ELensDataCategory::STMap:
		{
			STMapTable.RemoveZoomPoint(InFocus, InZoom);
			break;
		}
		case ELensDataCategory::NodalOffset:
		{
			NodalOffsetTable.RemoveZoomPoint(InFocus, InZoom);
			break;
		}
		case ELensDataCategory::Focus:
		case ELensDataCategory::Iris:
		default:
		{}
	}
}

void ULensFile::ClearAll()
{
	EncodersTable.ClearAll();
	LensDataTableUtils::EmptyTable(DistortionTable);
	LensDataTableUtils::EmptyTable(FocalLengthTable);
	LensDataTableUtils::EmptyTable(STMapTable);
	LensDataTableUtils::EmptyTable(ImageCenterTable);
	LensDataTableUtils::EmptyTable(NodalOffsetTable);
}

void ULensFile::ClearData(ELensDataCategory InDataCategory)
{
	switch(InDataCategory)
	{
		case ELensDataCategory::Distortion:
		{
			LensDataTableUtils::EmptyTable(DistortionTable);
			break;
		}
		case ELensDataCategory::ImageCenter:
		{
			LensDataTableUtils::EmptyTable(ImageCenterTable);
			break;
		}
		case ELensDataCategory::Zoom:
		{
			LensDataTableUtils::EmptyTable(FocalLengthTable);
				break;
		}
		case ELensDataCategory::STMap:
		{
			LensDataTableUtils::EmptyTable(STMapTable);
				break;
		}
		case ELensDataCategory::NodalOffset:
		{
			LensDataTableUtils::EmptyTable(NodalOffsetTable);
			break;
		}
		case ELensDataCategory::Focus:
		{
			EncodersTable.Focus.Reset();
			break;
		}
		case ELensDataCategory::Iris:
		{
			EncodersTable.Iris.Reset();
			break;
		}
		default:
		{
		}
	}
}

bool ULensFile::HasSamples(ELensDataCategory InDataCategory) const
{
	return GetTotalPointNum(InDataCategory) > 0 ? true : false;
}

int32 ULensFile::GetTotalPointNum(ELensDataCategory InDataCategory) const
{
	switch(InDataCategory)
	{
		case ELensDataCategory::Distortion:
		{
			return DistortionTable.GetTotalPointNum();
		}
		case ELensDataCategory::ImageCenter:
		{
			return ImageCenterTable.GetTotalPointNum();
		}
		case ELensDataCategory::Zoom:
		{
			return FocalLengthTable.GetTotalPointNum();
		}
		case ELensDataCategory::STMap:
		{
			return STMapTable.GetTotalPointNum();
		}
		case ELensDataCategory::NodalOffset:
		{
			return NodalOffsetTable.GetTotalPointNum();
		}
		case ELensDataCategory::Focus:
		{
			return EncodersTable.GetNumFocusPoints();
		}
		case ELensDataCategory::Iris:
		{
			return EncodersTable.GetNumIrisPoints();
		}
		default:
		{
			return -1;
		}
	}
}

const FBaseLensTable* ULensFile::GetDataTable(ELensDataCategory InDataCategory) const
{
	switch(InDataCategory)
	{
		case ELensDataCategory::Distortion:
		{
			return &DistortionTable;
		}
		case ELensDataCategory::ImageCenter:
		{
			return &ImageCenterTable;
		}
		case ELensDataCategory::Zoom:
		{
			return &FocalLengthTable;
		}
		case ELensDataCategory::STMap:
		{
			return &STMapTable;
		}
		case ELensDataCategory::NodalOffset:
		{
			return &NodalOffsetTable;
		}
		case ELensDataCategory::Focus:
		case ELensDataCategory::Iris:
		default:
		{
			// No base table for now.
			return nullptr;
		}
	}
}

void ULensFile::PostInitProperties()
{
	Super::PostInitProperties();
	
	const FIntPoint DisplacementMapResolution = GetDefault<UCameraCalibrationSettings>()->GetDisplacementMapResolution();
	CreateIntermediateDisplacementMaps(DisplacementMapResolution);

	// Set a Lens file reference to all tables
	DistortionTable.LensFile =
		FocalLengthTable.LensFile = ImageCenterTable.LensFile =
		NodalOffsetTable.LensFile = STMapTable.LensFile = this;

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void ULensFile::Tick(float DeltaTime)
{
	if (CalibratedMapProcessor)
	{
		CalibratedMapProcessor->Update();
	}

	UpdateDerivedData();
}

void ULensFile::UpdateDisplacementMapResolution(const FIntPoint NewDisplacementMapResolution)
{
	CreateIntermediateDisplacementMaps(NewDisplacementMapResolution);

	// Mark all points in the STMap table as dirty, so that they will update their derived data on the next tick
	if (DataMode == ELensDataMode::STMap)
	{
		for (FSTMapFocusPoint& FocusPoint : STMapTable.GetFocusPoints())
		{
			for (FSTMapZoomPoint& ZoomPoint : FocusPoint.ZoomPoints)
			{
				ZoomPoint.DerivedDistortionData.bIsDirty = true;
			}
		}
	}
}

TStatId ULensFile::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULensFile, STATGROUP_Tickables);
}

void ULensFile::UpdateDerivedData()
{
	if(DataMode == ELensDataMode::STMap)
	{
		for(FSTMapFocusPoint& FocusPoint : STMapTable.GetFocusPoints())
		{
			for (FSTMapZoomPoint& ZoomPoint : FocusPoint.ZoomPoints)
			{
				if (ZoomPoint.DerivedDistortionData.bIsDirty)
				{
					//Early exit if source map does not exist
					if (ZoomPoint.STMapInfo.DistortionMap == nullptr)
					{
						ZoomPoint.DerivedDistortionData.bIsDirty = false;
						continue;
					}

					//Early exit it the source map is not yet loaded (but leave it marked dirty so it tries again later)
					if (ZoomPoint.STMapInfo.DistortionMap->GetResource() == nullptr ||
						ZoomPoint.STMapInfo.DistortionMap->GetResource()->IsProxy())
					{
						continue;
					}

					const FIntPoint CurrentDisplacementMapResolution = GetDefault<UCameraCalibrationSettings>()->GetDisplacementMapResolution();

					//Create required undistortion texture for newly added points
					if ((ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap == nullptr)
						|| (ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap->SizeX != CurrentDisplacementMapResolution.X)
						|| (ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap->SizeY != CurrentDisplacementMapResolution.Y))

					{
						ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap = LensFileUtils::CreateDisplacementMapRenderTarget(this, CurrentDisplacementMapResolution);
					}

					//Create required distortion texture for newly added points
					if ((ZoomPoint.DerivedDistortionData.DistortionDisplacementMap == nullptr)
						|| (ZoomPoint.DerivedDistortionData.DistortionDisplacementMap->SizeX != CurrentDisplacementMapResolution.X)
						|| (ZoomPoint.DerivedDistortionData.DistortionDisplacementMap->SizeY != CurrentDisplacementMapResolution.Y))
					{
						ZoomPoint.DerivedDistortionData.DistortionDisplacementMap = LensFileUtils::CreateDisplacementMapRenderTarget(this, CurrentDisplacementMapResolution);
					}

					check(ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap);
					check(ZoomPoint.DerivedDistortionData.DistortionDisplacementMap);

					FDerivedDistortionDataJobArgs JobArgs;
					JobArgs.Focus = FocusPoint.Focus;
					JobArgs.Zoom = ZoomPoint.Zoom;
					JobArgs.Format = ZoomPoint.STMapInfo.MapFormat;
					JobArgs.SourceDistortionMap = ZoomPoint.STMapInfo.DistortionMap;
					JobArgs.OutputUndistortionDisplacementMap = ZoomPoint.DerivedDistortionData.UndistortionDisplacementMap;
					JobArgs.OutputDistortionDisplacementMap = ZoomPoint.DerivedDistortionData.DistortionDisplacementMap;
					JobArgs.JobCompletedCallback.BindUObject(this, &ULensFile::OnDistortionDerivedDataJobCompleted);
					if (CalibratedMapProcessor->PushDerivedDistortionDataJob(MoveTemp(JobArgs)))
					{
						++DerivedDataInFlightCount;
						ZoomPoint.DerivedDistortionData.bIsDirty = false;
					}
				}
			}
		
		}
	}
}

void ULensFile::CreateIntermediateDisplacementMaps(const FIntPoint DisplacementMapResolution)
{
	UndistortionDisplacementMapHolders.Reset(DisplacementMapHolderCount);
	DistortionDisplacementMapHolders.Reset(DisplacementMapHolderCount);
	for (int32 Index = 0; Index < DisplacementMapHolderCount; ++Index)
	{
		UTextureRenderTarget2D* NewUndistortionMap = LensFileUtils::CreateDisplacementMapRenderTarget(GetTransientPackage(), DisplacementMapResolution);
		UTextureRenderTarget2D* NewDistortionMap = LensFileUtils::CreateDisplacementMapRenderTarget(GetTransientPackage(), DisplacementMapResolution);
		UndistortionDisplacementMapHolders.Add(NewUndistortionMap);
		DistortionDisplacementMapHolders.Add(NewDistortionMap);
	}
}

ULensFile* FLensFilePicker::GetLensFile() const
{
	if(bUseDefaultLensFile)
	{
		UCameraCalibrationSubsystem* SubSystem = GEngine->GetEngineSubsystem<UCameraCalibrationSubsystem>();
		return SubSystem->GetDefaultLensFile();
	}
	else
	{
		return LensFile;
	}
}

