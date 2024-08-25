// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "Components/DMMaterialLinkedComponent.h"
#include "DMDefs.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMaterialValue.generated.h"
 
class UDMMaterialParameter;
class UDynamicMaterialModel;
class UMaterial;
class UMaterialExpressionParameter;
class UMaterialInstanceDynamic;

#if WITH_EDITOR
class FAssetThumbnailPool;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class SWidget;
struct IDMMaterialBuildStateInterface;
#endif

class UDMMaterialValue;

/**
 * A value used inside a material. Can be exported as a material parameter.
 */
UCLASS(BlueprintType, Abstract, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Value"))
class DYNAMICMATERIAL_API UDMMaterialValue : public UDMMaterialLinkedComponent
{
	GENERATED_BODY()
 
	friend class UDynamicMaterialModel;

#if WITH_EDITOR
	friend class SDMMaterialEditor;
#endif
 
public:
	static const FString ParameterPathToken;
	static const TCHAR* ParameterNamePrefix;

#if WITH_EDITOR
	static const FName ValueName;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialValue* CreateMaterialValue(UDynamicMaterialModel* InMaterialModel, const FString& InName, EDMValueType InType, bool bInLocal);
#endif
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const;
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMValueType GetType() const { return Type; }
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FText GetTypeName() const;
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FText GetDescription() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsDefaultValue() const PURE_VIRTUAL(UDMMaterialValue::IsDefaultValue, return false;)

	bool CanResetToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle) const { return !IsDefaultValue(); }
 
	/**
	 * Subclasses should implement a SetDefaultValue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void ApplyDefaultValue() PURE_VIRTUAL(UDMMaterialValue::ApplyDefaultValue)

	void ResetToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle) { ApplyDefaultValue(); }
 
	/**
	 * Resets to the default default value. 0, nullptr, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void ResetDefaultValue() PURE_VIRTUAL(UDMMaterialValue::ResetDefaultValue)
#endif

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsLocal() const { return bLocal; }
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialParameter* GetParameter() const { return Parameter; }
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FName GetMaterialParameterName() const;
 
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetParameterName(FName InBaseName);

	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
		PURE_VIRTUAL(UDMMaterialStageSource::GenerateExpressions)
#endif
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	int32 FindIndex() const;
 
	int32 FindIndexSafe() const;
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetPreviewMaterial();
 
	/**
	 * Returns the output index (channel WHOLE_CHANNEL) if this expression has pre-masked outputs.
	 * Returns INDEX_NONE if it is not supported.
	 */
	virtual int32 GetInnateMaskOutput(int32 OutputChannels) const;

	TSharedPtr<IDetailTreeNode> GetDetailTreeNode();
	TSharedPtr<IPropertyHandle> GetPropertyHandle();

	virtual FName GetMainPropertyName() const { return NAME_None; }

	/** Return true if, when setting the base stage, the same value should be applied to the mask stage. */
	virtual bool IsWholeLayerValue() const { return false; }
#endif
 
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const;
 
	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType = EDMUpdateType::Value) override;
#if WITH_EDITOR
	virtual void DoClean() override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
#endif
	//~ End UDMMaterialComponent
 
#if WITH_EDITOR
	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostCDOContruct() override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	virtual void BeginDestroy() override;
	//~ End UObject
#endif
 
protected:
	static TMap<EDMValueType, TStrongObjectPtr<UClass>> TypeClasses;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMValueType Type;
 
	/**
	 * True: The value is local to the stage it is used in. 
	 * False: The value is a global value that can be used anywhere
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bLocal;

	/**
	 * The parameter name used to expose this value in a material.
	 * If it isn't provided, GetFName() will be used instead.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMMaterialParameter> Parameter;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> PreviewMaterial;
#endif

#if WITH_EDITOR 
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
	TSharedPtr<IDetailTreeNode> DetailTreeNode;
	TSharedPtr<IPropertyHandle> PropertyHandle;
#endif
 
	UDMMaterialValue(EDMValueType InType);
 
	virtual void OnValueUpdated(bool bInForceStructureUpdate);
 
	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent

	#if WITH_EDITOR
	void CreatePreviewMaterial();
	void UpdatePreviewMaterial();
 
	//~ Begin UDMMaterialComponent
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
 
	void EnsureDetailObjects();
#endif

private:
	UDMMaterialValue();
};
