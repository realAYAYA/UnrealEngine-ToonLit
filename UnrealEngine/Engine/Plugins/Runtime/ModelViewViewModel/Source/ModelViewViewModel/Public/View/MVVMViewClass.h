// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Bindings/MVVMCompiledBindingLibrary.h"
#include "Extensions/WidgetBlueprintGeneratedClassExtension.h"
#include "Types/MVVMExecutionMode.h"
#include "Types/MVVMViewModelContext.h"

#include "UObject/Package.h"
#include "MVVMViewClass.generated.h"


class UMVVMUserWidgetBinding;
class UMVVMView;
class UMVVMViewClass;
class UMVVMViewModelBlueprintExtension;
class UMVVMViewModelContextResolver;
class UUserWidget;

namespace UE::MVVM::Private
{
	struct FMVVMViewBlueprintCompiler;
}

/**
 * Shared data to find or create a ViewModel at runtime.
 */
USTRUCT()
struct FMVVMViewClass_SourceCreator
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	UObject* CreateInstance(const UMVVMViewClass* ViewClass, UMVVMView* View, UUserWidget* UserWidget) const;

	void DestroyInstance(const UObject* ViewModel, const UMVVMView* View) const;

	UClass* GetSourceClass() const
	{
		return ExpectedSourceType.Get();
	}

	bool IsSourceAUserWidgetProperty() const
	{
		return (Flags & (uint8)ESourceFlags::IsUserWidgetProperty) != 0;
	}
	
	bool CanBeSet() const
	{
		return (Flags & (uint8)ESourceFlags::CanBeSet) != 0;
	}

	bool CanBeEvaluated() const
	{
		return (Flags & (uint8)ESourceFlags::CanBeEvaluated) != 0;
	}

	FName GetSourceName() const
	{
		return PropertyName;
	}
	
	FName GetParentSourceName() const
	{
		return ParentSourceName;
	}

#if UE_WITH_MVVM_DEBUGGING
	struct FToStringArgs
	{
		bool bUseDisplayName = true;
		bool bAddCreationMode = true;
		bool bAddFlags = true;

		MODELVIEWVIEWMODEL_API static FToStringArgs Short();
		MODELVIEWVIEWMODEL_API static FToStringArgs All();
	};

	/** @return a human readable version of the source that can be use for debugging purposes. */
	MODELVIEWVIEWMODEL_API FString ToString(const FMVVMCompiledBindingLibrary& CompiledBindingLibrary, FToStringArgs Args) const;
#endif

#if WITH_EDITOR
	MODELVIEWVIEWMODEL_API void PostSerialize(const FArchive& Ar);
#endif

private:
	/** Class type to create a source at runtime. */
	UPROPERTY()
	TSubclassOf<UObject> ExpectedSourceType;

	/** The resolver to fetch the source at runtime. */
	UPROPERTY(Instanced)
	TObjectPtr<UMVVMViewModelContextResolver> Resolver = nullptr;

	/** Info to find the ViewModel instance at runtime. */
	UPROPERTY()
	FMVVMViewModelContext GlobalViewModelInstance;

	/**
	 * A resolvable path to retrieve the source instance at runtime.
	 * It can be a path "Property = Object.Function.Object".
	 * It can be a UFunction's name of a FProperty's name.
	 */
	UPROPERTY()
	FMVVMVCompiledFieldPath FieldPath;

	/** The source name and the property's name of the view (if the flag IsUserWidgetProperty is set). */
	UPROPERTY()
	FName PropertyName;

	/** The name of the parent source if it's the dynamic source. */
	UPROPERTY()
	FName ParentSourceName;

	enum class ESourceFlags : uint8
	{
		None = 0,
		TypeCreateInstance = 1 << 0,
		IsUserWidgetProperty = 1 << 1,
		IsOptional = 1 << 2,
		CanBeSet = 1 << 3,
		CanBeEvaluated = 1 << 4,
	};

	UPROPERTY()
	uint8 Flags = (uint8)ESourceFlags::None;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bCreateInstance_DEPRECATED = false;

	UPROPERTY()
	bool bIsUserWidgetProperty_DEPRECATED = true;

	UPROPERTY()
	bool bOptional_DEPRECATED = false;
#endif
};

#if WITH_EDITOR
template<>
struct TStructOpsTypeTraits<FMVVMViewClass_SourceCreator> : public TStructOpsTypeTraitsBase2<FMVVMViewClass_SourceCreator>
{
	enum
	{
		WithPostSerialize = true,
	};
};
#endif


/**
 * A compiled and shared binding for ViewModel<->View
 */
USTRUCT()
struct MODELVIEWVIEWMODEL_API FMVVMViewClass_CompiledBinding
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	/** @return The id for the FieldId on the source (if forward) or the destination (if backward). */
	FMVVMVCompiledFieldId GetSourceFieldId() const
	{
		return FieldId;
	}

	/**
	 * @return The unique name of the source object that contains the SourceFieldId.
	 * It implements INotifyFieldValueChanged if it's not a One Time.
	 * It can also be the name of a FProperty on the source object.
	 */
	FName GetSourceName() const
	{
		return SourcePropertyName;
	}

	/** @return The SourceCreator index when the binding is of the EvaluateSourceCreator type. */
	int32 GetEvaluateSourceCreatorBindingIndex() const
	{
		return EvaluateSourceCreatorIndex;
	}

	/** @return true if SourceFieldId is not from a property but the object is the UserWidget. */
	bool IsSourceUserWidget() const
	{
		return (Flags & EBindingFlags::SourceObjectIsSelf) != 0;
	}

	/** @return the binding. From source to destination (if forward) or from destination to source (if backward). */
	const FMVVMVCompiledBinding& GetBinding() const
	{
		return Binding;
	}

	/** @return true if this Binding should be executed at initialization. */
	bool NeedsExecutionAtInitialization() const
	{
		return (Flags & EBindingFlags::ExecuteAtInitialization) != 0;
	}

	/** @return true if this Binding should be executed once (at initialization) but should not be executed when the SourceFieldId value changes. */
	bool IsOneTime() const
	{
		return (Flags & EBindingFlags::OneTime) != 0;
	}

	/** @return true if the binding is enabled by default. */
	bool IsEnabledByDefault() const
	{
		return (Flags & EBindingFlags::EnabledByDefault) != 0;
	}

	/** @return true if it's normal that the binding could not find it's source when registering it. */
	bool IsRegistrationOptional() const
	{
		return (Flags & EBindingFlags::ViewModelOptional) != 0;
	}
	
	/** @return true if the binding is not valid and we should only evaluate the source creator. */
	bool IsEvaluateSourceCreatorBinding() const
	{
		return EvaluateSourceCreatorIndex != INDEX_NONE;
	}

	/**
	 * @return true if the binding use a conversion function and that the conversion function is complex.
	 * The conversion function is complex, there is no input. The inputs are calculated in the BP function.
	 */
	bool IsConversionFunctionComplex() const
	{
		return Binding.IsComplexFunction();
	}
	
	/** How the binding should be executed. */
	EMVVMExecutionMode GetExecuteMode() const;

#if UE_WITH_MVVM_DEBUGGING
	struct FToStringArgs
	{
		bool bUseDisplayName = true;
		bool bAddFieldPath = true;
		bool bAddFieldId = true;
		bool bAddFlags = true;
		bool bAddBindingId = true;

		MODELVIEWVIEWMODEL_API static FToStringArgs Short();
		MODELVIEWVIEWMODEL_API static FToStringArgs All();
	};


	UE_DEPRECATED(5.3, "ToString with no argument is deprecated.")
	FString ToString() const;

	/** @return a human readable version of the binding that can be use for debugging purposes. */
	FString ToString(const FMVVMCompiledBindingLibrary& CompiledBindingLibrary, FToStringArgs Args) const;
#endif

#if WITH_EDITOR
	FGuid GetEditorId() const
	{
		return EditorId;
	}
#endif

private:
	UPROPERTY()
	FMVVMVCompiledFieldId FieldId;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FName SourcePropertyName;

	UPROPERTY()
	FMVVMVCompiledBinding Binding;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	EMVVMExecutionMode ExecutionMode = EMVVMExecutionMode::Immediate;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	int8 EvaluateSourceCreatorIndex = INDEX_NONE;

	enum EBindingFlags
	{
		None = 0,
		ExecuteAtInitialization = 1 << 0,
		Unused01 = 1 << 1,
		OneTime = 1 << 2,
		EnabledByDefault = 1 << 3,
		/** The source (viewmodel) can be nullptr and the binding could failed and should not log a warning. */
		ViewModelOptional = 1 << 4,
		Unused02 = 1 << 5,
		/** In development, (when the Blueprint maybe not be compiled with the latest data), the ExecutionMode may not reflect the default project setting value. */
		OverrideExecuteMode = 1 << 6,
		/** When the source object is the object itself. */
		SourceObjectIsSelf = 1 << 7,
	};

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	uint8 Flags = EBindingFlags::None;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid EditorId;
#endif
};


/**
 * Shared between every instances of the same View class.
 */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewClass : public UWidgetBlueprintGeneratedClassExtension
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	//~ Begin UWidgetBlueprintGeneratedClassExtension
	virtual void Initialize(UUserWidget* UserWidget) override;
	virtual void Construct(UUserWidget* UserWidget) override;
	virtual void Destruct(UUserWidget* UserWidget) override;
#if WITH_EDITOR
	virtual void BeginDestroy() override;
#endif
	//~ End UWidgetBlueprintGeneratedClassExtension

public:
	/** Should it automatically execute the bindings when the view is constructed or they will be executed manually. */
	bool InitializeSourcesOnConstruct() const
	{
		return bInitializeSourcesOnConstruct;
	}
	/** Should it automatically execute the bindings when the view is constructed or they will be executed manually. */
	bool InitializeBindingsOnConstruct() const
	{
		return bInitializeBindingsOnConstruct;
	}
	
	/** Get the list of the needed ViewModel. */
	const TArrayView<const FMVVMViewClass_SourceCreator> GetViewModelCreators() const
	{
		return MakeArrayView(SourceCreators);
	}

	/** Get the container of all the bindings. */
	const FMVVMCompiledBindingLibrary& GetBindingLibrary() const
	{
		return BindingLibrary;
	}

	/**  */
	const TArrayView<const FMVVMViewClass_CompiledBinding> GetCompiledBindings() const
	{
		return MakeArrayView(CompiledBindings);
	}

	/** */
	const FMVVMViewClass_CompiledBinding& GetCompiledBinding(int32 Index) const
	{
		return CompiledBindings[Index];
	}

#if UE_WITH_MVVM_DEBUGGING
	void Log(FMVVMViewClass_SourceCreator::FToStringArgs SourceArgs, FMVVMViewClass_CompiledBinding::FToStringArgs BindingArgs) const;
#endif

private:
	/**  */
	TArrayView<FMVVMViewClass_CompiledBinding> GetCompiledBindings()
	{
		return MakeArrayView(CompiledBindings);
	}

#if WITH_EDITOR
	void HandleBlueprintCompiled();
#endif

private:
	/** Data to retrieve/create the sources (could be viewmodel, widget, ...). */
	UPROPERTY()
	TArray<FMVVMViewClass_SourceCreator> SourceCreators;

	/** */
	UPROPERTY()
	TArray<FMVVMViewClass_CompiledBinding> CompiledBindings;

	/** All the bindings shared between all the View instance. */
	UPROPERTY()
	FMVVMCompiledBindingLibrary BindingLibrary;

	/** */
	int32 ViewCounter = 0;

	/** */
	UPROPERTY()
	bool bInitializeSourcesOnConstruct = true;
	/** */
	UPROPERTY()
	bool bInitializeBindingsOnConstruct = true;

#if WITH_EDITORONLY_DATA
	FDelegateHandle BluerpintCompiledHandle;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMBindingName.h"
#include "View/MVVMView.h"
#endif
