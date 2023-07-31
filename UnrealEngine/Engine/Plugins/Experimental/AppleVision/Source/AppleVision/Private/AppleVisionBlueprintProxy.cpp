// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppleVisionBlueprintProxy.h"
#include "IAppleVisionPlugin.h"

UAppleVisionDetectFacesAsyncTaskBlueprintProxy::UAppleVisionDetectFacesAsyncTaskBlueprintProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldTick(true)
{
}

void UAppleVisionDetectFacesAsyncTaskBlueprintProxy::Tick(float DeltaTime)
{
	if (!AsyncTask.IsValid())
	{
		bShouldTick = false;
		OnFailure.Broadcast(FaceDetectionResult);
		return;
	}

	if (AsyncTask->IsDone())
	{
		bShouldTick = false;
		// Fire the right delegate
		if (!AsyncTask->HadError())
		{
			FaceDetectionResult = AsyncTask->GetResult();
			OnSuccess.Broadcast(FaceDetectionResult);
		}
		else
		{
			OnFailure.Broadcast(FaceDetectionResult);
		}
	}
}

UAppleVisionDetectFacesAsyncTaskBlueprintProxy* UAppleVisionDetectFacesAsyncTaskBlueprintProxy::CreateProxyObjectForDetectFaces(UTexture* SourceImage)
{
	UAppleVisionDetectFacesAsyncTaskBlueprintProxy* Proxy = NewObject<UAppleVisionDetectFacesAsyncTaskBlueprintProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);

	Proxy->AsyncTask = IAppleVisionPlugin::Get().DetectFaces(SourceImage);

	return Proxy;
}
