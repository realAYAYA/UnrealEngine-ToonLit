// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGSettingsHelpers.h"

#include "PCGCommon.h"
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
		NewParamsPin->Properties.bAllowMultipleData = true;
		NewParamsPin->Properties.SetAllowMultipleConnections(true);
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

	template <uint32 N>
	TArray<FPCGSettingsOverridableParam> GetAllOverridableParams_Internal(const UStruct* InClass, const FPCGGetAllOverridableParamsConfig& InConfig, TArray<const FProperty*, TInlineAllocator<N>>& InAlreadySeenProperties)
	{
		TArray<FName> LabelCache;

		TArray<FPCGSettingsOverridableParam> Res;

		const EFieldIteratorFlags::SuperClassFlags SuperFlag = InConfig.bExcludeSuperProperties ? EFieldIteratorFlags::ExcludeSuper : EFieldIteratorFlags::IncludeSuper;

		check(InClass);

		auto GatherAliases = [](const FProperty* InProperty, FPCGSettingsOverridableParam& InParam) -> void
		{
#if WITH_EDITOR
			if (InProperty->HasMetaData(PCGObjectMetadata::OverrideAliases))
			{
				FPCGPropertyAliases Result;
				TArray<FString> TempStringArray;
				const FString& AliasesStr = InProperty->GetMetaData(PCGObjectMetadata::OverrideAliases);
				AliasesStr.ParseIntoArray(TempStringArray, TEXT(","));

				if (!TempStringArray.IsEmpty())
				{
					Algo::Transform(TempStringArray, Result.Aliases, [](const FString& In) -> FName { return FName(In); });
					// We always emplace at level 0, it will be incremented by the recursion if it is deeper than 1 level.
					InParam.MapOfAliases.Emplace(0, std::move(Result));
				}
			}
#endif // WITH_EDITOR
		};

		for (TFieldIterator<FProperty> InputIt(InClass, SuperFlag, EFieldIteratorFlags::ExcludeDeprecated); InputIt; ++InputIt)
		{
			const FProperty* Property = *InputIt;
			if (!Property)
			{
				continue;
			}

			bool bValid = true;

#if WITH_EDITOR
			auto HasMetadata = [Property](const TArray<FName>& InValues, bool bIsConjunction) -> bool
			{
				bool bFound = false;
				for (const FName& Metadata : InValues)
				{
					if (Property->HasMetaData(Metadata) && !bIsConjunction)
					{
						bFound = true;
						if (!bIsConjunction)
						{
							break;
						}
					}
					else if (bIsConjunction)
					{
						bFound = false;
						break;
					}
				}

				return bFound;
			};

			if (!InConfig.IncludeMetadataValues.IsEmpty())
			{
				bValid &= HasMetadata(InConfig.IncludeMetadataValues, InConfig.bIncludeMetadataIsConjunction);
			}

			if (!InConfig.ExcludeMetadataValues.IsEmpty())
			{
				bValid &= !HasMetadata(InConfig.ExcludeMetadataValues, InConfig.bExcludeMetadataIsConjunction);
			}
#endif // WITH_EDITOR

			auto HasPropertyFlags = [Property](uint64 InFlags, bool bIsConjunction) -> bool
			{
				return bIsConjunction ? Property->HasAllPropertyFlags(InFlags) : Property->HasAnyPropertyFlags(InFlags);
			};

			if (InConfig.IncludePropertyFlags != 0)
			{
				bValid &= HasPropertyFlags(InConfig.IncludePropertyFlags, InConfig.bIncludePropertyFlagsIsConjunction);
			}

			if (InConfig.ExcludePropertyFlags != 0)
			{
				bValid &= !HasPropertyFlags(InConfig.ExcludePropertyFlags, InConfig.bExcludePropertyFlagsIsConjunction);
			}

			// Don't allow to override the seed if the settings doesn't use the seed.
			if (!InConfig.bUseSeed && Property->GetOwnerClass() && Property->GetOwnerClass()->IsChildOf<UPCGSettings>())
			{
				bValid &= (Property->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettings, Seed));
			}

			if (!bValid)
			{
				continue;
			}

			auto RecursiveExtraction = [&InConfig, &Res, &LabelCache, Property, InClass, &GatherAliases, &InAlreadySeenProperties](const UStruct* NextClass)
			{
				// Reached max depth
				if (InConfig.MaxStructDepth == 0)
				{
					return;
				}

				// Use the seed, and don't check metadata for PCG overridable.
				FPCGGetAllOverridableParamsConfig RecurseConfig = InConfig;
				InAlreadySeenProperties.Add(Property);
				RecurseConfig.bUseSeed = true;
#if WITH_EDITOR
				RecurseConfig.IncludeMetadataValues.Remove(PCGObjectMetadata::Overridable);
#endif // WITH_EDITOR

				if (RecurseConfig.MaxStructDepth > 0)
				{
					RecurseConfig.MaxStructDepth--;
				}

				for (FPCGSettingsOverridableParam& ChildParam : GetAllOverridableParams_Internal(NextClass, RecurseConfig, InAlreadySeenProperties))
				{
					FName Label = ChildParam.Label;
					bool bHasNameClash = false;
#if WITH_EDITOR
					// Don't check for label clash, as they would all be equal to None in non-editor build
					const int32 CachedLabelIndex = LabelCache.IndexOfByKey(Label);
					if (CachedLabelIndex != INDEX_NONE)
					{
						// If we have a clash, we will use the full path, so mark this param and the other that clashed to use the full path.
						Res[CachedLabelIndex].bHasNameClash = true;
						Res[CachedLabelIndex].Label = FName(Res[CachedLabelIndex].GetPropertyPath());
						bHasNameClash = true;
					}

					LabelCache.AddUnique(Label);
#endif // WITH_EDITOR

					FPCGSettingsOverridableParam& Param = Res.Emplace_GetRef();
					Param.PropertiesNames.Add(Property->GetFName());
					Param.PropertiesNames.Append(std::move(ChildParam.PropertiesNames));
					Param.Properties.Add(Property);
					Param.Properties.Append(std::move(ChildParam.Properties));
					Param.PropertyClass = InClass;
					if (!ChildParam.bHasNameClash && !bHasNameClash)
					{
						Param.Label = Label;
					}
					else
					{
#if WITH_EDITOR
						Param.Label = FName(Param.GetPropertyPath());
#endif // WITH_EDITOR
						Param.bHasNameClash = true;
					}

#if WITH_EDITOR
					GatherAliases(Property, Param);
					// For all already found aliases, add them to the current param, with the index incremented by 1.
					for (TPair<int32, FPCGPropertyAliases>& It : ChildParam.MapOfAliases)
					{
						Param.MapOfAliases.Emplace(It.Key + 1, std::move(It.Value));
					}
#endif // WITH_EDITOR
				}

				InAlreadySeenProperties.Pop(EAllowShrinking::No);
			};

			const FProperty* PropertyToCheck = Property;
			if (InConfig.bExtractArrays && Property->IsA<FArrayProperty>())
			{
				PropertyToCheck = CastFieldChecked<FArrayProperty>(Property)->Inner;
			}

			// Always extract instanced objects, but be careful with infinite recursion (class having an instanced property on itself)
			if (PropertyToCheck->IsA<FObjectProperty>() && !InAlreadySeenProperties.Contains(PropertyToCheck) && (InConfig.bExtractObjects || PropertyToCheck->HasAllPropertyFlags(CPF_InstancedReference)))
			{
				RecursiveExtraction(CastFieldChecked<FObjectProperty>(PropertyToCheck)->PropertyClass);
			}
			// Validate that the property can be overriden by params
			else if (PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(PropertyToCheck))
			{
				FName Label = NAME_None;
				bool bHasNameClash = false;
#if WITH_EDITOR
				Label = *Property->GetAuthoredName();
				const int32 CachedLabelIndex = LabelCache.IndexOfByKey(Label);
				if (CachedLabelIndex != INDEX_NONE)
				{
					// If we have a clash, we will use the full path, so mark this param and the other that clashed to use the full path.
					Res[CachedLabelIndex].bHasNameClash = true;
					Res[CachedLabelIndex].Label = FName(Res[CachedLabelIndex].GetPropertyPath());
					bHasNameClash = true;
				}

				LabelCache.AddUnique(Label);
#endif // WITH_EDITOR

				FPCGSettingsOverridableParam& Param = Res.Emplace_GetRef();
				Param.PropertiesNames.Add(Property->GetFName());
				Param.Properties.Add(Property);
				Param.PropertyClass = InClass;
				Param.bHasNameClash = bHasNameClash;
#if WITH_EDITOR
				Param.Label = bHasNameClash ? FName(Param.GetPropertyPath()) : Label;
				GatherAliases(Property, Param);
#endif // WITH_EDITOR
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyToCheck))
			{
				RecursiveExtraction(StructProperty->Struct);
			}
		}

		return Res;
	}

	TArray<FPCGSettingsOverridableParam> GetAllOverridableParams(const UStruct* InClass, const FPCGGetAllOverridableParamsConfig& InConfig)
	{
		constexpr uint32 InlineAllocator = 16;
		// Array of already seen properties to catch infinite recursion
		TArray<const FProperty*, TInlineAllocator<InlineAllocator>> AlreadySeenProperties;

		return GetAllOverridableParams_Internal(InClass, InConfig, AlreadySeenProperties);
	}
}
