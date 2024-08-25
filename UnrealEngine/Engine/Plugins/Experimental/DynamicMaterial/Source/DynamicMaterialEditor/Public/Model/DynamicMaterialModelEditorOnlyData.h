// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "DMEDefs.h"
#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "Misc/NotifyHook.h"
#include "UObject/WeakObjectPtrFwd.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"
#include "DynamicMaterialModelEditorOnlyData.generated.h"

class UDMMaterialComponent;
class UDMMaterialParameter;
class UDMMaterialProperty;
class UDMMaterialSlot;
class UDMMaterialValue;
class UDMMaterialValueFloat1;
class UDMTextureUV;
class UDynamicMaterialModel;
class UDynamicMaterialModelEditorOnlyData;
class UMaterialExpression;
struct FDMComponentPathSegment;
struct FDMComponentPath;
struct FDMMaterialBuildState;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialBuilt, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnValueListUpdated, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnValueUpdated, UDynamicMaterialModel*, UDMMaterialValue*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnSlotListUpdated, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnTextureUVUpdated, UDynamicMaterialModel*, UDMTextureUV*);

UENUM(BlueprintType)
enum class EDMState : uint8
{
	Idle,
	Building
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDynamicMaterialModelEditorOnlyData : public UObject, public IDynamicMaterialModelEditorOnlyDataInterface, public FNotifyHook, public IDMBuildable
{
	GENERATED_BODY()

	friend class UDynamicMaterialModelFactory;

public:
	static const FString SlotsPathToken;
	static const FString RGBSlotPathToken; // @TODO This will probably need to change when other slot types are opened up.
	static const FString OpacitySlotPathToken; // @TODO This will probably need to change when other slot types are opened up.
	static const FString PropertiesPathToken;

	static UDynamicMaterialModelEditorOnlyData* Get(UDynamicMaterialModel* InModel);
	static UDynamicMaterialModelEditorOnlyData* Get(TWeakObjectPtr<UDynamicMaterialModel> InModelWeak);
	static UDynamicMaterialModelEditorOnlyData* Get(const TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>& InInterface);
	static UDynamicMaterialModelEditorOnlyData* Get(IDynamicMaterialModelEditorOnlyDataInterface* InInterface);

	UDynamicMaterialModelEditorOnlyData();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const { return MaterialModel; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetGeneratedMaterial() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EMaterialDomain> GetDomain() const { return Domain; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDomain(TEnumAsByte<EMaterialDomain> InDomain);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EBlendMode> GetBlendMode() const { return BlendMode; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetBlendMode(TEnumAsByte<EBlendMode> InBlendMode);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialShadingModel GetShadingModel() const { return ShadingModel; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetShadingModel(EDMMaterialShadingModel InShadingModel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void OpenMaterialEditor() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, UDMMaterialProperty*> GetMaterialProperties() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialProperty* GetMaterialProperty(EDMMaterialPropertyType MaterialProperty) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<UDMMaterialSlot*>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlot(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialSlot* AddSlot();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RemoveSlot(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlotForMaterialProperty(EDMMaterialPropertyType Property) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TArray<EDMMaterialPropertyType> GetMaterialPropertiesForSlot(UDMMaterialSlot* Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void AssignMaterialPropertyToSlot(EDMMaterialPropertyType Property, UDMMaterialSlot* Slot);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void UnassignMaterialProperty(EDMMaterialPropertyType Property);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsPixelAnimationFlagSet() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPixelAnimationFlag(bool bInFlagValue);
	
	FDMOnMaterialBuilt& GetOnMaterialBuiltDelegate() { return OnMaterialBuiltDelegate; }
	FDMOnValueListUpdated& GetOnValueListUpdateDelegate() { return OnValueListUpdateDelegate; }
	FDMOnValueUpdated& GetOnValueUpdateDelegate() { return OnValueUpdateDelegate; }
	FDMOnSlotListUpdated& GetOnSlotListUpdateDelegate() { return OnSlotListUpdateDelegate; }
	FDMOnTextureUVUpdated& GetOnTextureUVUpdateDelegate() { return OnTextureUVUpdateDelegate; }

	void GenerateOpacityExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState, UDMMaterialSlot* InFromSlot,
		EDMMaterialPropertyType InFromProperty, UMaterialExpression*& OutExpression, int32& OutOutputIndex, int32& OutOutputChannel) const;

	TSharedRef<FDMMaterialBuildState> CreateBuildState(UMaterial* InMaterialToBuild, bool bInDirtyAssets = true) const;

	//~ Begin FNotifyHook
	virtual void NotifyPreChange(class FEditPropertyChain* PropertyAboutToChange) {}
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged);
	//~ End FNotifyHook

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	//~ End UObject

	void SaveEditor();

	//~ Begin IDMBuildable
	virtual void DoBuild_Implementation(bool bInDirtyAssets) override;
	//~ End IDMBuildable

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	virtual void PostEditorDuplicate() override;
	virtual void RequestMaterialBuild() override;
	virtual void OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType) override;
	virtual void OnValueListUpdate() override;
	virtual void OnTextureUVUpdated(UDMTextureUV* InTextureUV) override;
	virtual void LoadDeprecatedModelData(UDynamicMaterialModel* InMaterialModel) override;
	virtual TSharedRef<IDMMaterialBuildStateInterface> CreateBuildStateInterface(UMaterial* InMaterialToBuild) const override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDynamicMaterialModel> MaterialModel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	EDMState State = EDMState::Idle;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TEnumAsByte<EMaterialDomain> Domain = EMaterialDomain::MD_Surface;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TEnumAsByte<EBlendMode> BlendMode = EBlendMode::BLEND_Translucent;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialShadingModel ShadingModel = EDMMaterialShadingModel::Unlit;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>> Properties;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, TObjectPtr<UDMMaterialSlot>> PropertySlotMap;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialSlot>> Slots;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UMaterialExpression>> Expressions;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bCreateMaterialPackage;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bPixelAnimationFlag;

	FDMOnMaterialBuilt OnMaterialBuiltDelegate;
	FDMOnValueListUpdated OnValueListUpdateDelegate;
	FDMOnValueUpdated OnValueUpdateDelegate;
	FDMOnSlotListUpdated OnSlotListUpdateDelegate;
	FDMOnTextureUVUpdated OnTextureUVUpdateDelegate;

	void CreateMaterial();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void BuildMaterial(bool bInDirtyAssets);

	FString GetMaterialAssetPath() const;
	FString GetMaterialAssetName() const;
	FString GetMaterialPackageName(const FString& MaterialBaseName) const;

	void OnSlotConnectorsUpdated(UDMMaterialSlot* Slot);

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	virtual void ReinitComponents() override;
	virtual void ResetData() override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

	void LoadDeprecatedModelData_Base(bool bInCreateMaterialPackage, EBlendMode InBlendMode, EDMMaterialShadingModel InShadingModel);
	void LoadDeprecatedModelData_Expressions(TArray<TObjectPtr<UMaterialExpression>>& InExpressions);
	void LoadDeprecatedModelData_Properties(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InProperties);
	void LoadDeprecatedModelData_Slots(TArray<TObjectPtr<UObject>>& InSlots);
	void LoadDeprecatedModelData_PropertySlotMap(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InPropertySlotMap);
};
