// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Binding/DynamicPropertyPath.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "FieldNotification/FieldId.h"

#include "WidgetBlueprintGeneratedClass.generated.h"

class UWidget;
class UUserWidget;
class UWidgetAnimation;
class UWidgetBlueprintGeneratedClassExtension;
class UWidgetTree;

UENUM()
enum class EBindingKind : uint8
{
	Function,
	Property
};

USTRUCT()
struct FDelegateRuntimeBinding
{
	GENERATED_USTRUCT_BODY()

	/** The widget that will be bound to the live data. */
	UPROPERTY()
	FString ObjectName;

	/** The property on the widget that will have a binding placed on it. */
	UPROPERTY()
	FName PropertyName;

	/** The function or property we're binding to on the source object. */
	UPROPERTY()
	FName FunctionName;

	/**  */
	UPROPERTY()
	FDynamicPropertyPath SourcePath;

	/** The kind of binding we're performing, are we binding to a property or a function. */
	UPROPERTY()
	EBindingKind Kind = EBindingKind::Property;
};


/**
 * The widget blueprint generated class allows us to create blueprint-able widgets for UMG at runtime.
 * All WBPGC's are of UUserWidget classes, and they perform special post initialization using this class
 * to give themselves many of the same capabilities as AActor blueprints, like dynamic delegate binding for
 * widgets.
 */
UCLASS()
class UMG_API UWidgetBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()
	friend class FWidgetBlueprintCompilerContext;

public:
	UWidgetBlueprintGeneratedClass();

private:

	/** A tree of the widget templates to be created */
	UPROPERTY()
	TObjectPtr<UWidgetTree> WidgetTree;
	
	/** The extension that are considered static to the class */
	UPROPERTY()
	TArray<TObjectPtr<UWidgetBlueprintGeneratedClassExtension>> Extensions;

	/** List Field Notifies. No index here on purpose to prevent saving them. */
	UPROPERTY()
	TArray<FFieldNotificationId> FieldNotifyNames;

	int32 FieldNotifyStartBitNumber;

	/** The classes native parent requires a native tick */
	UPROPERTY()
	uint32 bClassRequiresNativeTick :1;

#if WITH_EDITORONLY_DATA
public:
	UPROPERTY(Transient)
	uint32 bCanCallPreConstruct : 1;
#endif

public:
	UPROPERTY()
	TArray< FDelegateRuntimeBinding > Bindings;

	UPROPERTY()
	TArray< TObjectPtr<UWidgetAnimation> > Animations;

	/**
	 * All named slots, even the ones that have content already filled into them by a parent class and are not
	 * available for extension.
	 **/
	UPROPERTY()
	TArray<FName> NamedSlots;

	/**
	 * Available Named Slots for content in a subclass.  These are slots that are accumulated from all super
	 * classes on compile.  They will exclude any named slots that are filled by a parent class.
	 **/
	UPROPERTY()
	TArray<FName> AvailableNamedSlots;

	/**
	 * These are the set of named slots that can be used on an instance of the widget.  This set is slightly
	 * different from available named slots, because ones designated UNamedSlot::bExposeOnInstanceOnly == true
	 * will also be in this list, even though they wont be in AvailableNamedSlots, if are inherited, as inherited
	 * named slots do not have the capability to remove existing content in a named slot.
	 **/
	UPROPERTY()
	TArray<FName> InstanceNamedSlots;
	
public:
	UWidgetTree* GetWidgetTreeArchetype() const { return WidgetTree; }
	void SetWidgetTreeArchetype(UWidgetTree* InWidgetTree);

	void GetNamedSlotArchetypeContent(TFunctionRef<void(FName /*SlotName*/, UWidget* /*Content*/)> Predicate) const;

	// Walks up the hierarchy looking for a valid widget tree.
	UWidgetBlueprintGeneratedClass* FindWidgetTreeOwningClass() const;

	// Execute the callback for every FieldId defined in the BP class
	void ForEachField(TFunctionRef<bool(::UE::FieldNotification::FFieldId FielId)> Callback) const;

	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual bool NeedsLoadForServer() const override;
	virtual void PostLoadDefaultObject(UObject* Object) override;
	//~ End UObject interface

	virtual void PurgeClass(bool bRecompilingOnLoad) override;

	/**
	 * This is the function that makes UMG work.  Once a user widget is constructed, it will post load
	 * call into its generated class and ask to be initialized.  The class will perform all the delegate
	 * binding and wiring necessary to have the user's widget perform as desired.
	 */
	void InitializeWidget(UUserWidget* UserWidget) const;

	void InitializeFieldNotification(const UUserWidget*);

	static void InitializeWidgetStatic(UUserWidget* UserWidget
		, const UClass* InClass
		, UWidgetTree* InWidgetTree
		, const UClass* InWidgetTreeWidgetClass
		, const TArrayView<UWidgetAnimation*> InAnimations
		, const TArrayView<const FDelegateRuntimeBinding> InBindings);

	bool ClassRequiresNativeTick() const { return bClassRequiresNativeTick; }

#if WITH_EDITOR
	void SetClassRequiresNativeTick(bool InClassRequiresNativeTick);
#endif

	/** Find the first extension of the requested type. */
	template<typename ExtensionType>
	ExtensionType* GetExtension()
	{
		return Cast<ExtensionType>(GetExtension(ExtensionType::StaticClass()));
	}

	/** Find the first extension of the requested type. */
	UWidgetBlueprintGeneratedClassExtension* GetExtension(TSubclassOf<UWidgetBlueprintGeneratedClassExtension> InExtensionType);

	/** Find the extensions of the requested type. */
	TArray<UWidgetBlueprintGeneratedClassExtension*> GetExtensions(TSubclassOf<UWidgetBlueprintGeneratedClassExtension> InExtensionType);

	template<typename Predicate>
	void ForEachExtension(Predicate Pred) const
	{
		for (UWidgetBlueprintGeneratedClassExtension* Extension : Extensions)
		{
			check(Extension);
			Pred(Extension);
		}
	}

private:
	static void InitializeBindingsStatic(UUserWidget* UserWidget, const TArrayView<const FDelegateRuntimeBinding> InBindings, const TMap<FName, FObjectPropertyBase*>& InPropertyMap);
	static void BindAnimationsStatic(UUserWidget* Instance, const TArrayView<UWidgetAnimation*> InAnimations, const TMap<FName, FObjectPropertyBase*>& InPropertyMap);
};
