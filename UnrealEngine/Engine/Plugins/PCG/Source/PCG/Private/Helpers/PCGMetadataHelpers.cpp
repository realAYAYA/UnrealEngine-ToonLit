// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGMetadataHelpers.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"	
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGMetadataHelpers"

namespace PCGMetadataHelpers
{
	bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2)
	{
		return Metadata1 && Metadata2 && Metadata1->GetRoot() == Metadata2->GetRoot();
	}

	const UPCGMetadata* GetParentMetadata(const UPCGMetadata* Metadata)
	{
		check(Metadata);
		TWeakObjectPtr<const UPCGMetadata> Parent = Metadata->GetParentPtr();

		// We're expecting the parent to either be null, or to be valid - if not, then it has been deleted
		// which is going to cause some issues.
		//check(Parent.IsExplicitlyNull() || Parent.IsValid());
		return Parent.Get();
	}

	const UPCGMetadata* GetConstMetadata(const UPCGData* InData)
	{
		return InData ? InData->ConstMetadata() : nullptr;
	}

	UPCGMetadata* GetMutableMetadata(UPCGData* InData)
	{
		return InData ? InData->MutableMetadata() : nullptr;
	}

	bool CreateObjectPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter)
	{
		if (!InAttributeBase)
		{
			return false;
		}

		if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				FString Path = static_cast<const FPCGMetadataAttribute<FString>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
				OutSoftObjectPath = FSoftObjectPath(Path);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftObjectPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}

		return false;
	}

	bool CreateObjectOrClassPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter)
	{
		if (!InAttributeBase)
		{
			return false;
		}

		if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				FString Path = static_cast<const FPCGMetadataAttribute<FString>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
				OutSoftObjectPath = FSoftObjectPath(Path);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftObjectPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftClassPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}

		return false;
	}


	bool CopyAttributes(UPCGData* TargetData, const UPCGData* SourceData, const TArray<TTuple<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector, EPCGMetadataTypes>>& AttributeSelectorsWithOutputType, bool bSameOrigin, FPCGContext* OptionalContext)
	{
		check(TargetData && SourceData);
		const UPCGMetadata* SourceMetadata = SourceData->ConstMetadata();
		UPCGMetadata* TargetMetadata = TargetData->MutableMetadata();

		if (!SourceMetadata || !TargetMetadata)
		{
			return false;
		}

		bool bSuccess = false;

		for (const auto& SelectorTuple : AttributeSelectorsWithOutputType)
		{
			const FPCGAttributePropertyInputSelector& InputSource = SelectorTuple.Get<0>();
			const FPCGAttributePropertyOutputSelector& OutputTarget = SelectorTuple.Get<1>();
			const EPCGMetadataTypes RequestedOutputType = SelectorTuple.Get<2>();

			const FName LocalSourceAttribute = InputSource.GetName();
			const FName LocalDestinationAttribute = OutputTarget.GetName();

			if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute && !SourceMetadata->HasAttribute(LocalSourceAttribute))
			{
				PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("InputMissingAttribute", "Input does not have the '{0}' attribute"), FText::FromName(LocalSourceAttribute)), OptionalContext);
				continue;
			}

			// We need accessors if we have a multi entry source attribute or we have extractors
			const bool bIsMultiEntries = SourceData->IsA<UPCGParamData>() && SourceMetadata->GetLocalItemCount() > 1;
			const bool bInputHasAnyExtra = !InputSource.GetExtraNames().IsEmpty();
			const bool bOutputHasAnyExtra = !OutputTarget.GetExtraNames().IsEmpty();
			const bool bSourceIsAttribute = InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute;
			const bool bTargetIsAttribute = OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute;
			// Cast is only required if it is on an output attribute that has no extra (that we will create)
			const bool bOutputTypeCast = bTargetIsAttribute && !bOutputHasAnyExtra && (RequestedOutputType != EPCGMetadataTypes::Unknown);

			const bool bNeedAccessors = bIsMultiEntries || bInputHasAnyExtra || bOutputHasAnyExtra || !bSourceIsAttribute || !bTargetIsAttribute || bOutputTypeCast;

			// If no accessor, copy over the attribute
			if (!bNeedAccessors)
			{
				if (bSameOrigin && LocalSourceAttribute == LocalDestinationAttribute)
				{
					// Nothing to do if we try to copy an attribute into itself in the original data.
					continue;
				}

				const FPCGMetadataAttributeBase* SourceAttribute = SourceMetadata->GetConstAttribute(LocalSourceAttribute);
				// Presence of attribute was already checked before, this should not return null
				check(SourceAttribute);

				if (FPCGMetadataAttributeBase* NewAttr = TargetMetadata->CopyAttribute(SourceAttribute, LocalDestinationAttribute, /*bKeepParent=*/false, /*bCopyEntries=*/true,/*bCopyValues=*/true))
				{
					// To keep the previous behavior, we force the copied attribute to have its default value set to the first entry.
					NewAttr->SetDefaultValueToFirstEntry();
				}
				else
				{
					PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailedCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)));
					continue;
				}
			}
			else // Create a new attribute of the accessed field's type manually
			{
				TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, InputSource);
				TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, InputSource);

				if (!InputAccessor.IsValid() || !InputKeys.IsValid())
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor or iterator"), OptionalContext);
					continue;
				}

				const uint16 OutputType = bOutputTypeCast ? static_cast<uint16>(RequestedOutputType) : InputAccessor->GetUnderlyingType();

				if (bOutputTypeCast && InputAccessor->GetUnderlyingType() == OutputType && bSameOrigin && LocalSourceAttribute == LocalDestinationAttribute)
				{
					// Nothing to do if we try to cast an attribute on itself with the same type
					continue;
				}

				// If we have a cast, make sure it is valid
				if (bOutputTypeCast && !PCG::Private::IsBroadcastableOrConstructible(InputAccessor->GetUnderlyingType(), OutputType))
				{
					PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("CastInvalid", "Cannot convert InputAttribute '{0}' of type {1} into {2}"), InputSource.GetDisplayText(), PCG::Private::GetTypeNameText(InputAccessor->GetUnderlyingType()), PCG::Private::GetTypeNameText(OutputType)), OptionalContext);
					continue;
				}

				// If the target is an attribute, only create a new one if the attribute we don't have any extra.
				// If it has any extra, it will try to write to it.
				if (!bOutputHasAnyExtra && bTargetIsAttribute)
				{
					auto CreateAttribute = [TargetMetadata, LocalDestinationAttribute, &InputAccessor](auto Dummy) -> bool
					{
						using AttributeType = decltype(Dummy);
						AttributeType DefaultValue{};
						if (!InputAccessor->Get(DefaultValue, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey), EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible))
						{
							// It's OK to fail getting the default value, if for example the input accessor is a property. In that case, just fallback on 0.
							DefaultValue = PCG::Private::MetadataTraits<AttributeType>::ZeroValue();
						}

						return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(TargetMetadata, LocalDestinationAttribute, std::move(DefaultValue)) != nullptr;
					};

					if (!PCGMetadataAttribute::CallbackWithRightType(OutputType, CreateAttribute))
					{
						PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)), OptionalContext);
						continue;
					}
				}

				TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TargetData, OutputTarget);
				TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(TargetData, OutputTarget);

				if (!OutputAccessor.IsValid() || !OutputKeys.IsValid())
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("FailedToCreateOutputAccessor", "Failed to create output accessor or iterator"), OptionalContext);
					continue;
				}

				if (OutputAccessor->IsReadOnly())
				{
					PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), OutputTarget.GetDisplayText()), OptionalContext);
					continue;
				}

				// Final verification (if not already done), if we can put the value of input into output
				if (!bOutputTypeCast && !PCG::Private::IsBroadcastableOrConstructible(OutputType, OutputAccessor->GetUnderlyingType()))
				{
					PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("CannotConvertTypes", "Cannot convert input type {0} into output type {1}"), PCG::Private::GetTypeNameText(OutputType), PCG::Private::GetTypeNameText(OutputAccessor->GetUnderlyingType())), OptionalContext);
					continue;
				}

				// At this point, we are ready.
				PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams Params;
				Params.InKeys = InputKeys.Get();
				Params.InAccessor = InputAccessor.Get();
				Params.OutKeys = OutputKeys.Get();
				Params.OutAccessor = OutputAccessor.Get();
				Params.IterationCount = PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams::Out;
				Params.Flags = EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible;

				if (!PCGMetadataElementCommon::CopyFromAccessorToAccessor(Params))
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("ErrorGettingSettingValues", "Error while getting/setting values"), OptionalContext);
					continue;
				}
			}

			bSuccess = true;
		}

		return bSuccess;
	}

	bool CopyAttribute(const FPCGCopyAttributeParams& InParams)
	{
		if (!InParams.TargetData || !InParams.SourceData)
		{
			return false;
		}

		TArray<TTuple<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector, EPCGMetadataTypes>> AttributeSelectors;
		FPCGAttributePropertyInputSelector InputSource = InParams.InputSource.CopyAndFixLast(InParams.SourceData);
		FPCGAttributePropertyOutputSelector OutputTarget = InParams.OutputTarget.CopyAndFixSource(&InputSource, InParams.SourceData);

		AttributeSelectors.Emplace(MoveTemp(InputSource), MoveTemp(OutputTarget), InParams.OutputType);
		return CopyAttributes(InParams.TargetData, InParams.SourceData, AttributeSelectors, InParams.bSameOrigin, InParams.OptionalContext);
	}

	bool CopyAllAttributes(const UPCGData* SourceData, UPCGData* TargetData, FPCGContext* OptionalContext)
	{
		if (!TargetData || !SourceData)
		{
			return false;
		}

		const UPCGMetadata* SourceMetadata = SourceData->ConstMetadata();
		if (!SourceMetadata)
		{
			return false;
		}

		TArray<TTuple<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector, EPCGMetadataTypes>> AttributeSelectors;
		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		SourceMetadata->GetAttributes(AttributeNames, AttributeTypes);

		for (const FName& AttributeName : AttributeNames)
		{
			TTuple<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector, EPCGMetadataTypes>& Selectors = AttributeSelectors.Emplace_GetRef();
			Selectors.Get<0>().SetAttributeName(AttributeName);
			Selectors.Get<1>().SetAttributeName(AttributeName);
			Selectors.Get<2>() = EPCGMetadataTypes::Unknown;
		}

		return CopyAttributes(TargetData, SourceData, AttributeSelectors, /*bSameOrigin=*/false, OptionalContext);
	}
}

#undef LOCTEXT_NAMESPACE