// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneParameterSection.generated.h"

struct FMovieSceneParameterPropertyInterface;
struct FMovieSceneConstParameterPropertyInterface;


UINTERFACE()
class MOVIESCENETRACKS_API UMovieSceneParameterSectionExtender : public UInterface
{
public:
	GENERATED_BODY()
};

class MOVIESCENETRACKS_API IMovieSceneParameterSectionExtender
{
public:

	GENERATED_BODY()

	void ExtendEntity(UMovieSceneEntitySystemLinker* EntityLinker, const UE::MovieScene::FEntityImportParams& Params, UE::MovieScene::FImportedEntity* OutImportedEntity);

private:

	virtual void ExtendEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const UE::MovieScene::FEntityImportParams& Params, UE::MovieScene::FImportedEntity* OutImportedEntity) = 0;
};



/**
 * Structure representing the animated value of a scalar parameter.
 */
struct FScalarParameterNameAndValue
{
	/** Creates a new FScalarParameterAndValue with a parameter name and a value. */
	FScalarParameterNameAndValue(FName InParameterName, float InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the scalar parameter. */
	FName ParameterName;

	/** The animated value of the scalar parameter. */
	float Value;
};

/**
 * Structure representing the value of a bool parameter.
 */
struct FBoolParameterNameAndValue
{
	/** Creates a new FScalarParameterAndValue with a parameter name and a value. */
	FBoolParameterNameAndValue(FName InParameterName, bool InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the bool parameter. */
	FName ParameterName;

	/** The value of the bool parameter. */
	bool Value;
};

/**
 * Structure representing the animated value of a vector2D parameter.
 */
struct FVector2DParameterNameAndValue
{
	/** Creates a new FVectorParameterAndValue with a parameter name and a value. */
	FVector2DParameterNameAndValue(FName InParameterName, FVector2D InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the vector parameter. */
	FName ParameterName;

	/** The animated value of the vector parameter. */
	FVector2D Value;
};

/**
 * Structure representing the animated value of a vector parameter.
 */
struct FVectorParameterNameAndValue
{
	/** Creates a new FVectorParameterAndValue with a parameter name and a value. */
	FVectorParameterNameAndValue(FName InParameterName, FVector InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the vector parameter. */
	FName ParameterName;

	/** The animated value of the vector parameter. */
	FVector Value;
};

/**
 * Structure representing the animated value of a color parameter.
 */
struct FColorParameterNameAndValue
{
	/** Creates a new FColorParameterAndValue with a parameter name and a value. */
	FColorParameterNameAndValue(FName InParameterName, FLinearColor InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the color parameter. */
	FName ParameterName;

	/** The animated value of the color parameter. */
	FLinearColor Value;
};

struct FTransformParameterNameAndValue
{

	/** The name of the transform  parameter. */
	FName ParameterName;
	/** Translation component */
	FVector Translation;
	/** Rotation component */
	FRotator Rotation;
	/** Scale component */
	FVector Scale;

	FTransformParameterNameAndValue(FName InParameterName,const FVector& InTranslation,
		const FRotator& InRotation, const FVector& InScale) : Translation(InTranslation),
		Rotation(InRotation), Scale(InScale)
	{
		ParameterName = InParameterName;	
	}
};

/**
 * Structure representing an bool  parameter and it's associated animation curve.
 */
USTRUCT()
struct MOVIESCENETRACKS_API FBoolParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FBoolParameterNameAndCurve()
	{}

	/** Creates a new FScalarParameterNameAndCurve for a specific scalar parameter. */
	FBoolParameterNameAndCurve(FName InParameterName);

	/** The name of the scalar parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** The curve which contains the animation data for the scalar parameter. */
	UPROPERTY()
	FMovieSceneBoolChannel ParameterCurve;
};

/**
 * Structure representing an animated scalar parameter and it's associated animation curve.
 */
USTRUCT()
struct MOVIESCENETRACKS_API FScalarParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FScalarParameterNameAndCurve()
	{}

	/** Creates a new FScalarParameterNameAndCurve for a specific scalar parameter. */
	FScalarParameterNameAndCurve(FName InParameterName);

	/** The name of the scalar parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** The curve which contains the animation data for the scalar parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel ParameterCurve;
};

/**
 * Structure representing an animated vector2D parameter and it's associated animation curve.
 */
USTRUCT()
struct MOVIESCENETRACKS_API FVector2DParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FVector2DParameterNameAndCurves()
	{}

	/** Creates a new FVectorParameterNameAndCurve for a specific vector parameter. */
	FVector2DParameterNameAndCurves(FName InParameterName);

	/** The name of the vector parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** The curve which contains the animation data for the x component of the vector parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel XCurve;

	/** The curve which contains the animation data for the y component of the vector parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel YCurve;

};


/**
 * Structure representing an animated vector parameter and it's associated animation curve.
 */
USTRUCT()
struct MOVIESCENETRACKS_API FVectorParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FVectorParameterNameAndCurves() 
	{}

	/** Creates a new FVectorParameterNameAndCurve for a specific vector parameter. */
	FVectorParameterNameAndCurves(FName InParameterName);

	/** The name of the vector parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** The curve which contains the animation data for the x component of the vector parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel XCurve;

	/** The curve which contains the animation data for the y component of the vector parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel YCurve;

	/** The curve which contains the animation data for the z component of the vector parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel ZCurve;
};


/**
* Structure representing an animated color parameter and it's associated animation curve.
*/
USTRUCT()
struct MOVIESCENETRACKS_API FColorParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FColorParameterNameAndCurves()
	{}

	/** Creates a new FVectorParameterNameAndCurve for a specific color parameter. */
	FColorParameterNameAndCurves(FName InParameterName);

	/** The name of the color parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** The curve which contains the animation data for the red component of the color parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel RedCurve;

	/** The curve which contains the animation data for the green component of the color parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel GreenCurve;

	/** The curve which contains the animation data for the blue component of the color parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel BlueCurve;

	/** The curve which contains the animation data for the alpha component of the color parameter. */
	UPROPERTY()
	FMovieSceneFloatChannel AlphaCurve;
};

/**
* Structure representing an animated transform parameter and it's associated animation curve.
*/
USTRUCT()
struct MOVIESCENETRACKS_API FTransformParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FTransformParameterNameAndCurves()
	{}

	/** Creates a new FVectorParameterNameAndCurve for a specific color parameter. */
	FTransformParameterNameAndCurves(FName InParameterName);

	/** The name of the transform  parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	/** Translation curves */
	UPROPERTY()
	FMovieSceneFloatChannel Translation[3];

	/** Rotation curves */
	UPROPERTY()
	FMovieSceneFloatChannel Rotation[3];

	/** Scale curves */
	UPROPERTY()
	FMovieSceneFloatChannel Scale[3];
};
/**
 * A single movie scene section which can contain data for multiple named parameters.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneParameterSection
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
{
	GENERATED_UCLASS_BODY()

public:
	/** Adds a a key for a specific scalar parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddScalarParameterKey(FName InParameterName, FFrameNumber InTime, float InValue);

	/** Adds a a key for a specific bool parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddBoolParameterKey(FName InParameterName, FFrameNumber InTime, bool InValue);

	/** Adds a a key for a specific vector2D parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddVector2DParameterKey(FName InParameterName, FFrameNumber InTime, FVector2D InValue);

	/** Adds a a key for a specific vector parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddVectorParameterKey(FName InParameterName, FFrameNumber InTime, FVector InValue);

	/** Adds a a key for a specific color parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddColorParameterKey(FName InParameterName, FFrameNumber InTime, FLinearColor InValue);

	/** Adds a a key for a specific color parameter at the specified time with the specified value. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void AddTransformParameterKey(FName InParameterName, FFrameNumber InTime, const FTransform& InValue);

	/** 
	 * Removes a scalar parameter from this section. 
	 * 
	 * @param InParameterName The name of the scalar parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveScalarParameter(FName InParameterName);

	/**
	 * Removes a bool parameter from this section.
	 *
	 * @param InParameterName The name of the bool parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveBoolParameter(FName InParameterName);

	/**
	 * Removes a vector2D parameter from this section.
	 *
	 * @param InParameterName The name of the vector2D parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveVector2DParameter(FName InParameterName);

	/**
     * Removes a vector parameter from this section.
    *
    * @param InParameterName The name of the vector parameter to remove.
    * @returns True if a parameter with that name was found and removed, otherwise false.
    */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveVectorParameter(FName InParameterName);

	/**
	 * Removes a color parameter from this section.
	 *
	 * @param InParameterName The name of the color parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveColorParameter(FName InParameterName);

	/**
	 * Removes a transform parameter from this section.
	 *
	 * @param InParameterName The name of the transform parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	bool RemoveTransformParameter(FName InParameterName);

	/** Gets the animated scalar parameters and their associated curves. */
	TArray<FScalarParameterNameAndCurve>& GetScalarParameterNamesAndCurves();
	const TArray<FScalarParameterNameAndCurve>& GetScalarParameterNamesAndCurves() const;

	/** Gets the animated bool parameters and their associated curves. */
	TArray<FBoolParameterNameAndCurve>& GetBoolParameterNamesAndCurves();
	const TArray<FBoolParameterNameAndCurve>& GetBoolParameterNamesAndCurves() const;

	/** Gets the animated vector2D parameters and their associated curves. */
	TArray<FVector2DParameterNameAndCurves>& GetVector2DParameterNamesAndCurves();
	const TArray<FVector2DParameterNameAndCurves>& GetVector2DParameterNamesAndCurves() const;

	/** Gets the animated vector parameters and their associated curves. */
	TArray<FVectorParameterNameAndCurves>& GetVectorParameterNamesAndCurves();
	const TArray<FVectorParameterNameAndCurves>& GetVectorParameterNamesAndCurves() const;

	/** Gets the animated color parameters and their associated curves. */
	TArray<FColorParameterNameAndCurves>& GetColorParameterNamesAndCurves();
	const TArray<FColorParameterNameAndCurves>& GetColorParameterNamesAndCurves() const;

	/** Gets the animated transform parameters and their associated curves. */
	TArray<FTransformParameterNameAndCurves>& GetTransformParameterNamesAndCurves();
    const TArray<FTransformParameterNameAndCurves>& GetTransformParameterNamesAndCurves() const;

	/** Gets the set of all parameter names used by this section. */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Section")
	void GetParameterNames(TSet<FName>& ParameterNames) const;

	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;
	virtual bool PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder) override;

	void ExternalPopulateEvaluationField(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder);

protected:

	//~ UMovieSceneSection interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual void ReconstructChannelProxy();

protected:
	/** The bool parameter names and their associated curves. */
	UPROPERTY()
	TArray<FBoolParameterNameAndCurve> BoolParameterNamesAndCurves;

	/** The scalar parameter names and their associated curves. */
	UPROPERTY()
	TArray<FScalarParameterNameAndCurve> ScalarParameterNamesAndCurves;

	/** The vector3D parameter names and their associated curves. */
	UPROPERTY()
	TArray<FVector2DParameterNameAndCurves> Vector2DParameterNamesAndCurves;

	/** The vector parameter names and their associated curves. */
	UPROPERTY()
	TArray<FVectorParameterNameAndCurves> VectorParameterNamesAndCurves;

	/** The color parameter names and their associated curves. */
	UPROPERTY()
	TArray<FColorParameterNameAndCurves> ColorParameterNamesAndCurves;

	/** The transform  parameter names and their associated curves. */
	UPROPERTY()
	TArray<FTransformParameterNameAndCurves> TransformParameterNamesAndCurves;
};