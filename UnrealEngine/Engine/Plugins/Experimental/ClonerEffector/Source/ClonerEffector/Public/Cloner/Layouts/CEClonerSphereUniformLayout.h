// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerLayoutBase.h"
#include "CEPropertyChangeDispatcher.h"
#include "CEClonerSphereUniformLayout.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UCEClonerSphereUniformLayout : public UCEClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;

public:
	UCEClonerSphereUniformLayout()
		: UCEClonerLayoutBase(
			TEXT("SphereUniform")
			, TEXT("/Script/Niagara.NiagaraSystem'/ClonerEffector/Systems/NS_ClonerSphere.NS_ClonerSphere'")
		)
	{}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetCount(int32 InCount);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	int32 GetCount() const
	{
		return Count;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	float GetRadius() const
	{
		return Radius;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetRatio(float InRatio);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	float GetRatio() const
	{
		return Ratio;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetOrientMesh(bool InbOrientMesh);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	bool GetOrientMesh() const
	{
		return bOrientMesh;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	FRotator GetRotation() const
	{
		return Rotation;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|SphereUniform")
	CLONEREFFECTOR_API void SetScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|SphereUniform")
	FVector GetScale() const
	{
		return Scale;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UCEClonerLayoutBase
	virtual void OnLayoutParametersChanged(UCEClonerComponent* InComponent) override;
	//~ End UCEClonerLayoutBase

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCount", Getter="GetCount", Category="Layout")
	int32 Count = 30;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRadius", Getter="GetRadius", Category="Layout")
	float Radius = 200.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRatio", Getter="GetRatio", Category="Layout", meta=(ClampMin="0", ClampMax="1"))
	float Ratio = 1.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetOrientMesh", Getter="GetOrientMesh", Category="Layout")
	bool bOrientMesh = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRotation", Getter="GetRotation", Category="Layout")
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetScale", Getter="GetScale", Category="Layout", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01"))
	FVector Scale = FVector(1.f, 1.f, 1.f);

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TCEPropertyChangeDispatcher<UCEClonerSphereUniformLayout> PropertyChangeDispatcher;
#endif
};