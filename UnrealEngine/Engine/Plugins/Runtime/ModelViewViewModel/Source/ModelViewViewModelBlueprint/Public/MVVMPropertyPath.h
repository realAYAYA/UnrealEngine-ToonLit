// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Types/MVVMBindingName.h"
#include "Types/MVVMFieldVariant.h"
#include "UObject/Class.h"

#include "MVVMPropertyPath.generated.h"

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
	/** Get the binding name, resolves reference deprecation / redirectors / etc before returning */
	FName GetFieldName() const
	{
		// Resolve any redirectors
		if (!BindingReference.GetMemberName().IsNone())
		{
			if (BindingKind == EBindingKind::Property)
			{
				if (BindingReference.IsLocalScope())
				{
					if (UPackage* Package = BindingReference.GetMemberParentPackage())
					{
						UObjectBase* FoundObject = FindObjectWithOuter(Package, UScriptStruct::StaticClass(), *BindingReference.GetMemberScopeName());
						if (UScriptStruct* Struct = Cast<UScriptStruct>(static_cast<UObject*>(FoundObject)))
						{
							if (const FProperty* FoundProperty = FindUFieldOrFProperty<FProperty>(Struct, BindingReference.GetMemberName(), EFieldIterationFlags::IncludeAll))
							{
								return FoundProperty->GetFName();
							}
						}
					}
				}
				else if (BindingReference.ResolveMember<FProperty>())
				{
					return BindingReference.GetMemberName();
				}
			}
			else if (BindingKind == EBindingKind::Function)
			{
				if (BindingReference.ResolveMember<UFunction>())
				{
					return BindingReference.GetMemberName();
				}
			}
		}

		return FName();
	}

	/** */
	UE::MVVM::FMVVMConstFieldVariant GetField() const
	{
		if (!BindingReference.GetMemberName().IsNone())
		{
			if (BindingKind == EBindingKind::Property)
			{
				if (BindingReference.IsLocalScope())
				{
					if (UPackage* Package = BindingReference.GetMemberParentPackage())
					{
						UObjectBase* FoundObject = FindObjectWithOuter(Package, UScriptStruct::StaticClass(), *BindingReference.GetMemberScopeName());
						if (UScriptStruct* Struct = Cast<UScriptStruct>(static_cast<UObject*>(FoundObject)))
						{
							if (const FProperty* FoundProperty = FindUFieldOrFProperty<FProperty>(Struct, BindingReference.GetMemberName(), EFieldIterationFlags::IncludeAll))
							{
								return UE::MVVM::FMVVMConstFieldVariant(FoundProperty);
							}
						}
					}
				}
				else
				{
					return UE::MVVM::FMVVMConstFieldVariant(BindingReference.ResolveMember<FProperty>());
				}
			}
			else if (BindingKind == EBindingKind::Function)
			{
				return UE::MVVM::FMVVMConstFieldVariant(BindingReference.ResolveMember<UFunction>());
			}
		}
		return UE::MVVM::FMVVMConstFieldVariant();
	}

	/** */
	void SetBindingReference(UE::MVVM::FMVVMConstFieldVariant InField)
	{
		if (InField.IsEmpty())
		{
			Reset();
			return;
		}

		BindingReference = CreateMemberReference(InField);

		if (InField.IsProperty())
		{
			BindingKind = EBindingKind::Property;
		}
		else if (InField.IsFunction())
		{
			BindingKind = EBindingKind::Function;
		}
		else
		{
			ensureAlwaysMsgf(false, TEXT("Binding to field of unknown type!"));
		}
	}

	/** */
	void Reset()
	{
		BindingReference = FMemberReference();
	}

	/** */
	void SetDeprecatedBindingReference(const FMemberReference& InBindingReference, EBindingKind InBindingKind)
	{
		BindingReference = InBindingReference;
		BindingKind = InBindingKind;
	}

public:
	bool operator==(const FMVVMBlueprintFieldPath& Other) const
	{
		return BindingReference.GetMemberName() == Other.BindingReference.GetMemberName();
	}
	bool operator!=(const FMVVMBlueprintFieldPath& Other) const
	{
		return !operator==(Other);
	}

private:
	/**
	 * Create a serializable member reference from this field.
	 */
	static FMemberReference CreateMemberReference(UE::MVVM::FMVVMConstFieldVariant InField)
	{
		FMemberReference BindingReference = FMemberReference();
		if (InField.IsProperty())
		{
			const FProperty* Property = InField.GetProperty();
			if (Property->GetOwnerClass())
			{
				BindingReference.SetFromField<FProperty>(Property, false);
			}
			else
			{
				BindingReference.SetGlobalField(Property->GetFName(), Property->GetOutermost());
				BindingReference.SetLocalMember(Property->GetFName(), Property->GetOwnerStruct(), FGuid());
			}
		}
		else if (InField.IsFunction())
		{
			// Functions should never be self context references
			const UFunction* Function = InField.GetFunction();
			bool bSelfContext = false;
			BindingReference.SetFromField<UFunction>(Function, bSelfContext);
		}
		return BindingReference;
	}
};

inline uint32 GetTypeHash(const FMVVMBlueprintFieldPath& FieldPath)
{
	return GetTypeHash(FieldPath.GetFieldName());
}

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
	UPROPERTY(EditAnywhere, Category = "MVVM")
	TArray<FMVVMBlueprintFieldPath> Paths;

	UPROPERTY(EditAnywhere, Category = "MVVM")
	FName WidgetName;

	UPROPERTY(EditAnywhere, Category = "MVVM")
	FGuid ContextId;

#if WITH_EDITORONLY_DATA
	// Use the Paths. BindingReference and BindingKind are deprecated.
	UPROPERTY()
	FMemberReference BindingReference;
	UPROPERTY()
	EBindingKind BindingKind = EBindingKind::Function;
#endif

public:
	/** Get the binding name, resolves reference deprecation / redirectors / etc before returning */
	TArray<FName> GetPaths() const
	{
		TArray<FName> Result;
		Result.Reserve(Paths.Num());

		for (const FMVVMBlueprintFieldPath& Path : Paths)
		{
			Result.Add(Path.GetFieldName());
		}

		return Result;
	}

	/** Get the binding name, resolves reference deprecation / redirectors / etc before returning */
	TArray<UE::MVVM::FMVVMConstFieldVariant> GetFields() const
	{
		TArray<UE::MVVM::FMVVMConstFieldVariant> Result;
		Result.Reserve(Paths.Num());

		for (const FMVVMBlueprintFieldPath& Path : Paths)
		{
			Result.Add(Path.GetField());
		}

		return Result;
	}

	bool BasePropertyPathContains(UE::MVVM::FMVVMConstFieldVariant Field) const
	{
		return Paths.ContainsByPredicate([Field](const FMVVMBlueprintFieldPath& FieldPath) { return FieldPath.GetField() == Field;  });
	}

	void SetBasePropertyPath(UE::MVVM::FMVVMConstFieldVariant InField)
	{
		ResetBasePropertyPath();
		AppendBasePropertyPath(InField);
	}

	void AppendBasePropertyPath(UE::MVVM::FMVVMConstFieldVariant InField)
	{
		if (!InField.IsEmpty() && !InField.GetName().IsNone())
		{
			Paths.AddDefaulted_GetRef().SetBindingReference(InField);
		}
	}

	void ResetBasePropertyPath()
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
		return !IsFromWidget() && !IsFromViewModel() && BindingReference.GetMemberName() == FName();
	}

	/**
	 * Get the full path without the first property name.
	 * returns Field.SubProperty.SubProperty from ViewModel.Field.SubProeprty.SubProperty
	 */
	FString GetBasePropertyPath() const
	{
		TStringBuilder<512> Result;
		for (const FMVVMBlueprintFieldPath& Path : Paths)
		{
			if (Result.Len() > 0)
			{
				Result << TEXT('.');
			}
			Result << Path.GetFieldName();
		}
		return Result.ToString();
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
			if (!BindingReference.GetMemberName().IsNone())
			{
				Paths.AddDefaulted_GetRef().SetDeprecatedBindingReference(BindingReference, BindingKind);
				BindingReference = FMemberReference();
			}
		}
	}
};

inline uint32 GetTypeHash(const FMVVMBlueprintPropertyPath& Path)
{
	uint32 Hash = 0;
	TArray<FName> Paths = Path.GetPaths();

	for (const FName& SubPath : Paths)
	{
		Hash = HashCombine(Hash, GetTypeHash(SubPath));
	}
	return Hash;
}

template<>
struct TStructOpsTypeTraits<FMVVMBlueprintPropertyPath> : public TStructOpsTypeTraitsBase2<FMVVMBlueprintPropertyPath>
{
	enum
	{
		WithPostSerialize = true,
	};
};