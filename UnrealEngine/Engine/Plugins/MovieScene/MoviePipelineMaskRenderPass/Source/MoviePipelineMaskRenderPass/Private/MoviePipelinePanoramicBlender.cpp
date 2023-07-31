// Copyright Epic Games, Inc. All Rights Reserved.
#include "MoviePipelinePanoramicBlender.h"
#include "MoviePipelinePanoramicPass.h"
#include "Async/ParallelFor.h"

FMoviePipelinePanoramicBlender::FMoviePipelinePanoramicBlender(TSharedPtr<MoviePipeline::IMoviePipelineOutputMerger> InOutputMerger, const FIntPoint InOutputResolution)
	: OutputMerger(InOutputMerger)
{
	OutputEquirectangularMapSize = InOutputResolution;
}

static FLinearColor GetColorBilinearFiltered(const FImagePixelData* InSampleData, const FVector2D& InSamplePixelCoords, bool& OutClipped, bool bInForceAlphaToOpaque = false)
{
	// Pixel coordinates assume that 0.5, 0.5 is the center of the pixel, so we subtract half to make it indexable.
	const FVector2D PixelCoordinateIndex = InSamplePixelCoords - 0.5f;

	// Get surrounding pixels indices
	FIntPoint LowerLeftPixelIndex = FIntPoint(FMath::FloorToInt(PixelCoordinateIndex.X), FMath::FloorToInt(PixelCoordinateIndex.Y));
	FIntPoint LowerRightPixelIndex = FIntPoint(LowerLeftPixelIndex + FIntPoint(1, 0));
	FIntPoint UpperLeftPixelIndex = FIntPoint(LowerLeftPixelIndex + FIntPoint(0, 1));
	FIntPoint UpperRightPixelIndex = FIntPoint(LowerLeftPixelIndex + FIntPoint(1, 1));

	// Clamp pixels indices to pixels array bounds.
	// ToDo: Is this needed? Should we handle wrap around for the bottom right pixel? What gives
	auto ClampPixelCoords = [&](FIntPoint& InOutPixelCoords, const FIntPoint& InArraySize)
	{
		if (InOutPixelCoords.X > InArraySize.X - 1 ||
			InOutPixelCoords.Y > InArraySize.Y - 1 ||
			InOutPixelCoords.X < 0 ||
			InOutPixelCoords.Y < 0)
		{
			OutClipped = true;
		}
		InOutPixelCoords = FIntPoint(FMath::Clamp(InOutPixelCoords.X, 0, InArraySize.X - 1), FMath::Clamp(InOutPixelCoords.Y, 0, InArraySize.Y - 1));
	};
	ClampPixelCoords(LowerLeftPixelIndex, InSampleData->GetSize());
	ClampPixelCoords(LowerRightPixelIndex, InSampleData->GetSize());
	ClampPixelCoords(UpperLeftPixelIndex, InSampleData->GetSize());
	ClampPixelCoords(UpperRightPixelIndex, InSampleData->GetSize());

	// Fetch the colors for the four pixels. We convert to FLinearColor here so that our accumulation
	// is done in linear space with enough precision. The samples are probably in F16 color right now.
	FLinearColor LowerLeftPixelColor;
	FLinearColor LowerRightPixelColor;
	FLinearColor UpperLeftPixelColor;
	FLinearColor UpperRightPixelColor;

	int64 SizeInBytes = 0;
	const void* SrcRawDataPtr = nullptr;
	InSampleData->GetRawData(SrcRawDataPtr, SizeInBytes);

	switch (InSampleData->GetType())
	{
	case EImagePixelType::Float16:
	{
		const FFloat16Color* ColorDataF16 = static_cast<const FFloat16Color*>(SrcRawDataPtr);
		LowerLeftPixelColor = FLinearColor(ColorDataF16[LowerLeftPixelIndex.X + (LowerLeftPixelIndex.Y * InSampleData->GetSize().X)]);
		LowerRightPixelColor = FLinearColor(ColorDataF16[LowerRightPixelIndex.X + (LowerRightPixelIndex.Y * InSampleData->GetSize().X)]);
		UpperLeftPixelColor = FLinearColor(ColorDataF16[UpperLeftPixelIndex.X + (UpperLeftPixelIndex.Y * InSampleData->GetSize().X)]);
		UpperRightPixelColor = FLinearColor(ColorDataF16[UpperRightPixelIndex.X + (UpperRightPixelIndex.Y * InSampleData->GetSize().X)]);
	}
	break;
	case EImagePixelType::Float32:
	{
		const FLinearColor* ColorDataF32 = static_cast<const FLinearColor*>(SrcRawDataPtr);
		LowerLeftPixelColor = ColorDataF32[LowerLeftPixelIndex.X + (LowerLeftPixelIndex.Y * InSampleData->GetSize().X)];
		LowerRightPixelColor = ColorDataF32[LowerRightPixelIndex.X + (LowerRightPixelIndex.Y * InSampleData->GetSize().X)];
		UpperLeftPixelColor = ColorDataF32[UpperLeftPixelIndex.X + (UpperLeftPixelIndex.Y * InSampleData->GetSize().X)];
		UpperRightPixelColor = ColorDataF32[UpperRightPixelIndex.X + (UpperRightPixelIndex.Y * InSampleData->GetSize().X)];
	}
	break;
	default:
		// Not implemented
		check(0);
	}

	// Interpolate between the 4 pixels based on the exact sub-pixel offset of the incoming coordinate (which may not be centered)
	const float FracX = 1.f - FMath::Frac(InSamplePixelCoords.X);
	const float FracY = 1.f - FMath::Frac(InSamplePixelCoords.Y);
	FLinearColor InterpolatedPixelColor = 
		 (LowerLeftPixelColor * (1.0f - FracX) + LowerRightPixelColor * FracX) * (1.0f - FracY)
		 + (UpperLeftPixelColor * (1.0f - FracX) + UpperRightPixelColor * FracX) * FracY;

	// Force final color alpha to opaque if requested
	if (bInForceAlphaToOpaque)
	{
		InterpolatedPixelColor.A = 1.0f;
	}

	return InterpolatedPixelColor;
}

DECLARE_CYCLE_STAT(TEXT("STAT_MoviePipeline_PanoBlend"), STAT_MoviePipeline_PanoBlend, STATGROUP_MoviePipeline);

void FMoviePipelinePanoramicBlender::OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData)
{
	SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_PanoBlend);

	// This function is called every time a sample comes in from the GPU (after being accumulated) and needs to handle
	// multiple samples from multiple frames being in flight at once. First step is to search to see if we're already
	// working on an output frame for this sample.
	TSharedPtr<FPanoramicOutputFrame> OutputFrame = nullptr;
	TSharedPtr<FPanoramicBlendData> BlendDataTarget = nullptr;

	FPanoramicImagePixelDataPayload* DataPayload = InData->GetPayload<FPanoramicImagePixelDataPayload>();
	check(DataPayload);

	const double BlendStartTime = FPlatformTime::Seconds();

	// We calculate all of these immediately on the task thread because it's read-only and we need the
	// information calculated by it to allocate memory when we start working on a sample.
	FIntPoint SampleSize = DataPayload->Pane.Resolution;
	FRotator SampleRotation = FRotator(DataPayload->Pane.CameraRotation);
	const float SampleHalfHorizontalFoVDegrees = 0.5f * DataPayload->Pane.HorizontalFieldOfView;
	const float SampleHalfVerticalFoVDegrees = 0.5f * DataPayload->Pane.VerticalFieldOfView; // FMath::RadiansToDegrees((FMath::Atan((0.5f * SampleSize.X / FMath::Tan(DataPayload->Pane.HorizontalFieldOfView / 2.f))))); // CTransformTools::getVFovFromHFovAndARInDegrees(iSampleFovInDegrees, static_cast<float>(iSampleSize.X) / static_cast<float>(iSampleSize.Y));
	const float SampleHalfHorizontalFoVCosine = FMath::Cos(FMath::DegreesToRadians(SampleHalfHorizontalFoVDegrees));
	const float SampleHalfVerticalFoVCosine = FMath::Cos(FMath::DegreesToRadians(SampleHalfVerticalFoVDegrees));

	// Now calculate which direction the Panoramic Pane (that this sample represents) was facing originally.
	const float SampleYawRad = FMath::DegreesToRadians(SampleRotation.Yaw);
	const float SamplePitchRad = FMath::DegreesToRadians(SampleRotation.Pitch);
	const FVector SampleDirectionOnTheta = FVector(FMath::Cos(SampleYawRad), FMath::Sin(SampleYawRad), 0);
	const FVector SampleDirectionOnPhi = FVector(FMath::Cos(SamplePitchRad), 0.f, FMath::Sin(SamplePitchRad));

	// Now construct a projection matrix representing the sample matching the original perspective it was taken from.
	const FMatrix SampleProjectionMatrix = FReversedZPerspectiveMatrix(FMath::DegreesToRadians(SampleHalfHorizontalFoVDegrees), SampleSize.X, SampleSize.Y, DataPayload->Pane.NearClippingPlane);

	// For our given output size, figure out how many degrees each pixel represents.
	const float EquiRectMapThetaStep = 360.f / (float)OutputEquirectangularMapSize.X;
	const float EquiRectMapPhiStep = 180.f / (float)OutputEquirectangularMapSize.Y;

	// Compute the index bounds in the equirectangular map corresponding to the sample bounds, so we don't loop over unnecessary pixels.
	// This is approximated according to the weighting function for blending too.
	// This assumes that the origin of the equirectangular map (0,0) has a yaw/pitch equal to -180/-90.
	// Phi evolves in the opposite direction of Y (Y's origin is up-left)
	// Pitch is clamped, because there is no vertical wrapping in the map
	// Yaw is not clamped, because horizontal wrapping is possible. 
	// The MinBound for X can actually be greater than the MaxBound due to wrapping, modulo is applied at eval time to ensure it wraps right.
	float SampleYawMin = SampleRotation.Yaw - SampleHalfHorizontalFoVDegrees;
	float SampleYawMax = SampleRotation.Yaw + SampleHalfHorizontalFoVDegrees;
	int32 PixelIndexHorzMinBound = FMath::FloorToInt(((SampleYawMin) + 180.f) / EquiRectMapThetaStep);
	int32 PixelIndexHorzMaxBound = FMath::FloorToInt(((SampleYawMax) + 180.f) / EquiRectMapThetaStep);

	float SamplePitchMin = FMath::Max(SampleRotation.Pitch - SampleHalfVerticalFoVDegrees, -90.f); // Clamped to [-90, 90]
	float SamplePitchMax = FMath::Min(SampleRotation.Pitch + SampleHalfVerticalFoVDegrees, 90.f); // Clamped to [-90, 90]
	int32 PixelIndexVertMinBound = FMath::Max((OutputEquirectangularMapSize.Y) - FMath::FloorToInt((SamplePitchMax + 90.f) / EquiRectMapPhiStep), 0);
	int32 PixelIndexVertMaxBound = FMath::Min((OutputEquirectangularMapSize.Y) - FMath::FloorToInt((SamplePitchMin + 90.f) / EquiRectMapPhiStep), OutputEquirectangularMapSize.Y);

	{
		// Do a quick lock while we're iterating/adding to the PendingData array so a second sample
		// doesn't come in mid iteration.
		FScopeLock ScopeLock(&GlobalQueueDataMutex);

		for (TPair<FMoviePipelineFrameOutputState, TSharedPtr<FPanoramicOutputFrame>>& KVP : PendingData)
		{
			if (KVP.Key.OutputFrameNumber == DataPayload->SampleState.OutputState.OutputFrameNumber)
			{
				OutputFrame = KVP.Value;
				break;
			}
		}

		if (!OutputFrame)
		{
			// UE_LOG(LogMovieRenderPipeline, Log, TEXT("Starting a new Output Frame in the Panoramic Blender for frame: %d"), DataPayload->SampleState.OutputState.OutputFrameNumber);

			OutputFrame = PendingData.Add(DataPayload->SampleState.OutputState, MakeShared<FPanoramicOutputFrame>());
			int32 EyeMultiplier = DataPayload->Pane.EyeIndex == -1 ? 1 : 2;
			int32 TotalSampleCount = DataPayload->Pane.NumHorizontalSteps * DataPayload->Pane.NumVerticalSteps * EyeMultiplier;
			OutputFrame->NumSamplesTotal = TotalSampleCount;

			{
				LLM_SCOPE_BYNAME(TEXT("MoviePipeline/PanoBlendFrameOutput"));
				OutputFrame->OutputEquirectangularMap.SetNumZeroed(OutputEquirectangularMapSize.X * OutputEquirectangularMapSize.Y * EyeMultiplier);
			}
		}
	}

	// Now that we know which output frame we're contributing towards, we'll ask it for
	// our own copy of the data so that we can blend without worrying about other threads.
	check(OutputFrame);
	{
		FScopeLock ScopeLock(&GlobalQueueDataMutex);
		BlendDataTarget = MakeShared<FPanoramicBlendData>();
		BlendDataTarget->EyeIndex = DataPayload->Pane.EyeIndex;
		BlendDataTarget->bFinished = false;
		BlendDataTarget->OriginalDataPayload = StaticCastSharedRef<FPanoramicImagePixelDataPayload>(DataPayload->Copy());
		TArray<TSharedPtr<FPanoramicBlendData>>& EyeArray = OutputFrame->BlendedData.FindOrAdd(DataPayload->Pane.EyeIndex);
		EyeArray.Add(BlendDataTarget);
	}

	// Okay, our BlendDataTarget is now our own unique copy of the data to work on.
	BlendDataTarget->BlendStartTime = FPlatformTime::Seconds();

	// Build a rect that describes which part of the output map we'll be rendering into
	BlendDataTarget->OutputBoundsMin = FIntPoint(PixelIndexHorzMinBound, PixelIndexVertMinBound);
	BlendDataTarget->OutputBoundsMax = FIntPoint(PixelIndexHorzMaxBound, PixelIndexVertMaxBound);

	BlendDataTarget->PixelWidth = BlendDataTarget->OutputBoundsMax.X - BlendDataTarget->OutputBoundsMin.X;
	BlendDataTarget->PixelHeight = BlendDataTarget->OutputBoundsMax.Y - BlendDataTarget->OutputBoundsMin.Y;

	// These need to be zeroed as we don't always touch every pixel in the rect with blending
	// and they get +='d together later.
	{
		LLM_SCOPE_BYNAME(TEXT("MoviePipeline/PanoBlendPerTaskOutput"));
		BlendDataTarget->Data.SetNumZeroed((BlendDataTarget->PixelWidth) * (BlendDataTarget->PixelHeight));
	}

	// Finally we can perform our actual blending. We blend into our intermediate buffer
	// instead of the final output array to avoid multiple threads contending for pixels.
	for (int32 Y = PixelIndexVertMinBound; Y < PixelIndexVertMaxBound; Y++)
	{
		for (int32 X = PixelIndexHorzMinBound; X < PixelIndexHorzMaxBound; X++)
		{
			// These X, Y coordinates are in output resolution space which is where we want to blend to.
			// Our X bounds may go OOB, but we wrap horizontally so we need to figure out the proper X index.
			const int32 OutputPixelX = ((X % OutputEquirectangularMapSize.X) + OutputEquirectangularMapSize.X) % OutputEquirectangularMapSize.X;
			const int32 OutputPixelY = Y;

			// Get the spherical coordinates (Theta and Phi) corresponding to the X and Y of the equirectangular map coordinates, converted to
			// [-180, 180] and [-90, 90] coordinate space respectively. The half pixel offset is used to make the center of a pixel be considered
			// that coordinate, and Phi increments in the opposite direction of Y.
			const float Theta = EquiRectMapThetaStep * (((float)OutputPixelX) + 0.5f) - 180.f;
			const float Phi = EquiRectMapPhiStep * (((float)OutputEquirectangularMapSize.Y - OutputPixelY) + 0.5f) - 90.f;

			// Now convert the spherical coordinates into an actual direction (on the output map)
			const float ThetaDeg = FMath::DegreesToRadians(Theta);
			const float PhiDeg = FMath::DegreesToRadians(Phi);
			const FVector OutputDirection(FMath::Cos(PhiDeg) * FMath::Cos(ThetaDeg), FMath::Cos(PhiDeg) * FMath::Sin(ThetaDeg), FMath::Sin(PhiDeg));
			const FVector OutputDirectionTheta = FVector(FMath::Cos(ThetaDeg), FMath::Sin(ThetaDeg), 0);
			const FVector OutputDirectionPhi = FVector(FMath::Cos(PhiDeg), 0.f, FMath::Sin(PhiDeg));

			// Now we can compute how much the sample should influence this pixel. It is weighted by angular distance to the direction
			// so that the edges have less influence (where they'd be more distorted anyways).
			const float DirectionPhiDot = FVector::DotProduct(OutputDirectionPhi, SampleDirectionOnPhi); // ToDo: This only considers the whole Pano Pane and not per pixel of sample?
			const float DirectionThetaDot = FVector::DotProduct(OutputDirectionTheta, SampleDirectionOnTheta);
			const float WeightTheta = FMath::Max(DirectionThetaDot - SampleHalfHorizontalFoVCosine, 0.0f) / (1.0f - SampleHalfHorizontalFoVCosine);
			const float WeightPhi = FMath::Max(DirectionPhiDot - SampleHalfVerticalFoVCosine, 0.0f) / (1.0f - SampleHalfVerticalFoVCosine);

			const float SampleWeight = WeightTheta * WeightPhi;
			const float SampleWeightSquared = SampleWeight* SampleWeight; // Exponential falloff produces a nicer blending result.

			// The sample weight may be very small and not worth influencing this pixel.
			if (SampleWeightSquared > KINDA_SMALL_NUMBER)
			{
				// Transform the direction vector from the equirectangular map world space to sample world space
				FVector4 DirectionInSampleWorldSpace = FVector4(SampleRotation.UnrotateVector(OutputDirection), 1.0f);

				static const FMatrix UnrealCoordinateConversion = FMatrix(
					FPlane(0, 0, 1, 0),
					FPlane(1, 0, 0, 0),
					FPlane(0, 1, 0, 0),
					FPlane(0, 0, 0, 1));
				DirectionInSampleWorldSpace = UnrealCoordinateConversion.TransformFVector4(DirectionInSampleWorldSpace);

				// Then project that direction into sample clip space
				FVector4 DirectionInSampleClipSpace = SampleProjectionMatrix.TransformFVector4(DirectionInSampleWorldSpace);

				// Converted into normalized device space (Divide by w for perspective)
				FVector DirectionInSampleNDSpace = FVector(DirectionInSampleClipSpace) / DirectionInSampleClipSpace.W;

				// Get the final pixel coordinates (direction in screen space)
				FVector2D DirectionInSampleScreenSpace = ((FVector2D(DirectionInSampleNDSpace) + 1.0f) / 2.0f) * FVector2D(SampleSize.X, SampleSize.Y);

				// Flip the Y value due to Y's zero coordinate being top left.
				DirectionInSampleScreenSpace.Y = ((float)SampleSize.Y - DirectionInSampleScreenSpace.Y) - 1.0f;

				// Do a bilinear color sample at the pixel coordinates (from the sample), weight it, and add it to the output map. We store
				// weights separately so that we can preserve the alpha channel of the main image.
				bool bClipped = false;
				FLinearColor SampleColor = GetColorBilinearFiltered(InData.Get(), DirectionInSampleScreenSpace, bClipped, true);

				if (!bClipped)
				{
					// When we calculate the actual output location we need to shift the X/Y. This is because up until now the math has been done in
					// output resolution space, but each sample only allocates a color map big enough for itself. It'll get shifted back out to the
					// right location later.
					int32 SampleOutputX = OutputPixelX - BlendDataTarget->OutputBoundsMin.X;
					// Mod this again by our output map so we don't OOB on it. It'll wrap weirdly in the output map but should restore fine.
					SampleOutputX = ((SampleOutputX % (BlendDataTarget->PixelWidth)) + (BlendDataTarget->PixelWidth)) % (BlendDataTarget->PixelWidth); // Positive Mod
					int32 SampleOutputY = Y;
					SampleOutputY -= BlendDataTarget->OutputBoundsMin.Y;
						
					const int32 FinalIndex = SampleOutputX + (SampleOutputY * (BlendDataTarget->PixelWidth));
					BlendDataTarget->Data[FinalIndex] += SampleColor * SampleWeightSquared;
					// OutputEquirectangularMap[OutputPixelX + (OutputPixelY * OutputEquirectangularMapSize.X)] += SampleColor * SampleWeightSquared; // ToDo move to weight map.
				}
			}
		}
	}

	BlendDataTarget->BlendEndTime = FPlatformTime::Seconds();

	// Blend the new sample into the output map as soon as possible, so that we can free the temporary memory held
	// by this sample. This part is single-threaded (with respect to the other tasks).
	{
		// Lock access to our output map
		FScopeLock ScopeLock(&OutputDataMutex);


		// For Stereo images, just offset the second eye by the entire output size so it goes on the bottom half
		// of the image. We've already made the array twice as big as it needs to be (in the case of stereo) and
		// below EyeIndex 0 (left eye) will go in the first half of the image, eye index 1 in the second.
		int32 EyeOffset = 0;
		if (BlendDataTarget->OriginalDataPayload->Pane.EyeIndex != -1)
		{
			EyeOffset = (OutputEquirectangularMapSize.X * OutputEquirectangularMapSize.Y) * BlendDataTarget->OriginalDataPayload->Pane.EyeIndex;
		}

		for (int32 SampleY = 0; SampleY < BlendDataTarget->PixelHeight; SampleY++)
		{
			for (int32 SampleX = 0; SampleX < BlendDataTarget->PixelWidth; SampleX++)
			{
				int32 OriginalX = SampleX + BlendDataTarget->OutputBoundsMin.X;
				int32 OriginalY = SampleY + BlendDataTarget->OutputBoundsMin.Y;
				const int32 OutputPixelX = ((OriginalX % OutputEquirectangularMapSize.X) + OutputEquirectangularMapSize.X) % OutputEquirectangularMapSize.X;
				const int32 OutputPixelY = OriginalY;

				int32 SourceIndex = SampleX + (SampleY * (BlendDataTarget->PixelWidth));
				int32 DestIndex = OutputPixelX + (OutputPixelY * OutputEquirectangularMapSize.X);
				OutputFrame->OutputEquirectangularMap[DestIndex + EyeOffset] += BlendDataTarget->Data[SourceIndex];
			}
		}

		bool bDebugSamples = DataPayload->SampleState.bWriteSampleToDisk;
		if (bDebugSamples)
		{
			// Write each blended sample to the output as a debug sample so we can inspect the job blending is doing for each pane.
			// Hack up the debug output name a bit so they're unique.
			if (BlendDataTarget->OriginalDataPayload->Pane.EyeIndex >= 0)
			{
				BlendDataTarget->OriginalDataPayload->Debug_OverrideFilename = FString::Printf(TEXT("/%s_PaneX_%d_PaneY_%dEye_%d-Blended.%d"),
					*BlendDataTarget->OriginalDataPayload->PassIdentifier.Name, BlendDataTarget->OriginalDataPayload->Pane.HorizontalStepIndex,
					BlendDataTarget->OriginalDataPayload->Pane.VerticalStepIndex, DataPayload->Pane.EyeIndex, BlendDataTarget->OriginalDataPayload->SampleState.OutputState.OutputFrameNumber);
			}
			else
			{
				BlendDataTarget->OriginalDataPayload->Debug_OverrideFilename = FString::Printf(TEXT("/%s_PaneX_%d_PaneY_%d-Blended.%d"),
					*BlendDataTarget->OriginalDataPayload->PassIdentifier.Name, BlendDataTarget->OriginalDataPayload->Pane.HorizontalStepIndex,
					BlendDataTarget->OriginalDataPayload->Pane.VerticalStepIndex, BlendDataTarget->OriginalDataPayload->SampleState.OutputState.OutputFrameNumber);
			}

			// Now that the sample has been blended pass it (and the memory it owned, we already read from it) to the debug output step.
			TUniquePtr<TImagePixelData<FLinearColor>> FinalPixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(BlendDataTarget->PixelWidth, BlendDataTarget->PixelHeight), TArray64<FLinearColor>(MoveTemp(BlendDataTarget->Data)), BlendDataTarget->OriginalDataPayload);
			ensure(OutputMerger.IsValid());
			OutputMerger.Pin()->OnSingleSampleDataAvailable_AnyThread(MoveTemp(FinalPixelData));
		}
		else
		{
			// Ensure we reset the memory we allocated, to minimize concurrent allocations.
			BlendDataTarget->Data.Reset();
		}
	}

	// This should only be set to true when this thread is really finished with the work.
	bool bIsLastSample = false;
	{
		FScopeLock ScopeLock(&GlobalQueueDataMutex);
		BlendDataTarget->bFinished = true;

		// Check to see if all of the samples have come in from the GPU and have been blended
		int32 NumFinishedSamples = 0;

		// For each eye
		for(TPair<int32, TArray<TSharedPtr<FPanoramicBlendData>>>& KVP : OutputFrame->BlendedData)
		{
			// For each sample in the eye
			for (int32 Index = 0; Index < KVP.Value.Num(); Index++)
			{
				NumFinishedSamples += KVP.Value[Index]->bFinished ? 1 : 0;
			}
		}

		bIsLastSample = NumFinishedSamples == OutputFrame->NumSamplesTotal;
	}

	if (bIsLastSample)
	{
		{
			// Now that we've accumulated all the pixel values, we need to normalize them.
			for (int32 PixelIndex = 0; PixelIndex < OutputFrame->OutputEquirectangularMap.Num(); PixelIndex++)
			{
				FLinearColor& Pixel = OutputFrame->OutputEquirectangularMap[PixelIndex];
				Pixel.R /= Pixel.A;
				Pixel.G /= Pixel.A;
				Pixel.B /= Pixel.A;
				
				// Now that we've used the weight stored in the alpha channel, restore it to full opaque-ness so
				// the resulting png/exrs don't end up semi-transparent.
				Pixel.A = 1.f;
			}
		}
		TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> NewPayload = DataPayload->Copy();

		int32 OutputSizeX = OutputEquirectangularMapSize.X;
		int32 OutputSizeY = DataPayload->Pane.EyeIndex >= 0 ? OutputEquirectangularMapSize.Y * 2 : OutputEquirectangularMapSize.Y;
		TUniquePtr<TImagePixelData<FLinearColor>> FinalPixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(OutputSizeX, OutputSizeY), TArray64<FLinearColor>(MoveTemp(OutputFrame->OutputEquirectangularMap)), NewPayload);

		if(ensure(OutputMerger.IsValid()))
		{
			OutputMerger.Pin()->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
		}

		// Remove this frame from the PendingData map so that we don't hold references for the duration of the render.
		// It's important that we use the manually looked up OutputFrame from PendingData
		// as PendingData uses the equality operator. Some combinations of temporal sampling + slowmo tracks results in different
		// original source frame numbers, which would cause the tmap lookup to fail and thus returning an empty frame.
		PendingData.Remove(DataPayload->SampleState.OutputState);
	}

}

void FMoviePipelinePanoramicBlender::OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData)
{
	// This is used for debug output, just pass it straight through.
	ensure(OutputMerger.IsValid());
	OutputMerger.Pin()->OnSingleSampleDataAvailable_AnyThread(MoveTemp(InData));
}

static FMoviePipelineMergerOutputFrame MoviePipelineDummyOutputFrame;

FMoviePipelineMergerOutputFrame& FMoviePipelinePanoramicBlender::QueueOutputFrame_GameThread(const FMoviePipelineFrameOutputState& CachedOutputState)
{
	// Unsupported, the main Output Builder should be the one tracking this.
	check(0);
	return MoviePipelineDummyOutputFrame;
}

void FMoviePipelinePanoramicBlender::AbandonOutstandingWork()
{
	// Not yet implemented
	check(0);
}
