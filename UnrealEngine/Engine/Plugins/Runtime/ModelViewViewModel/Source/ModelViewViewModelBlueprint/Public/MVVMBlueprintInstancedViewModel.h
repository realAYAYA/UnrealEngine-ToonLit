// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "PropertyBag.h"
#include "Templates/SubclassOf.h"
#include "ViewModel/MVVMInstancedViewModelGeneratedClass.h"
#include "MVVMBlueprintInstancedViewModel.generated.h"

/**
 *
 */
UCLASS(Abstract, Within = MVVMBlueprintView)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintInstancedViewModelBase : public UObject
{
	GENERATED_BODY()

public:
	UMVVMBlueprintInstancedViewModelBase();

	void GenerateClass(bool bForceGeneration);

	UClass* GetGeneratedClass() const
	{
		return GeneratedClass;
	}

protected:
	virtual void PreloadObjectsForCompilation();
	virtual bool IsClassDirty() const;
	virtual void CleanClass();
	virtual void AddProperties();
	virtual void ConstructClass();
	virtual void SetDefaultValues();
	virtual void ClassGenerated();

protected:
	struct FInitializePropertyArgs
	{
		bool bFieldNotify = false;
		bool bReadOnly = false;
		bool bNetwork = false;
		bool bPrivate = false;
	};

	bool IsValidFieldName(const FName NewPropertyName) const;
	bool IsValidFieldName(const FName NewPropertyName, UStruct* NewOwner) const;
	FProperty* CreateProperty(const FProperty* FromProperty, UStruct* NewOwner);
	FProperty* CreateProperty(const FProperty* FromProperty, UStruct* NewOwner, FName NewPropertyName);
	void InitializeProperty(FProperty* NewProperty, FInitializePropertyArgs& Args);
	void LinkProperty(FProperty* NewProperty) const;
	void LinkProperty(FProperty* NewProperty, UStruct* NewOwner) const;
	void AddOnRepFunction(FProperty* NewProperty);
	void SafeRename(UObject* Object);
	void SetDefaultValue(const FProperty* SourceProperty, void const* SourceValuePtr, const FProperty* DestinationProperty);

public:
	/** The base object of the generated class. */
	UPROPERTY(EditAnywhere, Category = "Viewmodel", meta=(AllowedClasses = "/Script/FieldNotification.NotifyFieldValueChanged", DisallowedClasses = "/Script/UMG.Widget"))
	TSubclassOf<UObject> ParentClass;

	UPROPERTY()
	TObjectPtr<UMVVMInstancedViewModelGeneratedClass> GeneratedClass;

protected:
	//~ if it changes, the GeneratedClass needs to be removed.
	UPROPERTY()
	TSubclassOf<UMVVMInstancedViewModelGeneratedClass> GeneratedClassType;
};

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintInstancedViewModel_PropertyBag : public UMVVMBlueprintInstancedViewModelBase
{
	GENERATED_BODY()

public:
	const UStruct* GetSourceStruct() const
	{
		return Variables.GetValue().GetScriptStruct();
}

	const uint8* GetSourceDefaults() const
	{
		return Variables.GetValue().GetMemory();
	}

protected:
	virtual bool IsClassDirty() const override;
	virtual void CleanClass() override;
	virtual void AddProperties() override;
	virtual void SetDefaultValues() override;
	virtual void ClassGenerated() override;

private:
	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	FInstancedPropertyBag Variables;

	UPROPERTY()
	uint64 PropertiesHash = 0;

	TMap<const FProperty*, FProperty*> FromPropertyToCreatedProperty;

public:
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#endif
