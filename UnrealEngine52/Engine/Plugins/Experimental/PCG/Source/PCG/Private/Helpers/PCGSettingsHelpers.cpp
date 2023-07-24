// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGSettingsHelpers.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#include "UObject/EnumProperty.h"

namespace PCGSettingsHelpers
{
	template<typename T>
	bool GetParamValue(T& Value, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InKey = 0)
	{
		if (InAttribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
		{
			Value = static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InKey);
			return true;
		}
		else
		{
			return false;
		}
	}

	template<typename T, typename U, typename... OtherTypes>
	bool GetParamValue(T& Value, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InKey = 0)
	{
		if (InAttribute->GetTypeId() == PCG::Private::MetadataTypes<U>::Id)
		{
			Value = (T)static_cast<const FPCGMetadataAttribute<U>*>(InAttribute)->GetValueFromItemKey(InKey);
			return true;
		}
		else
		{
			return GetParamValue<T, OtherTypes...>(Value, InAttribute, InKey);
		}
	}

	void SetValue(UPCGParamData* InParams, UObject* Object, FProperty* Property)
	{
		check(InParams && Property);
		const FPCGMetadataAttributeBase* MatchingAttribute = InParams->Metadata ? InParams->Metadata->GetConstAttribute(Property->GetFName()) : nullptr;

		// TODO? Support arrays (1) which would be possible if we have a consistent naming scheme
		if (!MatchingAttribute)
		{
			return;
		}

		bool bTypeError = false;

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				double Value = 0;
				if (GetParamValue<double, float>(Value, MatchingAttribute))
				{
					NumericProperty->SetFloatingPointPropertyValue(NumericProperty->ContainerPtrToValuePtr<double>(Object, 0), Value);
				}
				else
				{
					bTypeError = true;
				}
			}
			else if(NumericProperty->IsInteger())
			{
				int64 Value = 0;
				if (GetParamValue<int64, int32>(Value, MatchingAttribute))
				{
					NumericProperty->SetIntPropertyValue(NumericProperty->ContainerPtrToValuePtr<int64>(Object, 0), Value);
				}
				else
				{
					bTypeError = true;
				}
			}
			else
			{
				// Not a float and not an integer...?
				bTypeError = true;
			}
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool Value = 0;
			if(GetParamValue<bool>(Value, MatchingAttribute))
			{
				BoolProperty->SetPropertyValue(BoolProperty->ContainerPtrToValuePtr<bool>(Object, 0), Value);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			FString Value;
			if (GetParamValue<FString>(Value, MatchingAttribute))
			{
				StringProperty->SetPropertyValue(StringProperty->ContainerPtrToValuePtr<FString>(Object, 0), Value);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			FString Value;
			if (GetParamValue<FString>(Value, MatchingAttribute))
			{
				FName NValue(Value);
				NameProperty->SetPropertyValue(NameProperty->ContainerPtrToValuePtr<FName>(Object), NValue);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();

			int64 Value;
			if (GetParamValue<int64, int32, uint32, int16, uint16, int8, uint8>(Value, MatchingAttribute))
			{
				UnderlyingProperty->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Object, 0), Value);
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const FName PropertyTypeName = StructProperty->Struct->GetFName();
			void* PropertyValue = StructProperty->ContainerPtrToValuePtr<void>(Object, 0);

			if (PropertyTypeName == NAME_Vector)
			{
				FVector Value = FVector::Zero();
				if(GetParamValue<FVector, double, float>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FVector*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Vector4)
			{
				FVector4 Value = FVector4::Zero();
				if (GetParamValue<FVector4, double, float>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FVector4*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Quat)
			{
				FQuat Value = FQuat::Identity;
				if (GetParamValue<FQuat, FRotator>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FQuat*>(PropertyValue) = Value;
				}
			}
			else if (PropertyTypeName == NAME_Transform)
			{
				FTransform Value = FTransform::Identity;
				if (GetParamValue<FTransform, FVector>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FTransform*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Rotator)
			{
				FRotator Value = FRotator::ZeroRotator;
				if (GetParamValue<FRotator, FQuat>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FRotator*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			//else if (PropertyTypeName == NAME_Color)
			//else if (PropertyTypeName == NAME_LinearColor)
			//else if soft object path to something?
			else
			{
				bTypeError = true;
			}
		}

		if (bTypeError)
		{
			// Mismatching types
			UE_LOG(LogPCG, Warning, TEXT("Mismatching type in params vs. property %s"), *Property->GetFName().ToString());
		}
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	int ComputeSeedWithOverride(const UPCGSettings* InSettings, const UPCGComponent* InComponent, UPCGParamData* InParams)
	{
		check(InSettings);

		const int SettingsSeed = InParams ? PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGSettings, Seed), InSettings->Seed, InParams) : InSettings->Seed;
		return InComponent ? PCGHelpers::ComputeSeed(SettingsSeed, InComponent->Seed) : SettingsSeed;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void DeprecationBreakOutParamsToNewPin(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
	{
		// Check basic conditions for which the code below should run.
		if(!InOutNode || InputPins.IsEmpty() || !InputPins[0] || InputPins[0]->Properties.AllowedTypes != EPCGDataType::Any)
		{
			return;
		}

		// Check if the node already has a param pin, if so, nothing to do.
		if (InOutNode->GetInputPin(PCGPinConstants::DefaultParamsLabel))
		{
			return;
		}

		// Also no need to add a param pin if it has no overriable params
		if (!InOutNode->GetSettings() || InOutNode->GetSettings()->OverridableParams().IsEmpty())
		{
			return;
		}

		UPCGPin* InPin = InputPins[0];

		// Add params pin with good defaults (UpdatePins will ensure pin details are correct later).
		UPCGPin* NewParamsPin = NewObject<UPCGPin>(InOutNode);
		NewParamsPin->Node = InOutNode;
		NewParamsPin->Properties.AllowedTypes = EPCGDataType::Param;
		NewParamsPin->Properties.Label = PCGPinConstants::DefaultParamsLabel;
		NewParamsPin->Properties.bAllowMultipleConnections = true;
		NewParamsPin->Properties.bAllowMultipleData = true;
		InputPins.Add(NewParamsPin);

		// Make list of param pins that In pin is currently connected to.
		TArray<UPCGPin*> UpstreamParamPins;
		for (const UPCGEdge* Connection : InPin->Edges)
		{
			if (Connection->InputPin && Connection->InputPin->Properties.AllowedTypes == EPCGDataType::Param)
			{
				UpstreamParamPins.Add(Connection->InputPin);
			}
		}

		// Break all connections to param pins, and connect the first such pin to the new params pin on this node.
		for (UPCGPin* Pin : UpstreamParamPins)
		{
			InPin->BreakEdgeTo(Pin);

			// Params never support multiple connections as a rule (user must merge params themselves), so just connect first.
			if (!NewParamsPin->IsConnected())
			{
				NewParamsPin->AddEdgeTo(Pin);
			}
		}
	}

	template <typename ClassType>
	TArray<FPCGSettingsOverridableParam> GetAllOverridableParamsImpl(const ClassType* InClass, const FPCGGetAllOverridableParamsConfig& InConfig)
	{
		// TODO: Was not a concern until now, and we didn't have a solution, but this function
		// only worked if we don't have names clashes in overriable parameters.
		// The previous override solution was flattening structs, and only override use the struct member name,
		// not prefixed by the struct name or anything else.
		// We cannot prefix it now, because it will break existing node that were assuming the flattening.
		// We'll keep this behavior for now, as it might be solved by passing structs instead of param data,
		// but we'll still at least raise a warning if there is a clash.
		TSet<FName> LabelCache;

		TArray<FPCGSettingsOverridableParam> Res;

		// Can't check metadata in non-editor build
#if WITH_EDITOR
		const bool bCheckMetadata = !InConfig.MetadataValues.IsEmpty();
#endif // WITH_EDITOR

		const bool bCheckExcludePropertyFlags = (InConfig.ExcludePropertyFlags != 0);
		const EFieldIteratorFlags::SuperClassFlags SuperFlag = InConfig.bExcludeSuperProperties ? EFieldIteratorFlags::ExcludeSuper : EFieldIteratorFlags::IncludeSuper;

		check(InClass);

		for (TFieldIterator<FProperty> InputIt(InClass, SuperFlag, EFieldIteratorFlags::ExcludeDeprecated); InputIt; ++InputIt)
		{
			const FProperty* Property = *InputIt;
			if (!Property)
			{
				continue;
			}

			bool bValid = true;

#if WITH_EDITOR
			if (bCheckMetadata)
			{
				bool bFoundAny = false;
				for (const FName& Metadata : InConfig.MetadataValues)
				{
					if (Property->HasMetaData(Metadata))
					{
						bFoundAny = true;
						break;
					}
				}

				bValid &= bFoundAny;
			}
#endif // WITH_EDITOR

			if (bCheckExcludePropertyFlags)
			{
				bValid &= !Property->HasAnyPropertyFlags(InConfig.ExcludePropertyFlags);
			}

			// Don't allow to override the seed if the settings doesn't use the seed.
			if (!InConfig.bUseSeed)
			{
				bValid &= (Property->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettings, Seed));
			}

			if (!bValid)
			{
				continue;
			}

			// Validate that the property can be overriden by params
			if (PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(Property))
			{
				FName Label = NAME_None;
#if WITH_EDITOR
				// GetDisplayNameText is not available in non-editor build.
				Label = *Property->GetDisplayNameText().ToString();
				if (LabelCache.Contains(Label))
				{
					UE_LOG(LogPCG, Warning, TEXT("%s property clashes with another property already found. It is a limitation at the moment and this property will be ignored."), *Label.ToString());
					continue;
				}

				LabelCache.Add(Label);
#endif // WITH_EDITOR

				FPCGSettingsOverridableParam& Param = Res.Emplace_GetRef();
				Param.Label = Label;
				Param.PropertiesNames.Add(Property->GetFName());
				Param.PropertyClass = InClass;
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				// Reached max depth
				if (InConfig.MaxStructDepth == 0)
				{
					continue;
				}

				// Use the seed, and don't check metadata.
				FPCGGetAllOverridableParamsConfig RecurseConfig = InConfig;
				RecurseConfig.bUseSeed = true;
#if WITH_EDITOR
				RecurseConfig.MetadataValues.Empty();
#endif // WITH_EDITOR

				if (RecurseConfig.MaxStructDepth > 0)
				{
					RecurseConfig.MaxStructDepth--;
				}

				for (const FPCGSettingsOverridableParam& ChildParam : GetAllOverridableParams(StructProperty->Struct, RecurseConfig))
				{
					FName Label = ChildParam.Label;
#if WITH_EDITOR
					// Don't check for label clash, as they would all be equal to None in non-editor build
					if (LabelCache.Contains(Label))
					{
						UE_LOG(LogPCG, Warning, TEXT("%s property clashes with another property already found. It is a limitation at the moment and this property will be ignored."), *Label.ToString());
						continue;
					}

					LabelCache.Add(Label);
#endif // WITH_EDITOR

					FPCGSettingsOverridableParam& Param = Res.Emplace_GetRef();
					Param.Label = Label;
					Param.PropertiesNames.Add(Property->GetFName());
					Param.PropertiesNames.Append(ChildParam.PropertiesNames);
					Param.PropertyClass = InClass;
				}
			}
		}

		return Res;
	}

	TArray<FPCGSettingsOverridableParam> GetAllOverridableParams(const UClass* InClass, const FPCGGetAllOverridableParamsConfig& InConfig)
	{
		return GetAllOverridableParamsImpl(InClass, InConfig);
	}

	TArray<FPCGSettingsOverridableParam> GetAllOverridableParams(const UScriptStruct* InStruct, const FPCGGetAllOverridableParamsConfig& InConfig)
	{
		return GetAllOverridableParamsImpl(InStruct, InConfig);
	}
}
