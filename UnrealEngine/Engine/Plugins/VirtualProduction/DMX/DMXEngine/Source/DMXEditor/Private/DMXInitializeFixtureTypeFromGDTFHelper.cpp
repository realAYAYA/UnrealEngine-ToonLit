// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXInitializeFixtureTypeFromGDTFHelper.h"

#include "DMXEditorLog.h"
#include "DMXProtocolSettings.h"
#include "DMXZipper.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXGDTFAssetImportData.h"
#include "Library/DMXImportGDTF.h"

#include "XmlFile.h"
#include "XmlNode.h"
#include "Algo/Copy.h"
#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"


#define LOCTEXT_NAMESPACE "DMXInitializeFixtureTypeFromGDTFHelper"

namespace UE::DMX::DMXInitializeFixtureTypeFromGDTFHelper::Private
{
	/**
	 * Depending on the GDTF Spec, some XML Attribute Names might start with or without DMX.
	 *
	 * @param ParentNode		The Parent Node to search in
	 * @param NodeName			The Attribute Name with DMX tag. This is the correct form, e.g. 'DMXMode', not 'Mode', 'DMXChannels', not 'Channels'.
	 * @return					Returns a pointer to the child node, or nullptr if the child cannot be found
	 */
	const FXmlNode* FindChildNodeEvenIfDMXSubstringIsMissing(const FXmlNode& ParentNode, const FString& AttributeNameWithDMXTag)
	{
		if (const FXmlNode* ChildNodePtrWithSubstring = ParentNode.FindChildNode(AttributeNameWithDMXTag))
		{
			return ChildNodePtrWithSubstring;
		}
		else if (const FXmlNode* ChildNodePtrWithoutSubstring = ParentNode.FindChildNode(AttributeNameWithDMXTag.RightChop(3)))
		{
			return ChildNodePtrWithoutSubstring;
		}

		return nullptr;
	}

	/**
	 * Depending on the GDTF Spec, some XML Attribute Names might start with or without DMX.
	 *
	 * @param ParentNode		The Parent Node to search in
	 * @param NodeName			The Attribute Name with DMX tag. This is the correct form, e.g. 'DMXMode', not 'Mode', 'DMXChannels', not 'Channels'.
	 * @return					Returns the Attribute as String
	 */
	FString FindAttributeEvenIfDMXSubstringIsMissing(const FXmlNode& ParentNode, const FString& AttributeNameWithDMXTag)
	{
		FString Attribute = ParentNode.GetAttribute(AttributeNameWithDMXTag);
		if (Attribute.IsEmpty())
		{
			Attribute = ParentNode.GetAttribute(AttributeNameWithDMXTag.RightChop(3));
		}

		return Attribute;
	}

	/** Helper to interprete a GDTF Channel */
	class FDMXGDTFChannelInterpreter
	{
	public:
		/** Initializes the bIsMatrix member */
		static bool IsMatrix(const FXmlNode* InDMXChannelNode, const TArray<const FXmlNode*>& InGeometryNodes)
		{
			return GetNumCells(InDMXChannelNode, InGeometryNodes) > 1;
		}

		/** Properties of a channel */
		struct FChannelProperties
		{
			FString AttributeName;
			int32 FirstChannel;
			EDMXFixtureSignalFormat SignalFormat;
			bool bLSBMode;
			int32 NumCells;
			int32 DefaultValue;
		};

		/** Returns the properties of the channel */
		static bool GetChannelProperties(const FXmlNode* InDMXChannelNode, const TArray<const FXmlNode*>& GeometryNodes, FChannelProperties& OutChannelProperties)
		{
			if (!ensureMsgf(InDMXChannelNode->GetTag() == TEXT("dmxchannel") || InDMXChannelNode->GetTag() == TEXT("channel"), TEXT("Trying to read Channel node, but node is tagged '%s'."), *InDMXChannelNode->GetTag()))
			{
				return false;
			}

			OutChannelProperties.NumCells = GetNumCells(InDMXChannelNode, GeometryNodes);

			if (!GetAttributeName(InDMXChannelNode, OutChannelProperties.AttributeName))
			{
				return false;
			}

			GetDefaultValue(InDMXChannelNode, OutChannelProperties.DefaultValue);
			GetDataType(InDMXChannelNode, OutChannelProperties.SignalFormat, OutChannelProperties.bLSBMode);

			TArray<int32> Offsets;
			if (IsMatrix(InDMXChannelNode, GeometryNodes))
			{
				Offsets = GetMatrixOffsetArray(InDMXChannelNode, GeometryNodes);
			}
			else
			{
				Offsets = GetOffsetArray(InDMXChannelNode);
			}
			if (Offsets.IsEmpty())
			{
				return false;
			}
			OutChannelProperties.FirstChannel = *Algo::MinElement(Offsets);

			return true;
		}

	private:
		/** Returns the number of cells if it is a matrix, or 1, if it is not a matrix */
		static int32 GetNumCells(const FXmlNode* InDMXChannelNode, const TArray<const FXmlNode*>& GeometryNodes)
		{
			if (!ensureMsgf(InDMXChannelNode->GetTag() == TEXT("dmxchannel") || InDMXChannelNode->GetTag() == TEXT("channel"), TEXT("Trying to read Channel node, but node is tagged '%s'."), *InDMXChannelNode->GetTag()))
			{
				return false;
			}

			constexpr TCHAR GeometryTag[] = TEXT("Geometry");
			const FString GeometryName = InDMXChannelNode->GetAttribute(GeometryTag);

			// Count the number of geometries this channel references
			const int32 NumReferencesInGeometry = Algo::CountIf(GeometryNodes, [&GeometryName, GeometryTag](const FXmlNode* GeometryNode)
				{
					if (GeometryNode->GetAttribute(GeometryTag) == GeometryName)
					{
						return true;
					}

					return false;
				});

			return FMath::Max(1, NumReferencesInGeometry);
		}

		/** 
		 * Returns the attribute name of the channel node. 
		 * As per legacy considers only the first function - FDMXFixtureFunction is not in support of more than one. 
		 */
		static bool GetAttributeName(const FXmlNode* InDMXChannelNode, FString& OutName)
		{
			constexpr TCHAR LogicalChannelTag[] = TEXT("LogicalChannel");
			const FXmlNode* LogicalChannelNode = InDMXChannelNode->FindChildNode(LogicalChannelTag);
			if (!LogicalChannelNode)
			{
				return false;
			}

			// Name
			constexpr TCHAR AttributeTag[] = TEXT("Attribute");
			FString AttributeName = LogicalChannelNode->GetAttribute(AttributeTag);
			
			OutName = AttributeName;
			return true;
		}

		/** 
		 * Gets the default value of the channel. Fails quietly if no default value present.
		 * As per legacy considers only the first function - FDMXFixtureFunction is not in support of more than one. 
		 */
		static void GetDefaultValue(const FXmlNode* InDMXChannelNode, int32& OutDefaultValue)
		{
			constexpr TCHAR LogicalChannelTag[] = TEXT("LogicalChannel");
			const FXmlNode* LogicalChannelNode = InDMXChannelNode->FindChildNode(LogicalChannelTag);
			if (!LogicalChannelNode)
			{
				return;
			}

			constexpr TCHAR ChannelFunctionTag[] = TEXT("ChannelFunction");
			const FXmlNode* ChannelFunctionNode = LogicalChannelNode->FindChildNode(ChannelFunctionTag);
			if (!ChannelFunctionNode)
			{
				return;
			}

			constexpr TCHAR DefaultTag[] = TEXT("Default");
			if (LexTryParseString(OutDefaultValue, *ChannelFunctionNode->GetAttribute(DefaultTag)))
			{
				return;
			}

			return;
		}

		/** Returns an array of offsets from an XmlNode */
		static TArray<int32> GetOffsetArray(const FXmlNode* XmlNode)
		{
			constexpr TCHAR OffsetTag[] = TEXT("DMXOffset");
			TArray<int32> OffsetArray;
			TArray<FString> OffsetStrArray;
			FindAttributeEvenIfDMXSubstringIsMissing(*XmlNode, OffsetTag).ParseIntoArray(OffsetStrArray, TEXT(","));

			for (int32 OffsetIndex = 0; OffsetIndex < OffsetStrArray.Num(); ++OffsetIndex)
			{
				int32 OffsetValue;
				LexTryParseString(OffsetValue, *OffsetStrArray[OffsetIndex]);
				OffsetArray.Add(OffsetValue);
			}

			return OffsetArray;
		}

		/** Returns an array of offsets from an XmlNode */
		static TArray<int32> GetMatrixOffsetArray(const FXmlNode* InDMXChannelNode, const TArray<const FXmlNode*>& GeometryNodes)
		{
			constexpr TCHAR GeometryTag[] = TEXT("Geometry");
			const FString GeometryName = InDMXChannelNode->GetAttribute(GeometryTag);

			TArray<const FXmlNode*> ReferencedGeometryNodes;
			Algo::CopyIf(GeometryNodes, ReferencedGeometryNodes, [GeometryTag, GeometryName](const FXmlNode* GeometryNode)
				{
					if (GeometryNode->GetAttribute(GeometryTag) == GeometryName)
					{
						return true;
					}

					return false;
				});

			if (ReferencedGeometryNodes.IsEmpty())
			{
				// Try to parse as normal channel instead
				return GetOffsetArray(InDMXChannelNode);
			}

			if (const FXmlNode* XmlNode = ReferencedGeometryNodes[0])
			{
				constexpr TCHAR BreakTag[] = TEXT("Break");
				if (const FXmlNode* BreakNode = XmlNode->FindChildNode(BreakTag))
				{
					return GetOffsetArray(BreakNode);
				}
			}

			return TArray<int32>();
		}

		/** Returns the data type offset array implies */
		static void GetDataType(const FXmlNode* InDMXChannelNode, EDMXFixtureSignalFormat& OutSignalFormat, bool& OutLSBOrder)
		{
			// For the data type we always need to refer to the offset of the channel, not the geometry
			TArray<int32> ChannelOffestArray = GetOffsetArray(InDMXChannelNode);

			// Compute number of used addresses
			int32 AddressMin = DMX_MAX_ADDRESS;
			int32 AddressMax = 0;
			for (const int32& Address : ChannelOffestArray)
			{
				AddressMin = FMath::Min(AddressMin, Address);
				AddressMax = FMath::Max(AddressMax, Address);
			}
			const int32 NumUsedAddresses = FMath::Clamp(AddressMax - AddressMin + 1, 1, DMX_MAX_FUNCTION_SIZE);

			OutSignalFormat = static_cast<EDMXFixtureSignalFormat>(NumUsedAddresses - 1);

			// Offsets represent the channels in MSB order. If they are in reverse order, it means this Function uses LSB format.
			if (ChannelOffestArray.Num() > 1)
			{
				OutLSBOrder = ChannelOffestArray[0] > ChannelOffestArray[1];
			}
			else
			{
				OutLSBOrder = false;
			}
		}
	};
};

bool FDMXInitializeFixtureTypeFromGDTFHelper::GenerateModesFromGDTF(UDMXEntityFixtureType& InOutFixtureType, const UDMXImportGDTF& InGDTF)
{
	FDMXInitializeFixtureTypeFromGDTFHelper Instance;
	return Instance.GenerateModesFromGDTFInternal(InOutFixtureType, InGDTF);
}

bool FDMXInitializeFixtureTypeFromGDTFHelper::GenerateModesFromGDTFInternal(UDMXEntityFixtureType& InOutFixtureType, const UDMXImportGDTF& InGDTF) const
{
	UDMXGDTFAssetImportData* GDTFAssetImportData = InGDTF.GetGDTFAssetImportData();
	if (!ensureMsgf(GDTFAssetImportData, TEXT("Found GDTF Asset that has no GDTF asset import data subobject.")))
	{
		return false;
	}

	const TSharedRef<FDMXZipper> Zip = MakeShared<FDMXZipper>();
	if (!Zip->LoadFromData(GDTFAssetImportData->GetRawSourceData()))
	{
		return false;
	}

	const FDMXZipper::FDMXScopedUnzipToTempFile ScopedUnzippedDescriptionXml(Zip, TEXT("Description.xml"));
	if (ScopedUnzippedDescriptionXml.TempFilePathAndName.IsEmpty())
	{
		return false;
	}

	const TSharedRef<FXmlFile> XmlFile = MakeShared<FXmlFile>();
	if (!XmlFile->LoadFile(ScopedUnzippedDescriptionXml.TempFilePathAndName))
	{
		return false;
	}

	// Largely a copy of GDTF Factory, but avoiding the asset
	const FXmlNode* RootNode = XmlFile->GetRootNode();
	if (!RootNode)
	{
		return false;
	}

	constexpr TCHAR FixtureTypeTag[] = TEXT("FixtureType");
	const FXmlNode* FixtureTypeNode = RootNode->FindChildNode(FixtureTypeTag);
	if (!FixtureTypeNode)
	{
		return false;
	}

	// Iterate modes and build the DMXFixtureMode array from here
	const FXmlNode* const ModesNode = UE::DMX::DMXInitializeFixtureTypeFromGDTFHelper::Private::FindChildNodeEvenIfDMXSubstringIsMissing(*FixtureTypeNode, TEXT("DMXModes"));
	if (!ModesNode)
	{
		return false;
	}

	for (const FXmlNode* DMXModeNode : ModesNode->GetChildrenNodes())
	{
		FDMXFixtureMode Mode;
		if (GenerateMode(*FixtureTypeNode, *DMXModeNode, Mode))
		{
			InOutFixtureType.Modes.Add(Mode);
		}
	}

	for (int32 ModeIndex = 0; ModeIndex < InOutFixtureType.Modes.Num(); ModeIndex++)
	{
		InOutFixtureType.UpdateChannelSpan(ModeIndex);
	}
	InOutFixtureType.GetOnFixtureTypeChanged().Broadcast(&InOutFixtureType);

	CleanupAttributes(InOutFixtureType);

	return true;
}

bool FDMXInitializeFixtureTypeFromGDTFHelper::GenerateMode(const FXmlNode& InFixtureTypeNode, const FXmlNode& InDMXModeNode, FDMXFixtureMode& OutMode) const
{
	if (!ensureMsgf(InFixtureTypeNode.GetTag() == TEXT("fixturetype"), TEXT("Trying to read Fixture Type node, but node is tagged '%s'."), *InFixtureTypeNode.GetTag()))
	{
		return false;
	}
	if (!ensureMsgf(InDMXModeNode.GetTag() == TEXT("dmxmode") || InDMXModeNode.GetTag() == TEXT("mode"), TEXT("Trying to read Channel node, but node is tagged '%s'."), *InDMXModeNode.GetTag()))
	{
		return false;
	}

	// Get geometry nodes specific to this mode (can be empty)
	TArray<const FXmlNode*> GeometryNodes;
	constexpr TCHAR GeometriesTag[] = TEXT("Geometries");
	if (const FXmlNode* GeometriesNode = InFixtureTypeNode.FindChildNode(GeometriesTag))
	{
		constexpr TCHAR GeometryTag[] = TEXT("Geometry");
		const FString GeometryName = InDMXModeNode.GetAttribute(GeometryTag);

		// Root Geometry of this Mode
		const FXmlNode* const* GeometryNodePtr = Algo::FindByPredicate(GeometriesNode->GetChildrenNodes(), [&GeometryName, GeometryTag](const FXmlNode* GeometryNode)
			{
				constexpr TCHAR NameTag[] = TEXT("Name");
				if (GeometryNode->GetAttribute(NameTag) == GeometryName)
				{
					return true;
				}

				return false;
			});

		if (GeometryNodePtr)
		{
			GeometryNodes = GetChildrenRecursive(**GeometryNodePtr);
		}
	}

	// Depending on the GDTF spec in use modes may be stored in the "Modes" or "DMXModes" node.
	const FXmlNode* const ChannelsNode = UE::DMX::DMXInitializeFixtureTypeFromGDTFHelper::Private::FindChildNodeEvenIfDMXSubstringIsMissing(InDMXModeNode, TEXT("DMXChannels"));
	if (!ChannelsNode)
	{
		return false;
	}

	FDMXFixtureMode Mode;
	Mode.FixtureMatrixConfig.CellAttributes.Reset();

	constexpr TCHAR NameTag[] = TEXT("Name");
	Mode.ModeName = InDMXModeNode.GetAttribute(NameTag); // Not using the class's default attribute

	for (const FXmlNode* DMXChannelNode : ChannelsNode->GetChildrenNodes())
	{
		using namespace UE::DMX::DMXInitializeFixtureTypeFromGDTFHelper::Private;

		FDMXGDTFChannelInterpreter::FChannelProperties ChannelProperties;
		if (!FDMXGDTFChannelInterpreter::GetChannelProperties(DMXChannelNode, GeometryNodes, ChannelProperties))
		{
			continue;
		}

		if (FDMXGDTFChannelInterpreter::IsMatrix(DMXChannelNode, GeometryNodes))
		{
			FDMXFixtureCellAttribute MatrixAttribute;
			MatrixAttribute.Attribute = FDMXAttributeName(*ChannelProperties.AttributeName);
			MatrixAttribute.bUseLSBMode = ChannelProperties.bLSBMode;
			MatrixAttribute.DataType = ChannelProperties.SignalFormat;
			MatrixAttribute.DefaultValue = ChannelProperties.DefaultValue;

			Mode.FixtureMatrixConfig.CellAttributes.Add(MatrixAttribute);
			Mode.FixtureMatrixConfig.YCells = ChannelProperties.NumCells;
			if (!Mode.bFixtureMatrixEnabled)
			{
				Mode.FixtureMatrixConfig.FirstCellChannel = ChannelProperties.FirstChannel;
				Mode.bFixtureMatrixEnabled = true;
			}
		}
		else
		{
			FDMXFixtureFunction Function;
			Function.FunctionName = ChannelProperties.AttributeName;
			Function.Attribute = FDMXAttributeName(*ChannelProperties.AttributeName);
			Function.Channel = ChannelProperties.FirstChannel;
			Function.bUseLSBMode = ChannelProperties.bLSBMode;
			Function.DataType = ChannelProperties.SignalFormat;
			Function.DefaultValue = ChannelProperties.DefaultValue;

			Mode.Functions.Add(Function);
		}
	}

	CleanupMode(Mode);

	OutMode = Mode;
	return true;
}

void FDMXInitializeFixtureTypeFromGDTFHelper::CleanupMode(FDMXFixtureMode& InOutMode) const
{
	TOptional<TRange<int32>> MatrixRange;
	if (InOutMode.bFixtureMatrixEnabled)
	{
		// Only one single, consecutive matrix is supported by the engine in this version.
		MatrixRange = TRange<int32>(InOutMode.FixtureMatrixConfig.FirstCellChannel, InOutMode.FixtureMatrixConfig.GetLastChannel() + 1);
		const FDMXFixtureFunction* OverlappingFunction = Algo::FindByPredicate(InOutMode.Functions, [MatrixRange](const FDMXFixtureFunction& Function)
			{
				const TRange<int32> FunctionRange(Function.Channel, Function.GetLastChannel() + 1);
				return FunctionRange.Overlaps(MatrixRange.GetValue());
			});

		if (OverlappingFunction)
		{
			UE_LOG(LogDMXEditor, Warning, TEXT("Mode '%s' contains many matrices, but this version of Unreal Engine only supports one matrix. Skipping import of mode."), *InOutMode.ModeName);
			InOutMode.Functions.Reset();
			InOutMode.bFixtureMatrixEnabled = false;
			InOutMode.FixtureMatrixConfig.CellAttributes.Reset();
			InOutMode.ModeName = FString::Printf(TEXT("n/a '%s' [not supported in this Engine Version]"), *InOutMode.ModeName);
		}
	}

	// Make sure functions are in consecutive order, insert 'reserved' channels where no channel is specified
	if (!InOutMode.Functions.IsEmpty())
	{
		Algo::SortBy(InOutMode.Functions, [](const FDMXFixtureFunction& Function)
			{
				return Function.Channel;
			});

		// Fill in blank functions
		const int32 LastFunctionChannel = InOutMode.Functions.Last().Channel;
		for (int32 Channel = 1; Channel < LastFunctionChannel; Channel++)
		{
			const FDMXFixtureFunction* FunctionOnChannelPtr = Algo::FindByPredicate(InOutMode.Functions, [Channel](const FDMXFixtureFunction& Function)
				{
					const TRange<int32> FunctionRange(Function.Channel, Function.GetLastChannel() + 1);
					return FunctionRange.Contains(Channel);
				});

			const bool bChannelHasFunction = FunctionOnChannelPtr != nullptr;
			const bool bChannelHasMatrix = MatrixRange.IsSet() && MatrixRange.GetValue().Contains(Channel);

			if (!bChannelHasFunction && !bChannelHasMatrix)
			{
				FDMXFixtureFunction EmptyFunction;
				EmptyFunction.Channel = Channel;
				EmptyFunction.FunctionName = TEXT("<empty>");
				InOutMode.Functions.Add(EmptyFunction);
			}
		}
	}

	Algo::SortBy(InOutMode.Functions, [](const FDMXFixtureFunction& Function)
		{
			return Function.Channel;
		});
}

void FDMXInitializeFixtureTypeFromGDTFHelper::CleanupAttributes(UDMXEntityFixtureType& InOutFixtureType) const
{
	// Get Protocol Setting's default attributes
	const UDMXProtocolSettings* ProtocolSettings = GetDefault<UDMXProtocolSettings>();
	TMap<FName, TArray<FString> > SettingsAttributeNameToKeywordsMap;
	for (const FDMXAttribute& Attribute : ProtocolSettings->Attributes)
	{
		TArray<FString> Keywords = Attribute.GetKeywords();

		SettingsAttributeNameToKeywordsMap.Emplace(Attribute.Name, Keywords);
	}

	for (FDMXFixtureMode& Mode : InOutFixtureType.Modes)
	{
		TArray<FName> AssignedAttributeNames;
		for (FDMXFixtureFunction& Function : Mode.Functions)
		{
			const TTuple<FName, TArray<FString>>* SettingsAttributeNameToKeywordPairPtr = Algo::FindByPredicate(SettingsAttributeNameToKeywordsMap, [Function](const TTuple<FName, TArray<FString>> AttributeNameToKeywordPair)
				{
					if (Function.Attribute.Name == AttributeNameToKeywordPair.Key ||
						AttributeNameToKeywordPair.Value.Contains(Function.Attribute.Name.ToString()))
					{
						return true;
					}

					return false;
				});

			if (SettingsAttributeNameToKeywordPairPtr && !AssignedAttributeNames.Contains(SettingsAttributeNameToKeywordPairPtr->Key))
			{
				Function.Attribute.Name = SettingsAttributeNameToKeywordPairPtr->Key;
				AssignedAttributeNames.Add(SettingsAttributeNameToKeywordPairPtr->Key);
			}
			else
			{
				AssignedAttributeNames.Add(Function.Attribute.Name);
			}
		}

		if (Mode.bFixtureMatrixEnabled)
		{
			TArray<FName> AssignedCellAttributeNames;
			for (FDMXFixtureCellAttribute& CellAttribute : Mode.FixtureMatrixConfig.CellAttributes)
			{
				const TTuple<FName, TArray<FString>>* SettingsAttributeNameToKeywordPairPtr = Algo::FindByPredicate(SettingsAttributeNameToKeywordsMap, [CellAttribute](const TTuple<FName, TArray<FString>> AttributeNameToKeywordPair)
					{
						if (CellAttribute.Attribute.Name == AttributeNameToKeywordPair.Key ||
							AttributeNameToKeywordPair.Value.Contains(CellAttribute.Attribute.Name.ToString()))
						{
							return true;
						}

						return false;
					});

				if (SettingsAttributeNameToKeywordPairPtr && !AssignedAttributeNames.Contains(SettingsAttributeNameToKeywordPairPtr->Key))
				{
					CellAttribute.Attribute.Name = SettingsAttributeNameToKeywordPairPtr->Key;
					AssignedAttributeNames.Add(SettingsAttributeNameToKeywordPairPtr->Key);
				}
				else
				{
					AssignedAttributeNames.Add(CellAttribute.Attribute.Name);
				}
			}
		}
	}
}

TArray<const FXmlNode*> FDMXInitializeFixtureTypeFromGDTFHelper::GetChildrenRecursive(const FXmlNode& ParentNode) const
{
	TArray<const FXmlNode*> Result;

	for (const FXmlNode* Child : ParentNode.GetChildrenNodes())
	{
		Result.Add(Child);
		Result.Append(GetChildrenRecursive(*Child));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
