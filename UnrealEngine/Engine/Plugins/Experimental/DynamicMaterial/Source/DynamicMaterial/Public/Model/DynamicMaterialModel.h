// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Containers/ContainersFwd.h"

#if WITH_EDITOR
#include "UObject/ScriptInterface.h"
#endif

#include "DynamicMaterialModel.generated.h"

class UDMMaterialComponent;
class UDMMaterialParameter;
class UDMMaterialValue;
class UDMMaterialValueFloat1;
class UDMTextureUV;
class UDynamicMaterialInstance;
enum class EDMMaterialPropertyType : uint8;
enum class EDMMaterialShadingModel : uint8;
enum class EDMUpdateType : uint8;
enum class EDMValueType : uint8;
struct FDMComponentPath;

#if WITH_EDITORONLY_DATA
class IDynamicMaterialModelEditorOnlyDataInterface;
class UDMMaterialProperty;
class UDMMaterialSlot;
class UMaterial;
class UMaterialExpression;
enum EBlendMode : int;
#endif

UCLASS(ClassGroup = "Material Designer", DefaultToInstanced, BlueprintType, meta = (DisplayThumbnail = "true"))
class DYNAMICMATERIAL_API UDynamicMaterialModel : public UObject
{
	GENERATED_BODY()

public:
	static const FString ValuesPathToken;
	static const FString ParametersPathToken;
	static const FName GlobalOpacityParameterName;

	UDynamicMaterialModel();

	bool IsModelValid() const;

	UDynamicMaterialInstance* GetDynamicMaterialInstance() const { return DynamicMaterialInstance; }
	void SetDynamicMaterialInstance(UDynamicMaterialInstance* InDynamicMaterialInstance);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetGeneratedMaterial() const { return DynamicMaterial; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValueFloat1* GetGlobalOpacityValue() const { return GlobalOpacityValue; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialComponent* GetComponentByPath(const FString& InPath) const;

	UDMMaterialComponent* GetComponentByPath(FDMComponentPath& InPath) const;

	template<typename InComponentClass>
	UDMMaterialComponent* GetComponentByPath(FDMComponentPath& InPath) const
	{
		return Cast<InComponentClass>(GetComponentByPath(InPath));
	}

#if WITH_EDITOR
	friend class UDynamicMaterialModelEditorOnlyData;
	friend class UDynamicMaterialModelFactory;

	UFUNCTION(BlueprintPure, Category = "Material Designer", Meta = (DisplayName = "Get Editor Only Data"))
	TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface> BP_GetEditorOnlyData() const { return EditorOnlyDataSI; }

	IDynamicMaterialModelEditorOnlyDataInterface* GetEditorOnlyData() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<UDMMaterialValue*>& GetValues() const { return Values; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValue* GetValueByName(FName InName) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValue* GetValueByIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialValue* AddValue(EDMValueType InType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RemoveValueByName(FName InName);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RemoveValueByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasParameterName(FName InParameterName) const;

	/** Creates a new parameter and assigns it a unique name. */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialParameter* CreateUniqueParameter(FName InBaseName);

	/** Updates the name on an existing parameter. */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RenameParameter(UDMMaterialParameter* InParameter, FName InBaseName);

	/** Removes parameter by the name assigned to this parameter object. */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void FreeParameter(UDMMaterialParameter* InParameter);

	/**
	 * Removes this specific object from the parameter map if the name is in use by a different parameter.
	 * Returns true if, after this call, the object is not in the parameter map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool ConditionalFreeParameter(UDMMaterialParameter* InParameter);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void ResetData();

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	//~ End UObject

	void PostEditorDuplicate();
#endif

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialValue>> Values;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDMMaterialValueFloat1> GlobalOpacityValue;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDMMaterialParameter> GlobalOpacityParameter;

	UPROPERTY(VisibleInstanceOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TMap<FName, TWeakObjectPtr<UDMMaterialParameter>> ParameterMap;

	UPROPERTY(VisibleInstanceOnly, Instanced, BlueprintReadOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> DynamicMaterial = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UDynamicMaterialInstance> DynamicMaterialInstance;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface> EditorOnlyDataSI;
#endif

#if WITH_EDITOR
	FName CreateUniqueParameterName(FName InBaseName);

	void ReinitComponents();

	void FixGlobalOpacityVars();
#endif

	/**
	 * These properties used to exist on the editor-only subclass of this model.
	 * They now exist on the editor-only component of this model.
	 */

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	TEnumAsByte<EBlendMode> BlendMode;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	EDMMaterialShadingModel ShadingModel;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	TMap<EDMMaterialPropertyType, TObjectPtr<UObject>> Properties;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	TMap<EDMMaterialPropertyType, TObjectPtr<UObject>> PropertySlotMap;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	TArray<TObjectPtr<UObject>> Slots;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	TArray<TObjectPtr<UMaterialExpression>> Expressions;

	UE_DEPRECATED(5.3, "Moved to editor-only subobject.")
	UPROPERTY()
	bool bCreateMaterialPackage;
#endif
};
