// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerEffectorShared.h"
#include "CEClonerLayoutBase.h"
#include "CEPropertyChangeDispatcher.h"
#include "CEClonerGridLayout.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UCEClonerGridLayout : public UCEClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;

public:
	UCEClonerGridLayout()
		: UCEClonerLayoutBase(
			TEXT("Grid")
			, TEXT("/Script/Niagara.NiagaraSystem'/ClonerEffector/Systems/NS_ClonerGrid.NS_ClonerGrid'")
		)
	{}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetCountX(int32 InCountX);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountX() const
	{
		return CountX;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetCountY(int32 InCountY);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountY() const
	{
		return CountY;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetCountZ(int32 InCountZ);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountZ() const
	{
		return CountZ;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetSpacingX(float InSpacingX);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingX() const
	{
		return SpacingX;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetSpacingY(float InSpacingY);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingY() const
	{
		return SpacingY;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetSpacingZ(float InSpacingZ);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingZ() const
	{
		return SpacingZ;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetConstraint(ECEClonerGridConstraint InConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	ECEClonerGridConstraint GetConstraint() const
	{
		return Constraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetInvertConstraint(bool bInInvertConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	bool GetInvertConstraint() const
	{
		return bInvertConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetSphereConstraint(const FCEClonerGridConstraintSphere& InConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FCEClonerGridConstraintSphere& GetSphereConstraint() const
	{
		return SphereConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetCylinderConstraint(const FCEClonerGridConstraintCylinder& InConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FCEClonerGridConstraintCylinder& GetCylinderConstraint() const
	{
		return CylinderConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	CLONEREFFECTOR_API void SetTextureConstraint(const FCEClonerGridConstraintTexture& InConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FCEClonerGridConstraintTexture& GetTextureConstraint() const
	{
		return TextureConstraint;
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

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountX", Getter="GetCountX", Category="Layout")
	int32 CountX = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountY", Getter="GetCountY", Category="Layout")
	int32 CountY = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountZ", Getter="GetCountZ", Category="Layout")
	int32 CountZ = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingX", Getter="GetSpacingX", Category="Layout")
	float SpacingX = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingY", Getter="GetSpacingY", Category="Layout")
	float SpacingY = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingZ", Getter="GetSpacingZ", Category="Layout")
	float SpacingZ = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetConstraint", Getter="GetConstraint", Category="Layout")
	ECEClonerGridConstraint Constraint = ECEClonerGridConstraint::None;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInvertConstraint", Getter="GetInvertConstraint", Category="Layout", meta=(EditCondition="Constraint != ECEClonerGridConstraint::None", EditConditionHides))
	bool bInvertConstraint = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSphereConstraint", Getter="GetSphereConstraint", Category="Layout", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Sphere", EditConditionHides))
	FCEClonerGridConstraintSphere SphereConstraint;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCylinderConstraint", Getter="GetCylinderConstraint", Category="Layout", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Cylinder", EditConditionHides))
	FCEClonerGridConstraintCylinder CylinderConstraint;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetTextureConstraint", Getter="GetTextureConstraint", Category="Layout", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Texture", EditConditionHides))
	FCEClonerGridConstraintTexture TextureConstraint;

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TCEPropertyChangeDispatcher<UCEClonerGridLayout> PropertyChangeDispatcher;
#endif
};