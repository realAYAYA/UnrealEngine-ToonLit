// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreData.h"
#include "PropertyBag.h"
#include "UObject/Object.h"
#include "PropertyAnimatorCoreContext.generated.h"

struct FInstancedStruct;
class UPropertyAnimatorCoreBase;
class UPropertyAnimatorCoreConverterBase;
class UPropertyAnimatorCoreGroupBase;
class UScriptStruct;

/** Mode supported for properties value */
UENUM(BlueprintType)
enum class EPropertyAnimatorCoreMode : uint8
{
	Absolute,
	Additive,
};

/** Context for properties linked to an animator */
UCLASS(MinimalAPI, BlueprintType)
class UPropertyAnimatorCoreContext : public UObject
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreBase;
	friend class FPropertyAnimatorCoreEditorContextTypeCustomization;

public:
	const FPropertyAnimatorCoreData& GetAnimatedProperty() const
	{
		return AnimatedProperty;
	}

	UPropertyAnimatorCoreBase* GetAnimator() const;

	/** Get the handler responsible for this property type */
	UPropertyAnimatorCoreHandlerBase* GetHandler() const;

	/** Get the active group of this property */
	UPropertyAnimatorCoreGroupBase* GetGroup() const
	{
		return Group;
	}

	PROPERTYANIMATORCORE_API void SetAnimated(bool bInAnimated);
	bool IsAnimated() const
	{
		return bAnimated;
	}

	PROPERTYANIMATORCORE_API void SetMode(EPropertyAnimatorCoreMode InMode);
	EPropertyAnimatorCoreMode GetMode() const
	{
		return Mode;
	}

	PROPERTYANIMATORCORE_API void SetConverterClass(TSubclassOf<UPropertyAnimatorCoreConverterBase> InConverterClass);
	TSubclassOf<UPropertyAnimatorCoreConverterBase> GetConverterClass() const
	{
		return ConverterClass;
	}

	PROPERTYANIMATORCORE_API void SetGroupName(FName InGroupName);
	FName GetGroupName() const
	{
		return GroupName;
	}

	/** Get converter rule if any */
	template <typename InRuleType
		UE_REQUIRES(TModels_V<CStaticStructProvider, InRuleType>)>
	InRuleType* GetConverterRule()
	{
		return static_cast<InRuleType*>(GetConverterRulePtr(InRuleType::StaticStruct()));
	}

protected:
	//~ Begin UObject
	PROPERTYANIMATORCORE_API virtual void PostLoad() override;
#if WITH_EDITOR
	PROPERTYANIMATORCORE_API virtual void PreEditChange(FProperty* InPropertyAboutToChange) override;
	PROPERTYANIMATORCORE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	/** Called once, when the property is linked to this context */
	virtual void OnAnimatedPropertyLinked() {}

	/** Called when the animated property owner is updated */
	virtual void OnAnimatedPropertyOwnerUpdated(UObject* InPreviousOwner, UObject* InNewOwner) {}

private:
	void ConstructInternal(const FPropertyAnimatorCoreData& InProperty);
	void SetAnimatedPropertyOwner(UObject* InNewOwner);

	PROPERTYANIMATORCORE_API void* GetConverterRulePtr(const UScriptStruct* InStruct);

	void CheckEditMode();
	void CheckEditConverterRule();

	void OnAnimatedChanged();
	void OnModeChanged();
	void OnGroupNameChanged();

	/** Sets the evaluation result for the resolved property */
	PROPERTYANIMATORCORE_API void SetEvaluationResult(const FPropertyAnimatorCoreData& InResolvedProperty, const FInstancedPropertyBag& InEvaluatedValues);

	/** Use this to resolve virtual linked property */
	PROPERTYANIMATORCORE_API TArray<FPropertyAnimatorCoreData> ResolveProperty(bool bInForEvaluation) const;

	/** Restore property based on mode */
	void Restore();

	/** Allocate and save properties */
	void Save();

	void SetGroup(UPropertyAnimatorCoreGroupBase* InGroup);

	bool IsResolvable() const;
	bool IsConverted() const;

	/** Get the supported group names that can manage this property */
	UFUNCTION()
	TArray<FName> GetSupportedGroupNames() const;

	/** Animation is enabled for this property */
	UPROPERTY(EditInstanceOnly, Setter="SetAnimated", Getter="IsAnimated", Category="Animator", meta=(AllowPrivateAccess="true"))
	bool bAnimated = true;

	/** Edit condition for modes */
	UPROPERTY(Transient)
	bool bEditMode = true;

	/** Current mode used for this property */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(HideEditConditionToggle, EditCondition="bEditMode", EditConditionHides, AllowPrivateAccess="true"))
	EPropertyAnimatorCoreMode Mode = EPropertyAnimatorCoreMode::Absolute;

	/** Edit condition for converter rule */
	UPROPERTY(Transient)
	bool bEditConverterRule = false;

	/** If a converter is used, rules may be used to convert the property */
	UPROPERTY(EditInstanceOnly, Category="Animator", meta=(HideEditConditionToggle, EditCondition="bEditConverterRule", EditConditionHides, AllowPrivateAccess="true"))
	FInstancedStruct ConverterRule;

	/** The unique group name that manages this property */
	UPROPERTY(EditInstanceOnly, Category="Animator", Setter, Getter, meta=(GetOptions="GetSupportedGroupNames", AllowPrivateAccess="true"))
	FName GroupName = NAME_None;

	/** Active group of this property */
	UPROPERTY()
	TObjectPtr<UPropertyAnimatorCoreGroupBase> Group;

	/** Store original property values for resolved properties */
	UPROPERTY(NonTransactional)
	FInstancedPropertyBag OriginalPropertyValues;

	/** Store delta property values for resolved properties */
	UPROPERTY(NonTransactional)
	FInstancedPropertyBag DeltaPropertyValues;

	/** Converter class used for this property */
	UPROPERTY()
	TSubclassOf<UPropertyAnimatorCoreConverterBase> ConverterClass;

	/** Used to access property value and update it */
	UPROPERTY(Transient)
	TWeakObjectPtr<UPropertyAnimatorCoreHandlerBase> HandlerWeak;

	/** Animated property linked to this options */
	UPROPERTY()
	FPropertyAnimatorCoreData AnimatedProperty;
};