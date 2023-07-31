// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyComponent.h"
#include "WaterBodyCustomComponent.generated.h"

class UStaticMeshComponent;

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API UWaterBodyCustomComponent : public UWaterBodyComponent
{
	GENERATED_UCLASS_BODY()
	friend class AWaterBodyCustom;
public:
	/** AWaterBody Interface */
	virtual EWaterBodyType GetWaterBodyType() const override { return EWaterBodyType::Transition; }
	virtual TArray<UPrimitiveComponent*> GetCollisionComponents(bool bInOnlyEnabledComponents = true) const override;
	virtual TArray<UPrimitiveComponent*> GetStandardRenderableComponents() const override;
	virtual bool CanEverAffectWaterMesh() const { return false; }

protected:
	/** AWaterBody Interface */
	virtual void Reset() override;
	virtual void BeginUpdateWaterBody() override;
	virtual void OnUpdateBody(bool bWithExclusionVolumes) override;
	virtual void CreateOrUpdateWaterMID() override;
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
#if WITH_EDITOR
	virtual TArray<TSharedRef<FTokenizedMessage>> CheckWaterBodyStatus() const override;

	virtual const TCHAR* GetWaterSpriteTextureName() const override;

	virtual bool IsIconVisible() const override;
#endif // WITH_EDITOR

protected:
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<UStaticMeshComponent> MeshComp;
};