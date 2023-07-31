// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneColorTemplate.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "UObject/UnrealType.h"
#include "MovieSceneCommonHelpers.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Styling/SlateColor.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Evaluation/Blending/MovieSceneBlendingActuator.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneColorTemplate)

DECLARE_CYCLE_STAT(TEXT("Color Track Token Execute"), MovieSceneEval_ColorTrack_TokenExecute, STATGROUP_MovieSceneEval);

enum class EColorType : uint8
{
	/** FSlateColor */
	Slate, 
	/** FLinearColor */
	Linear,
	/** FColor */
	Color,
};

struct FColorToken
{
	FLinearColor ColorValue;

	FColorToken() {}
	FColorToken(FLinearColor InColorValue) : ColorValue(InColorValue){}

	void Apply(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		if (!Type.IsSet() && !DeduceColorType(Object, Bindings))
		{
			return;
		}

		switch(Type.GetValue())
		{
			case EColorType::Slate:		ApplySlateColor(Object, Bindings);		break;
			case EColorType::Linear: 	ApplyLinearColor(Object, Bindings);		break;
			case EColorType::Color: 	ApplyColor(Object, Bindings);			break;
		}
	}

	static FColorToken Get(const UObject& InObject, FTrackInstancePropertyBindings& Bindings)
	{
		FColorToken Token;

		if (Token.DeduceColorType(InObject, Bindings))
		{
			switch (Token.Type.GetValue())
			{
			case EColorType::Color: 	Token.ColorValue = Bindings.GetCurrentValue<FColor>(InObject);							break;
			case EColorType::Slate: 	Token.ColorValue = Bindings.GetCurrentValue<FSlateColor>(InObject).GetSpecifiedColor();	break;
			case EColorType::Linear:	Token.ColorValue = Bindings.GetCurrentValue<FLinearColor>(InObject);					break;
			}
		}

		return Token;
	}

private:


	void ApplyColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		const bool bConvertBackToSRgb = true;
		if (ULightComponent* LightComponent = Cast<ULightComponent>(&Object))
		{
			// Light components have to be handled specially here because their set function takes two values, the linear color
			// and whether or not the linear color needs to be converted back to sRGB.  All other other set function cases should
			// follow the sequencer convention of having a single parameter of the correct type, which in this case is an FColor
			// already in sRGB format.
			if (Bindings.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULightComponent, LightColor))
			{
				LightComponent->SetLightColor( ColorValue, bConvertBackToSRgb );
				return;
			}
		}
		else if (USkyLightComponent* SkyLightComponent = Cast<USkyLightComponent>(&Object))
		{
			if (Bindings.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USkyLightComponent, LightColor))
			{
				SkyLightComponent->SetLightColor( ColorValue );
				return;
			}
		}

		FColor SRgbColorValue = ColorValue.ToFColor( bConvertBackToSRgb );
		Bindings.CallFunction<FColor>( Object, SRgbColorValue );
	}

	void ApplySlateColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		FSlateColor NewColor(ColorValue);
		Bindings.CallFunction<FSlateColor>(Object, NewColor);
	}

	void ApplyLinearColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		Bindings.CallFunction<FLinearColor>(Object, ColorValue);
	}

	bool DeduceColorType(const UObject& InObject, FTrackInstancePropertyBindings& Bindings)
	{
		if (Type.IsSet())
		{
			return true;
		}

		const FStructProperty* StructProp = CastField<const FStructProperty>(Bindings.GetProperty(InObject));
		if (!StructProp || !StructProp->Struct)
		{
			return false;
		}

		FName StructName = StructProp->Struct->GetFName();
		static const FName SlateColor("SlateColor");
		if( StructName == NAME_Color )
		{
			// We assume the color we get back is in sRGB, assigning it to a linear color will implicitly
			// convert it to a linear color instead of using ReinterpretAsLinear which will just change the
			// bytes into floats using divide by 255.
			Type = EColorType::Color;
		}
		else if( StructName == SlateColor )
		{
			Type = EColorType::Slate;
		}
		else
		{
			Type = EColorType::Linear;
		}

		return true;
	}

	/** Optional deduced color type - when empty, this needs deducing */
	TOptional<EColorType> Type;
};

struct FColorTrackPreAnimatedState : IMovieScenePreAnimatedToken
{
	FColorToken Token;
	FTrackInstancePropertyBindings Bindings;

	FColorTrackPreAnimatedState(FColorToken InToken, const FTrackInstancePropertyBindings& InBindings)
		: Token(InToken)
		, Bindings(InBindings)
	{
	}

	virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& Params) override
	{
		Token.Apply(Object, Bindings);
	}
};

struct FColorTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FTrackInstancePropertyBindings& PropertyBindings;
	FColorTokenProducer(FTrackInstancePropertyBindings& InPropertyBindings) : PropertyBindings(InPropertyBindings) {}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		return FColorTrackPreAnimatedState(FColorToken::Get(Object, PropertyBindings), PropertyBindings);
	}
};

struct FColorTokenActuator : TMovieSceneBlendingActuator<FLinearColor>
{
	PropertyTemplate::FSectionData PropertyData;
	FColorTokenActuator(const PropertyTemplate::FSectionData& InPropertyData)
		: TMovieSceneBlendingActuator<FLinearColor>(FMovieSceneBlendingActuatorID(InPropertyData.PropertyID))
		, PropertyData(InPropertyData)
	{}

	virtual FLinearColor RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		return FColorToken::Get(*InObject, *PropertyData.PropertyBindings).ColorValue;
	}

	virtual void Actuate(UObject* InObject, const FLinearColor& InFinalValue, const TBlendableTokenStack<FLinearColor>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		ensureMsgf(InObject, TEXT("Attempting to evaluate a Color track with a null object."));

		if (InObject != nullptr)
		{
			FTrackInstancePropertyBindings& PropertyBindings = *PropertyData.PropertyBindings;

			OriginalStack.SavePreAnimatedState(Player, *InObject, PropertyData.PropertyID, FColorTokenProducer(PropertyBindings));

			// Apply a token
			FColorToken(InFinalValue).Apply(*InObject, PropertyBindings);
		}
	}
	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, typename TCallTraits<FLinearColor>::ParamType InValue, const TBlendableTokenStack<FLinearColor>& OriginalStack, const FMovieSceneContext& Context) const
	{
		FLinearColor Value = InValue;
		InterrogationData.Add(Value, FMovieScenePropertySectionTemplate::GetColorInterrogationKey());
	};

};

FMovieSceneColorSectionTemplate::FMovieSceneColorSectionTemplate(const UMovieSceneColorSection& Section, const UMovieSceneColorTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath().ToString())
	, BlendType(Section.GetBlendType().Get())
{
	Curves[0] = Section.GetRedChannel();
	Curves[1] = Section.GetGreenChannel();
	Curves[2] = Section.GetBlueChannel();
	Curves[3] = Section.GetAlphaChannel();
}

void FMovieSceneColorSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FFrameTime Time = Context.GetTime();
	UE::MovieScene::TMultiChannelValue<float, 4> AnimationData;

	for (uint8 Index = 0; Index < 4; ++Index)
	{
		float ChannelValue = 0.f;
		if (Curves[Index].Evaluate(Time, ChannelValue))
		{
			AnimationData.Set(Index, ChannelValue);
		}
	}

	// Only blend the token if at least one of the channels was animated
	if (!AnimationData.IsEmpty())
	{
		// Actuator type ID for this property
		FMovieSceneAnimTypeID UniquePropertyID = GetPropertyTypeID();
		FMovieSceneBlendingActuatorID ActuatorTypeID = FMovieSceneBlendingActuatorID(UniquePropertyID);
		if (!ExecutionTokens.GetBlendingAccumulator().FindActuator<FLinearColor>(ActuatorTypeID))
		{
			PropertyTemplate::FSectionData SectionData;
			SectionData.Initialize(PropertyData.PropertyName, PropertyData.PropertyPath);

			ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared<FColorTokenActuator>(SectionData));
		}

		const float Weight = EvaluateEasing(Time);
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FLinearColor>(AnimationData, BlendType, Weight));
	}
}

void FMovieSceneColorSectionTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	const FFrameTime Time = Context.GetTime();
	UE::MovieScene::TMultiChannelValue<float, 4> AnimationData;

	for (uint8 Index = 0; Index < 4; ++Index)
	{
		float ChannelValue = 0.f;
		if (Curves[Index].Evaluate(Time, ChannelValue))
		{
			AnimationData.Set(Index, ChannelValue);
		}
	}


	FMovieSceneAnimTypeID TypeID = GetPropertyTypeID();
	static FMovieSceneBlendingActuatorID ActuatorTypeID(TypeID);
	if (!Container.GetAccumulator().FindActuator<FLinearColor>(ActuatorTypeID))
	{
		PropertyTemplate::FSectionData SectionData;
		SectionData.Initialize(PropertyData.PropertyName, PropertyData.PropertyPath);
		Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared<TPropertyActuator<FLinearColor>>(SectionData));
	}

	if (!AnimationData.IsEmpty())
	{
		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<FLinearColor>(AnimationData, BlendType, Weight));
	}
}
