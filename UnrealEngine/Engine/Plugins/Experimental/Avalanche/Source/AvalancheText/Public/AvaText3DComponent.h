// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTextDefs.h"
#include "Containers/Ticker.h"
#include "Font/AvaFont.h"
#include "Text3DComponent.h"
#include "AvaText3DComponent.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;

enum class EAvaTextRefreshReason : uint8
{
	NoRefresh                 = 0,
	GeometryChange            = 1 << 0,
	LayoutChange              = 1 << 1,
	MaterialInstancesChange   = 1 << 2,
	MaterialParametersChange  = 1 << 3,
	FullRefresh               = 0xFF
};
ENUM_CLASS_FLAGS(EAvaTextRefreshReason)

UCLASS(MinimalAPI, ClassGroup="Text3D", PrioritizeCategories=("Text", "Layout", "Geometry", "Style", "Materials"), meta=(BlueprintSpawnableComponent))
class UAvaText3DComponent : public UText3DComponent
{
	friend class FAvaTextVisualizer;
	friend class FAvaToolboxTextVisualizer;
	friend class FAvaTextComponentCustomization;

	GENERATED_BODY()

public:
	UAvaText3DComponent();

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetEnforceUpperCase(bool bInEnforceUpperCase);
	bool IsEnforcingUpperCase() const { return bEnforceUpperCase; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMotionDesignFont(const FAvaFont& InFont);
	const FAvaFont& GetMotionDesignFont() const { return MotionDesignFont; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetAlignment(FAvaTextAlignment InAlignment);
	const FAvaTextAlignment& GetAlignment() const { return Alignment; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetColoringStyle(const EAvaTextColoringStyle& InColoringStyle);
	EAvaTextColoringStyle GetColoringStyle() const { return ColoringStyle; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetColor(const FLinearColor& InColor);
	const FLinearColor& GetColor() const { return Color; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetExtrudeColor(const FLinearColor& InColor);
	const FLinearColor& GetExtrudeColor() const { return ExtrudeColor; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetBevelColor(const FLinearColor& InColor);
	const FLinearColor& GetBevelColor() const { return BevelColor; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetGradientSettings(const FAvaLinearGradientSettings& InGradientSettings);
	const FAvaLinearGradientSettings& GetGradientSettings() const { return GradientSettings; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetGradientColors(const FLinearColor& InColorA, const FLinearColor& InColorB);

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMainTexture(UTexture2D* InMainTexture);
	UTexture2D* GetMainTexture() const { return MainTexture; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetTiling(const FVector2D InTiling);
	const FVector2D& GetTiling() const { return Tiling; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetCustomMaterial(UMaterialInterface* InCustomMaterial);
	UMaterialInterface* GetCustomMaterial() const { return CustomMaterial; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetTranslucencyStyle(EAvaTextTranslucency InTranslucencyStyle);
	EAvaTextTranslucency GetTranslucencyStyle() const { return TranslucencyStyle; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetOpacity(float InOpacity);
	float GetOpacity() const { return Opacity; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMaskOrientation(EAvaMaterialMaskOrientation InMaskOrientation);
	EAvaMaterialMaskOrientation GetMaskOrientation() const { return MaskOrientation; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMaskSmoothness(float InMaskSmoothness);
	float GetMaskSmoothness() const { return MaskSmoothness; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMaskOffset(float InMaskOffset);
	float GetMaskOffset() const { return MaskOffset; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetMaskRotation(float InMaskRotation);
	float GetMaskRotation() const { return MaskRotation; }

	UFUNCTION(BlueprintSetter)
	AVALANCHETEXT_API void SetIsUnlit(bool bInIsUnlit);
	bool GetIsUnlit() const { return bIsUnlit; }

	using FOnGetMaterialWithSettings = TDelegate<UMaterialInterface*(const UMaterialInterface* InPreviousMaterial, const FAvaTextMaterialSettings& InSettings)>;

	AVALANCHETEXT_API FOnGetMaterialWithSettings& GetMaterialProviderDelegate();

	AVALANCHETEXT_API void ForEachMID(TUniqueFunction<void(UMaterialInstanceDynamic* InMID)>&& InFunc) const;

	AVALANCHETEXT_API FVector GetGradientDirection() const;

	AVALANCHETEXT_API void RefreshMaterialInstances();

protected:
	void RefreshText3D();
	void MarkRenderDirty();

	void MarkForGeometryRefresh();
	bool IsMarkedForGeometryRefresh() const;

	void MarkForLayoutRefresh();
	bool IsMarkedForLayoutRefresh() const;

	/** Mark this Text3D component so that at the next RefreshText3D call it will refresh both materials and their parameters*/
	void MarkForMaterialsFullRefresh();
	bool IsMarkedForMaterialRefresh() const;

	void MarkForMaterialParametersRefresh();
	bool IsMarkedForMaterialParametersRefresh() const;

	void RemoveRefreshReason(EAvaTextRefreshReason InRefreshReasonToRemove);

	void MarkForFullRefresh();
	void ClearRefreshReasons();

	void RegisterCallbacks();
	void RefreshParentRootComponent();
	void SetupParentRootAndCallbacks();

	void SetupTextMaterials(const EAvaTextColoringStyle& InColoringStyle);
	void SetupTextGeometry();
	void SetupTextLayout();

	void SetupSolidMaterial();
	void SetupGradientMaterial();
	void SetupCustomMaterial();
	void RefreshTranslucencyValues();
	void SetupTexturedMaterial();

	void RefreshStoredBoundsValues();

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostInitProperties() override;
	virtual void PostEditImport() override;
	//~ Begin UObject

	void PostDuplicateActions();

	/** Will schedule a Text Refresh to be executed by Core Ticker */
	void ScheduleTextRefreshOnTicker();

	void OnTextGeneratedActions();
	void OnTextGeometryGenerated();

	void OnRootComponentTransformUpdated(USceneComponent* InSceneComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleportType);

	/**
	 * This function can be used to obtain implicit data from the base UText3DComponent.
	 * 
	 * Previously, the logic from UAvaText3DComponent was handled by AAvaTextActor, which is now mostly a wrapper for UAvaText3DComponent.
	 * Therefore, previously saved AAvaTextActor assets will have UText3DComponent materials compatible with UAvaText3DComponent.
	 * This function extracts implicit information from those materials, and updates UAvaText3DComponent properties accordingly.
	 * Furthermore, materials themselves will be retrieved, so that this component won't have to store them as well.
	 * 
	 * Additionally, also text, font and layout information will be retrieved, to avoid potential data loss from older assets.
	 */
	void RetrieveDataFromBaseComponent();

	/** Retrieves font and text from the values stored in the component */
	void RetrieveCurrentTextAndFont();

	/** Retrieves alignment information from the values stored in the component */
	void RetrieveCurrentAlignment();

	/** Retrieves materials from the component, according to the current settings*/
	void RetrieveCurrentMaterials();

	void RetrieveCurrentColoringStyle();

	void RetrieveSolidMaterialInstances();
	void RetrieveGradientMaterialInstance();
	void RetrieveTexturedMaterialInstance();
	void RetrieveCustomMaterialReference();

	void RetrieveTranslucentMaterialParameters();
	void RetrieveMaskedMaterialParameters(const UMaterialInstanceDynamic* InMaterialInstance);
	void RetrieveSimpleTranslucentMaterialParameters(const UMaterialInstanceDynamic* InMaterialInstance);

	UMaterialInstanceDynamic* GetMIDWithSettings(const UMaterialInterface* InPreviousMaterial, const FAvaTextMaterialSettings& InSettings);

	EAvaTextMaterialFeatures GetMaterialFeaturesFromProperties() const;

	/**
	 * UMaterialInstanceDynamic references are transient.
	 * We need to retrieve them from material when loading assets using this component, in order to avoid having to create the material again.
	 * Refer to UAvaText3DComponent::RetrieveCurrentMaterials().
	 *
	 * Additionally, this helps when loading old versions of the AAvaTextActor, which was hosting all the custom properties now handled by this component (UAvaText3DComponent)
	 */

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TextMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExtrudeTextMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BevelTextMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TexturedTextMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GradientTextMaterialInstance;

	TWeakObjectPtr<USceneComponent> ParentRootComponent;

	FOnGetMaterialWithSettings OnGetMaterialWithSettingsDelegate;

	/** Used to filter which refreshes need be performed when calling RefreshText3D */
	EAvaTextRefreshReason TextRefreshReason;

	FVector LocalBoundsLowerLeftCorner;
	FVector LocalBoundsExtent;

	FDelegateHandle OnTextGeometryGeneratedDelegate;
	FDelegateHandle OnTransformUpdatedDelegate;
	FTSTicker::FDelegateHandle TickerHandle;

private:
#if WITH_EDITOR
	static void RegisterOnPropertyChangeFunctions();
#endif

	void RefreshOpacity() const;
	void RefreshColor() const;
	void RefreshExtrudeColor() const;
	void RefreshBevelColor() const;

	void RefreshAlignment();
	void RefreshMotionDesignFont();
	void RefreshHorizontalAlignment();
	void RefreshVerticalAlignment();
	void RefreshUnlit();
	void RefreshMasked();
	void RefreshGeometry();
	void RefreshLayout();
	void RefreshGradientValues();
	AVALANCHETEXT_API void RefreshGradient();
	void RefreshMaskValues();
	void RefreshMask();
	void RefreshMaterialGeometryParameterValues();

	void RefreshFormatting();

	void SetMaterialGeometryParameterValues(UMaterialInstanceDynamic* InMaterial, const FVector& TextScaleFactor) const;
	void SetMaskedMaterialValues(UMaterialInstanceDynamic* InMaterial);

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject
#endif

	//~ Begin USceneComponent
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual void OnAttachmentChanged() override;
	virtual void OnRegister() override;
	//~ End USceneComponent

	//~ Begin UText3DComponent
	virtual void FormatText(FText& InOutText) const override;
	//~ End UText3DComponent

	UPROPERTY(EditAnywhere, Getter="IsEnforcingUpperCase", Setter="SetEnforceUpperCase", Category="Text", meta=(DisplayAfter="Text", AllowPrivateAccess="true"))
	bool bEnforceUpperCase = false;

	UPROPERTY(EditAnywhere, Getter="GetMotionDesignFont", Setter="SetMotionDesignFont", Category="Text", meta=(DisplayAfter="Text", AllowPrivateAccess="true"))
	FAvaFont MotionDesignFont;

	UPROPERTY(EditAnywhere, Getter="GetAlignment", Setter="SetAlignment", Category="Layout", meta=(DisplayAfter="VerticalAlignment", AllowPrivateAccess="true"))
	FAvaTextAlignment Alignment;

	UPROPERTY(EditAnywhere, Getter="GetColoringStyle", Setter="SetColoringStyle", Category="Style", meta=(DisplayAfter="Text", AllowPrivateAccess="true"))
	EAvaTextColoringStyle ColoringStyle = EAvaTextColoringStyle::Solid;

	UPROPERTY(EditAnywhere, Getter="GetColor", Setter="SetColor", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::Solid", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, Getter="GetExtrudeColor", Setter="SetExtrudeColor", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::Solid && Extrude > 0.0", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	FLinearColor ExtrudeColor = FLinearColor::Gray;

	UPROPERTY(EditAnywhere, Getter="GetBevelColor", Setter="SetBevelColor", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::Solid && Bevel > 0.0", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	FLinearColor BevelColor = FLinearColor::Gray;

	UPROPERTY(EditAnywhere, Getter="GetGradientSettings", Setter="SetGradientSettings", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::Gradient", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	FAvaLinearGradientSettings GradientSettings;

	UPROPERTY(EditAnywhere, Getter="GetMainTexture", Setter="SetMainTexture", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::FromTexture", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	TObjectPtr<UTexture2D> MainTexture;

	UPROPERTY(EditAnywhere, Getter="GetTiling", Setter="SetTiling", Category="Style", meta=(Delta=0.1, EditCondition="ColoringStyle == EAvaTextColoringStyle::FromTexture", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	FVector2D Tiling;

	UPROPERTY(EditAnywhere, Getter="GetCustomMaterial", Setter="SetCustomMaterial", Category="Style", meta=(EditCondition="ColoringStyle == EAvaTextColoringStyle::CustomMaterial", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> CustomMaterial = nullptr;

	UPROPERTY(EditAnywhere, Getter="GetTranslucencyStyle", Setter="SetTranslucencyStyle", Category="Style", meta=(EditCondition="ColoringStyle != EAvaTextColoringStyle::CustomMaterial", EditConditionHides,DisplayAfter="Text", AllowPrivateAccess="true"))
	EAvaTextTranslucency TranslucencyStyle = EAvaTextTranslucency::None;

	UPROPERTY(EditAnywhere, Getter="GetOpacity", Setter="SetOpacity", Category="Style", meta=(ClampMin=0.0, ClampMax=1.0, EditCondition="TranslucencyStyle == EAvaTextTranslucency::Translucent && ColoringStyle != EAvaTextColoringStyle::CustomMaterial", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	float Opacity = 0.75f;

	UPROPERTY(EditAnywhere, Getter="GetMaskOrientation", Setter="SetMaskOrientation", Category="Style", meta=(EditCondition="TranslucencyStyle == EAvaTextTranslucency::GradientMask", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	EAvaMaterialMaskOrientation MaskOrientation = EAvaMaterialMaskOrientation::LeftRight;

	UPROPERTY(EditAnywhere, Getter="GetMaskSmoothness", Setter="SetMaskSmoothness", Category="Style", meta=(ClampMin=0.0, ClampMax=1.0, EditCondition="TranslucencyStyle == EAvaTextTranslucency::GradientMask", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	float MaskSmoothness = 0.1f;

	UPROPERTY(EditAnywhere, Getter="GetMaskOffset", Setter="SetMaskOffset", Category="Style", meta=(ClampMin=0.0, ClampMax=1.0, EditCondition="TranslucencyStyle == EAvaTextTranslucency::GradientMask", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	float MaskOffset = 0.5f;

	UPROPERTY(EditAnywhere, Getter="GetMaskRotation", Setter="SetMaskRotation", Category="Style", meta=(ClampMin=0.0, ClampMax=1.0, EditCondition="MaskOrientation == EAvaMaterialMaskOrientation::Custom && TranslucencyStyle == EAvaTextTranslucency::GradientMask", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	float MaskRotation = 0.0f;

	UPROPERTY(EditAnywhere, Getter="GetIsUnlit", Setter="SetIsUnlit", Category="Style", meta=(EditCondition="ColoringStyle != EAvaTextColoringStyle::Custom", EditConditionHides, DisplayAfter="Text", AllowPrivateAccess="true"))
	bool bIsUnlit = true;

#if WITH_EDITOR
	static inline TMap<FName, TFunction<void(UAvaText3DComponent*)>> PropertyChangeFunctions;
#endif
};
