// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TG_Expression.h"
#include "TG_Texture.h"
#include "TG_Expression_MaterialBase.generated.h"

class RenderMaterial_BP;
typedef std::shared_ptr<RenderMaterial_BP> RenderMaterial_BPPtr;


// Describe the possible attributes extracted from a MAterial during a DrawMaterial call
// this is used as high level data.
UENUM(BlueprintType)
enum class EDrawMaterialAttributeTarget : uint8
{
	BaseColor = 0,
	Metallic,
	Specular,
	Roughness,
	Anisotropy,
	Emissive,
	Opacity,
	OpacityMask,
	Normal,
	Tangent,

	/// Always has to be the last
	Count UMETA(Hidden),
};


UCLASS(abstract)
class TEXTUREGRAPH_API UTG_Expression_MaterialBase : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_DYNAMIC_EXPRESSION(TG_Category::Default)

	// Zohaib: Right now, we only support tile mode.
	// IMO, We dont need to show it user.
	// Disabling it for now.

	// Whether to run the material in tiled mode (Tiles in output settings)
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = NoCategory, meta = (TGType = "TG_Setting")
	bool TiledMode = true;

	// The output of the material expressed as a texture
	UPROPERTY(meta = (TGType = "TG_Output") )
	FTG_Texture Output;

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;

	// Validate internal checks, warnings and errors
	virtual bool Validate(MixUpdateCyclePtr	Cycle) override;

	// Access the list of attributes available for rendering from the current Material
	const TArray<EDrawMaterialAttributeTarget>& GetAvailableMaterialAttributeIds() const { return AvailableMaterialAttributeIds; }
	const TArray<FName>& GetAvailableMaterialAttributeNames() const { return AvailableMaterialAttributeNames; }

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance; // A local Instance Material is recreated for the Material member

	TArray<EDrawMaterialAttributeTarget>	AvailableMaterialAttributeIds; // The set of material properties available for rendering
	TArray<FName>							AvailableMaterialAttributeNames; // same with the attribute names

	virtual void Initialize() override;

	void GenerateMaterialAttributeOptions(); // Based on the current Material, list of material attributes available

	TiledBlobPtr CreateRenderMaterialJob(FTG_EvaluationContext* InContext, const FString& InName, const FString& InMaterialPath, const BufferDescriptor& InDescriptor, EDrawMaterialAttributeTarget InDrawMaterialAttributeTarget);
	TiledBlobPtr CreateRenderMaterialJob(FTG_EvaluationContext* InContext, const RenderMaterial_BPPtr& InRenderMaterial, const BufferDescriptor& InDescriptor, EDrawMaterialAttributeTarget InDrawMaterialAttributeTarget);
	void LinkMaterialParameters(FTG_EvaluationContext* InContext, JobUPtr& InMaterialJob, const UMaterial* InMaterial, BufferDescriptor InDescriptor);

	virtual void CopyVarGeneric(const FTG_Argument& Arg, FTG_Var* InVar, bool CopyVarToArg);
	virtual void SetMaterialInternal(UMaterialInterface* InMaterial);

	virtual EDrawMaterialAttributeTarget GetRenderedAttributeId() { return EDrawMaterialAttributeTarget::Emissive;}
	virtual TObjectPtr<UMaterialInterface> GetMaterial() const { return nullptr;}

private:
	void AddSignatureParam(TArray<FMaterialParameterInfo> OutParameterInfoScalar, FName CPPTypeName, FTG_Signature::FInit& SignatureInit, bool IsScalar = false) const;

	static EDrawMaterialAttributeTarget ConvertEMaterialPropertyToEDrawMaterialAttributeTarget(EMaterialProperty InMaterialProperty);
};

 