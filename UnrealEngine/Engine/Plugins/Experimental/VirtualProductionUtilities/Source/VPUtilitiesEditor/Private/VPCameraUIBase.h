// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VPCameraUIBase.generated.h"


class ACameraActor;
class UCameraComponent;


UCLASS()
class UVPCameraUIBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> SelectedCamera;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Camera")
	TObjectPtr<UCameraComponent> SelectedCameraComponent;

protected:
	virtual bool Initialize() override;
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Camera")
	void OnSelectedCameraChanged();

private:
	void OnEditorSelectionChanged(UObject* NewSelection);
	void OnEditorSelectNone();
};