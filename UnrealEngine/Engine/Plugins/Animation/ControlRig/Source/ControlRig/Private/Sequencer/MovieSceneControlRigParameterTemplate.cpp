// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneControlRigParameterTemplate.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "ControlRig.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimCustomInstanceHelper.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "IControlRigObjectBinding.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "ControlRigObjectBinding.h"
#include "Evaluation/Blending/BlendableTokenStack.h"
#include "Evaluation/Blending/MovieSceneBlendingActuatorID.h"
#include "TransformNoScale.h"
#include "ControlRigComponent.h"
#include "SkeletalMeshRestoreState.h"
#include "Rigs/FKControlRig.h"
#include "UObject/UObjectAnnotation.h"
#include "Rigs/RigHierarchy.h"
#include "TransformConstraint.h"
#include "TransformableHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneControlRigParameterTemplate)

//#include "Particles/ParticleSystemComponent.h"

DECLARE_CYCLE_STAT(TEXT("ControlRig Parameter Track Evaluate"), MovieSceneEval_ControlRigTemplateParameter_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("ControlRig Parameter Track Token Execute"), MovieSceneEval_ControlRigParameterTrack_TokenExecute, STATGROUP_MovieSceneEval);

template<typename T>
struct TNameAndValue
{
	FName Name;
	T Value;
};


/**
 * Structure representing the animated value of a scalar parameter.
 */
struct FScalarParameterStringAndValue
{
	/** Creates a new FScalarParameterAndValue with a parameter name and a value. */
	FScalarParameterStringAndValue(FName InParameterName, float InValue)
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
 * Structure representing the animated value of a bool parameter.
 */
struct FBoolParameterStringAndValue
{
	/** Creates a new FBoolParameterAndValue with a parameter name and a value. */
	FBoolParameterStringAndValue(FName InParameterName, bool InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the bool parameter. */
	FName ParameterName;
	/** The animated value of the bool parameter. */
	bool Value;
};

/**
 * Structure representing the animated value of a int parameter.
 */
struct FIntegerParameterStringAndValue
{
	FIntegerParameterStringAndValue(FName InParameterName, int32 InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	FName ParameterName;
	int32 Value;
};

struct FControlSpaceAndValue
{
	FControlSpaceAndValue(FName InControlName, const FMovieSceneControlRigSpaceBaseKey& InValue)
	{
		ControlName = InControlName;
		Value = InValue;
	}
	FName ControlName;
	FMovieSceneControlRigSpaceBaseKey Value;
};
/**
 * Structure representing the animated value of a vector2D parameter.
 */
struct FVector2DParameterStringAndValue
{
	/** Creates a new FVector2DParameterAndValue with a parameter name and a value. */
	FVector2DParameterStringAndValue(FName InParameterName, FVector2D InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the vector2D parameter. */
	FName ParameterName;

	/** The animated value of the vector2D parameter. */
	FVector2D Value;
};

/**
 * Structure representing the animated value of a vector parameter.
 */
struct FVectorParameterStringAndValue
{
	/** Creates a new FVectorParameterAndValue with a parameter name and a value. */
	FVectorParameterStringAndValue(FName InParameterName, FVector InValue)
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
struct FColorParameterStringAndValue
{
	/** Creates a new FColorParameterAndValue with a parameter name and a value. */
	FColorParameterStringAndValue(FName InParameterName, FLinearColor InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the color parameter. */
	FName ParameterName;

	/** The animated value of the color parameter. */
	FLinearColor Value;
};

struct FEulerTransformParameterStringAndValue
{

	/** The name of the transform  parameter. */
	FName ParameterName;
	/** Transform component */
	FEulerTransform Transform;

	FEulerTransformParameterStringAndValue(FName InParameterName, const FEulerTransform& InTransform)
		: Transform(InTransform)
	{
		ParameterName = InParameterName;
	}
};

struct FConstraintAndActiveValue
{
	FConstraintAndActiveValue(TWeakObjectPtr<UTickableConstraint> InConstraint, TWeakObjectPtr<UTickableConstraint> InSpawnConstraint, bool InValue)
		: Constraint(InConstraint)
		, SpawnConstraint(InSpawnConstraint)
		, Value(InValue)
	{}
	TWeakObjectPtr<UTickableConstraint> Constraint;
	TWeakObjectPtr<UTickableConstraint> SpawnConstraint;
	bool Value;
};

struct FControlRigAnimTypeIDs;

/** Thread-safe because objects can be destroyed on background threads */
using FControlRigAnimTypeIDsPtr = TSharedPtr<FControlRigAnimTypeIDs, ESPMode::ThreadSafe>;

/**
 * Control rig anim type IDs are a little complex - they require a unique type ID for every bone
 * and they must be unique per-animating control rig. To efficiently support finding these each frame,
 * We store a cache of the type IDs in a container on an object annotation for each control rig.
 */
struct FControlRigAnimTypeIDs
{
	/** Get the anim type IDs for the specified section */
	static FControlRigAnimTypeIDsPtr Get(const UControlRig* ControlRig)
	{
		struct FControlRigAnimTypeIDsAnnotation
		{
			// IsDefault should really have been implemented as a trait rather than a function so that this type isn't necessary
			bool IsDefault() const
			{
				return Ptr == nullptr;
			}
			FControlRigAnimTypeIDsPtr Ptr;
		};

		// Function-local static so that this only gets created once it's actually required
		static FUObjectAnnotationSparse<FControlRigAnimTypeIDsAnnotation, true> AnimTypeIDAnnotation;

		FControlRigAnimTypeIDsAnnotation TypeIDs = AnimTypeIDAnnotation.GetAnnotation(ControlRig);
		if (TypeIDs.Ptr != nullptr)
		{
			return TypeIDs.Ptr;
		}

		FControlRigAnimTypeIDsPtr NewPtr = MakeShared<FControlRigAnimTypeIDs, ESPMode::ThreadSafe>();
		AnimTypeIDAnnotation.AddAnnotation(ControlRig, FControlRigAnimTypeIDsAnnotation{NewPtr});
		return NewPtr;
	}

	/** Find the anim-type ID for the specified scalar parameter */
	FMovieSceneAnimTypeID FindScalar(FName InParameterName)
	{
		return FindImpl(InParameterName, ScalarAnimTypeIDsByName);
	}
	/** Find the anim-type ID for the specified Vector2D parameter */
	FMovieSceneAnimTypeID FindVector2D(FName InParameterName)
	{
		return FindImpl(InParameterName, Vector2DAnimTypeIDsByName);
	}
	/** Find the anim-type ID for the specified vector parameter */
	FMovieSceneAnimTypeID FindVector(FName InParameterName)
	{
		return FindImpl(InParameterName, VectorAnimTypeIDsByName);
	}
	/** Find the anim-type ID for the specified transform parameter */
	FMovieSceneAnimTypeID FindTransform(FName InParameterName)
	{
		return FindImpl(InParameterName, TransformAnimTypeIDsByName);
	}
private:

	/** Sorted map should give the best trade-off for lookup speed with relatively small numbers of bones (O(log n)) */
	using MapType = TSortedMap<FName, FMovieSceneAnimTypeID, FDefaultAllocator, FNameFastLess>;

	static FMovieSceneAnimTypeID FindImpl(FName InParameterName, MapType& InOutMap)
	{
		if (const FMovieSceneAnimTypeID* Type = InOutMap.Find(InParameterName))
		{
			return *Type;
		}
		FMovieSceneAnimTypeID New = FMovieSceneAnimTypeID::Unique();
		InOutMap.Add(InParameterName, FMovieSceneAnimTypeID::Unique());
		return New;
	}
	/** Array of existing type identifiers */
	MapType ScalarAnimTypeIDsByName;
	MapType Vector2DAnimTypeIDsByName;
	MapType VectorAnimTypeIDsByName;
	MapType TransformAnimTypeIDsByName;
};

/**
 * Cache structure that is stored per-section that defines bitmasks for every
 * index within each curve type. Set bits denote that the curve should be 
 * evaluated. Only ever initialized once since the template will get re-created
 * whenever the control rig section changes
 */
struct FEvaluatedControlRigParameterSectionChannelMasks : IPersistentEvaluationData
{
	TBitArray<> ScalarCurveMask;
	TBitArray<> BoolCurveMask;
	TBitArray<> IntegerCurveMask;
	TBitArray<> EnumCurveMask;
	TBitArray<> Vector2DCurveMask;
	TBitArray<> VectorCurveMask;
	TBitArray<> ColorCurveMask;
	TBitArray<> TransformCurveMask;

	void Initialize(const UMovieSceneControlRigParameterSection* Section,
		TArrayView<const FScalarParameterNameAndCurve> Scalars,
		TArrayView<const FBoolParameterNameAndCurve> Bools,
		TArrayView<const FIntegerParameterNameAndCurve> Integers,
		TArrayView<const FEnumParameterNameAndCurve> Enums,
		TArrayView<const FVector2DParameterNameAndCurves> Vector2Ds,
		TArrayView<const FVectorParameterNameAndCurves> Vectors,
		TArrayView<const FColorParameterNameAndCurves> Colors,
		TArrayView<const FTransformParameterNameAndCurves> Transforms
		)
	{
		const TArray<bool>& ControlsMask = Section->GetControlsMask();

		const FChannelMapInfo* ChannelInfo = nullptr;

		ScalarCurveMask.Add(false, Scalars.Num());
		BoolCurveMask.Add(false, Bools.Num());
		IntegerCurveMask.Add(false, Integers.Num());
		EnumCurveMask.Add(false, Enums.Num());
		Vector2DCurveMask.Add(false, Vector2Ds.Num());
		VectorCurveMask.Add(false, Vectors.Num());
		ColorCurveMask.Add(false, Colors.Num());
		TransformCurveMask.Add(false, Transforms.Num());
		
		for (int32 Index = 0; Index < Scalars.Num(); ++Index)
		{
			const FScalarParameterNameAndCurve& Scalar = Scalars[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Scalar.ParameterName);
			ScalarCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Bools.Num(); ++Index)
		{
			const FBoolParameterNameAndCurve& Bool = Bools[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Bool.ParameterName);
			BoolCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Integers.Num(); ++Index)
		{
			const FIntegerParameterNameAndCurve& Integer = Integers[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Integer.ParameterName);
			IntegerCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Enums.Num(); ++Index)
		{
			const FEnumParameterNameAndCurve& Enum = Enums[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Enum.ParameterName);
			EnumCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->ControlIndex]);
		}
		for (int32 Index = 0; Index < Vector2Ds.Num(); ++Index)
		{
			const FVector2DParameterNameAndCurves& Vector2D = Vector2Ds[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Vector2D.ParameterName);
			Vector2DCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Vectors.Num(); ++Index)
		{
			const FVectorParameterNameAndCurves& Vector = Vectors[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Vector.ParameterName);
			VectorCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Colors.Num(); ++Index)
		{
			const FColorParameterNameAndCurves& Color = Colors[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Color.ParameterName);
			ColorCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
		for (int32 Index = 0; Index < Transforms.Num(); ++Index)
		{
			const FTransformParameterNameAndCurves& Transform = Transforms[Index];
			ChannelInfo = Section->ControlChannelMap.Find(Transform.ParameterName);
			TransformCurveMask[Index] = (!ChannelInfo || ControlsMask[ChannelInfo->MaskIndex]);
		}
	}
};
// Static hack because we cannot add this to the function parameters for EvaluateCurvesWithMasks due to hotfix restrictions
static FEvaluatedControlRigParameterSectionChannelMasks* HACK_ChannelMasks = nullptr;

struct FEvaluatedControlRigParameterSectionValues
{
	FEvaluatedControlRigParameterSectionValues() = default;

	FEvaluatedControlRigParameterSectionValues(FEvaluatedControlRigParameterSectionValues&&) = default;
	FEvaluatedControlRigParameterSectionValues& operator=(FEvaluatedControlRigParameterSectionValues&&) = default;

	// Non-copyable
	FEvaluatedControlRigParameterSectionValues(const FEvaluatedControlRigParameterSectionValues&) = delete;
	FEvaluatedControlRigParameterSectionValues& operator=(const FEvaluatedControlRigParameterSectionValues&) = delete;

	/** Array of evaluated scalar values */
	TArray<FScalarParameterStringAndValue, TInlineAllocator<2>> ScalarValues;
	/** Array of evaluated bool values */
	TArray<FBoolParameterStringAndValue, TInlineAllocator<2>> BoolValues;
	/** Array of evaluated integer values */
	TArray<FIntegerParameterStringAndValue, TInlineAllocator<2>> IntegerValues;
	/** Array of evaluated Spaces */
	TArray<FControlSpaceAndValue, TInlineAllocator<2>> SpaceValues;
	/** Array of evaluated vector2d values */
	TArray<FVector2DParameterStringAndValue, TInlineAllocator<2>> Vector2DValues;
	/** Array of evaluated vector values */
	TArray<FVectorParameterStringAndValue, TInlineAllocator<2>> VectorValues;
	/** Array of evaluated color values */
	TArray<FColorParameterStringAndValue, TInlineAllocator<2>> ColorValues;
	/** Array of evaluated transform values */
	TArray<FEulerTransformParameterStringAndValue, TInlineAllocator<2>> TransformValues;
	/** Array of evaluated constraint values */
	TArray<FConstraintAndActiveValue, TInlineAllocator<2>> ConstraintsValues;
};

/** Token for control rig control parameters */
struct FControlRigTrackTokenFloat
{
	FControlRigTrackTokenFloat() {}
	
	FControlRigTrackTokenFloat(float InValue)
		:Value(InValue)
	{}

	float Value;

};

struct FControlRigTrackTokenBool
{
	FControlRigTrackTokenBool() {}

	FControlRigTrackTokenBool(bool InValue)
		:Value(InValue)
	{}

	bool Value;
};

struct FControlRigTrackTokenVector2D
{
	FControlRigTrackTokenVector2D() {}
	FControlRigTrackTokenVector2D(FVector2D InValue)
		:Value(InValue)
	{}

	FVector2D Value;
};

struct FControlRigTrackTokenVector
{
	FControlRigTrackTokenVector() {}
	FControlRigTrackTokenVector(FVector InValue)
		:Value(InValue)
	{}

	FVector Value;
};

struct FControlRigTrackTokenTransform
{
	FControlRigTrackTokenTransform() {}
	FControlRigTrackTokenTransform(FEulerTransform InValue)
		: Value(InValue)
	{}
	FEulerTransform Value;

};



// Specify a unique runtime type identifier for rig control track tokens
template<> FMovieSceneAnimTypeID GetBlendingDataType<FControlRigTrackTokenFloat>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

template<> FMovieSceneAnimTypeID GetBlendingDataType<FControlRigTrackTokenBool>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

template<> FMovieSceneAnimTypeID GetBlendingDataType<FControlRigTrackTokenVector2D>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

template<> FMovieSceneAnimTypeID GetBlendingDataType<FControlRigTrackTokenVector>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

template<> FMovieSceneAnimTypeID GetBlendingDataType<FControlRigTrackTokenTransform>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}



/** Define working data types for blending calculations  */
template<>  struct TBlendableTokenTraits<FControlRigTrackTokenFloat>
{
	typedef UE::MovieScene::TMaskedBlendable<float, 1> WorkingDataType;
};

template<>  struct TBlendableTokenTraits<FControlRigTrackTokenBool>
{
	typedef UE::MovieScene::TMaskedBlendable<bool, 1> WorkingDataType;
};

template<> struct TBlendableTokenTraits<FControlRigTrackTokenVector2D>
{
	typedef UE::MovieScene::TMaskedBlendable<float, 2> WorkingDataType;
};

template<> struct TBlendableTokenTraits<FControlRigTrackTokenVector>
{
	typedef UE::MovieScene::TMaskedBlendable<float, 3> WorkingDataType;
};

template<>  struct TBlendableTokenTraits<FControlRigTrackTokenTransform>
{
	typedef UE::MovieScene::TMaskedBlendable<float, 9> WorkingDataType;
};




namespace UE
{
namespace MovieScene
{

	void MultiChannelFromData(const FControlRigTrackTokenFloat& In, TMultiChannelValue<float, 1>& Out)
	{
		Out = { In.Value };
	}

	void ResolveChannelsToData(const TMultiChannelValue<float, 1>& In, FControlRigTrackTokenFloat& Out)
	{
		Out.Value = In[0];
	}

	void MultiChannelFromData(const FControlRigTrackTokenBool& In, TMultiChannelValue<bool, 1>& Out)
	{
		Out = { In.Value };
	}

	void ResolveChannelsToData(const TMultiChannelValue<bool, 1>& In, FControlRigTrackTokenBool& Out)
	{
		Out.Value = In[0];
	}

	void MultiChannelFromData(const FControlRigTrackTokenVector2D& In, TMultiChannelValue<float, 2>& Out)
	{
		Out = { In.Value.X, In.Value.Y };
	}

	void ResolveChannelsToData(const TMultiChannelValue<float, 2>& In, FControlRigTrackTokenVector2D& Out)
	{
		Out.Value = FVector2D(In[0], In[1]);
	}

	void MultiChannelFromData(const FControlRigTrackTokenVector& In, TMultiChannelValue<float, 3>& Out)
	{
		Out = { In.Value.X, In.Value.Y, In.Value.Z };
	}

	void ResolveChannelsToData(const TMultiChannelValue<float, 3>& In, FControlRigTrackTokenVector& Out)
	{
		Out.Value = FVector(In[0], In[1], In[2]);
	}

	void MultiChannelFromData(const FControlRigTrackTokenTransform& In, TMultiChannelValue<float, 9>& Out)
	{
		FVector Translation = In.Value.GetLocation();
		FVector Rotation = In.Value.Rotator().Euler();
		FVector Scale = In.Value.GetScale3D();
		Out = { Translation.X, Translation.Y, Translation.Z, Rotation.X, Rotation.Y, Rotation.Z, Scale.X, Scale.Y, Scale.Z };

	}

	void ResolveChannelsToData(const TMultiChannelValue<float, 9>& In, FControlRigTrackTokenTransform& Out)
	{
		Out.Value = FEulerTransform(
			FRotator::MakeFromEuler(FVector(In[3], In[4], In[5])),
			FVector(In[0], In[1], In[2]),
			FVector(In[6], In[7], In[8])
		);
	}

} // namespace MovieScene
} // namespace UE

//since initialization can blow up selection, may need to just reselect, used in a few places
static void SelectControls(UControlRig* ControlRig, TArray<FName>& SelectedNames)
{
	if (ControlRig)
	{
		ControlRig->ClearControlSelection();
		for (const FName& Name : SelectedNames)
		{
			ControlRig->SelectControl(Name, true);
		}
	}
}

void FControlRigBindingHelper::BindToSequencerInstance(UControlRig* ControlRig)
{
	check(ControlRig);
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
	{
		if(SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			bool bWasCreated = false;
			if (UControlRigLayerInstance* AnimInstance = FAnimCustomInstanceHelper::BindToSkeletalMeshComponent<UControlRigLayerInstance>(SkeletalMeshComponent, bWasCreated))
			{
				if (bWasCreated || !AnimInstance->HasControlRigTrack(ControlRig->GetUniqueID()))
				{
					AnimInstance->RecalcRequiredBones();
					AnimInstance->AddControlRigTrack(ControlRig->GetUniqueID(), ControlRig);
					//initialization can blow up selection
					TArray<FName> SelectedControls = ControlRig->CurrentControlSelection();
					ControlRig->Initialize();
					ControlRig->RequestInit();
					ControlRig->SetBoneInitialTransformsFromSkeletalMeshComponent(SkeletalMeshComponent, true);
					ControlRig->Evaluate_AnyThread();
					TArray<FName> NewSelectedControls = ControlRig->CurrentControlSelection();
					if (SelectedControls != NewSelectedControls)
					{
						SelectControls(ControlRig, SelectedControls);
					}
				}
			}
		}
	}
	else if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
	{
		if (ControlRigComponent->GetControlRig() != ControlRig)
		{
			ControlRigComponent->Initialize();
			/*
			Previously with Sequencer and CR Components we would assign the CR to a Component
			that the sequencer was using, in any world. This looks like it was causing issues
			with two worlds running with pre-forward solve events so now we only do that if
			in non-game, and if in game (which includes PIE), we don't re-set the
			CR Component's CR, but instead grab the CR from it and then use that for evaluation.
			*/
			if (ControlRigComponent->GetWorld())
			{
				if (ControlRigComponent->GetWorld()->IsGameWorld() == false)
				{
					ControlRigComponent->SetControlRig(ControlRig);
				}
			}
		}
	}
}

void FControlRigBindingHelper::UnBindFromSequencerInstance(UControlRig* ControlRig)
{
	check(ControlRig);

	if (!ControlRig->IsValidLowLevel() ||
	    ControlRig->HasAnyFlags(RF_BeginDestroyed) ||
		!IsValid(ControlRig))
	{
		return;
	}
	
	if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
	{
		// todo: how do we reset the state?
		//ControlRig->Initialize();
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
	{
		if (!SkeletalMeshComponent->IsValidLowLevel() ||
			SkeletalMeshComponent->HasAnyFlags(RF_BeginDestroyed) ||
			!IsValid(SkeletalMeshComponent))
		{
			return;
		}

		UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(SkeletalMeshComponent->GetAnimInstance());
		if (AnimInstance)
		{
			if (!AnimInstance->IsValidLowLevel() ||
                AnimInstance->HasAnyFlags(RF_BeginDestroyed) ||
                !IsValid(AnimInstance))
			{
				return;
			}

			AnimInstance->ResetNodes();
			AnimInstance->RecalcRequiredBones();
			AnimInstance->RemoveControlRigTrack(ControlRig->GetUniqueID());
		}

		FAnimCustomInstanceHelper::UnbindFromSkeletalMeshComponent< UControlRigLayerInstance>(SkeletalMeshComponent);
	}
}


struct FControlRigParameterPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FControlRigParameterPreAnimatedTokenProducer(FMovieSceneSequenceIDRef InSequenceID)
		: SequenceID(InSequenceID)
	{}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{

		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(FMovieSceneSequenceIDRef InSequenceID)
				: SequenceID(InSequenceID)
			{

			}
			void SetSkelMesh(USkeletalMeshComponent* InComponent)
			{
				SkeletalMeshRestoreState.SaveState(InComponent);
			}

			virtual void RestoreState(UObject& InObject, const UE::MovieScene::FRestoreStateParams& Params) override
			{

				if (UControlRig* ControlRig = Cast<UControlRig>(&InObject))
				{
					if (ControlRig->GetObjectBinding())
					{
						// control rig evaluate critical section: when restoring the state, we can be poking into running instances of Control Rigs
						// on the anim thread so using a lock here to avoid this thread and anim thread both touching the rig
						// at the same time, which can lead to various issues like double-freeing some random array when doing SetControlValue
						// note: the critical section accepts recursive locking so it is ok that we
						// are calling evaluate_anythread later within the same scope
						FScopeLock EvaluateLock(&ControlRig->GetEvaluateMutex());
						
						//Restore control rig first
						const bool bSetupUndo = false;
						if (URigHierarchy* RigHierarchy = ControlRig->GetHierarchy())
						{

							FRigElementKey ControlKey;
							ControlKey.Type = ERigElementType::Control;
							for (FControlSpaceAndValue& SpaceNameAndValue : SpaceValues)
							{
								ControlKey.Name = SpaceNameAndValue.ControlName;
								switch (SpaceNameAndValue.Value.SpaceType)
								{
								case EMovieSceneControlRigSpaceType::Parent:
									RigHierarchy->SwitchToDefaultParent(ControlKey);
									break;
								case EMovieSceneControlRigSpaceType::World:
									RigHierarchy->SwitchToWorldSpace(ControlKey);
									break;
								case EMovieSceneControlRigSpaceType::ControlRig:
								{
#if WITH_EDITOR
									URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
									RigHierarchy->SwitchToParent(ControlKey, SpaceNameAndValue.Value.ControlRigElement, false, true, Dependencies, nullptr);
#else
									RigHierarchy->SwitchToParent(ControlKey, SpaceNameAndValue.Value.ControlRigElement);
#endif
								}
								break;
								}
							}
							

							for (TNameAndValue<float>& Value : ScalarValues)
							{
								if (ControlRig->FindControl(Value.Name))
								{
									ControlRig->SetControlValue<float>(Value.Name, Value.Value, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
								}
							}

							for (TNameAndValue<bool>& Value : BoolValues)
							{
								if (ControlRig->FindControl(Value.Name))
								{
									ControlRig->SetControlValue<bool>(Value.Name, Value.Value, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
								}
							}

							for (TNameAndValue<int32>& Value : IntegerValues)
							{
								if (ControlRig->FindControl(Value.Name))
								{
									ControlRig->SetControlValue<int32>(Value.Name, Value.Value, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
								}
							}
							for (int32 TwiceHack = 0; TwiceHack < 2; ++TwiceHack)
							{
								for (TNameAndValue<FVector2D>& Value : Vector2DValues)
								{
									if (ControlRig->FindControl(Value.Name))
									{
										const FVector3f Vector3(Value.Value.X, Value.Value.Y, 0.f);
										//okay to use vector3 for 2d here
										ControlRig->SetControlValue<FVector3f>(Value.Name, Vector3, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
									}
								}

								for (TNameAndValue<FVector>& Value : VectorValues)
								{
									if (ControlRig->FindControl(Value.Name))
									{
										ControlRig->SetControlValue<FVector3f>(Value.Name, (FVector3f)Value.Value, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
									}
								}

								for (TNameAndValue<FEulerTransform>& Value : TransformValues)
								{
									if (FRigControlElement* ControlElement = ControlRig->FindControl(Value.Name))
									{
										switch (ControlElement->Settings.ControlType)
										{
										case ERigControlType::Transform:
										{
											ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(Value.Name, Value.Value.ToFTransform(), true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
											ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Value.Value.Rotation);
											break;
										}
										case ERigControlType::TransformNoScale:
										{
											FTransformNoScale NoScale = Value.Value.ToFTransform();
											ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(Value.Name, NoScale, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
											ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Value.Value.Rotation);
											break;
										}
										case ERigControlType::EulerTransform:
										{
											FEulerTransform EulerTransform = Value.Value;
											ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(Value.Name, EulerTransform, true, FRigControlModifiedContext(EControlRigSetKey::Never), bSetupUndo);
											break;
										}
										default:
										{
											break;
										}
										}
									}
								}
							}
						}
						//make sure to evaluate the control rig
						ControlRig->Evaluate_AnyThread();

						//unbind instances and reset animbp
						FControlRigBindingHelper::UnBindFromSequencerInstance(ControlRig);

						//do a tick and restore skel mesh
						if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
						{
							// Restore pose after unbinding to force the restored pose
							SkeletalMeshComponent->SetUpdateAnimationInEditor(true);
							SkeletalMeshComponent->SetUpdateClothInEditor(true);
							SkeletalMeshComponent->TickAnimation(0.f, false);

							SkeletalMeshComponent->RefreshBoneTransforms();
							SkeletalMeshComponent->RefreshFollowerComponents();
							SkeletalMeshComponent->UpdateComponentToWorld();
							SkeletalMeshComponent->FinalizeBoneTransform();
							SkeletalMeshComponent->MarkRenderTransformDirty();
							SkeletalMeshComponent->MarkRenderDynamicDataDirty();
							SkeletalMeshRestoreState.RestoreState(SkeletalMeshComponent);
						}
						//only unbind if not a component
						if (Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()) == nullptr)
						{
							ControlRig->GetObjectBinding()->UnbindFromObject();
						}

					}
				}
			}

			FMovieSceneSequenceID SequenceID;
			TArray< FControlSpaceAndValue> SpaceValues;
			TArray< TNameAndValue<float> > ScalarValues;
			TArray< TNameAndValue<bool> > BoolValues;
			TArray< TNameAndValue<int32> > IntegerValues;
			TArray< TNameAndValue<FVector> > VectorValues;
			TArray< TNameAndValue<FVector2D> > Vector2DValues;
			TArray< TNameAndValue<FEulerTransform> > TransformValues;
			FSkeletalMeshRestoreState SkeletalMeshRestoreState;

		};


		FToken Token(SequenceID);

		if (UControlRig* ControlRig = Cast<UControlRig>(&Object))
		{
			TArray<FRigControlElement*> Controls = ControlRig->AvailableControls();
			FRigControlValue Value;
			
			for (FRigControlElement* ControlElement : Controls)
			{
				switch (ControlElement->Settings.ControlType)
				{
					case ERigControlType::Bool:
					{
						const bool Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<bool>();
						Token.BoolValues.Add(TNameAndValue<bool>{ ControlElement->GetName(), Val });
						break;
					}
					case ERigControlType::Integer:
					{
						const int32 Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<int32>();
						Token.IntegerValues.Add(TNameAndValue<int32>{ ControlElement->GetName(), Val });
						break;
					}
					case ERigControlType::Float:
					{
						const float Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<float>();
						Token.ScalarValues.Add(TNameAndValue<float>{ ControlElement->GetName(), Val });
						break;
					}
					case ERigControlType::Vector2D:
					{
						const FVector3f Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
						Token.Vector2DValues.Add(TNameAndValue<FVector2D>{ ControlElement->GetName(), FVector2D(Val.X, Val.Y) });
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Rotator:
					{
						URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
						FMovieSceneControlRigSpaceBaseKey SpaceValue;
						//@helge how can we get the correct current space here? this is for restoring it.
						//for now just using parent space
						//FRigElementKey DefaultParent = RigHierarchy->GetFirstParent(ControlElement->GetKey());
						SpaceValue.ControlRigElement = ControlElement->GetKey();
						SpaceValue.SpaceType = EMovieSceneControlRigSpaceType::Parent;
						Token.SpaceValues.Add(FControlSpaceAndValue(ControlElement->GetName(), SpaceValue));
						const FVector3f Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
						Token.VectorValues.Add(TNameAndValue<FVector>{ ControlElement->GetName(), (FVector)Val });
						//mz todo specify rotator special so we can do quat interps
						break;
					}
					case ERigControlType::Transform:
					{
						FMovieSceneControlRigSpaceBaseKey SpaceValue;
						//@helge how can we get the correct current space here? this is for restoring it.
						//for now just using parent space
						//FRigElementKey DefaultParent = RigHierarchy->GetFirstParent(ControlElement->GetKey());
						SpaceValue.ControlRigElement = ControlElement->GetKey();
						SpaceValue.SpaceType = EMovieSceneControlRigSpaceType::Parent;
						Token.SpaceValues.Add(FControlSpaceAndValue(ControlElement->GetName(), SpaceValue));
						const FTransform Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransform_Float>().ToTransform();
						FEulerTransform EulerTransform(Val);
						EulerTransform.Rotation = ControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);
						Token.TransformValues.Add(TNameAndValue<FEulerTransform>{ ControlElement->GetName(), EulerTransform });
						break;
					}
					case ERigControlType::TransformNoScale:
					{
						const FTransformNoScale NoScale = 
							ControlRig->GetHierarchy()
							->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
						FEulerTransform EulerTransform(NoScale.ToFTransform());
						EulerTransform.Rotation = ControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);
						Token.TransformValues.Add(TNameAndValue<FEulerTransform>{ ControlElement->GetName(), EulerTransform });
						break;
					}
					case ERigControlType::EulerTransform:
					{
						const FEulerTransform EulerTransform = 
							ControlRig->GetHierarchy()
							->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
						Token.TransformValues.Add(TNameAndValue<FEulerTransform>{ ControlElement->GetName(), EulerTransform });
						break;
					}
				}
			}

			if (ControlRig->GetObjectBinding())
			{
				if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
				{
					if (ControlRigComponent->GetControlRig() != ControlRig)
					{
						ControlRigComponent->Initialize();
						/*
						Previously with Sequencer and CR Components we would assign the CR to a Component 
						that the sequencer was using, in any world. This looks like it was causing issues 
						with two worlds running with pre-forward solve events so now we only do that if 
						in non-game, and if in game (which includes PIE), we don't re-set the 
						CR Component's CR, but instead grab the CR from it and then use that for evaluation.
						*/
						if (ControlRigComponent->GetWorld())
						{
							if (ControlRigComponent->GetWorld()->IsGameWorld() == false)
							{
								ControlRigComponent->SetControlRig(ControlRig);
							}
						}
					}
					else
					{
						TArray<FName> SelectedControls = ControlRig->CurrentControlSelection();
						ControlRig->Initialize();
						TArray<FName> NewSelectedControls = ControlRig->CurrentControlSelection();
						if (SelectedControls != NewSelectedControls)
						{
							SelectControls(ControlRig,SelectedControls);
						}
					}
				}
				else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
				{
					Token.SetSkelMesh(SkeletalMeshComponent);
				}
			}
		}

		return MoveTemp(Token);
	}

	FMovieSceneSequenceID SequenceID;
	TArray< FControlSpaceAndValue> SpaceValues;
	TArray< TNameAndValue<bool> > BoolValues;
	TArray< TNameAndValue<int32> > IntegerValues;
	TArray< TNameAndValue<float> > ScalarValues;
	TArray< TNameAndValue<FVector2D> > Vector2DValues;
	TArray< TNameAndValue<FVector> > VectorValues;
	TArray< TNameAndValue<FTransform> > TransformValues;

};

static UControlRig* GetControlRig(const UMovieSceneControlRigParameterSection* Section,UObject* BoundObject)
{
	UControlRig* ControlRig = Section->GetControlRig();
	if (ControlRig->GetObjectBinding())
	{
		if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
		{
			if (AActor* Actor = Cast<AActor>(BoundObject))
			{
				if (UControlRigComponent* NewControlRigComponent = Actor->FindComponentByClass<UControlRigComponent>())
				{
					if (NewControlRigComponent->GetWorld())
					{
						if (NewControlRigComponent->GetWorld()->IsGameWorld())
						{
							ControlRig = NewControlRigComponent->GetControlRig();
							if (!ControlRig)
							{
								NewControlRigComponent->Initialize();
								ControlRig = NewControlRigComponent->GetControlRig();
							}
							if (ControlRig)
							{
								if (ControlRig->GetObjectBinding() == nullptr)
								{
									ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
								}
								if (ControlRig->GetObjectBinding()->GetBoundObject() != NewControlRigComponent)
								{
									ControlRig->GetObjectBinding()->BindToObject(BoundObject);
								}
							}
						}
						else if (NewControlRigComponent != ControlRigComponent)
						{
							NewControlRigComponent->SetControlRig(ControlRig);
						}
					}
				}
			}
		}
		else if (UControlRigComponent* NewControlRigComponent = Cast<UControlRigComponent>(BoundObject))
		{
			if (NewControlRigComponent->GetWorld())
			{
				if (NewControlRigComponent->GetWorld()->IsGameWorld())
				{
					ControlRig = NewControlRigComponent->GetControlRig();
					if (!ControlRig)
					{
						NewControlRigComponent->Initialize();
						ControlRig = NewControlRigComponent->GetControlRig();
					}
					if (ControlRig)
					{
						if (ControlRig->GetObjectBinding() == nullptr)
						{
							ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
						}
						if (ControlRig->GetObjectBinding()->GetBoundObject() != NewControlRigComponent)
						{
							ControlRig->GetObjectBinding()->BindToObject(BoundObject);
						}
					}
				}
				else if (NewControlRigComponent != ControlRigComponent)
				{
					NewControlRigComponent->SetControlRig(ControlRig);
				}
			}
		}
	}
	else
	{
		return nullptr;
	}
	return ControlRig;
}

/* Simple token used for non-blendables*/
struct FControlRigParameterExecutionToken : IMovieSceneExecutionToken
{
	FControlRigParameterExecutionToken(const UMovieSceneControlRigParameterSection* InSection,
		const FEvaluatedControlRigParameterSectionValues& Values)
	:	Section(InSection)
	{
		BoolValues = Values.BoolValues;
		IntegerValues = Values.IntegerValues;
		SpaceValues = Values.SpaceValues;
		ConstraintsValues = Values.ConstraintsValues;
	}
	FControlRigParameterExecutionToken(FControlRigParameterExecutionToken&&) = default;
	FControlRigParameterExecutionToken& operator=(FControlRigParameterExecutionToken&&) = default;

	// Non-copyable
	FControlRigParameterExecutionToken(const FControlRigParameterExecutionToken&) = delete;
	FControlRigParameterExecutionToken& operator=(const FControlRigParameterExecutionToken&) = delete;

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ControlRigParameterTrack_TokenExecute)
		
		FMovieSceneSequenceID SequenceID = Operand.SequenceID;
		UControlRig* ControlRig = Section->GetControlRig();

		// Update the animation's state
		
		if (ControlRig)
		{
			const UMovieSceneSequence* Sequence = Player.State.FindSequence(Operand.SequenceID);
			TArrayView<TWeakObjectPtr<>> BoundObjects = Player.FindBoundObjects(Operand);

			if (Sequence && BoundObjects.Num() > 0 && BoundObjects[0].Get())
			{
				if (!ControlRig->GetObjectBinding())
				{
					ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
				}

				if (!ControlRig->GetObjectBinding()->GetBoundObject())
				{
					ControlRig->GetObjectBinding()->BindToObject(BoundObjects[0].Get());
					TArray<FName> SelectedControls = ControlRig->CurrentControlSelection();
					ControlRig->Initialize();
					if (ControlRig->IsA<UFKControlRig>())
					{
						UMovieSceneControlRigParameterTrack* Track = Section->GetTypedOuter<UMovieSceneControlRigParameterTrack>();
						if (Track)
						{
							Track->ReplaceControlRig(ControlRig, true);
						}
					}
					TArray<FName> NewSelectedControls = ControlRig->CurrentControlSelection();
					if (SelectedControls != NewSelectedControls)
					{
						SelectControls(ControlRig, SelectedControls);
					}
				}
				// ensure that pre animated state is saved, must be done before bind
				Player.SavePreAnimatedState(*ControlRig, FMovieSceneControlRigParameterTemplate::GetAnimTypeID(), FControlRigParameterPreAnimatedTokenProducer(Operand.SequenceID));

				FControlRigBindingHelper::BindToSequencerInstance(ControlRig);

				if (ControlRig->GetObjectBinding())
				{
					if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
					{
						if (UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(SkeletalMeshComponent->GetAnimInstance()))
						{
							float Weight = 1.0f;
							FControlRigIOSettings InputSettings;
							InputSettings.bUpdateCurves = true;
							InputSettings.bUpdatePose = true;
							AnimInstance->UpdateControlRigTrack(ControlRig->GetUniqueID(), Weight, InputSettings, true);
						}
					}
					else
					{
						ControlRig = GetControlRig(Section, BoundObjects[0].Get());
					}
				}
			}		
		}

		//Do Bool straight up no blending
		if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
		{
			bool bWasDoNotKey = false;

			bWasDoNotKey = Section->GetDoNotKey();
			Section->SetDoNotKey(true);

			if (ControlRig)
			{
				const bool bSetupUndo = false;
				ControlRig->SetAbsoluteTime((float)Context.GetFrameRate().AsSeconds(Context.GetTime()));
				for (const FControlSpaceAndValue& SpaceNameAndValue : SpaceValues)
				{
					if (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(SpaceNameAndValue.ControlName))
					{
						if (URigHierarchy* RigHierarchy = ControlRig->GetHierarchy())
						{
							if (FRigControlElement* RigControl = ControlRig->FindControl(SpaceNameAndValue.ControlName))
							{
								const FRigElementKey ControlKey = RigControl->GetKey();

								switch (SpaceNameAndValue.Value.SpaceType)
								{
									case EMovieSceneControlRigSpaceType::Parent:
										RigHierarchy->SwitchToDefaultParent(ControlKey);
										break;
									case EMovieSceneControlRigSpaceType::World:
										RigHierarchy->SwitchToWorldSpace(ControlKey);
										break;
									case EMovieSceneControlRigSpaceType::ControlRig:
										{
#if WITH_EDITOR
											URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
											RigHierarchy->SwitchToParent(ControlKey, SpaceNameAndValue.Value.ControlRigElement, false, true, Dependencies, nullptr);
#else
											RigHierarchy->SwitchToParent(ControlKey, SpaceNameAndValue.Value.ControlRigElement);
#endif	
										}
										break;
								}
							}
						}
						/*
						FRigControlElement* RigControl = ControlRig->FindControl(SpaceNameAndValue.ParameterName);
						if (RigControl && RigControl->Settings.ControlType == ERigControlType::Bool)
						{
							ControlRig->SetControlValue<bool>(SpaceNameAndValue.SpaceNameAndValue, SpaceNameAndValue.Value, true, EControlRigSetKey::Never, bSetupUndo);
						}
						*/
					}
				}


				for (const FBoolParameterStringAndValue& BoolNameAndValue : BoolValues)
				{
					if (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(BoolNameAndValue.ParameterName))
					{
						FRigControlElement* RigControl = ControlRig->FindControl(BoolNameAndValue.ParameterName);
						if (RigControl && RigControl->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
							RigControl->Settings.AnimationType != ERigControlAnimationType::VisualCue
							&& RigControl->Settings.ControlType == ERigControlType::Bool)
						{
							ControlRig->SetControlValue<bool>(BoolNameAndValue.ParameterName, BoolNameAndValue.Value, true, EControlRigSetKey::Never,bSetupUndo);
						}
					}
				}

				for (const FIntegerParameterStringAndValue& IntegerNameAndValue : IntegerValues)
				{
					if (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(IntegerNameAndValue.ParameterName))
					{
						FRigControlElement* RigControl = ControlRig->FindControl(IntegerNameAndValue.ParameterName);
						if (RigControl && RigControl->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
							RigControl->Settings.AnimationType != ERigControlAnimationType::VisualCue
							&& RigControl->Settings.ControlType == ERigControlType::Integer)
						{
							ControlRig->SetControlValue<int32>(IntegerNameAndValue.ParameterName, IntegerNameAndValue.Value, true, EControlRigSetKey::Never,bSetupUndo);
						}
					}
				}

				const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(ControlRig->GetWorld());
				for (FConstraintAndActiveValue& ConstraintValue : ConstraintsValues)
				{
					//if the constraint isn't valid try to make a copy of our spawn copy if it exists and add that
					if (ConstraintValue.Constraint.IsValid() == false && ConstraintValue.SpawnConstraint.IsValid())
					{
						UTickableConstraint* NewOne = Controller.AddConstraintFromCopy(ConstraintValue.SpawnConstraint.Get());
						ConstraintValue.Constraint = NewOne;
						UMovieSceneControlRigParameterSection* NonConstSection = const_cast<UMovieSceneControlRigParameterSection*>(Section);
						NonConstSection->ReplaceConstraint(ConstraintValue.SpawnConstraint.Get()->GetFName(), NewOne);
					}
					if (ConstraintValue.Constraint.IsValid())
					{
						//For Control Rig we may need to explicitly set the control rig
						ConstraintValue.Constraint->ResolveBoundObjects(Operand.SequenceID, Player, ControlRig);
						ConstraintValue.Constraint->SetActive(ConstraintValue.Value);
					}
				}
				//unfortunately for Constraints with ControlRig we need to resolve all Parents also. Don't need to do children since they wil be handled by
				//the channel resolve above
				TArray< TObjectPtr<UTickableConstraint>> Constraints =  Controller.GetAllConstraints();
				for (UTickableConstraint* TickConstraint : Constraints)
				{
					if (UTickableTransformConstraint* TransformConstraint = Cast< UTickableTransformConstraint>(TickConstraint))
					{
						if (TransformConstraint->ParentTRSHandle)
						{
							TransformConstraint->ParentTRSHandle->ResolveBoundObjects(Operand.SequenceID, Player, ControlRig);
						}
					}
				}
			}
			Section->SetDoNotKey(bWasDoNotKey);
		}

	}

	const UMovieSceneControlRigParameterSection* Section;
	/** Array of evaluated bool values */
	TArray<FBoolParameterStringAndValue, TInlineAllocator<2>> BoolValues;
	/** Array of evaluated integer values */
	TArray<FIntegerParameterStringAndValue, TInlineAllocator<2>> IntegerValues;
	/** Array of Space Values*/
	TArray<FControlSpaceAndValue, TInlineAllocator<2>> SpaceValues;
	/** Array of evaluated bool values */
	TArray<FConstraintAndActiveValue, TInlineAllocator<2>> ConstraintsValues;
};


FMovieSceneControlRigParameterTemplate::FMovieSceneControlRigParameterTemplate(
	const UMovieSceneControlRigParameterSection& Section,
	const UMovieSceneControlRigParameterTrack& Track)
	: FMovieSceneParameterSectionTemplate(Section)
	, Enums(Section.GetEnumParameterNamesAndCurves())
	, Integers(Section.GetIntegerParameterNamesAndCurves())
	, Spaces(Section.GetSpaceChannels())
	, Constraints(Section.GetConstraintsChannels())
{}



struct TControlRigParameterActuatorFloat : TMovieSceneBlendingActuator<FControlRigTrackTokenFloat>
{
	TControlRigParameterActuatorFloat(FMovieSceneAnimTypeID& InAnimID, const FName& InParameterName, const UMovieSceneControlRigParameterSection* InSection)
		: TMovieSceneBlendingActuator<FControlRigTrackTokenFloat>(FMovieSceneBlendingActuatorID(InAnimID))
		, ParameterName(InParameterName)
		, SectionData(InSection)
	{}


	FControlRigTrackTokenFloat RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();

		UControlRig* ControlRig = Section ? GetControlRig(Section, InObject) : nullptr;

		if (ControlRig)
		{
			FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
			if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
				ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue
				&& ControlElement->Settings.ControlType == ERigControlType::Float)
			{
				const float Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<float>();
				return FControlRigTrackTokenFloat(Val);
			}

		}
		return FControlRigTrackTokenFloat();
	}

	void Actuate(UObject* InObject, const FControlRigTrackTokenFloat& InFinalValue, const TBlendableTokenStack<FControlRigTrackTokenFloat>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();
		
		bool bWasDoNotKey = false;
		if (Section)
		{
			const bool bSetupUndo = false;
			bWasDoNotKey = Section->GetDoNotKey();
			Section->SetDoNotKey(true);

			UControlRig* ControlRig = GetControlRig(Section, InObject);

			if (ControlRig && (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(ParameterName)))
			{
				FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
				if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
					ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue && ControlElement->Settings.ControlType == ERigControlType::Float)
				{
					ControlRig->SetControlValue<float>(ParameterName, InFinalValue.Value, true, EControlRigSetKey::Never,bSetupUndo);
				}
			}

			Section->SetDoNotKey(bWasDoNotKey);
		}
	}


	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const FControlRigTrackTokenFloat& InValue, const TBlendableTokenStack<FControlRigTrackTokenFloat>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		FFloatInterrogationData Data;
		Data.Val = InValue.Value;
		Data.ParameterName = ParameterName;
		InterrogationData.Add(FFloatInterrogationData(Data), UMovieSceneControlRigParameterSection::GetFloatInterrogationKey());
	}

	FName ParameterName;
	TWeakObjectPtr<const UMovieSceneControlRigParameterSection> SectionData;

};


struct TControlRigParameterActuatorVector2D : TMovieSceneBlendingActuator<FControlRigTrackTokenVector2D>
{
	TControlRigParameterActuatorVector2D(FMovieSceneAnimTypeID& InAnimID,  const FName& InParameterName, const UMovieSceneControlRigParameterSection* InSection)
		: TMovieSceneBlendingActuator<FControlRigTrackTokenVector2D>(FMovieSceneBlendingActuatorID(InAnimID))
		, ParameterName(InParameterName)
		, SectionData(InSection)
	{}



	FControlRigTrackTokenVector2D RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();

		UControlRig* ControlRig = Section ? GetControlRig(Section, InObject) : nullptr;

		if (ControlRig)
		{
			FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
			if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
				ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue
				&& (ControlElement->Settings.ControlType == ERigControlType::Vector2D))
			{
				FVector3f Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
				return FControlRigTrackTokenVector2D(FVector2D(Val.X, Val.Y));
			}
		}
		return FControlRigTrackTokenVector2D();
	}

	void Actuate(UObject* InObject, const FControlRigTrackTokenVector2D& InFinalValue, const TBlendableTokenStack<FControlRigTrackTokenVector2D>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();

		bool bWasDoNotKey = false;
		if (Section)
		{
			const bool bSetupUndo = false;
			bWasDoNotKey = Section->GetDoNotKey();
			Section->SetDoNotKey(true);

			UControlRig* ControlRig = GetControlRig(Section, InObject);


			if (ControlRig && (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(ParameterName)))
			{
				FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
				if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
					ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue
					&& (ControlElement->Settings.ControlType == ERigControlType::Vector2D))
				{
					const FVector3f Value(InFinalValue.Value.X, InFinalValue.Value.Y, 0.f);
					ControlRig->SetControlValue<FVector3f>(ParameterName, Value, true, EControlRigSetKey::Never,bSetupUndo);
				}
			}

			Section->SetDoNotKey(bWasDoNotKey);
		}
	}
	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const FControlRigTrackTokenVector2D& InValue, const TBlendableTokenStack<FControlRigTrackTokenVector2D>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		FVector2DInterrogationData Data;
		Data.Val = InValue.Value;
		Data.ParameterName = ParameterName;
		InterrogationData.Add(FVector2DInterrogationData(Data), UMovieSceneControlRigParameterSection::GetVector2DInterrogationKey());
	}

	FName ParameterName;
	TWeakObjectPtr<const UMovieSceneControlRigParameterSection> SectionData;

};


struct TControlRigParameterActuatorVector : TMovieSceneBlendingActuator<FControlRigTrackTokenVector>
{
	TControlRigParameterActuatorVector(FMovieSceneAnimTypeID& InAnimID, const FName& InParameterName, const UMovieSceneControlRigParameterSection* InSection)
		: TMovieSceneBlendingActuator<FControlRigTrackTokenVector>(FMovieSceneBlendingActuatorID(InAnimID))
		, ParameterName(InParameterName)
		, SectionData(InSection)
	{}



	FControlRigTrackTokenVector RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();

		UControlRig* ControlRig = Section ? GetControlRig(Section, InObject) : nullptr;

		if (ControlRig)
		{
			FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
			
			if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
				ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue
				&& (ControlElement->Settings.ControlType == ERigControlType::Position || ControlElement->Settings.ControlType == ERigControlType::Scale || ControlElement->Settings.ControlType == ERigControlType::Rotator))
			{
				FVector3f Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
				return FControlRigTrackTokenVector((FVector)Val);
			}
		}
		return FControlRigTrackTokenVector();
	}

	void Actuate(UObject* InObject, const FControlRigTrackTokenVector& InFinalValue, const TBlendableTokenStack<FControlRigTrackTokenVector>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();
		
		bool bWasDoNotKey = false;
		if (Section)
		{
			const bool bSetupUndo = false;
			bWasDoNotKey = Section->GetDoNotKey();
			Section->SetDoNotKey(true);
			UControlRig* ControlRig = GetControlRig(Section, InObject);

			if (ControlRig && (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(ParameterName)))
			{
				FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
				if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
					ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue
					&& (ControlElement->Settings.ControlType == ERigControlType::Position || ControlElement->Settings.ControlType == ERigControlType::Scale || ControlElement->Settings.ControlType == ERigControlType::Rotator))
				{
						ControlRig->SetControlValue<FVector3f>(ParameterName, (FVector3f)InFinalValue.Value, true, EControlRigSetKey::Never,bSetupUndo);
				}
			}

			Section->SetDoNotKey(bWasDoNotKey);
		}
	}
	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const FControlRigTrackTokenVector& InValue, const TBlendableTokenStack<FControlRigTrackTokenVector>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		FVectorInterrogationData Data;
		Data.Val = InValue.Value;
		Data.ParameterName = ParameterName;		
		InterrogationData.Add(FVectorInterrogationData(Data), UMovieSceneControlRigParameterSection::GetVectorInterrogationKey());
	}


	FName ParameterName;
	TWeakObjectPtr<const UMovieSceneControlRigParameterSection> SectionData;

};



struct TControlRigParameterActuatorTransform : TMovieSceneBlendingActuator<FControlRigTrackTokenTransform>
{
	TControlRigParameterActuatorTransform(FMovieSceneAnimTypeID& InAnimID, const FName& InParameterName, const UMovieSceneControlRigParameterSection* InSection)
		: TMovieSceneBlendingActuator<FControlRigTrackTokenTransform>(FMovieSceneBlendingActuatorID(InAnimID))
		, ParameterName(InParameterName)
		, SectionData(InSection)
	{}

	FControlRigTrackTokenTransform RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();

		UControlRig* ControlRig = Section ? GetControlRig(Section, InObject) : nullptr;

		if (ControlRig && Section && (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(ParameterName)))
		{
			FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
			if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
				ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue)
			{
				if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::Transform)
				{
					const FTransform Val = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransform_Float>().ToTransform();
					FEulerTransform EulerTransform(Val);
					EulerTransform.Rotation = ControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);
					return FControlRigTrackTokenTransform(EulerTransform);
				}
				else if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
				{
					FTransformNoScale ValNoScale = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
					FTransform Val = ValNoScale;
					FEulerTransform EulerTransform(Val);
					EulerTransform.Rotation = ControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);
					return FControlRigTrackTokenTransform(EulerTransform);
				}
				else if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
				{
					FEulerTransform EulerTransform = ControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
					return FControlRigTrackTokenTransform(EulerTransform);
				}
			}
		}
		return FControlRigTrackTokenTransform();
	}

	void Actuate(UObject* InObject, const FControlRigTrackTokenTransform& InFinalValue, const TBlendableTokenStack<FControlRigTrackTokenTransform>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		const UMovieSceneControlRigParameterSection* Section = SectionData.Get();
		if (Section)
		{
			UMovieSceneTrack * Track = Cast<UMovieSceneTrack>(Section->GetOuter());
			if (Track && Track->GetSectionToKey())
			{
				Section = Cast<const UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());
			}
		}

		bool bWasDoNotKey = false;
		if (Section)
		{
			bWasDoNotKey = Section->GetDoNotKey();
			Section->SetDoNotKey(true);
			const bool bSetupUndo = false;
			UControlRig* ControlRig = GetControlRig(Section, InObject);

			if (ControlRig && (Section->ControlsToSet.Num() == 0 || Section->ControlsToSet.Contains(ParameterName)))
			{
				FRigControlElement* ControlElement = ControlRig->FindControl(ParameterName);
				if (ControlElement && ControlElement->Settings.AnimationType != ERigControlAnimationType::ProxyControl &&
					ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue)
				{
					if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::Transform)
					{
						ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ParameterName, InFinalValue.Value.ToFTransform(), true, EControlRigSetKey::Never, bSetupUndo);
						ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, InFinalValue.Value.Rotator());
					}
					else if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
					{
						const FTransformNoScale NoScale = InFinalValue.Value.ToFTransform();
						ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ParameterName, NoScale, true, EControlRigSetKey::Never, bSetupUndo);
						ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, InFinalValue.Value.Rotator());
					}
					else if (ControlElement && ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
					{
						ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ParameterName, InFinalValue.Value, true, EControlRigSetKey::Never, bSetupUndo);
					}
				}
			}

			Section->SetDoNotKey(bWasDoNotKey);
		}
	}
	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const FControlRigTrackTokenTransform& InValue, const TBlendableTokenStack<FControlRigTrackTokenTransform>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		FEulerTransformInterrogationData Data;
		Data.Val = InValue.Value;
		Data.ParameterName = ParameterName;
		InterrogationData.Add(FEulerTransformInterrogationData(Data), UMovieSceneControlRigParameterSection::GetTransformInterrogationKey());
	}
	FName ParameterName;
	TWeakObjectPtr<const UMovieSceneControlRigParameterSection> SectionData;

};


void FMovieSceneControlRigParameterTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FFrameTime Time = Context.GetTime();

	const UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSourceSection());
	if (Section && Section->GetControlRig())
	{
		FEvaluatedControlRigParameterSectionChannelMasks* ChannelMasks = PersistentData.FindSectionData<FEvaluatedControlRigParameterSectionChannelMasks>();
		if (!ChannelMasks)
		{
			// Naughty const_cast here, but we can't create this inside Initialize because of hotfix restrictions
			// The cast is ok because we actually do not have any threading involved
			ChannelMasks = &const_cast<FPersistentEvaluationData&>(PersistentData).GetOrAddSectionData<FEvaluatedControlRigParameterSectionChannelMasks>();
			ChannelMasks->Initialize(Section, Scalars, Bools, Integers, Enums, Vector2Ds, Vectors, Colors, Transforms);
		}

		UMovieSceneTrack* Track = Cast<UMovieSceneTrack>(Section->GetOuter());
		if (!Track)
		{
			return;
		}


		//Do blended tokens
		FEvaluatedControlRigParameterSectionValues Values;

		HACK_ChannelMasks = ChannelMasks;
		EvaluateCurvesWithMasks(Context, Values);
		HACK_ChannelMasks = nullptr;

		float Weight = EvaluateEasing(Context.GetTime());
		if (EnumHasAllFlags(Section->TransformMask.GetChannels(), EMovieSceneTransformChannel::Weight))
		{
			float ManualWeight = 1.f;
			Section->Weight.Evaluate(Context.GetTime(), ManualWeight);
			Weight *= ManualWeight;
		}

		//Do basic token
		FControlRigParameterExecutionToken ExecutionToken(Section,Values);
		ExecutionTokens.Add(MoveTemp(ExecutionToken));

		FControlRigAnimTypeIDsPtr TypeIDs = FControlRigAnimTypeIDs::Get(Section->GetControlRig());

		for (const FScalarParameterStringAndValue& ScalarNameAndValue : Values.ScalarValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindScalar(ScalarNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!ExecutionTokens.GetBlendingAccumulator().FindActuator< FControlRigTrackTokenFloat>(ActuatorTypeID))
			{
				ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorFloat>(AnimTypeID, ScalarNameAndValue.ParameterName, Section));
			}
			ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FControlRigTrackTokenFloat>(ScalarNameAndValue.Value, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 3> VectorData;
		for (const FVectorParameterStringAndValue& VectorNameAndValue : Values.VectorValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindVector(VectorNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!ExecutionTokens.GetBlendingAccumulator().FindActuator< FControlRigTrackTokenVector>(ActuatorTypeID))
			{
				ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorVector>(AnimTypeID,  VectorNameAndValue.ParameterName,Section));
			}
			VectorData.Set(0, VectorNameAndValue.Value.X);
			VectorData.Set(1, VectorNameAndValue.Value.Y);
			VectorData.Set(2, VectorNameAndValue.Value.Z);

			ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FControlRigTrackTokenVector>(VectorData, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 2> Vector2DData;
		for (const FVector2DParameterStringAndValue& Vector2DNameAndValue : Values.Vector2DValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindVector2D(Vector2DNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!ExecutionTokens.GetBlendingAccumulator().FindActuator< FControlRigTrackTokenVector2D>(ActuatorTypeID))
			{
				ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorVector2D>(AnimTypeID,  Vector2DNameAndValue.ParameterName, Section));
			}
			Vector2DData.Set(0, Vector2DNameAndValue.Value.X);
			Vector2DData.Set(1, Vector2DNameAndValue.Value.Y);

			ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FControlRigTrackTokenVector2D>(Vector2DData, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 9> TransformData;
		for (const FEulerTransformParameterStringAndValue& TransformNameAndValue : Values.TransformValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindTransform(TransformNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!ExecutionTokens.GetBlendingAccumulator().FindActuator< FControlRigTrackTokenTransform>(ActuatorTypeID))
			{
				ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorTransform>(AnimTypeID,  TransformNameAndValue.ParameterName, Section));
			}

			const FEulerTransform& Transform = TransformNameAndValue.Transform;

			TransformData.Set(0, Transform.Location.X);
			TransformData.Set(1, Transform.Location.Y);
			TransformData.Set(2, Transform.Location.Z);

			TransformData.Set(3, Transform.Rotation.Roll);
			TransformData.Set(4, Transform.Rotation.Pitch);
			TransformData.Set(5, Transform.Rotation.Yaw);

			TransformData.Set(6, Transform.Scale.X);
			TransformData.Set(7, Transform.Scale.Y);
			TransformData.Set(8, Transform.Scale.Z);
			ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FControlRigTrackTokenTransform>(TransformData, Section->GetBlendType().Get(), Weight));
		}

	}
}


void FMovieSceneControlRigParameterTemplate::EvaluateCurvesWithMasks(const FMovieSceneContext& Context, FEvaluatedControlRigParameterSectionValues& Values) const
{
	const FFrameTime Time = Context.GetTime();

	check(HACK_ChannelMasks);


	const UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSourceSection());
	if (Section)
	{
		// Reserve the value arrays to avoid re-allocation
		Values.ScalarValues.Reserve(Scalars.Num());
		Values.BoolValues.Reserve(Bools.Num());
		Values.SpaceValues.Reserve(Spaces.Num());
		Values.IntegerValues.Reserve(Integers.Num() + Enums.Num()); // Both enums and integers output to the integer value array
		Values.Vector2DValues.Reserve(Vector2Ds.Num());
		Values.VectorValues.Reserve(Vectors.Num());
		Values.ColorValues.Reserve(Colors.Num());
		Values.TransformValues.Reserve(Transforms.Num());
		Values.ConstraintsValues.Reserve(Constraints.Num());

		// Populate each of the output arrays in turn
		for (int32 Index = 0; Index < Scalars.Num(); ++Index)
		{
			const FScalarParameterNameAndCurve& Scalar = this->Scalars[Index];
			float Value = 0;

			if (HACK_ChannelMasks->ScalarCurveMask[Index])
			{
				Scalar.ParameterCurve.Evaluate(Time, Value);
			}
			else
			{
				Value = (Section->GetBlendType().Get() == EMovieSceneBlendType::Additive
					|| Scalar.ParameterCurve.GetDefault().IsSet() == false) ? 0.0f :
					Scalar.ParameterCurve.GetDefault().GetValue();
			}

			Values.ScalarValues.Emplace(Scalar.ParameterName, Value);
		}

		// when playing animation, instead of scrubbing/stepping thru frames, InTime might have a subframe of 0.999928
		// leading to a decimal value of 24399.999928 (for example). This results in evaluating one frame less than expected
		// (24399 instead of 24400) and leads to spaces and constraints switching parents/state after the control changes
		// its transform. (float/double channels will interpolate to a value pretty close to the one at 24400 as its based
		// on that 0.999928 subframe value.
		const FFrameTime RoundTime = Time.RoundToFrame();
		for (int32 Index = 0; Index < Spaces.Num(); ++Index)
		{
			FMovieSceneControlRigSpaceBaseKey Value;
			const FSpaceControlNameAndChannel& Space = Spaces[Index];
			Space.SpaceCurve.Evaluate(RoundTime, Value);
			
			Values.SpaceValues.Emplace(Space.ControlName, Value);
		}

		for (int32 Index = 0; Index < Constraints.Num(); ++Index)
		{
			bool Value = false;
			const FConstraintAndActiveChannel& ConstraintAndActiveChannel = Constraints[Index];
			ConstraintAndActiveChannel.ActiveChannel.Evaluate(RoundTime, Value);
			if (ConstraintAndActiveChannel.Constraint.IsPending())
			{
				ConstraintAndActiveChannel.Constraint.LoadSynchronous();
			}
			Values.ConstraintsValues.Emplace(ConstraintAndActiveChannel.Constraint.Get(), ConstraintAndActiveChannel.ConstraintCopyToSpawn, Value);
		}
		
		for (int32 Index = 0; Index < Bools.Num(); ++Index)
		{
			bool Value = false;
			const FBoolParameterNameAndCurve& Bool = Bools[Index];
			if (HACK_ChannelMasks->BoolCurveMask[Index])
			{
				Bool.ParameterCurve.Evaluate(Time, Value);
			}
			else
			{
				Value = (Section->GetBlendType().Get() == EMovieSceneBlendType::Additive
					|| Bool.ParameterCurve.GetDefault().IsSet() == false) ? false :
					Bool.ParameterCurve.GetDefault().GetValue();
			}

			Values.BoolValues.Emplace(Bool.ParameterName, Value);
		}
		for (int32 Index = 0; Index < Integers.Num(); ++Index)
		{
			int32  Value = 0;
			const FIntegerParameterNameAndCurve& Integer = Integers[Index];
			if (HACK_ChannelMasks->IntegerCurveMask[Index])
			{
				Integer.ParameterCurve.Evaluate(Time, Value);
			}
			else
			{
				Value = (Section->GetBlendType().Get() == EMovieSceneBlendType::Additive
					|| Integer.ParameterCurve.GetDefault().IsSet() == false) ? 0 :
					Integer.ParameterCurve.GetDefault().GetValue();
			}

			Values.IntegerValues.Emplace(Integer.ParameterName, Value);
		}
		for (int32 Index = 0; Index < Enums.Num(); ++Index)
		{
			uint8  Value = 0;
			const FEnumParameterNameAndCurve& Enum = Enums[Index];
			if (HACK_ChannelMasks->EnumCurveMask[Index])
			{
				Enum.ParameterCurve.Evaluate(Time, Value);
			}
			else
			{
				Value = (Section->GetBlendType().Get() == EMovieSceneBlendType::Additive
					|| Enum.ParameterCurve.GetDefault().IsSet() == false) ? 0 :
					Enum.ParameterCurve.GetDefault().GetValue();

			}
			Values.IntegerValues.Emplace(Enum.ParameterName, (int32)Value);

		}
		for (int32 Index = 0; Index < Vector2Ds.Num(); ++Index)
		{
			FVector2f Value(ForceInitToZero);
			const FVector2DParameterNameAndCurves& Vector2D = Vector2Ds[Index];

			if (HACK_ChannelMasks->Vector2DCurveMask[Index])
			{
				Vector2D.XCurve.Evaluate(Time, Value.X);
				Vector2D.YCurve.Evaluate(Time, Value.Y);
			}
			else
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{
					if (Vector2D.XCurve.GetDefault().IsSet())
					{
						Value.X = Vector2D.XCurve.GetDefault().GetValue();
					}
					if (Vector2D.YCurve.GetDefault().IsSet())
					{
						Value.Y = Vector2D.YCurve.GetDefault().GetValue();
					}
				}
			}

			Values.Vector2DValues.Emplace(Vector2D.ParameterName, FVector2D(Value));
		}

		for (int32 Index = 0; Index < Vectors.Num(); ++Index)
		{
			FVector3f Value(ForceInitToZero);
			const FVectorParameterNameAndCurves& Vector = Vectors[Index];

			if (HACK_ChannelMasks->VectorCurveMask[Index])
			{
				Vector.XCurve.Evaluate(Time, Value.X);
				Vector.YCurve.Evaluate(Time, Value.Y);
				Vector.ZCurve.Evaluate(Time, Value.Z);
			}
			else
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{
					if (Vector.XCurve.GetDefault().IsSet())
					{
						Value.X = Vector.XCurve.GetDefault().GetValue();
					}
					if (Vector.YCurve.GetDefault().IsSet())
					{
						Value.Y = Vector.YCurve.GetDefault().GetValue();
					}
					if (Vector.ZCurve.GetDefault().IsSet())
					{
						Value.Z = Vector.ZCurve.GetDefault().GetValue();
					}
				}
			}

			Values.VectorValues.Emplace(Vector.ParameterName, (FVector)Value);
		}
		for (int32 Index = 0; Index < Colors.Num(); ++Index)
		{
			FLinearColor ColorValue = FLinearColor::White;
			const FColorParameterNameAndCurves& Color = Colors[Index];
			if (HACK_ChannelMasks->ColorCurveMask[Index])
			{
				Color.RedCurve.Evaluate(Time, ColorValue.R);
				Color.GreenCurve.Evaluate(Time, ColorValue.G);
				Color.BlueCurve.Evaluate(Time, ColorValue.B);
				Color.AlphaCurve.Evaluate(Time, ColorValue.A);
			}
			else
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{
					if (Color.RedCurve.GetDefault().IsSet())
					{
						ColorValue.R = Color.RedCurve.GetDefault().GetValue();
					}
					if (Color.GreenCurve.GetDefault().IsSet())
					{
						ColorValue.G = Color.GreenCurve.GetDefault().GetValue();
					}
					if (Color.BlueCurve.GetDefault().IsSet())
					{
						ColorValue.B = Color.BlueCurve.GetDefault().GetValue();
					}
					if (Color.AlphaCurve.GetDefault().IsSet())
					{
						ColorValue.A = Color.AlphaCurve.GetDefault().GetValue();
					}
				}
			}

			Values.ColorValues.Emplace(Color.ParameterName, ColorValue);
		}

		EMovieSceneTransformChannel ChannelMask = Section->GetTransformMask().GetChannels();
		for (int32 Index = 0; Index < Transforms.Num(); ++Index)
		{
			FVector3f Translation(ForceInitToZero), Scale(FVector3f::OneVector);
			if (Section->GetBlendType().Get() == EMovieSceneBlendType::Additive)
			{
				Scale = FVector3f(0.0f, 0.0f, 0.0f);
			}

			FRotator3f Rotator(0.0f, 0.0f, 0.0f);

			const FTransformParameterNameAndCurves& Transform = Transforms[Index];
			if (HACK_ChannelMasks->TransformCurveMask[Index])
			{
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::TranslationX))
				{
					Transform.Translation[0].Evaluate(Time, Translation[0]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Translation[0].GetDefault().IsSet())
					{
						Translation[0] = Transform.Translation[0].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::TranslationY))
				{
					Transform.Translation[1].Evaluate(Time, Translation[1]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Translation[1].GetDefault().IsSet())
					{
						Translation[1] = Transform.Translation[1].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::TranslationZ))
				{
					Transform.Translation[2].Evaluate(Time, Translation[2]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Translation[2].GetDefault().IsSet())
					{
						Translation[2] = Transform.Translation[2].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationX))
				{
					Transform.Rotation[0].Evaluate(Time, Rotator.Roll);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Rotation[0].GetDefault().IsSet())
					{
						Rotator.Roll = Transform.Rotation[0].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationY))
				{
					Transform.Rotation[1].Evaluate(Time, Rotator.Pitch);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Rotation[1].GetDefault().IsSet())
					{
						Rotator.Pitch = Transform.Rotation[1].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationZ))
				{
					Transform.Rotation[2].Evaluate(Time, Rotator.Yaw);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Rotation[2].GetDefault().IsSet())
					{
						Rotator.Yaw = Transform.Rotation[2].GetDefault().GetValue();
					}
				}
				//mz todo quat interp...
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::ScaleX))
				{
					Transform.Scale[0].Evaluate(Time, Scale[0]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Scale[0].GetDefault().IsSet())
					{
						Scale[0] = Transform.Scale[0].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::ScaleY))
				{
					Transform.Scale[1].Evaluate(Time, Scale[1]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Scale[1].GetDefault().IsSet())
					{
						Scale[1] = Transform.Scale[1].GetDefault().GetValue();
					}
				}
				if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::ScaleZ))
				{
					Transform.Scale[2].Evaluate(Time, Scale[2]);
				}
				else
				{
					if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive && Transform.Scale[2].GetDefault().IsSet())
					{
						Scale[2] = Transform.Scale[2].GetDefault().GetValue();
					}
				}
			}
			else //completely masked use default or zeroed, which is already set if additive
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{

					if (Transform.Translation[0].GetDefault().IsSet())
					{
						Translation[0] = Transform.Translation[0].GetDefault().GetValue();
					}
					if (Transform.Translation[1].GetDefault().IsSet())
					{
						Translation[1] = Transform.Translation[1].GetDefault().GetValue();
					}
					if (Transform.Translation[2].GetDefault().IsSet())
					{
						Translation[2] = Transform.Translation[2].GetDefault().GetValue();
					}

					if (Transform.Rotation[0].GetDefault().IsSet())
					{
						Rotator.Roll = Transform.Rotation[0].GetDefault().GetValue();
					}

					if (Transform.Rotation[1].GetDefault().IsSet())
					{
						Rotator.Pitch = Transform.Rotation[1].GetDefault().GetValue();
					}

					if (Transform.Rotation[2].GetDefault().IsSet())
					{
						Rotator.Yaw = Transform.Rotation[2].GetDefault().GetValue();
					}

					if (Transform.Scale[0].GetDefault().IsSet())
					{
						Scale[0] = Transform.Scale[0].GetDefault().GetValue();
					}

					if (Transform.Scale[1].GetDefault().IsSet())
					{
						Scale[1] = Transform.Scale[1].GetDefault().GetValue();
					}

					if (Transform.Scale[2].GetDefault().IsSet())
					{
						Scale[2] = Transform.Scale[2].GetDefault().GetValue();
					}
				}
			}
			FEulerTransformParameterStringAndValue NameAndValue(Transform.ParameterName, FEulerTransform(FRotator(Rotator), (FVector)Translation, (FVector)Scale));
			Values.TransformValues.Emplace(NameAndValue);
		}
	}
}


FMovieSceneAnimTypeID FMovieSceneControlRigParameterTemplate::GetAnimTypeID()
{
	return TMovieSceneAnimTypeID<FMovieSceneControlRigParameterTemplate>();
}


void FMovieSceneControlRigParameterTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ControlRigTemplateParameter_Evaluate)

	const FFrameTime Time = Context.GetTime();

	const UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSourceSection());
	if (Section && Section->GetControlRig() && MovieSceneHelpers::IsSectionKeyable(Section))
	{
		FEvaluatedControlRigParameterSectionChannelMasks ChannelMasks;
		ChannelMasks.Initialize(Section, Scalars, Bools, Integers, Enums, Vector2Ds, Vectors, Colors, Transforms);

		//Do blended tokens
		FEvaluatedControlRigParameterSectionValues Values;

		HACK_ChannelMasks = &ChannelMasks;
		EvaluateCurvesWithMasks(Context, Values);
		HACK_ChannelMasks = nullptr;

		FControlRigAnimTypeIDsPtr TypeIDs = FControlRigAnimTypeIDs::Get(Section->GetControlRig());

		float Weight = 1.f;

		//float Weight = EvaluateEasing(Context.GetTime());
	//if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	//{
	//	float ManualWeight = 1.f;
	//	TemplateData.ManualWeight.Evaluate(Context.GetTime(), ManualWeight);
	//	Weight *= ManualWeight;
	//}


		for (const FScalarParameterStringAndValue& ScalarNameAndValue : Values.ScalarValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindScalar(ScalarNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!Container.GetAccumulator().FindActuator< FControlRigTrackTokenFloat>(ActuatorTypeID))
			{
				Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorFloat>(AnimTypeID, ScalarNameAndValue.ParameterName, Section));
			}
			Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context,TBlendableToken<FControlRigTrackTokenFloat>(ScalarNameAndValue.Value, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 2> Vector2DData;
		for (const FVector2DParameterStringAndValue& Vector2DNameAndValue : Values.Vector2DValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindVector2D(Vector2DNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!Container.GetAccumulator().FindActuator< FControlRigTrackTokenVector>(ActuatorTypeID))
			{
				Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorVector2D>(AnimTypeID, Vector2DNameAndValue.ParameterName, Section));
			}
			Vector2DData.Set(0, Vector2DNameAndValue.Value.X);
			Vector2DData.Set(1, Vector2DNameAndValue.Value.Y);

			Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<FControlRigTrackTokenVector2D>(Vector2DData, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 3> VectorData;
		for (const FVectorParameterStringAndValue& VectorNameAndValue : Values.VectorValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindVector(VectorNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!Container.GetAccumulator().FindActuator< FControlRigTrackTokenVector>(ActuatorTypeID))
			{
				Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorVector>(AnimTypeID, VectorNameAndValue.ParameterName, Section));
			}
			VectorData.Set(0, VectorNameAndValue.Value.X);
			VectorData.Set(1, VectorNameAndValue.Value.Y);
			VectorData.Set(2, VectorNameAndValue.Value.Z);

			Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(),Context,TBlendableToken<FControlRigTrackTokenVector>(VectorData, Section->GetBlendType().Get(), Weight));
		}

		UE::MovieScene::TMultiChannelValue<float, 9> TransformData;
		for (const FEulerTransformParameterStringAndValue& TransformNameAndValue : Values.TransformValues)
		{
			FMovieSceneAnimTypeID AnimTypeID = TypeIDs->FindTransform(TransformNameAndValue.ParameterName);
			FMovieSceneBlendingActuatorID ActuatorTypeID(AnimTypeID);

			if (!Container.GetAccumulator().FindActuator< FControlRigTrackTokenTransform>(ActuatorTypeID))
			{
				Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared <TControlRigParameterActuatorTransform>(AnimTypeID, TransformNameAndValue.ParameterName, Section));
			}

			const FEulerTransform& Transform = TransformNameAndValue.Transform;

			TransformData.Set(0, Transform.Location.X);
			TransformData.Set(1, Transform.Location.Y);
			TransformData.Set(2, Transform.Location.Z);

			TransformData.Set(3, Transform.Rotation.Roll);
			TransformData.Set(4, Transform.Rotation.Pitch);
			TransformData.Set(5, Transform.Rotation.Yaw);

			TransformData.Set(6, Transform.Scale.X);
			TransformData.Set(7, Transform.Scale.Y);
			TransformData.Set(8, Transform.Scale.Z);
			Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(),ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<FControlRigTrackTokenTransform>(TransformData, Section->GetBlendType().Get(), Weight));
		}

	}
}

