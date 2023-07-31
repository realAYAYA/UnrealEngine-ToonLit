// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "Sections/MovieSceneSubSection.h"
#include "TemplateSequenceSection.generated.h"

/**
 * Defines the type of property scaling for a template sequence.
 */
UENUM()
enum class ETemplateSectionPropertyScaleType
{
	/** Scales a float property */
	FloatProperty,
	/** Scales the location channels (X, Y, Z) of a transform property */
	TransformPropertyLocationOnly,
	/** Scales the rotation channels (yaw, pitch, roll) of a transform property */
	TransformPropertyRotationOnly
};

/**
 * Defines a property scaling for a template sequence.
 */
USTRUCT()
struct FTemplateSectionPropertyScale
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ObjectBinding;

	UPROPERTY()
	FMovieScenePropertyBinding PropertyBinding;

	UPROPERTY()
	ETemplateSectionPropertyScaleType PropertyScaleType= ETemplateSectionPropertyScaleType::FloatProperty;

	UPROPERTY()
	FMovieSceneFloatChannel FloatChannel;
};

/**
 * Defines the section for a template sequence track.
 */
UCLASS()
class TEMPLATESEQUENCE_API UTemplateSequenceSection 
	: public UMovieSceneSubSection
	, public IMovieSceneEntityProvider
{
public:

	GENERATED_BODY()

	UTemplateSequenceSection(const FObjectInitializer& ObjInitializer);

	void AddPropertyScale(const FTemplateSectionPropertyScale& InPropertyScale);
	void RemovePropertyScale(int32 InIndex);

	// UMovieSceneSection interface
	virtual bool ShowCurveForChannel(const void *Channel) const override;
	virtual void OnDilated(float DilationFactor, FFrameNumber Origin) override;

protected:

	virtual EMovieSceneChannelProxyType CacheChannelProxy() override;

	virtual bool PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder) override;
	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;

public:

	UPROPERTY()
	TArray<FTemplateSectionPropertyScale> PropertyScales;
};
