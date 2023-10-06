// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * MaterialEditorInstanceConstant.h: This class is used by the material instance editor to hold a set of inherited parameters which are then pushed to a material instance.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "StaticParameterSet.h"
#include "Editor/UnrealEdTypes.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditorInstanceConstant.generated.h"

class UDEditorParameterValue;
class UMaterial;
class UMaterialInstanceConstant;
struct FPropertyChangedEvent;

USTRUCT()
struct FEditorParameterGroup
{
	GENERATED_USTRUCT_BODY()

	FEditorParameterGroup()
		: GroupAssociation(GlobalParameter)
		, GroupSortPriority(0)
	{}

	UPROPERTY()
	FName GroupName;

	UPROPERTY()
	TEnumAsByte<EMaterialParameterAssociation> GroupAssociation= EMaterialParameterAssociation::LayerParameter;

	UPROPERTY(EditAnywhere, editfixedsize, Instanced, Category=EditorParameterGroup)
	TArray<TObjectPtr<class UDEditorParameterValue>> Parameters;

	UPROPERTY()
	int32 GroupSortPriority=0;
};

USTRUCT()
struct FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorParameterValue)
	uint32 bOverride:1;

	UPROPERTY(EditAnywhere, Category=EditorParameterValue)
	FMaterialParameterInfo ParameterInfo;

	UPROPERTY()
	FGuid ExpressionId;

	FEditorParameterValue()
		: bOverride(false)
	{
	}
};

USTRUCT()
struct FEditorVectorParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorVectorParameterValue)
	FLinearColor ParameterValue;

	FEditorVectorParameterValue()
		: ParameterValue(ForceInit)
	{
	}
};

USTRUCT()
struct FEditorScalarParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorScalarParameterValue)
	float ParameterValue;

	FEditorScalarParameterValue()
		: ParameterValue(0)
	{
	}
};

USTRUCT()
struct FEditorTextureParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorTextureParameterValue)
	TObjectPtr<class UTexture> ParameterValue;

	FEditorTextureParameterValue()
		: ParameterValue(NULL)
	{
	}
};

USTRUCT()
struct FEditorFontParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorFontParameterValue)
	TObjectPtr<class UFont> FontValue;

	UPROPERTY(EditAnywhere, Category=EditorFontParameterValue)
	int32 FontPage;

	FEditorFontParameterValue()
		: FontValue(NULL)
		, FontPage(0)
	{
	}
};

USTRUCT()
struct FEditorMaterialLayersParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorLayersParameterValue)
	TObjectPtr<class UMaterialFunctionInterface> FunctionValue;

	FEditorMaterialLayersParameterValue()
		: FunctionValue(NULL)
	{
	}
};

USTRUCT()
struct FEditorStaticSwitchParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorStaticSwitchParameterValue)
	uint32 ParameterValue:1;

	FEditorStaticSwitchParameterValue()
		: ParameterValue(false)
	{
	}

	/** Constructor */
	FEditorStaticSwitchParameterValue(const FStaticSwitchParameter& InParameter)
		: ParameterValue(InParameter.Value)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterInfo = InParameter.ParameterInfo;
		ExpressionId = InParameter.ExpressionGUID;
	}
};

USTRUCT()
struct FComponentMaskParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=ComponentMaskParameter)
	uint32 R:1;

	UPROPERTY(EditAnywhere, Category=ComponentMaskParameter)
	uint32 G:1;

	UPROPERTY(EditAnywhere, Category=ComponentMaskParameter)
	uint32 B:1;

	UPROPERTY(EditAnywhere, Category=ComponentMaskParameter)
	uint32 A:1;

	FComponentMaskParameter()
		: R(false)
		, G(false)
		, B(false)
		, A(false)
	{
	}

	/** Constructor */
	FComponentMaskParameter(bool InR, bool InG, bool InB, bool InA) :
		R(InR),
		G(InG),
		B(InB),
		A(InA)
	{
	}
};

USTRUCT()
struct FEditorStaticComponentMaskParameterValue : public FEditorParameterValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorStaticComponentMaskParameterValue)
	struct FComponentMaskParameter ParameterValue;

	/** Constructor */
	FEditorStaticComponentMaskParameterValue() {}
	FEditorStaticComponentMaskParameterValue(const FStaticComponentMaskParameter& InParameter)
		: ParameterValue(InParameter.R, InParameter.G, InParameter.B, InParameter.A)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterInfo = InParameter.ParameterInfo;
		ExpressionId = InParameter.ExpressionGUID;
	}
};

UCLASS(hidecategories=Object, collapsecategories, MinimalAPI)
class UMaterialEditorInstanceConstant : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Physical material to use for this graphics material. Used for sounds, effects etc.*/
	UPROPERTY(EditAnywhere, Category=MaterialEditorInstanceConstant)
	TObjectPtr<class UPhysicalMaterial> PhysMaterial;

	// since the Parent may point across levels and the property editor needs to import this text, it must be marked lazy so it doesn't set itself to NULL in FindImportedObject
	UPROPERTY(EditAnywhere, Category=MaterialEditorInstanceConstant, meta=(DisplayThumbnail="true"))
	TObjectPtr<class UMaterialInterface> Parent;

	UPROPERTY(EditAnywhere, editfixedsize, Category=MaterialEditorInstanceConstant)
	TArray<struct FEditorParameterGroup> ParameterGroups;

	/** This is the refraction depth bias, larger values offset distortion to prevent closer objects from rendering into the distorted surface at acute viewing angles but increases the disconnect between surface and where the refraction starts. */
	UPROPERTY(EditAnywhere, Category=MaterialEditorInstanceConstant)
	float RefractionDepthBias;

	/** SubsurfaceProfile, for Screen Space Subsurface Scattering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	/** Defines if SubsurfaceProfile from tis instance is used or it uses the parent one. */
	UPROPERTY(EditAnywhere, Category = MaterialEditorInstanceConstant)
	uint32 bOverrideSubsurfaceProfile : 1;

	UPROPERTY()
	uint32 bOverrideBaseProperties_DEPRECATED : 1;

	UPROPERTY(transient, duplicatetransient)
	uint32 bIsFunctionPreviewMaterial : 1;

	UPROPERTY(transient, duplicatetransient)
	uint32 bIsFunctionInstanceDirty : 1;
	
	UPROPERTY(EditAnywhere, Category=MaterialOverrides)
	FMaterialInstanceBasePropertyOverrides BasePropertyOverrides;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceConstant> SourceInstance;

	UPROPERTY()
	TObjectPtr<class UMaterialFunctionInstance> SourceFunction;	

	UPROPERTY(transient, duplicatetransient)
	TArray<FMaterialParameterInfo> VisibleExpressions;

	/** The Lightmass override settings for this object. */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	struct FLightmassParameterizedMaterialSettings LightmassSettings;

	/** Should we use old style typed arrays for unassigned parameters instead of a None group (new style)? */
	UPROPERTY(EditAnywhere, Category=MaterialEditorInstanceConstant)
	uint32 bUseOldStyleMICEditorGroups:1;

	/** When set we will use the override from NaniteOverrideMaterial. Otherwise we inherit any override on the parent. */
	UPROPERTY(EditAnywhere, Category = MaterialEditorInstanceConstant, meta = (InlineEditConditionToggle))
	uint32 bNaniteOverride : 1;

	/** An override material which will be used instead of this one when rendering with nanite. */
	UPROPERTY(EditAnywhere, Category = MaterialEditorInstanceConstant, meta = (editcondition = "bNaniteOverride"))
	TObjectPtr<UMaterialInterface> NaniteOverrideMaterial;

	//~ Begin UObject Interface.
	UNREALED_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	UNREALED_API virtual void PostEditUndo() override;
#endif
	//~ End UObject Interface.

	/** Regenerates the parameter arrays. */
	UNREALED_API void RegenerateArrays();
#if WITH_EDITOR
	/** Sets back to zero the overrides for any parameters copied out of the layer stack */
	UNREALED_API void CleanParameterStack(int32 Index, EMaterialParameterAssociation MaterialType);
	/** Copies the overrides for any parameters copied out of the layer stack from the layer or blend */
	UNREALED_API void ResetOverrides(int32 Index, EMaterialParameterAssociation MaterialType);
#endif
	/** Copies the parameter array values back to the source instance. */
	UNREALED_API void CopyToSourceInstance(const bool bForceStaticPermutationUpdate = false);

	UNREALED_API void ApplySourceFunctionChanges();

	/** 
	 * Sets the source instance for this object and regenerates arrays. 
	 *
	 * @param MaterialInterface		Instance to use as the source for this material editor instance.
	 */
	UNREALED_API void SetSourceInstance(UMaterialInstanceConstant* MaterialInterface);

	UNREALED_API void CopyBasePropertiesFromParent();

	UNREALED_API void SetSourceFunction(UMaterialFunctionInstance* MaterialFunction);

	/** 
	 * Update the source instance parent to match this
	 */
	UNREALED_API void UpdateSourceInstanceParent();

	/** 
	 *  Returns group for parameter. Creates one if needed. 
	 *
	 * @param ParameterGroup		Name to be looked for.
	 */
	UNREALED_API FEditorParameterGroup & GetParameterGroup(FName& ParameterGroup);
	/** 
	 *  Creates/adds value to group retrieved from parent material . 
	 *
	 * @param ParameterValue		Current data to be grouped
	 * @param GroupName				Name of the group
	 */
	UNREALED_API void AssignParameterToGroup(UDEditorParameterValue* ParameterValue, const FName& GroupName);

	static UNREALED_API FName GlobalGroupPrefix;

	TWeakPtr<class IDetailsView> DetailsView;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TObjectPtr<class UMaterialInstanceConstant>> StoredLayerPreviews;

	UPROPERTY()
	TArray<TObjectPtr<class UMaterialInstanceConstant>> StoredBlendPreviews;
#endif

	/** Whether or not we should show only overridden properties*/
	bool bShowOnlyOverrides;
};



