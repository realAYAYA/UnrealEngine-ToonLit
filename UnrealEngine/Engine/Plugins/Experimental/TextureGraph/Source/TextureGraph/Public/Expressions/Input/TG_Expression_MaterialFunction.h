// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression_MaterialBase.h"
#include "Materials/MaterialFunction.h"
#include "TG_Texture.h"

#include "TG_Expression_MaterialFunction.generated.h"


UCLASS()
class TEXTUREGRAPH_API UTG_Expression_MaterialFunction : public UTG_Expression_MaterialBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	
	// The material function to employ for rendering
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Category = NoCategory, meta = (TGType = "TG_Setting", TGPinNotConnectable))
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction;
	void SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction);

	virtual bool CanHandleAsset(UObject* Asset) override;
	virtual void SetAsset(UObject* Asset) override;

	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Renders a material function output into a quad and makes it available for the texture graph.")); } 

protected:
	virtual void Initialize() override;

	UMaterialInterface* CreateMaterialReference();
	TObjectPtr<UMaterialInterface> ReferenceMaterial = nullptr;
	virtual TObjectPtr<UMaterialInterface> GetMaterial() const { return ReferenceMaterial; }
	virtual EDrawMaterialAttributeTarget GetRenderedAttributeId() { return EDrawMaterialAttributeTarget::Emissive; }

private:
	void SetMaterialFunctionInternal(UMaterialFunctionInterface* InMaterialFunction);

public:
	
	virtual FTG_Name GetDefaultName() const override { return TEXT("MaterialFunction");}
	virtual bool CanRenameTitle() const override;

	virtual FName GetCategory() const override { return TG_Category::Input; }

};

