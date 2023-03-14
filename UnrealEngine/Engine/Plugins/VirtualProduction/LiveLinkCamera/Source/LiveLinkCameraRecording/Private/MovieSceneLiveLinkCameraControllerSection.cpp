// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneLiveLinkCameraControllerSection.h"

#include "Evaluation/MovieScenePreAnimatedState.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSource.h"
#include "IMovieScenePlayer.h"
#include "LiveLinkCameraController.h"
#include "LiveLinkComponentController.h"
#include "LiveLinkControllerBase.h"
#include "MovieSceneExecutionToken.h"
#include "Roles/LiveLinkCameraRole.h"

void UMovieSceneLiveLinkCameraControllerSection::Initialize(ULiveLinkControllerBase* InLiveLinkController)
{
}

void UMovieSceneLiveLinkCameraControllerSection::Update(IMovieScenePlayer* Player, const UE::MovieScene::FEvaluationHookParams& Params) const
{
	if (!CachedLensFile || !bApplyNodalOffsetFromCachedLensFile)
	{
		return;
	}

	for (TWeakObjectPtr<>& BoundObject : Player->FindBoundObjects(Params.ObjectBindingID, Params.SequenceID))
	{
		if (ULiveLinkComponentController* LiveLinkComponent = Cast<ULiveLinkComponentController>(BoundObject.Get()))
		{
			// Find the LL camera controller in the component's controller map			
			if (TObjectPtr<ULiveLinkControllerBase>* Controller = LiveLinkComponent->ControllerMap.Find(ULiveLinkCameraRole::StaticClass()))
			{
				if (ULiveLinkCameraController* CameraController = Cast<ULiveLinkCameraController>(*Controller))
				{
					UActorComponent* CurrentComponentToControl = CameraController->GetAttachedComponent();
 					if (UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(CurrentComponentToControl))
 					{
						const FLensFileEvalData& LensFileEvalData = CameraController->GetLensFileEvalDataRef();

						FNodalPointOffset Offset;
						CachedLensFile->EvaluateNodalPointOffset(LensFileEvalData.Input.Focus, LensFileEvalData.Input.Zoom, Offset);

						CineCameraComponent->AddLocalOffset(Offset.LocationOffset);
						CineCameraComponent->AddLocalRotation(Offset.RotationOffset);
 					}
				}
			}
		}
	}
}
