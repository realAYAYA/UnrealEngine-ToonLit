// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Animators/PropertyAnimatorCoreBase.h"
#include "PropertyAnimatorCoreComponent.generated.h"

/** A container for controllers that holds properties in this actor */
UCLASS(MinimalAPI, ClassGroup=(Custom), AutoExpandCategories=("Animator"), HideCategories=("Activation", "Cooking", "AssetUserData", "Collision"), meta=(BlueprintSpawnableComponent))
class UPropertyAnimatorCoreComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreSubsystem;
	friend class UPropertyAnimatorCoreEditorStackCustomization;

public:
	/** Create an instance of this component class and adds it to an actor */
	static UPropertyAnimatorCoreComponent* FindOrAdd(AActor* InActor);

	UPropertyAnimatorCoreComponent();

	void SetAnimators(const TSet<TObjectPtr<UPropertyAnimatorCoreBase>>& InAnimators);
	const TSet<TObjectPtr<UPropertyAnimatorCoreBase>>& GetAnimators() const
	{
		return Animators;
	}

	int32 GetAnimatorsCount() const
	{
		return Animators.Num();
	}

	/** Set the state of all animators in this component */
	void SetAnimatorsEnabled(bool bInEnabled);
	bool GetAnimatorsEnabled() const
	{
		return bAnimatorsEnabled;
	}

	/** Set the magnitude for all animators in this component */
	void SetAnimatorsMagnitude(float InMagnitude);
	float GetAnimatorsMagnitude() const
	{
		return AnimatorsMagnitude;
	}

	/** Process a function for each controller, stops when false is returned otherwise continue until the end */
	void ForEachAnimator(TFunctionRef<bool(UPropertyAnimatorCoreBase*)> InFunction) const;

protected:
	static FName GetAnimatorName(const UPropertyAnimatorCoreBase* InAnimator);

	//~ Begin UActorComponent
	virtual void DestroyComponent(bool bPromoteChildren) override;
	//~ End UActorComponent

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	/** Adds a new controller and returns it casted */
	template<typename InAnimatorClass = UPropertyAnimatorCoreBase, typename = typename TEnableIf<TIsDerivedFrom<InAnimatorClass, UPropertyAnimatorCoreBase>::Value>::Type>
	InAnimatorClass* AddAnimator()
	{
		const UClass* AnimatorClass = InAnimatorClass::StaticClass();
		return Cast<InAnimatorClass>(AddAnimator(AnimatorClass));
	}

	/** Adds a new animator of that class */
	UPropertyAnimatorCoreBase* AddAnimator(const UClass* InAnimatorClass);

	/** Removes an existing animator */
	bool RemoveAnimator(UPropertyAnimatorCoreBase* InAnimator);

	/** Change global state for animators */
	void OnAnimatorsSetEnabled(const UWorld* InWorld, bool bInEnabled, bool bInTransact);

	void OnAnimatorsChanged();

	void OnAnimatorsEnabledChanged();

	/** Checks if this component animators should tick */
	bool ShouldAnimatorsTick() const;

	/** Animators linked to this actor, they contain only properties within this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Getter="GetAnimators", Setter="SetAnimators", Category="Animator", meta=(TitleProperty="AnimatorDisplayName"))
	TSet<TObjectPtr<UPropertyAnimatorCoreBase>> Animators;

	/** Global state for all animators controlled by this component */
	UPROPERTY(EditInstanceOnly, Getter="GetAnimatorsEnabled", Setter="SetAnimatorsEnabled", Category="Animator", meta=(DisplayPriority="0", AllowPrivateAccess="true"))
	bool bAnimatorsEnabled = true;

	/** Global magnitude for all animators controlled by this component */
	UPROPERTY(EditInstanceOnly, Getter, Setter, Category="Animator", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1", AllowPrivateAccess="true"))
	float AnimatorsMagnitude = 1.f;

private:
	virtual void TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InThisTickFunction) override;

	/** Transient copy of animators set when changes are detected to see the diff only */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TSet<TObjectPtr<UPropertyAnimatorCoreBase>> AnimatorsInternal;
};