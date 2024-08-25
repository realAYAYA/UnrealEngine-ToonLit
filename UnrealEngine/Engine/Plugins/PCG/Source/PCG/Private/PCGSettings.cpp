// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSettings.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "PCGSubsystem.h"
#include "Elements/PCGAddTag.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"

#include "Serialization/ArchiveObjectCrc32.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSettings)

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "PCGSettings"

namespace PCGSettings
{
	// For the deprecation of overridable params as display name to authored names
	
	// Arbitrary invalid index
	static constexpr int DeprecationAliasIndex = -2;
	// For property path concatenation
	static constexpr const TCHAR* PropertyPathSeparator = TEXT("/");
}

/** Custom Crc computation that ignores properties that will not affect the computed result of a node. */
class FPCGSettingsObjectCrc32 : public FArchiveObjectCrc32
{
public:
#if WITH_EDITOR
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
	{
		// Currently we rely on the 'UID' property getting included in the Crc. An example of this are asset settings
		// for which we avoid doing a full data CRC and instead rely on including hte UID. This property is transient
		// and will only serialize if IsPersistent() is false (see tests in FProperty::ShouldSerializeValue()).
		ensure(!IsPersistent());

		// Omit CRC'ing data collections here as it is very slow. Rely instead on UID.
		const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty);
		if (StructProperty && StructProperty->Struct && StructProperty->Struct->IsChildOf(FPCGDataCollection::StaticStruct()))
		{
			return true;
		}

		const bool bSkip = InProperty && (
			InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DebugSettings)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DeterminismSettings)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bDebug)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, Category)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, Description)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bExposeToLibrary)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, CachedOverridableParams)
			);

		return bSkip;
	}
#endif // WITH_EDITOR
};

FString FPCGSettingsOverridableParam::GetPropertyPath() const
{
	return FString::JoinBy(Properties, PCGSettings::PropertyPathSeparator, [](const FProperty* InProperty) { return InProperty ? InProperty->GetAuthoredName() : FString(); });
}

TArray<FName> FPCGSettingsOverridableParam::GenerateAllPossibleAliases() const
{
	if (Properties.IsEmpty() || !HasAliases())
	{
		return TArray<FName>{};
	}

	auto ConcatenateNames = [](const FName& Name1, const FName& Name2) -> FName
	{
		return FName(FString::Printf(TEXT("%s/%s"), *Name1.ToString(), *Name2.ToString()));
	};

	// If we have multiple properties, we'll generate all possible aliases, which is a combination of each aliases for each property in the path.
	TArray<FName> Result;
	for (int32 i = 0; i < Properties.Num(); ++i)
	{
		const FPCGPropertyAliases* It = MapOfAliases.Find(i);
		const TArray<FName>* CurrentAliases = It ? &It->Aliases : nullptr;
		const FProperty* CurrentProperty = Properties[i];
		const FName PropertyName = CurrentProperty ? CurrentProperty->GetFName() : NAME_None;

		// If no alias at this level, just concatenate the property name to all existing aliases
		if (!CurrentAliases || CurrentAliases->IsEmpty())
		{
			if (Result.IsEmpty())
			{
				Result.Add(PropertyName);
			}
			else
			{
				for (FName& Alias : Result)
				{
					Alias = ConcatenateNames(Alias, PropertyName);
				}
			}
		}
		else if (Result.IsEmpty())
		{
			if (CurrentAliases)
			{
				Result = *CurrentAliases;
			}

			// Also need to add the property name
			Result.Add(PropertyName);
		}
		else
		{
			TArray<FName> Temp;
			Temp.Reserve(Result.Num() * CurrentAliases->Num());
			for (const FName& Alias : Result)
			{
				for (const FName& OtherAlias : *CurrentAliases)
				{
					Temp.Add(ConcatenateNames(Alias, OtherAlias));
				}

				// Also need to add the property name
				Temp.Add(ConcatenateNames(Alias, PropertyName));
			}

			Result = std::move(Temp);
		}
	}

	return Result;
}

#if WITH_EDITOR
FString FPCGSettingsOverridableParam::GetDisplayPropertyPath() const
{
	return GetDisplayPropertyPathText().ToString();
}

FText FPCGSettingsOverridableParam::GetDisplayPropertyPathText() const
{
	if (bHasNameClash)
	{
		TArray<FText> PropertyNames;
		Algo::Transform(Properties, PropertyNames, [](const FProperty* InProperty) { return InProperty ? InProperty->GetDisplayNameText() : FText(); });
		return FText::Join(FText::FromString(PCGSettings::PropertyPathSeparator), PropertyNames);
	}
	else
	{
		return !Properties.IsEmpty() && Properties.Last() ? Properties.Last()->GetDisplayNameText() : FText();
	}
}
#endif // WITH_EDITOR

bool UPCGSettingsInterface::IsInstance() const
{
	return this != GetSettings();
}

void UPCGSettingsInterface::SetEnabled(bool bInEnabled)
{
	if (bEnabled != bInEnabled)
	{
		bEnabled = bInEnabled;
#if WITH_EDITOR
		if (UPCGSettings* Settings = GetSettings())
		{
			const EPCGChangeType ChangeType = Settings->GetChangeTypeForProperty(GET_MEMBER_NAME_CHECKED(UPCGSettingsInterface, bEnabled));
			OnSettingsChangedDelegate.Broadcast(Settings, ChangeType);
		}
#endif
	}
}

uint32 UPCGSettings::GetTypeNameHash() const
{
	return GetTypeHash(GetClass()->GetFName());
}

bool UPCGSettings::operator==(const UPCGSettings& Other) const
{
	if (this == &Other)
	{
		return true;
	}
	else
	{
		FPCGSettingsObjectCrc32 Ar;
		uint32 ThisCrc = Ar.Crc32(const_cast<UPCGSettings*>(this));
		uint32 OtherCrc = Ar.Crc32(const_cast<UPCGSettings*>(&Other));
		return ThisCrc == OtherCrc;
	}
}

#if WITH_EDITOR
void UPCGSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	// For versions older than the update of input selector, look for any Input Selector in the properties, and if we find one that is default,
	// we force to @LastCreated to keep the old behavior.
	if (DataVersion < FPCGCustomVersion::UpdateAttributePropertyInputSelector)
	{
		auto Recurse = [this](UStruct* InStructClass, void* InContainer)
		{
			auto RecurseImpl = [this](UStruct* InStructClass, void* InContainer, auto Callback) -> void
			{
				for (TFieldIterator<FStructProperty> It(InStructClass, EFieldIterationFlags::IncludeSuper); It; ++It)
				{
					const FStructProperty* Property = CastField<FStructProperty>(*It);
					if (!Property)
					{
						continue;
					}

					// If it is an Input Selector, apply deprecation
					if (Property->Struct == FPCGAttributePropertyInputSelector::StaticStruct())
					{
						Property->ContainerPtrToValuePtr<FPCGAttributePropertyInputSelector>(InContainer)->ApplyDeprecation(DataVersion);
					}
					else
					{
						// Otherwise, go deeper.
						Callback(Property->Struct.Get(), Property->ContainerPtrToValuePtr<void>(InContainer), Callback);
					}
				}
			};

			RecurseImpl(InStructClass, InContainer, RecurseImpl);
		};

		Recurse(GetClass(), this);
	}

	DataVersion = FPCGCustomVersion::LatestVersion;
}

bool UPCGSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	return PCGPinPropertiesHelpers::GetDefaultPinExtraIcon(InPin, OutExtraIcon, OutTooltip);
}

void UPCGSettings::PostEditUndo()
{
	// CachedOverridableParams was reset to previous value
	// Therefore we need to rebuild the properties array since it is transient.
	InitializeCachedOverridableParams();

	Super::PostEditUndo();
}
#endif // WITH_EDITOR

void UPCGSettings::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
}

void UPCGSettings::PostLoad()
{
	Super::PostLoad();

	// In editor, we always want to rebuild our cache, to make sure we are up to date.
	// At runtime, we can't rebuild our cache (that is why we cache it in the first place).
#if WITH_EDITOR
	InitializeCachedOverridableParams(/*bReset=*/true);
#else
	InitializeCachedOverridableParams(/*bReset=*/false);
#endif // WITH_EDITOR

#if WITH_EDITOR
	if (ExecutionMode_DEPRECATED != EPCGSettingsExecutionMode::Enabled)
	{
		bEnabled = ExecutionMode_DEPRECATED != EPCGSettingsExecutionMode::Disabled;
		bDebug = ExecutionMode_DEPRECATED == EPCGSettingsExecutionMode::Debug;
		ExecutionMode_DEPRECATED = EPCGSettingsExecutionMode::Enabled;
	}
#endif

	CacheCrc();
}

void UPCGSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	InitializeCachedOverridableParams();
#endif //WITH_EDITOR

	// For new objects, use type hash as a seed so we don't get multiple nodes doing randomness in an identical way (we had a case where
	// a Point Subset followed by a Density Noise produced uniform densities because the random seeds/streams were correllated).
	// It is still possible to chain nodes of the same type (Point Subset -> Point Subset) and see correllated behaviour, in which
	// case the random seed on one of them should be changed in the node settings.
	if (PCGHelpers::IsNewObjectAndNotDefault(this, /*bCheckHierarchy=*/true))
	{
		Seed = GetTypeNameHash();
	}

	CacheCrc();
}

void UPCGSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPCGCustomVersion::GUID);

#if WITH_EDITOR
	if (Ar.IsLoading())
	{
		// Some data migration must happen after the graph is fully initialized, such as manipulating node connections, so we
		// store off the loaded version number to be used later.
		DataVersion = Ar.CustomVer(FPCGCustomVersion::GUID);
	}
#endif // WITH_EDITOR

	// An additional custom version number that can be driven by external system users to track system modifications. To use a custom
	// version in user settings objects, override the GetUserCustomVersionGuid() method.
	const FGuid UserDataGuid = GetUserCustomVersionGuid();
	const bool bUsingCustomUserVersion = UserDataGuid != FGuid();

	if (bUsingCustomUserVersion)
	{
		Ar.UsingCustomVersion(UserDataGuid);

#if WITH_EDITOR
		if (Ar.IsLoading())
		{
			// Some data migration must happen after the graph is fully initialized, such as manipulating node connections, so we
			// store off the loaded version number to be used later.
			UserDataVersion = Ar.CustomVer(UserDataGuid);
		}
#endif // WITH_EDITOR
	}
}

void UPCGSettings::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	Super::PostSaveRoot(ObjectSaveContext);

#if WITH_EDITOR
	// This will get called when an external settings gets saved;
	// This is to trigger generation on save, if we've called changed properties from a blueprint
	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Structural);
#endif
}

#if WITH_EDITOR
UObject* UPCGSettings::GetJumpTargetForDoubleClick() const
{
	return const_cast<UObject*>(Cast<UObject>(this));
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// This is not true for everything, use a virtual call?
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

void UPCGSettings::FillOverridableParamsPins(TArray<FPCGPinProperties>& OutPins) const
{
	if (!HasOverridableParams())
	{
		return;
	}

	// Validating that we are not clashing with existing pins
	TMap<FName, EPCGDataType> InputPinsLabelsAndTypes;

	for (const FPCGPinProperties& InputPinProperties : OutPins)
	{
		InputPinsLabelsAndTypes.Emplace(InputPinProperties.Label, InputPinProperties.AllowedTypes);
	}

	// For debugging
	FString GraphName;
	FString NodeName;

	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		NodeName = ((Node->NodeTitle != NAME_None) ? Node->NodeTitle : Node->GetFName()).ToString();

		if (Node->GetGraph())
		{
			GraphName = Node->GetGraph()->GetName();
		}
	}
	else
	{
		NodeName = GetName();
	}

	// Adding the multi-pin connection for params.
	// If it already exists (and is the correct type), we can keep it.
	const EPCGDataType* PinType = InputPinsLabelsAndTypes.Find(PCGPinConstants::DefaultParamsLabel);
	if (PinType)
	{
		if (*PinType != EPCGDataType::Param)
		{
			const FString ParamsName = PCGPinConstants::DefaultParamsLabel.ToString();
			UE_LOG(LogPCG, Error, TEXT("[%s-%s] While adding pin '%s', another pin '%s' was found with invalid type (type must be Attribute Set). "
				"Rename or remove this pin to allow an override pin to be added automatically."),
				*GraphName, *NodeName, *ParamsName, *ParamsName);
		}
	}
	else
	{
		FPCGPinProperties& ParamPin = OutPins.Emplace_GetRef(PCGPinConstants::DefaultParamsLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/ true, /*bAllowMultipleData=*/ true);
		ParamPin.SetAdvancedPin();

#if WITH_EDITOR
		ParamPin.Tooltip = LOCTEXT("GlobalParamPinTooltip", "Atribute Set containing multiple parameters to override. Names must match perfectly.");
#endif // WITH_EDITOR
	}

	InputPinsLabelsAndTypes.Emplace(PCGPinConstants::DefaultParamsLabel, EPCGDataType::Param);

	for (const FPCGSettingsOverridableParam& OverridableParam : OverridableParams())
	{
		if (InputPinsLabelsAndTypes.Contains(OverridableParam.Label))
		{
			//const FString ParamsName = OverridableParam.Label.ToString();
			//UE_LOG(LogPCG, Warning, TEXT("[%s-%s] While automatically adding override pins, an existing pin was found with conflicting name '%s'. "
			//	"Rename or remove this pin to allow the automatic override pin to be added. Automatic override pin '%s' skipped."),
			//	*GraphName, *NodeName, *ParamsName, *ParamsName);
			continue;
		}

		InputPinsLabelsAndTypes.Emplace(OverridableParam.Label, EPCGDataType::Param);

		FPCGPinProperties& ParamPin = OutPins.Emplace_GetRef(OverridableParam.Label, EPCGDataType::Param, /*bInAllowMultipleConnections=*/ false, /*bAllowMultipleData=*/ false);
		ParamPin.SetAdvancedPin();
#if WITH_EDITOR

		if (!OverridableParam.Properties.IsEmpty())
		{
			const FProperty* Property = OverridableParam.Properties.Last();
			check(Property);
			static FName TooltipMetadata("Tooltip");
			FString Tooltip;
			if (const FString* TooltipPtr = Property->FindMetaData(TooltipMetadata))
			{
				Tooltip = *TooltipPtr + TEXT("\n");
			}

			ParamPin.Tooltip = FText::Format(LOCTEXT("OverridableParamPinTooltip", "{0}Attribute type is \"{1}\" and its exact name is \"{2}\""),
				FText::FromString(Tooltip),
				FText::FromString(Property->GetCPPType()),
				FText::FromString(OverridableParam.GetPropertyPath()));
		}
#endif // WITH_EDITOR
	}
}

TArray<FPCGPinProperties> UPCGSettings::AllInputPinProperties() const
{
	TArray<FPCGPinProperties> InputPins = InputPinProperties();
	FillOverridableParamsPins(InputPins);
	return InputPins;
}

EPCGDataType UPCGSettings::GetTypeUnionOfIncidentEdges(const FName& PinLabel) const
{
	EPCGDataType Result = EPCGDataType::None;

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const UPCGPin* Pin = Node ? Node->GetInputPin(PinLabel) : nullptr;
	if (Pin && Pin->EdgeCount() > 0)
	{
		for (const UPCGEdge* Edge : Pin->Edges)
		{
			const UPCGPin* OtherOutputPin = Edge ? Edge->GetOtherPin(Pin) : nullptr;
			if (OtherOutputPin)
			{
				Result |= OtherOutputPin->GetCurrentTypes();
			}
		}
	}

	return Result;
}

TArray<FPCGPinProperties> UPCGSettings::AllOutputPinProperties() const
{
	return OutputPinProperties();
}

TArray<FPCGPinProperties> UPCGSettings::DefaultInputPinProperties() const
{
	return InputPinProperties();
}

TArray<FPCGPinProperties> UPCGSettings::DefaultOutputPinProperties() const
{
	return OutputPinProperties();
}

FPCGElementPtr UPCGSettings::GetElement() const
{
	if (!CachedElement)
	{
		CacheLock.Lock();

		if (!CachedElement)
		{
			CachedElement = CreateElement();
		}

		CacheLock.Unlock();
	}

	return CachedElement;
}

UPCGNode* UPCGSettings::CreateNode() const
{
	return NewObject<UPCGNode>();
}

int UPCGSettings::GetSeed(const UPCGComponent* InSourceComponent) const
{
	return !bUseSeed ? 42 : (InSourceComponent ? PCGHelpers::ComputeSeed(Seed, InSourceComponent->Seed) : Seed);
}

#if WITH_EDITOR
void UPCGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	if (PropertyChangedEvent.Property && (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, CachedOverridableParams)))
	{
		// Need to rebuild properties, if it ever changes.
		InitializeCachedOverridableParams(/*bReset=*/true);
	}

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName != GET_MEMBER_NAME_CHECKED(UPCGSettings, DeterminismSettings))
	{
		// If we have a property name then get the change type for that property, otherwise assume deepest change type.
		EPCGChangeType ChangeType = EPCGChangeType::Cosmetic | EPCGChangeType::Settings | EPCGChangeType::Structural | EPCGChangeType::GenerationGrid;
		if (PropertyName != NAME_None)
		{
			ChangeType = GetChangeTypeForProperty(PropertyChangedEvent);
		}

		OnSettingsChangedDelegate.Broadcast(this, ChangeType);
	}

	CacheCrc();
}

EPCGChangeType UPCGSettings::GetChangeTypeForProperty(FPropertyChangedEvent& PropertyChangedEvent) const
{
	return GetChangeTypeForProperty(PropertyChangedEvent.GetPropertyName());
}

EPCGChangeType UPCGSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return EPCGChangeType::Settings | (IsStructuralProperty(InPropertyName) ? EPCGChangeType::Structural : EPCGChangeType::None);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UPCGSettings::DirtyCache()
{
	if (GEditor)
	{
		if (GEditor->PlayWorld)
		{
			if (UPCGSubsystem* PCGPIESubsystem = UPCGSubsystem::GetInstance(GEditor->PlayWorld.Get()))
			{
				PCGPIESubsystem->CleanFromCache(GetElement().Get(), this);
			}
		}

		if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
		{
			PCGSubsystem->CleanFromCache(GetElement().Get(), this);
		}
	}
}

bool UPCGSettings::CanEditChange(const FEditPropertyChain& InPropertyChain) const
{
	// Property is the actual property currently checked
	FProperty* Property = InPropertyChain.GetActiveNode() ? InPropertyChain.GetActiveNode()->GetValue() : nullptr;
	// Member property is the same as Property if it is not in a struct/array, otherwise it is the struct/array.
	FProperty* MemberProperty = InPropertyChain.GetActiveMemberNode() ? InPropertyChain.GetActiveMemberNode()->GetValue() : nullptr;

	// No property/member property, or if the property is marked edit const (in metadata), it is not editable.
	if (!Property || !MemberProperty || !Super::CanEditChange(InPropertyChain))
	{
		return false;
	}

	// If it is not marked edit const, properties that are not marked overridable or child properties explicitly marked non overridable are always editable.
	if (!MemberProperty->HasMetaData(PCGObjectMetadata::Overridable) || Property->HasMetaData(PCGObjectMetadata::NotOverridable))
	{
		return true;
	}

	// If the property can be overridden, mark it as EditConst if it is actually overridden 
	// (ie. the override pin associated with this property is connected)
	return !IsPropertyOverriddenByPin(Property);
}

bool UPCGSettings::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

void UPCGSettings::PostPaste()
{
	// Params are not properly built when importing from pasted text (because param
	// Properties are transient/unserialized), so we should reinitialize in post
	InitializeCachedOverridableParams(/*bReset=*/true);
}

void UPCGSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	check(InOutNode);

	if (DataVersion < FPCGCustomVersion::AddParamPinToOverridableNodes)
	{
		PCGSettingsHelpers::DeprecationBreakOutParamsToNewPin(InOutNode, InputPins, OutputPins);
	}

	if (DataVersion < FPCGCustomVersion::RenameDefaultParamsToOverride)
	{
		InOutNode->RenameInputPin(PCGPinConstants::Private::OldDefaultParamsLabel, PCGPinConstants::DefaultParamsLabel);
	}

	// FIXME: Should be behind a version number
	// Deprecation, renaming all the pins generated with the old labels to the new labels
	for (const FPCGSettingsOverridableParam& Param : CachedOverridableParams)
	{
		if (const FPCGPropertyAliases* OldLabels = Param.MapOfAliases.Find(PCGSettings::DeprecationAliasIndex))
		{
			for (const FName& OldLabel : OldLabels->Aliases)
			{
				InOutNode->RenameInputPin(OldLabel, Param.Label);
			}
		}
	}
}

void UPCGSettings::ApplyStructuralDeprecation(UPCGNode* InOutNode)
{
	if (!InOutNode)
	{
		return;
	}

	// Deprecate the "TagsAppliedOnOutput" feature, replaced by usage of the AddTag node.
	// Note that the additional nodes on a per-pin basis (we don't have a AddTag passthrough node, and would be hard to understand for users)
	// and will not create the additional nodes if the pins aren't connected, meaning that in this specific case the tags would be lost.
	// Implementation note: the version here doesn't match the feature change, but has been bumped just after this was deprecated.
	if (DataVersion < FPCGCustomVersion::GetPCGComponentDataMustOverlapSourceComponentByDefault && !TagsAppliedOnOutput_DEPRECATED.IsEmpty())
	{
		UPCGGraph* PCGGraph = InOutNode->GetGraph();
		check(PCGGraph);

		const FString TagsToAdd = FString::Join(TagsAppliedOnOutput_DEPRECATED, TEXT(","));
		bool bAddedAddTagNodes = false;

		const TArray<TObjectPtr<UPCGPin>> OutputPins = InOutNode->GetOutputPins();

		for (TObjectPtr<UPCGPin> OutputPin : OutputPins)
		{
			check(OutputPin);
			const int32 OutputEdgeCount = OutputPin->EdgeCount();

			if (OutputEdgeCount == 0)
			{
				continue;
			}

			// Insert an "Add Tag" node from that pin and move the edges accordingly
			UPCGAddTagSettings* AddTagSettings = nullptr;
			UPCGNode* AddTagNode = PCGGraph->AddNodeOfType(AddTagSettings);
			check(AddTagNode && AddTagSettings);

			AddTagSettings->bEnabled = InOutNode->GetSettings()->bEnabled;
			AddTagSettings->TagsToAdd = TagsToAdd;

			UPCGPin* AddTagInputPin = AddTagNode->GetInputPins()[0];
			UPCGPin* AddTagOutputPin = AddTagNode->GetOutputPins()[0];

			int32 SumSourceNodesPositionX = 0;
			int32 SumSourceNodesPositionY = 0;
			while (!OutputPin->Edges.IsEmpty())
			{
				UPCGPin* DownstreamPin = OutputPin->Edges[0]->OutputPin;
				check(DownstreamPin);

				int32 NodePositionX, NodePositionY;
				DownstreamPin->Node->GetNodePosition(NodePositionX, NodePositionY);
				SumSourceNodesPositionX += NodePositionX;
				SumSourceNodesPositionY += NodePositionY;

				check(DownstreamPin->Node);
				OutputPin->BreakEdgeTo(DownstreamPin);
				AddTagOutputPin->AddEdgeTo(DownstreamPin);
			}

			// Place the injected node halfway between the original node and the downstream nodes
			int32 NodePositionX, NodePositionY;
			InOutNode->GetNodePosition(NodePositionX, NodePositionY);
			const int32 SourceNodeLocalPositionMeanX = SumSourceNodesPositionX / OutputEdgeCount;
			const int32 SourceNodeLocalPositionMeanY = SumSourceNodesPositionY / OutputEdgeCount;

			AddTagNode->SetNodePosition((NodePositionX + SourceNodeLocalPositionMeanX) / 2, (NodePositionY + SourceNodeLocalPositionMeanY) / 2);
			AddTagNode->NodeComment = LOCTEXT("DeprecationAddTagNodeCreated", "Node added to replicate deprecated behavior of the Add Tags To Output functionality.").ToString();
			AddTagNode->bCommentBubbleVisible = 1;

			OutputPin->AddEdgeTo(AddTagInputPin);
			bAddedAddTagNodes = true;
		}

		if (bAddedAddTagNodes)
		{
			UE_LOG(LogPCG, Log, TEXT("Tags applied to output detected. One or more 'Add Tags' node was created automatically to repliecate the previous behavior."));
		}
	}
}
#endif // WITH_EDITOR

bool UPCGSettings::IsPropertyOverriddenByPin(const FProperty* InProperty) const
{
	if (!InProperty)
	{
		return false;
	}

	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		const FPCGSettingsOverridableParam* Param = OverridableParams().FindByPredicate([InProperty](const FPCGSettingsOverridableParam& ParamToCheck)
		{
			// In OverridableParam, the array of properties is the chain of properties from the Settings class to the wanted param.
			// Therefore the property we are editing would match the latest property of the array.
			return !ParamToCheck.Properties.IsEmpty() && ParamToCheck.Properties.Last() == InProperty;
		});

		if (Param)
		{
			if (const UPCGPin* Pin = Node->GetInputPin(Param->Label))
			{
				return Pin->IsConnected();
			}
		}
	}

	return false;
}

bool UPCGSettings::IsPropertyOverriddenByPin(const FName PropertyName) const
{
	return IsPropertyOverriddenByPin(TArrayView<const FName>(&PropertyName, 1));
}

bool UPCGSettings::IsPropertyOverriddenByPin(const TArrayView<const FName>& PropertyNameChain) const
{
	if (PropertyNameChain.IsEmpty())
	{
		return false;
	}

	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		const FPCGSettingsOverridableParam* Param = OverridableParams().FindByPredicate([&PropertyNameChain](const FPCGSettingsOverridableParam& ParamToCheck)
		{
			return ParamToCheck.PropertiesNames == PropertyNameChain;
		});

		if (Param)
		{
			if (const UPCGPin* Pin = Node->GetInputPin(Param->Label))
			{
				return Pin->IsConnected();
			}
		}
	}
	return false;
}

bool UPCGSettings::DoesPinSupportPassThrough(UPCGPin* InPin) const
{
	return InPin && !InPin->Properties.IsAdvancedPin();
}

EPCGDataType UPCGSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);

	if (HasDynamicPins() && InPin->IsOutputPin())
	{
		const UPCGNode* Node = Cast<const UPCGNode>(GetOuter());
		if (Node && Node->GetInputPin(PCGPinConstants::DefaultInputLabel) != nullptr)
		{
			const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
			return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
		}
	}

	return InPin->Properties.AllowedTypes;
}

#if WITH_EDITOR
TArray<FPCGSettingsOverridableParam> UPCGSettings::GatherOverridableParams() const
{
	PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
	Config.bUseSeed = bUseSeed;
	Config.IncludeMetadataValues.Add(PCGObjectMetadata::Overridable);
	Config.ExcludeMetadataValues.Add(PCGObjectMetadata::NotOverridable);

	return PCGSettingsHelpers::GetAllOverridableParams(GetClass(), Config);
}
#endif // WITH_EDITOR

void UPCGSettings::InitializeCachedOverridableParams(bool bReset)
{
	// Don't do it for default object
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSettings::InitializeCachedOverridableParams);

#if WITH_EDITOR
	if (CachedOverridableParams.IsEmpty() || bReset)
	{
		// Deprecation: Previously params labels were using the GetDisplayTextName, which is localized and will break existing pins.
		// Now it will be the authored name, but we still need to make sure the pins are not broken. 
		// For that matter, we will match names for properties between serialized and newly found ones. If their label don't match, we add the previous label as alias.
		// FIXME: Should be depending on a version, but we can't do that as an hotfix.
		TMap<FString, FName> PropertyPathToLabel;

		if (!CachedOverridableParams.IsEmpty())
		{
			for (const FPCGSettingsOverridableParam& Param : CachedOverridableParams)
			{
				if (Param.PropertiesNames.IsEmpty())
				{
					continue;
				}

				FString Path = FString::JoinBy(Param.PropertiesNames, PCGSettings::PropertyPathSeparator, [](const FName PropertyName) { return PropertyName.ToString();});
				PropertyPathToLabel.Emplace(Path, Param.Label);
			}
		}

		CachedOverridableParams = GatherOverridableParams();

		if (!PropertyPathToLabel.IsEmpty())
		{
			for (FPCGSettingsOverridableParam& Param : CachedOverridableParams)
			{
				if (Param.PropertiesNames.IsEmpty())
				{
					continue;
				}

				FString Path = FString::JoinBy(Param.PropertiesNames, PCGSettings::PropertyPathSeparator, [](const FName PropertyName) { return PropertyName.ToString();});
				if (FName* OldLabel = PropertyPathToLabel.Find(Path))
				{
					if (*OldLabel != Param.Label)
					{
						Param.MapOfAliases.FindOrAdd(PCGSettings::DeprecationAliasIndex).Aliases.Emplace(*OldLabel);
					}
				}
			}
		}
	}
#endif // WITH_EDITOR

	for (int32 i = 0; i < CachedOverridableParams.Num(); ++i)
	{
		FPCGSettingsOverridableParam& Param = CachedOverridableParams[i];

		if (Param.PropertiesNames.IsEmpty())
		{
			UE_LOG(LogPCG, Error, TEXT("[InitializeCachedOverridableParams] %s one param is missing its property names, we cannot use this one as overridable param"), *GetName());
			CachedOverridableParams.RemoveAt(i--);
			continue;
		}

		// In some cases, the property class is not serialized correctly. We can still try to recover from this.
		if (!Param.PropertyClass)
		{
			FixingOverridableParamPropertyClass(Param);
			
			if (!Param.PropertyClass)
			{
				UE_LOG(LogPCG, Error, TEXT("[InitializeCachedOverridableParams] %s one param is missing its property class, we cannot use this one as overridable param"), *GetName());
				CachedOverridableParams.RemoveAt(i--);
				continue;
			}
		}

		Param.Properties.Reset(Param.PropertiesNames.Num());

		// Some properties might not be available at runtime. Remove them from the list, since they won't be overridable anymore.
		const FProperty* CurrentProperty = Param.PropertyClass->FindPropertyByName(Param.PropertiesNames[0]);
		if (!CurrentProperty)
		{
			CachedOverridableParams.RemoveAt(i--);
			continue;
		}

		Param.Properties.Add(CurrentProperty);

		for (int32 j = 1; j < Param.PropertiesNames.Num(); ++j)
		{
			// If we have multiple depth properties, it should be Struct/Object properties by construction
			const UStruct* UnderlyingStruct = nullptr;
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty))
			{
				UnderlyingStruct = StructProperty->Struct;
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CurrentProperty))
			{
				UnderlyingStruct = ObjectProperty->PropertyClass;
			}

			if (ensure(UnderlyingStruct))
			{
				CurrentProperty = UnderlyingStruct->FindPropertyByName(Param.PropertiesNames[j]);
				check(CurrentProperty);
				Param.Properties.Add(CurrentProperty);
			}
			else
			{
				Param.Properties.Empty();
				break;
			}
		}
	}
}

void UPCGSettings::FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const
{
	if (!GetClass() || Param.PropertiesNames.IsEmpty())
	{
		return;
	}

	if (GetClass()->FindPropertyByName(Param.PropertiesNames[0]) != nullptr)
	{
		Param.PropertyClass = GetClass();
	}
}

void UPCGSettings::CacheCrc()
{
	FPCGSettingsObjectCrc32 Ar;
	const uint32 CrcValue = Ar.Crc32(const_cast<UPCGSettings*>(this));
	CachedCrc = FPCGCrc(CrcValue);
}

TArray<FPCGPinProperties> UPCGSettings::DefaultPointInputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	FPCGPinProperties& InputPinProperty = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPinProperty.SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> UPCGSettings::DefaultPointOutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

void UPCGSettingsInstance::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PostSettingsChanged();
#endif
}

void UPCGSettingsInstance::BeginDestroy()
{
#if WITH_EDITOR
	if (Settings)
	{
		Settings->OnSettingsChangedDelegate.RemoveAll(this);
	}
#endif

	Super::BeginDestroy();
}

void UPCGSettingsInstance::PostEditImport()
{
	Super::PostEditImport();

#if WITH_EDITOR
	PostSettingsChanged();
#endif
}

void UPCGSettingsInstance::SetSettings(UPCGSettings* InSettings)
{
#if WITH_EDITOR
	if (Settings)
	{
		Settings->OnSettingsChangedDelegate.RemoveAll(this);
	}
#endif

	Settings = InSettings;

#if WITH_EDITOR
	PostSettingsChanged();
#endif
}

#if WITH_EDITOR
void UPCGSettingsInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Some setting in the instance has changed. We don't have a flag for that yet (to add if needed)
	// However, we can make it behave like a standard change
	OnSettingsChangedDelegate.Broadcast(Settings, EPCGChangeType::Settings);
}

void UPCGSettingsInstance::OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType)
{
	if (InSettings == Settings)
	{
		OnSettingsChangedDelegate.Broadcast(InSettings, ChangeType);
	}
}

void UPCGSettingsInstance::PostSettingsChanged()
{
	OriginalSettings = Settings;

	if (Settings)
	{
		Settings->OnSettingsChangedDelegate.AddUObject(this, &UPCGSettingsInstance::OnSettingsChanged);
		Settings->ConditionalPostLoad();
	}
}
#endif

UPCGTrivialSettings::UPCGTrivialSettings()
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;
#endif
}

FPCGElementPtr UPCGTrivialSettings::CreateElement() const
{
	return MakeShared<FPCGTrivialElement>();
}

bool FPCGTrivialElement::ExecuteInternal(FPCGContext* Context) const
{
	// Pass-through
	Context->OutputData = Context->InputData;
	return true;
}

#undef LOCTEXT_NAMESPACE