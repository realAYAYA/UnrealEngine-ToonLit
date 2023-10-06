// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "LiveLinkRole.h"
#include "Misc/FrameRate.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "LiveLinkSubjectSettings.generated.h"

class ULiveLinkFrameInterpolationProcessor;
class ULiveLinkFramePreProcessor;
class ULiveLinkFrameTranslator;
class ULiveLinkRole;


// Base class for live link subject settings
UCLASS(MinimalAPI)
class ULiveLinkSubjectSettings : public UObject
{
public:
	GENERATED_BODY()

	/** List of available preprocessor the subject will use. */
	UPROPERTY(EditAnywhere, Instanced, Category = "LiveLink", meta = (DisplayName = "Pre Processors"))
	TArray<TObjectPtr<ULiveLinkFramePreProcessor>> PreProcessors;

	/** The interpolation processor the subject will use. */
	UPROPERTY(EditAnywhere, Instanced, Category = "LiveLink", meta = (DisplayName = "Interpolation"))
	TObjectPtr<ULiveLinkFrameInterpolationProcessor> InterpolationProcessor;

	/** List of available translator the subject can use. */
	UPROPERTY(EditAnywhere, Instanced, Category = "LiveLink", meta = (DisplayName = "Translators"))
	TArray<TObjectPtr<ULiveLinkFrameTranslator>> Translators;

	UPROPERTY()
	TSubclassOf<ULiveLinkRole> Role;

	/** Last FrameRate estimated by the subject. If in Timecode mode, this will come directly from the QualifiedFrameTime. */
	UPROPERTY(VisibleAnywhere, Category="LiveLink")
	FFrameRate FrameRate;
	
	/** If enabled, rebroadcast this subject */
	UPROPERTY(EditAnywhere, Category = "LiveLink")
    bool bRebroadcastSubject;

public:
	//~ Begin UObject interface
#if WITH_EDITOR
	LIVELINKINTERFACE_API virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface
};
