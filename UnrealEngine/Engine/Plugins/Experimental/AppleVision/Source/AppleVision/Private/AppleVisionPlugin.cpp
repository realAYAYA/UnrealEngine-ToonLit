// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IAppleVisionPlugin.h"
#include "AppleImageUtilsTypes.h"
#include "UObject/GCObject.h"
#include "Async/Async.h"
#include "Engine/Texture.h"

#if SUPPORTS_APPLE_VISION_1_0
	#import <Vision/Vision.h>

	// For runtime checks so that clang doesn't warn on targets < our SDK version
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wpartial-availability"
#endif

DECLARE_CYCLE_STAT(TEXT("Detect Faces"), STAT_DetectFaces, STATGROUP_Game);

DEFINE_LOG_CATEGORY(LogAppleVision);

class FAppleVisionPlugin :
	public IAppleVisionPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Performs a face detection computer vision task in the background
	 *
	 * @param SourceImage the image to detect faces in (NOTE: must support UAppleImageInterface)
	 *
	 * @return the async task that is doing the conversion
	 */
	virtual TSharedPtr<FAppleVisionDetectFacesAsyncTaskBase, ESPMode::ThreadSafe> DetectFaces(UTexture* SourceImage) override;
};

IMPLEMENT_MODULE(FAppleVisionPlugin, AppleVision);

void FAppleVisionPlugin::StartupModule()
{
}

void FAppleVisionPlugin::ShutdownModule()
{
}

class FAppleVisionDetectFacesAsyncTask :
	public FAppleVisionDetectFacesAsyncTaskBase,
	public FGCObject
{
public:
	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(SourceImage);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAppleVisionDetectFacesAsyncTask");
	}
	//~ FGCObject

	void SetErrorReason(const FString& InError)
	{
		Error = InError;
		bHadError = true;
		bIsDone = true;
	}
	void MarkDone() { bIsDone = true; }

	/** The UTexture that wraps the Apple image data underneath */
	UTexture* SourceImage;
};

#if SUPPORTS_APPLE_VISION_1_0
static inline CIImage* ToImage(IAppleImageInterface* AppleImageInterface)
{
	check(AppleImageInterface != nullptr);

	switch (AppleImageInterface->GetTextureType())
	{
		case EAppleTextureType::Image:
		{
			return AppleImageInterface->GetImage();
		}

		case EAppleTextureType::PixelBuffer:
		{
			CVPixelBufferRef PixelBuffer = AppleImageInterface->GetPixelBuffer();
			if (PixelBuffer != nullptr)
			{
				return [[CIImage imageWithCVPixelBuffer: PixelBuffer] autorelease];
			}
			break;
		}

		case EAppleTextureType::Surface:
		{
			IOSurfaceRef Surface = AppleImageInterface->GetSurface();
			if (Surface != nullptr)
			{
				return [[CIImage imageWithIOSurface: Surface] autorelease];
			}
			break;
		}
	}
	return nullptr;
}

FVector2D GetImageSize(IAppleImageInterface* AppleImageInterface)
{
	check(AppleImageInterface);

	FVector2D Size;
	// @todo joeg - add more types
	switch (AppleImageInterface->GetTextureType())
	{
		case EAppleTextureType::PixelBuffer:
		{
			CVPixelBufferRef PixelBuffer = AppleImageInterface->GetPixelBuffer();
			if (PixelBuffer != nullptr)
			{
				Size.X = CVPixelBufferGetWidth(PixelBuffer);
				Size.Y = CVPixelBufferGetHeight(PixelBuffer);
			}
			break;
		}
	}
	return Size;
}

FBox2D ToBox2D(CGRect NormalizedBounds, IAppleImageInterface* AppleImageInterface)
{
	const FVector2D ImageSize = GetImageSize(AppleImageInterface);

	// Apple returns a bounding box from 0..1 with the lower left corner being the origin
	FVector2D Min;
	Min.X = NormalizedBounds.origin.x * ImageSize.X;
	Min.Y = (1.f - NormalizedBounds.origin.y) * ImageSize.Y;

	FVector2D Max;
	Max.X = Min.X + (NormalizedBounds.size.width * ImageSize.X);
	Max.Y = Min.Y + (NormalizedBounds.size.height * ImageSize.Y);

	return FBox2D(Min, Max);
}
#endif

TSharedPtr<FAppleVisionDetectFacesAsyncTaskBase, ESPMode::ThreadSafe> FAppleVisionPlugin::DetectFaces(UTexture* SourceImage)
{
	TSharedPtr<FAppleVisionDetectFacesAsyncTask, ESPMode::ThreadSafe> DetectionTask = MakeShared<FAppleVisionDetectFacesAsyncTask, ESPMode::ThreadSafe>();
	DetectionTask->SourceImage = SourceImage;

	// Make sure our interface is supported
	IAppleImageInterface* AppleImage = Cast<IAppleImageInterface>(SourceImage);
	if (AppleImage == nullptr)
	{
		DetectionTask->SetErrorReason(TEXT("DetectFaces only supports UAppleImageInterface derived textures"));
		return DetectionTask;
	}

#if SUPPORTS_APPLE_VISION_1_0
	if (!FAppleVisionAvailability::SupportsAppleVision10())
	{
		DetectionTask->SetErrorReason(TEXT("DetectFaces requires iOS 11.0+"));
		return DetectionTask;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [DetectionTask]()
	{
		SCOPE_CYCLE_COUNTER(STAT_DetectFaces);

		IAppleImageInterface* AppleImageInterface = Cast<IAppleImageInterface>(DetectionTask->SourceImage);

		CIImage* Image = ToImage(AppleImageInterface);

		NSDictionary* Options = [[[NSDictionary alloc] init] autorelease];
		VNImageRequestHandler* RequestHandler = [[[VNImageRequestHandler alloc] initWithCIImage: Image options: Options] autorelease];

		VNDetectFaceRectanglesRequest* DetectFacesRequest = [[[VNDetectFaceRectanglesRequest alloc] init] autorelease];
		NSError* ErrorObj = nil;

		if ([RequestHandler performRequests: @[DetectFacesRequest] error: &ErrorObj] == YES)
		{
			if (DetectFacesRequest.results != nullptr)
			{
				FFaceDetectionResult& Result = DetectionTask->GetResult();
				Result.DetectedFaces.Reset(DetectFacesRequest.results.count);

				// Loop through each detected face
				for (VNFaceObservation* Face in DetectFacesRequest.results)
				{
					FDetectedFace NewFace;
					NewFace.Confidence = Face.confidence;
					NewFace.BoundingBox = ToBox2D(Face.boundingBox, AppleImageInterface);
					Result.DetectedFaces.Add(NewFace);
				}
			}
			// Notify any async listeners that we are done
			DetectionTask->MarkDone();
		}
		else
		{
			FString Error([ErrorObj localizedDescription]);
			UE_LOG(LogAppleVision, Error, TEXT("DetectFaces() failed with error (%s)"), *Error);

			DetectionTask->SetErrorReason(Error);
		}
	});
#endif
	return DetectionTask;
}

#if SUPPORTS_APPLE_VISION_1_0
	#pragma clang diagnostic pop
#endif
