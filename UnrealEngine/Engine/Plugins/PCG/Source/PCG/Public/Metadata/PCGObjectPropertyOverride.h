// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGPin.h"
#include "Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "PCGObjectPropertyOverride.generated.h"

class IPCGAttributeAccessor;
struct FPCGContext;

/**
* Represents the override source (to be read) and the object property (to be written).
*/
USTRUCT(BlueprintType)
struct FPCGObjectPropertyOverrideDescription
{
	GENERATED_BODY()

	FPCGObjectPropertyOverrideDescription() = default;

	FPCGObjectPropertyOverrideDescription(const FPCGAttributePropertyInputSelector& InInputSource, const FString& InPropertyTarget)
		: InputSource(InInputSource)
		, PropertyTarget(InPropertyTarget)
	{}

	/** Provide an attribute or property to read the override value from. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	/**
	* Provide an object property name to be overridden. If you have a property "A" on your object, use "A" as the property target.
	*
	* For example, if you want to override the "Is Editor Only" flag, find it in the details panel, right-click, select 'Copy Internal Name', and paste that as the property target.
	*
	* If you have a component property, such as the static mesh of a static mesh component, use "StaticMeshComponent.StaticMesh".
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};

namespace PCGObjectPropertyOverrideHelpers
{
	/** Create an advanced ParamData pin for capturing property overrides. */
	FPCGPinProperties CreateObjectPropertiesOverridePin(FName Label, const FText& Tooltip);

	/** Apply property overrides to the TargetObject directly from the ObjectPropertiesOverride pin. Use CreateObjectPropertiesOverridePin(). */
	void ApplyOverridesFromParams(const TArray<FPCGObjectPropertyOverrideDescription>& InObjectPropertyOverrideDescriptions, AActor* TargetActor, FName OverridesPinLabel, FPCGContext* Context);
}

/**
* Represents a single property override on the provided object. Applies an override function to read the InputAccessor
* and write its value to the OutputAccessor.
* 
* The InputAccessor's InputKeys are created from the given SourceData and InputSelector.
*/
struct FPCGObjectSingleOverride
{
	/** Initialize the single object override. Call before using Apply(InputKeyIndex, OutputKey). */
	void Initialize(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, const UStruct* TemplateClass, const UPCGData* SourceData, FPCGContext* Context);

	/** Returns true if initialization succeeded in creating the accessors and accessor keys. */
	bool IsValid() const;

	/** Applies a single property override to the object by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the object property. */
	bool Apply(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey);

private:
	TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys;
	TUniquePtr<const IPCGAttributeAccessor> ObjectOverrideInputAccessor;
	TUniquePtr<IPCGAttributeAccessor> ObjectOverrideOutputAccessor;

	// InputKeyIndex, OutputKeys
	using ApplyOverrideFunction = bool(FPCGObjectSingleOverride::*)(int32, IPCGAttributeAccessorKeys&);
	ApplyOverrideFunction ObjectOverrideFunction = nullptr;

	template <typename Type>
	bool ApplyImpl(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey)
	{
		if (!IsValid())
		{
			return false;
		}

		Type Value{};
		check(ObjectOverrideInputAccessor.IsValid());
		if (ObjectOverrideInputAccessor->Get<Type>(Value, InputKeyIndex, *InputKeys.Get(), EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible))
		{
			check(ObjectOverrideOutputAccessor.IsValid());
			if (ObjectOverrideOutputAccessor->Set<Type>(Value, OutputKey))
			{
				return true;
			}
		}

		return false;
	}
};

/**
* Represents a set of property overrides for the provided object. Provide a SourceData to read from, and a collection of ObjectPropertyOverrides matching the TemplateObject's class properties.
*/
template <typename T>
struct FPCGObjectOverrides
{
	FPCGObjectOverrides(T* TemplateObject) : OutputKey(TemplateObject)
	{}

	/** Initialize the object overrides. Call before using Apply(InputKeyIndex). */
	void Initialize(const TArray<FPCGObjectPropertyOverrideDescription>& OverrideDescriptions, T* TemplateObject, const UPCGData* SourceData, FPCGContext* Context)
	{
		if (!TemplateObject)
		{
			PCGLog::LogErrorOnGraph(NSLOCTEXT("PCGObjectPropertyOverride", "InitializeOverrideFailedNoObject", "Failed to initialize property overrides. No template object was provided."), Context);
			return;
		}

		const UStruct* ClassObject = nullptr;
		if constexpr (TIsDerivedFrom<T, UObject>::Value)
		{
			ClassObject = TemplateObject->GetClass();
		}
		else
		{
			ClassObject = TemplateObject->StaticStruct();
		}

		ObjectSingleOverrides.Reserve(OverrideDescriptions.Num());

		for (int32 i = 0; i < OverrideDescriptions.Num(); ++i)
		{
			FPCGAttributePropertyInputSelector InputSelector = OverrideDescriptions[i].InputSource.CopyAndFixLast(SourceData);

			if (InputSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				const UPCGMetadata* Metadata = SourceData ? SourceData->ConstMetadata() : nullptr;
				const FName AttributeName = InputSelector.GetAttributeName();

				if (!Metadata || !Metadata->HasAttribute(AttributeName))
				{
					PCGLog::LogWarningOnGraph(FText::Format(NSLOCTEXT("PCGObjectPropertyOverride", "InvalidAttribute", "Tried to initialize ObjectOverride for input '{0}', but the attribute '{1}' does not exist."), InputSelector.GetDisplayText(), FText::FromName(AttributeName)), Context);
					continue;
				}
			}

			const FString& OutputProperty = OverrideDescriptions[i].PropertyTarget;

			FPCGObjectSingleOverride Override;
			Override.Initialize(InputSelector, OutputProperty, ClassObject, SourceData, Context);

			if (Override.IsValid())
			{
				ObjectSingleOverrides.Add(std::move(Override));
			}
			else
			{
				PCGLog::LogErrorOnGraph(FText::Format(NSLOCTEXT("PCGObjectPropertyOverride", "InitializeOverrideFailed", "Failed to initialize override '{0}' for property {1} on object '{2}'."), InputSelector.GetDisplayText(), FText::FromString(OutputProperty), FText::FromName(ClassObject->GetFName())), Context);
			}
		}
	}

	/** Applies each property override to the object by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the object property. */
	bool Apply(int32 InputKeyIndex)
	{
		bool bAllSucceeded = true;

		for (FPCGObjectSingleOverride& ObjectSingleOverride : ObjectSingleOverrides)
		{
			bAllSucceeded &= ObjectSingleOverride.Apply(InputKeyIndex, OutputKey);
		}

		return bAllSucceeded;
	}

private:
	FPCGAttributeAccessorKeysSingleObjectPtr<T> OutputKey;
	TArray<FPCGObjectSingleOverride> ObjectSingleOverrides;
};

USTRUCT(BlueprintType, meta=(Deprecated = "5.4", DeprecationMessage="Use FPCGObjectPropertyOverrideDescription instead."))
struct FPCGActorPropertyOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};
