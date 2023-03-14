// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/SmartName.h"
#include "Animation/Skeleton.h"
#include "Curves/RichCurve.h"
#include "Misc/EnumRange.h"
#include "AnimCurveTypes.generated.h"

/** This is curve flags that are saved in asset and **/
UENUM(BlueprintType, meta=(Bitflags))
enum EAnimAssetCurveFlags
{
	AACF_NONE = 0 UMETA(Hidden),
	// Used as morph target curve
	AACF_DriveMorphTarget_DEPRECATED = 0x00000001 UMETA(Hidden), // This has moved to FAnimCurveType:bMorphTarget. Set per skeleton. DO NOT REMOVE UNTIL FrameworkObjectVersion.MoveCurveTypesToSkeleton expires.
	// Used as triggering event
	AACF_DriveAttribute_DEPRECATED = 0x00000002 UMETA(Hidden), // Set per skeleton. DO NOT REMOVE UNTIL FrameworkObjectVersion.MoveCurveTypesToSkeleton expires.
	// Is editable in Sequence Editor
	AACF_Editable						= 0x00000004 UMETA(DisplayName = "Editable"), // per asset
	// Used as a material curve
	AACF_DriveMaterial_DEPRECATED = 0x00000008 UMETA(Hidden), // This has moved to FAnimCurveType:bMaterial. Set per skeleton. DO NOT REMOVE UNTIL FrameworkObjectVersion.MoveCurveTypesToSkeleton expires.
	// Is a metadata 'curve'
	AACF_Metadata						= 0x00000010 UMETA(DisplayName = "Metadata"), // per asset
	// motifies bone track
	AACF_DriveTrack = 0x00000020 UMETA(Hidden), // @Todo: remove?
	// disabled, right now it's used by track
	AACF_Disabled = 0x00000040 UMETA(Hidden), // per asset
};
static const EAnimAssetCurveFlags AACF_DefaultCurve = AACF_Editable;

ENUM_RANGE_BY_FIRST_AND_LAST(EAnimAssetCurveFlags, AACF_DriveMorphTarget_DEPRECATED, AACF_Disabled);

/** UI Curve Parameter type
 * This gets name, and cached UID and use it when needed
 * Also it contains curve types 
 */
USTRUCT()
struct ENGINE_API FAnimCurveParam
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = FAnimCurveParam)
	FName Name;

	// name UID for fast access
	SmartName::UID_Type UID;

	FAnimCurveParam()
		: UID(SmartName::MaxUID)
	{}

	// initialize
	void Initialize(USkeleton* Skeleton);

	// this doesn't check CurveType flag
	// because it's possible you don't care about your curve types
	bool IsValid() const
	{
		return UID != SmartName::MaxUID;
	}

	bool IsValidToEvaluate() const
	{
		return IsValid();
	}
};
/**
 * Float curve data for one track
 */
USTRUCT(BlueprintType)
struct ENGINE_API FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	// Last observed name of the curve. We store this so we can recover from situations that
	// mean the skeleton doesn't have a mapped name for our UID (such as a user saving the an
	// animation but not the skeleton).
	UPROPERTY()
	FName		LastObservedName_DEPRECATED;

	UPROPERTY()
	FSmartName	Name;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FLinearColor Color;
#endif

private:
	// this flag is mostly used by editor only now
	// however I can't remove this to editor only because 
	// we need DEPRECATED Flag to be loaded in game
	// because those data are stored in asset, and skeleton might not be saved with it. 
	/** Curve Type Flags */
	UPROPERTY()
	int32		CurveTypeFlags;

public:
	FAnimCurveBase() 
		: CurveTypeFlags(0)
	{
#if WITH_EDITORONLY_DATA
		Color = MakeColor();
#endif
	}

	FAnimCurveBase(FSmartName InName, int32 InCurveTypeFlags)
		: Name(InName)
		, CurveTypeFlags(InCurveTypeFlags)
	{
#if WITH_EDITORONLY_DATA
		Color = MakeColor();
#endif
	}

	// To be able to use typedef'd types we need to serialize manually
	void PostSerialize(FArchive& Ar);

	/**
	 * Set InFlag to bValue
	 */
	void SetCurveTypeFlag(EAnimAssetCurveFlags InFlag, bool bValue);

	/**
	 * Toggle the value of the specified flag
	 */
	void ToggleCurveTypeFlag(EAnimAssetCurveFlags InFlag);

	/**
	 * Return true if InFlag is set, false otherwise 
	 */
	bool GetCurveTypeFlag(EAnimAssetCurveFlags InFlag) const;

	/**
	 * Set CurveTypeFlags to NewCurveTypeFlags
	 * This just overwrites CurveTypeFlags
	 */
	void SetCurveTypeFlags(int32 NewCurveTypeFlags);
	/** 
	 * returns CurveTypeFlags
	 */
	int32 GetCurveTypeFlags() const;

#if WITH_EDITORONLY_DATA
	/** Get the color used to display this curve in the editor */
	FLinearColor GetColor() const { return Color; }

private:
	/** Make an initial color */
	FLinearColor MakeColor();
#endif
};

USTRUCT(BlueprintType)
struct ENGINE_API FFloatCurve : public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve data for float. */
	UPROPERTY()
	FRichCurve	FloatCurve;

	FFloatCurve(){}

	FFloatCurve(FSmartName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(const FFloatCurve& SourceCurve);
	float Evaluate(float CurrentTime) const;
	void UpdateOrAddKey(float NewKey, float CurrentTime);
	void GetKeys(TArray<float>& OutTimes, TArray<float>& OutValues) const;
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
};

USTRUCT()
struct ENGINE_API FVectorCurve : public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	enum class EIndex
	{
		X = 0, 
		Y, 
		Z, 
		Max
	};

	/** Curve data for float. */
	UPROPERTY()
	FRichCurve	FloatCurves[3];

	FVectorCurve(){}

	FVectorCurve(FSmartName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(const FVectorCurve& SourceCurve);
	FVector Evaluate(float CurrentTime, float BlendWeight) const;
	void UpdateOrAddKey(const FVector& NewKey, float CurrentTime);
	void GetKeys(TArray<float>& OutTimes, TArray<FVector>& OutValues) const;
	bool DoesContainKey() const { return (FloatCurves[0].GetNumKeys() > 0 || FloatCurves[1].GetNumKeys() > 0 || FloatCurves[2].GetNumKeys() > 0);}
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
	int32 GetNumKeys() const;
};

USTRUCT(BlueprintType)
struct ENGINE_API FTransformCurve: public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve data for each transform. */
	UPROPERTY()
	FVectorCurve	TranslationCurve;

	/** Rotation curve - right now we use euler because quat also doesn't provide linear interpolation - curve editor can't handle quat interpolation
	 * If you hit gimbal lock, you should add extra key to fix it. This will cause gimbal lock. 
	 * @TODO: Eventually we'll need FRotationCurve that would contain rotation curve - that will interpolate as slerp or as quaternion 
	 */
	UPROPERTY()
	FVectorCurve	RotationCurve;

	UPROPERTY()
	FVectorCurve	ScaleCurve;

	FTransformCurve(){}

	FTransformCurve(FSmartName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(const FTransformCurve& SourceCurve);
	FTransform Evaluate(float CurrentTime, float BlendWeight) const;
	void UpdateOrAddKey(const FTransform& NewKey, float CurrentTime);
	void GetKeys(TArray<float>& OutTimes, TArray<FTransform>& OutValues) const;
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
	
	const FVectorCurve* GetVectorCurveByIndex(int32 Index) const;
	FVectorCurve* GetVectorCurveByIndex(int32 Index);
};

USTRUCT(BlueprintType)
struct ENGINE_API FCachedFloatCurve
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve Settings")
	FName CurveName;

private:
	mutable USkeleton::AnimCurveUID CachedUID;
	mutable FName CachedCurveName;

public:
	FCachedFloatCurve()
		: CachedUID(SmartName::MaxUID)
	{}

	bool IsValid(const UAnimSequenceBase* InAnimSequence) const;
	float GetValueAtPosition(const UAnimSequenceBase* InAnimSequence, const float& InPosition) const;
	const FFloatCurve* GetFloatCurve(const UAnimSequenceBase* InAnimSequence) const;

protected:
	USkeleton::AnimCurveUID GetAnimCurveUID(const UAnimSequenceBase* InAnimSequence) const;
};

/**
* This is array of curves that run when collecting curves natively 
*/
struct FCurveElement
{
	/** Curve Value */
	float					Value;
	/** Whether this value is set or not */
	bool					bValid;

	FCurveElement(float InValue)
		:  Value(InValue)
		,  bValid (true)
	{}

	FCurveElement()
		: Value(0.f)
		, bValid(false)
	{}

	bool IsValid() const 
	{
		return bValid;
	}

	void SetValue(float InValue)
	{
		Value = InValue;
		bValid = true;
	}
};

/**
 * This struct is used to create curve snap shot of current time when extracted
 */
template <typename InAllocator>
struct FBaseBlendedCurve
{
	typedef InAllocator   Allocator;
	/**
	* List of curve weights for this pose
	*/
	TArray<float, Allocator> CurveWeights;

	/**
	 * A bitmask to indicate which weights are valid.
	 */
	TBitArray<Allocator> ValidCurveWeights;

	/**
	* UID to array index look up table for Elements
	* This eliminates the look up cost
	*/
	TArray<uint16> const * UIDToArrayIndexLUT;

	/** 
	 * Valid Curve count 
	 * This should match Elements.Num() 
	 */
	uint16 NumValidCurveCount;

	/**
	 * constructor
	 */
	FBaseBlendedCurve()
		: UIDToArrayIndexLUT(nullptr)
		, NumValidCurveCount(0)
		, bInitialized(false)
	{
	}

	/** Initialize Curve Data from following data */
	void InitFrom(const FBoneContainer& RequiredBones)
	{
		UIDToArrayIndexLUT = &RequiredBones.GetUIDToArrayLookupTable();
		NumValidCurveCount = (uint16)RequiredBones.GetUIDToArrayIndexLookupTableValidCount();
		CurveWeights.Reset();
		CurveWeights.AddZeroed(NumValidCurveCount);
		ValidCurveWeights.Init(false, NumValidCurveCount);
		// no name, means no curve
		bInitialized = true;
	}

	void InitFrom(TArray<uint16> const * InUIDToArrayIndexLUT)
	{
		check(InUIDToArrayIndexLUT != nullptr);
		UIDToArrayIndexLUT = InUIDToArrayIndexLUT;
		NumValidCurveCount = (uint16)GetValidElementCount(UIDToArrayIndexLUT);
		CurveWeights.Reset();
		CurveWeights.AddZeroed(NumValidCurveCount);
		ValidCurveWeights.Init(false, NumValidCurveCount);
		// no name, means no curve
		bInitialized = true;
	}

	template <typename OtherAllocator>
	void InitFrom(const FBaseBlendedCurve<OtherAllocator>& InCurveToInitFrom)
	{
		// make sure this doesn't happen
		check(InCurveToInitFrom.UIDToArrayIndexLUT != nullptr);
		UIDToArrayIndexLUT = InCurveToInitFrom.UIDToArrayIndexLUT;
		NumValidCurveCount = InCurveToInitFrom.NumValidCurveCount;

		CurveWeights.Reset();
		CurveWeights.AddZeroed(NumValidCurveCount);
		ValidCurveWeights.Init(false, NumValidCurveCount);
		bInitialized = true;
	}

	void InitFrom(const FBaseBlendedCurve<Allocator>& InCurveToInitFrom)
	{
		// make sure this doesn't happen
		if (ensure(&InCurveToInitFrom != this))
		{
			check(InCurveToInitFrom.UIDToArrayIndexLUT != nullptr);
			UIDToArrayIndexLUT = InCurveToInitFrom.UIDToArrayIndexLUT;
			NumValidCurveCount = InCurveToInitFrom.NumValidCurveCount;
			CurveWeights.Reset();
			CurveWeights.AddZeroed(NumValidCurveCount);
			ValidCurveWeights.Init(false, NumValidCurveCount);
			bInitialized = true;
		}
	}

	/** Invalidate value of InUID */
	void InvalidateCurveWeight(USkeleton::AnimCurveUID InUid)
	{
		check(bInitialized);

		int32 ArrayIndex = GetArrayIndexByUID(InUid);
		if (ArrayIndex != INDEX_NONE)
		{
			CurveWeights[ArrayIndex] = 0.f;
			ValidCurveWeights[ArrayIndex] = false;
		}
	}

	/** Set value of InUID to InValue */
	void Set(USkeleton::AnimCurveUID InUid, float InValue)
	{
		check(bInitialized);

		int32 ArrayIndex = GetArrayIndexByUID(InUid);
		if (ArrayIndex != INDEX_NONE)
		{
			CurveWeights[ArrayIndex] = InValue;
			ValidCurveWeights[ArrayIndex] = true;
		}
	}

	/** Get Value of InUID - @todo : add validation check here and make sure caller also knows it's not valid*/
	float Get(USkeleton::AnimCurveUID InUid) const
	{
		check(bInitialized);

		int32 ArrayIndex = GetArrayIndexByUID(InUid);
		if (ArrayIndex != INDEX_NONE)
		{
			return CurveWeights[ArrayIndex];
		}

		return 0.f;
	}

	/** Get Value of InUID with validation and default value */
	float Get(USkeleton::AnimCurveUID InUid, bool& OutIsValid, float InDefaultValue=0.f) const
	{
		check(bInitialized);

		const int32 ArrayIndex = GetArrayIndexByUID(InUid);
		if ((ArrayIndex != INDEX_NONE) && ValidCurveWeights[ArrayIndex])
		{
			OutIsValid = true;
			return CurveWeights[ArrayIndex];
		}

		OutIsValid = false;
		return InDefaultValue;
	}

	/** Get Array Index by UID */
	int32 GetArrayIndexByUID(USkeleton::AnimCurveUID InUid) const
	{
		int32 ArrayIndex = (*UIDToArrayIndexLUT).IsValidIndex(InUid) ? (*UIDToArrayIndexLUT)[InUid] : MAX_uint16;
		if (ArrayIndex != MAX_uint16)
		{
			return ArrayIndex;
		}
		return INDEX_NONE;
	}

	/** return true if enabled. return false otherwise. */
	bool IsEnabled(USkeleton::AnimCurveUID InUid) const
	{
		check(bInitialized);

		return (GetArrayIndexByUID(InUid) != INDEX_NONE);
	}
	
	/** Get Valid Element Count from given UIDToArrayIndexLUT */
	static int32 GetValidElementCount(TArray<uint16> const* InUIDToArrayIndexLUT) 
	{
		int32 Count = 0;
		if (InUIDToArrayIndexLUT)
		{
			const int32 ArraySize = InUIDToArrayIndexLUT->Num();
			for (int32 Index = 0; Index < ArraySize; ++Index)
			{
				if ((*InUIDToArrayIndexLUT)[Index] != MAX_uint16)
				{
					++Count;
				}
			}
		}

		return Count;
	}	
	/**
	 * Blend (A, B) using Alpha, same as Lerp
	 */
	//@Todo curve flags won't transfer over - it only overwrites
	void Lerp(const FBaseBlendedCurve& A, const FBaseBlendedCurve& B, float Alpha)
	{
		check(A.Num() == B.Num());
		if (!FAnimWeight::IsRelevant(FMath::Abs(Alpha)))
		{
			// if blend is all the way for child1, then just copy its bone atoms
			Override(A);
		}
		else if (!FAnimWeight::IsRelevant(FMath::Abs(Alpha - 1.0f)))
		{
			// if blend is all the way for child2, then just copy its bone atoms
			Override(B);
		}
		else
		{
			InitFrom(A);

			// Only consider curve elements where either or both elements are valid.
			for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(A.ValidCurveWeights, B.ValidCurveWeights); It; ++It)
			{
				int32 Idx = It.GetIndex();
				ValidCurveWeights[Idx] = true;
				CurveWeights[Idx] = FMath::Lerp(A.CurveWeights[Idx], B.CurveWeights[Idx], Alpha);
			}
		}
	}

	/**
	 * Blend with Other using Alpha, same as Lerp 
	 */
	void LerpTo(const FBaseBlendedCurve& Other, float Alpha)
	{
		check(Num() == Other.Num());
		if (!FAnimWeight::IsRelevant(FMath::Abs(Alpha)))
		{
			return;
		}
		else if (!FAnimWeight::IsRelevant(FMath::Abs(Alpha - 1.0f)))
		{
			// if blend is all the way for child2, then just copy its bone atoms
			Override(Other);
		}
		else
		{
			// Only consider curve elements where either or both elements are valid.
			for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(ValidCurveWeights, Other.ValidCurveWeights); It; ++It)
			{
				int32 Idx = It.GetIndex();
				ValidCurveWeights[Idx] = true;
				CurveWeights[Idx] = FMath::Lerp(CurveWeights[Idx], Other.CurveWeights[Idx], Alpha);
			}
		}
	}
	/**
	 * Convert current curves to Additive (this - BaseCurve) if same found
	 */
	void ConvertToAdditive(const FBaseBlendedCurve& BaseCurve)
	{
		check(bInitialized);
		check(Num() == BaseCurve.Num());

		for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(ValidCurveWeights, BaseCurve.ValidCurveWeights); It; ++It)
		{
			int32 Idx = It.GetIndex();
			ValidCurveWeights[Idx] = true;
			CurveWeights[Idx] -= BaseCurve.CurveWeights[Idx];
		}
	}
	/**
	 * Accumulate the input curve with input Weight
	 */
	void Accumulate(const FBaseBlendedCurve& AdditiveCurve, float Weight)
	{
		check(bInitialized);
		check(Num() == AdditiveCurve.Num());

		if (FAnimWeight::IsRelevant(Weight))
		{
			for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(ValidCurveWeights, AdditiveCurve.ValidCurveWeights); It; ++It)
			{
				int32 Idx = It.GetIndex();
				ValidCurveWeights[Idx] = true;
				CurveWeights[Idx] += AdditiveCurve.CurveWeights[Idx] * Weight;
			}
		}
	}

	/**
	 * This doesn't blend but combine MAX(current weight, curvetocombine weight)
	 */
	void UseMaxValue(const FBaseBlendedCurve& CurveToCombine)
	{
		check(bInitialized);
		check(Num() == CurveToCombine.Num());

		for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(ValidCurveWeights, CurveToCombine.ValidCurveWeights); It; ++It)
		{
			int32 Idx = It.GetIndex();
			if (!ValidCurveWeights[Idx])
			{
				CurveWeights[Idx] = CurveToCombine.CurveWeights[Idx];
				ValidCurveWeights[Idx] = true;
			}
			else if (CurveToCombine.ValidCurveWeights[Idx])
			{
				CurveWeights[Idx] = FMath::Max(CurveWeights[Idx], CurveToCombine.CurveWeights[Idx]);
			}
		}
	}

	/**
	 * This doesn't blend but combine MIN(current weight, curvetocombine weight)
	 */
	void UseMinValue(const FBaseBlendedCurve& CurveToCombine)
	{
		check(bInitialized);
		check(Num() == CurveToCombine.Num());

		for (TConstDualEitherSetBitIterator<InAllocator, InAllocator> It(ValidCurveWeights, CurveToCombine.ValidCurveWeights); It; ++It)
		{
			int32 Idx = It.GetIndex();
			if (!ValidCurveWeights[Idx])
			{
				CurveWeights[Idx] = CurveToCombine.CurveWeights[Idx];
				ValidCurveWeights[Idx] = true;
			}
			else if (CurveToCombine.ValidCurveWeights[Idx])
			{
				CurveWeights[Idx] = FMath::Min(CurveWeights[Idx], CurveToCombine.CurveWeights[Idx]);
			}
		}
	}

	/**
	 * This combines IF the input does not contain valid curve
	 */
	void CombinePreserved(const FBaseBlendedCurve& CurveToCombine)
	{
		check(bInitialized);
		check(Num() == CurveToCombine.Num());

		for (TConstSetBitIterator<InAllocator> It(CurveToCombine.ValidCurveWeights); It; ++It)
		{
			int32 Idx = It.GetIndex();
			if (!ValidCurveWeights[Idx])
			{
				CurveWeights[Idx] = CurveToCombine.CurveWeights[Idx];
				ValidCurveWeights[Idx] = true;
			}
		}
	}
	/**
	 * This doesn't blend but later pose with valid curve value overrides
	 */
	void Combine(const FBaseBlendedCurve& CurveToCombine)
	{
		check(bInitialized);
		check(Num() == CurveToCombine.Num());

		for (TConstSetBitIterator<InAllocator> It(CurveToCombine.ValidCurveWeights); It; ++It)
		{
			int32 Idx = It.GetIndex();
			CurveWeights[Idx] = CurveToCombine.CurveWeights[Idx];
			ValidCurveWeights[Idx] = true;
		}
	}

	/**
	 * Override with inupt curve * weight
	 */
	void Override(const FBaseBlendedCurve& CurveToOverrideFrom, float Weight)
	{
		InitFrom(CurveToOverrideFrom);

		CurveWeights = CurveToOverrideFrom.CurveWeights;
		ValidCurveWeights = CurveToOverrideFrom.ValidCurveWeights;

		if (!FMath::IsNearlyEqual(Weight, 1.f))
		{
			for (TConstSetBitIterator<InAllocator> It(ValidCurveWeights); It; ++It)
			{
				CurveWeights[It.GetIndex()] *= Weight;
			}
		}
	}

	/**
	 * Override with input curve 
	 */
	void Override(const FBaseBlendedCurve& CurveToOverrideFrom)
	{
		// make sure this doesn't happen
		if (ensure(&CurveToOverrideFrom != this))
		{
			check(CurveToOverrideFrom.UIDToArrayIndexLUT != nullptr);
			UIDToArrayIndexLUT = CurveToOverrideFrom.UIDToArrayIndexLUT;
			NumValidCurveCount = CurveToOverrideFrom.NumValidCurveCount;
			CurveWeights = CurveToOverrideFrom.CurveWeights;
			ValidCurveWeights = CurveToOverrideFrom.ValidCurveWeights;
			bInitialized = true;
		}
	}

	/**
	 * Override with input curve, leaving input curve invalid
	 */
	void OverrideMove(FBaseBlendedCurve& CurveToOverrideFrom)
	{
		// make sure this doesn't happen
		if (ensure(&CurveToOverrideFrom != this))
		{
			check(CurveToOverrideFrom.UIDToArrayIndexLUT != nullptr);
			UIDToArrayIndexLUT = CurveToOverrideFrom.UIDToArrayIndexLUT;
			CurveToOverrideFrom.UIDToArrayIndexLUT = nullptr;
			NumValidCurveCount = CurveToOverrideFrom.NumValidCurveCount;
			CurveToOverrideFrom.NumValidCurveCount = 0;
			CurveWeights = MoveTemp(CurveToOverrideFrom.CurveWeights);
			ValidCurveWeights = MoveTemp(CurveToOverrideFrom.ValidCurveWeights);
			bInitialized = true;
			CurveToOverrideFrom.bInitialized = false;
		}
	}

	/** Return number of elements */
	int32 Num() const { return CurveWeights.Num(); }

	/** Return number of valid elements */
	int32 NumValid() const { return ValidCurveWeights.CountSetBits(); }

	/** CopyFrom as expected. */
	template <typename OtherAllocator>
	void CopyFrom(const FBaseBlendedCurve<OtherAllocator>& CurveToCopyFrom)
	{
		checkf(CurveToCopyFrom.IsValid(), TEXT("Copying data from an invalid curve UIDToArrayIndexLUT: 0x%x  (Sizes %i/%i)"), CurveToCopyFrom.UIDToArrayIndexLUT, (CurveToCopyFrom.UIDToArrayIndexLUT ? CurveToCopyFrom.UIDToArrayIndexLUT->Num() : -1), CurveToCopyFrom.CurveWeights.Num());
		UIDToArrayIndexLUT = CurveToCopyFrom.UIDToArrayIndexLUT;
		CurveWeights = CurveToCopyFrom.CurveWeights;
		ValidCurveWeights = CurveToCopyFrom.ValidCurveWeights;
		NumValidCurveCount = CurveToCopyFrom.NumValidCurveCount;
		bInitialized = true;
	}

	void CopyFrom(const FBaseBlendedCurve<Allocator>& CurveToCopyFrom)
	{
		if (&CurveToCopyFrom != this)
		{
			checkf(CurveToCopyFrom.IsValid(), TEXT("Copying data from an invalid curve UIDToArrayIndexLUT: 0x%x  (Sizes %i/%i)"), CurveToCopyFrom.UIDToArrayIndexLUT, (CurveToCopyFrom.UIDToArrayIndexLUT ? CurveToCopyFrom.UIDToArrayIndexLUT->Num() : -1), CurveToCopyFrom.CurveWeights.Num());
			UIDToArrayIndexLUT = CurveToCopyFrom.UIDToArrayIndexLUT;
			CurveWeights = CurveToCopyFrom.CurveWeights;
			ValidCurveWeights = CurveToCopyFrom.ValidCurveWeights;
			NumValidCurveCount = CurveToCopyFrom.NumValidCurveCount;
			bInitialized = true;
		}
	}

	/** Once moved, source is invalid */
	void MoveFrom(FBaseBlendedCurve<Allocator>& CurveToMoveFrom)
	{
		UIDToArrayIndexLUT = CurveToMoveFrom.UIDToArrayIndexLUT;
		CurveToMoveFrom.UIDToArrayIndexLUT = nullptr;
		NumValidCurveCount = CurveToMoveFrom.NumValidCurveCount;
		CurveToMoveFrom.NumValidCurveCount = 0;
		CurveWeights = MoveTemp(CurveToMoveFrom.CurveWeights);
		ValidCurveWeights = MoveTemp(CurveToMoveFrom.ValidCurveWeights);
		bInitialized = true;
		CurveToMoveFrom.bInitialized = false;
	}

	/** Empty */
	void Empty()
	{
		// Set to nullptr as we only received a ptr reference from USkeleton
		UIDToArrayIndexLUT = nullptr;
		CurveWeights.Reset();
		ValidCurveWeights.Reset();
		NumValidCurveCount = 0;
		bInitialized = false;
	}

	/**  Whether initialized or not */
	bool bInitialized;

	// Only checks bare minimal validity. (namely that we have a UID list and that it 
	// is the same size as our element list
	bool IsValid() const
	{
		return UIDToArrayIndexLUT != nullptr && (CurveWeights.Num() == NumValidCurveCount);
	}
};

struct ENGINE_API FBlendedCurve : public FBaseBlendedCurve<FAnimStackAllocator>
{
};

struct ENGINE_API FBlendedHeapCurve : public FBaseBlendedCurve<FDefaultAllocator>
{
};

UENUM(BlueprintType)
enum class ERawCurveTrackTypes : uint8
{
	RCT_Float UMETA(DisplayName = "Float Curve"),
	RCT_Vector UMETA(DisplayName = "Vector Curve", Hidden),
	RCT_Transform UMETA(DisplayName = "Transformation Curve"),
	RCT_MAX
};

/**
 * Raw Curve data for serialization
 */
USTRUCT()
struct FRawCurveTracks
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FFloatCurve>		FloatCurves;

#if WITH_EDITORONLY_DATA
	/**
	 * @note : Currently VectorCurves are not evaluated or used for anything else but transient data for modifying bone track
	 *			Note that it doesn't have UPROPERTY tag yet. In the future, we'd like this to be serialized, but not for now
	 **/
	UPROPERTY(transient)
	TArray<FVectorCurve>	VectorCurves;

	/**
	 * @note : TransformCurves are used to edit additive animation in editor. 
	 **/
	UPROPERTY()
	TArray<FTransformCurve>	TransformCurves;
#endif // #if WITH_EDITORONLY_DATA

	/**
	 * Evaluate curve data at the time CurrentTime, and add to Instance. It only evaluates Float Curve for now
	 *
	 * return true if curve exists, false otherwise
	 */
	void EvaluateCurveData( FBlendedCurve& Curves, float CurrentTime ) const;

#if WITH_EDITOR
	/**
	 *	Evaluate transform curves 
	 */
	ENGINE_API void EvaluateTransformCurveData(USkeleton * Skeleton, TMap<FName, FTransform>&OutCurves, float CurrentTime, float BlendWeight) const;

	/**
	* Add new float curve from the given UID if not existing and add the key with time/value
	*/
	ENGINE_API void AddFloatCurveKey(const FSmartName& NewCurve, int32 CurveFlags, float Time, float Value);
	ENGINE_API void RemoveRedundantKeys(float Tolerance = UE_SMALL_NUMBER, FFrameRate SampleRate = FFrameRate(0,0));

#endif // WITH_EDITOR
	/**
	 * Find curve data based on the curve UID
	 */
	ENGINE_API FAnimCurveBase * GetCurveData(USkeleton::AnimCurveUID Uid, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/**
	* Find curve data based on the curve UID
	*/
	ENGINE_API const FAnimCurveBase * GetCurveData(USkeleton::AnimCurveUID Uid, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float) const;

	/**
	 * Add new curve from the provided UID and return true if success
	 * bVectorInterpCurve == true, then it will create FVectorCuve, otherwise, FFloatCurve
	 */
	ENGINE_API bool AddCurveData(const FSmartName& NewCurve, int32 CurveFlags = AACF_DefaultCurve, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/**
	 * Delete curve data 
	 */
	ENGINE_API bool DeleteCurveData(const FSmartName& CurveToDelete, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/**
	 * Delete all curve data 
	 */
	ENGINE_API void DeleteAllCurveData(ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/**
	 * Duplicate curve data
	 * 
	 */
	ENGINE_API bool DuplicateCurveData(const FSmartName& CurveToCopy, const FSmartName& NewCurve, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/**
	 * Updates the DisplayName field of the curves from the provided name container
	 */
	ENGINE_API void RefreshName(const FSmartNameMapping* NameMapping, ERawCurveTrackTypes SupportedCurveType = ERawCurveTrackTypes::RCT_Float);

	/** 
	 * Serialize
	 */
	void PostSerialize(FArchive& Ar);

	/*
	 * resize curve length. If longer, it doesn't do any. If shorter, remove previous keys and add new key to the end of the frame. 
	 */
	void Resize(float TotalLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
	/** 
	 * Clear all keys
	 */
	void Empty()
	{
		FloatCurves.Empty();
#if WITH_EDITORONLY_DATA
		VectorCurves.Empty();
		TransformCurves.Empty();
#endif
	}

private:
	/** 
	 * Adding vector curve support - this is all transient data for now. This does not save and all these data will be baked into RawAnimationData
	 */

	/**
	 * Find curve data based on the curve UID
	 */
	template <typename DataType>
	DataType * GetCurveDataImpl(TArray<DataType>& Curves, USkeleton::AnimCurveUID Uid);

	/**
	* Find curve data based on the curve UID
	*/
	template <typename DataType>
	const DataType * GetCurveDataImpl(const TArray<DataType>& Curves, USkeleton::AnimCurveUID Uid) const;

	/**
	 * Add new curve from the provided UID and return true if success
	 * bVectorInterpCurve == true, then it will create FVectorCuve, otherwise, FFloatCurve
	 */
	template <typename DataType>
	bool AddCurveDataImpl(TArray<DataType>& Curves, const FSmartName& NewCurve, int32 CurveFlags);
	/**
	 * Delete curve data 
	 */
	template <typename DataType>
	bool DeleteCurveDataImpl(TArray<DataType>& Curves, const FSmartName& CurveToDelete);
	/**
	 * Duplicate curve data
	 * 
	 */
	template <typename DataType>
	bool DuplicateCurveDataImpl(TArray<DataType>& Curves, const FSmartName& CurveToCopy, const FSmartName& NewCurve);

	/**
	 * Updates the DisplayName field of the curves from the provided name container
	 */
	template <typename DataType>
	void UpdateLastObservedNamesImpl(TArray<DataType>& Curves, const FSmartNameMapping* NameMapping);
};

FArchive& operator<<(FArchive& Ar, FRawCurveTracks& D);
