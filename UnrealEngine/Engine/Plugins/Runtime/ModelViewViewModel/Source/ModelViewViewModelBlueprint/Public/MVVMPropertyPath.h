// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/MemberReference.h"
#include "Types/MVVMFieldVariant.h"

#include "MVVMPropertyPath.generated.h"

class UBlueprint;

/**
 * A single item in a Property Path
 */
USTRUCT()
struct FMVVMBlueprintFieldPath
{
	GENERATED_BODY()

private:
	/** Reference to property for this binding. */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	FMemberReference BindingReference;

	/** If we are referencing a UFunction or FProperty */
	UPROPERTY()
	EBindingKind BindingKind = EBindingKind::Function;

public:
	FMVVMBlueprintFieldPath() = default;

	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintFieldPath(const UBlueprint* InContext, UE::MVVM::FMVVMConstFieldVariant InField);

	/** Get the binding name, resolves reference deprecation / redirectors / etc before returning */
	MODELVIEWVIEWMODELBLUEPRINT_API FName GetFieldName(const UClass* SelfContext) const;

	/** */
	MODELVIEWVIEWMODELBLUEPRINT_API UE::MVVM::FMVVMConstFieldVariant GetField(const UClass* SelfContext) const;

	bool IsFieldLocalScope() const
	{
		return BindingReference.IsLocalScope();
	}

	MODELVIEWVIEWMODELBLUEPRINT_API UClass* GetParentClass(const UClass* SelfContext) const;

	EBindingKind GetBindingKind() const
	{
		return BindingKind;
	}

#if WITH_EDITOR
	/** */
	void SetDeprecatedBindingReference(const FMemberReference& InBindingReference, EBindingKind InBindingKind);
	/** */
	void SetDeprecatedSelfReference(const UBlueprint* Context);
#endif

public:
	bool operator==(const FMVVMBlueprintFieldPath& Other) const
	{
		return BindingReference.IsSameReference(Other.BindingReference)
			&& BindingKind == Other.BindingKind;
	}
	bool operator!=(const FMVVMBlueprintFieldPath& Other) const
	{
		return !operator==(Other);
	}

private:
	UE::MVVM::FMVVMConstFieldVariant GetFieldInternal(const UClass* SelfContext) const;
};

/**
 * Base path to properties for MVVM view models and widgets.
 * 
 * Used to associate properties within MVVM bindings in editor & during MVVM compilation
 */
USTRUCT(BlueprintType)
struct FMVVMBlueprintPropertyPath
{
	GENERATED_BODY()

private:
	/** Reference to property for this binding. */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	TArray<FMVVMBlueprintFieldPath> Paths;

	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FName WidgetName;

	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FGuid ContextId;

#if WITH_EDITORONLY_DATA
	// Use the Paths. BindingReference and BindingKind are deprecated.
	UPROPERTY()
	FMemberReference BindingReference_DEPRECATED;
	UPROPERTY()
	EBindingKind BindingKind_DEPRECATED = EBindingKind::Function;
#endif

public:
	bool HasPaths() const
	{
		return Paths.Num() > 0;
	}

	TArrayView<FMVVMBlueprintFieldPath const> GetFieldPaths() const
	{
		return Paths;
	}

	/** Get the binding names, resolves reference deprecation / redirectors / etc before returning */
	MODELVIEWVIEWMODELBLUEPRINT_API TArray<FName> GetFieldNames(const UClass* SelfContext) const;

	/** Get the binding fields, resolves reference deprecation / redirectors / etc before returning */
	MODELVIEWVIEWMODELBLUEPRINT_API TArray<UE::MVVM::FMVVMConstFieldVariant> GetFields(const UClass* SelfContext) const;

	/**
	 * Get the full path without the first property name.
	 * returns Field.SubProperty.SubProperty from ViewModel.Field.SubProeprty.SubProperty
	 */
	MODELVIEWVIEWMODELBLUEPRINT_API FString GetPropertyPath(const UClass* SelfContext) const;

	MODELVIEWVIEWMODELBLUEPRINT_API bool HasFieldInLocalScope() const;

	bool PropertyPathContains(const UClass* InSelfContext, UE::MVVM::FMVVMConstFieldVariant Field) const
	{
		return Paths.ContainsByPredicate([InSelfContext, Field](const FMVVMBlueprintFieldPath& FieldPath) { return FieldPath.GetField(InSelfContext) == Field; });
	}

	void SetPropertyPath(const UBlueprint* InContext, UE::MVVM::FMVVMConstFieldVariant InField)
	{
		ResetPropertyPath();
		AppendPropertyPath(InContext, InField);
	}

	void AppendPropertyPath(const UBlueprint* InContext, UE::MVVM::FMVVMConstFieldVariant InField)
	{
		if (!InField.IsEmpty() && !InField.GetName().IsNone())
		{
			Paths.Emplace(InContext, InField);
		}
	}

	void ResetPropertyPath()
	{
		Paths.Reset();
	}

	void ResetSource()
	{
		ContextId = FGuid();
		WidgetName = FName();
	}

	bool IsFromWidget() const
	{
		return !WidgetName.IsNone();
	}

	bool IsFromViewModel() const
	{
		return ContextId.IsValid();
	}

	FGuid GetViewModelId() const
	{
		return ContextId;
	}

	void SetViewModelId(FGuid InViewModelId)
	{
		WidgetName = FName();
		ContextId = InViewModelId;
	}

	FName GetWidgetName() const
	{
		return WidgetName;
	}

	void SetWidgetName(FName InWidgetName)
	{
		ContextId = FGuid();
		WidgetName = InWidgetName;
	}

	bool IsEmpty() const
	{
		return !IsFromWidget() && !IsFromViewModel() && BindingReference_DEPRECATED.GetMemberName() == FName();
	}

	bool operator==(const FMVVMBlueprintPropertyPath& Other) const
	{
		return WidgetName == Other.WidgetName && 
			ContextId == Other.ContextId && 
			Paths == Other.Paths;
	}

	bool operator!=(const FMVVMBlueprintPropertyPath& Other) const
	{
		return !operator==(Other);
	}

	void PostSerialize(const FArchive& Ar)
	{
		if (Ar.IsLoading())
		{
			if (!BindingReference_DEPRECATED.GetMemberName().IsNone())
			{
				Paths.AddDefaulted_GetRef().SetDeprecatedBindingReference(BindingReference_DEPRECATED, BindingKind_DEPRECATED);
				BindingReference_DEPRECATED = FMemberReference();
			}
		}
	}
};

template<>
struct TStructOpsTypeTraits<FMVVMBlueprintPropertyPath> : public TStructOpsTypeTraitsBase2<FMVVMBlueprintPropertyPath>
{
	enum
	{
		WithPostSerialize = true,
	};
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Types/MVVMBindingName.h"
#endif
