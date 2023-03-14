// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXGDTFImporter.h"
#include "Library/DMXImportGDTF.h"
#include "DMXEditorLog.h"
#include "Widgets/SDMXGDTFOptionWindow.h"
#include "Factories/DMXGDTFImportUI.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "PackageTools.h"
#include "Serialization/BufferArchive.h"
#include "Containers/UnrealString.h"
#include "Factories/TextureFactory.h"
#include "Engine/Texture.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "XmlNode.h"
#include "XmlFile.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "DMXGDTFImporter"


namespace UE::DMX::DMXGDTFImporter::Private
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
};

FDMXGDTFImporter::FDMXGDTFImporter(const FDMXGDTFImportArgs& InImportArgs)
    : ImportArgs(InImportArgs)
    , bIsXMLParsedSuccessfully(false)
{
    ImportName = ObjectTools::SanitizeObjectName(ImportArgs.Name.ToString());
}

UDMXImportGDTF* FDMXGDTFImporter::Import()
{
    UDMXImportGDTF* CreatedObject = nullptr;

    // Import GDTF XML into UObject
    if (ImportArgs.ImportUI->bImportXML && bIsXMLParsedSuccessfully)
    {
        CreatedObject = CreateGDTFDesctription();
    }

    return CreatedObject;
}

UPackage* FDMXGDTFImporter::GetPackage(const FString& InImportName)
{
    UPackage* Package = nullptr;
    FString NewPackageName;

    if (!ImportArgs.ImportUI->bUseSubDirectory && ImportArgs.Parent != nullptr && ImportArgs.Parent->IsA(UPackage::StaticClass()))
    {
        Package = StaticCast<UPackage*>(ImportArgs.Parent.Get());
    }

    if (Package == nullptr)
    {
        if (ImportArgs.Parent != nullptr && ImportArgs.Parent->GetOutermost() != nullptr)
        {
            NewPackageName = FPackageName::GetLongPackagePath(ImportArgs.Parent->GetOutermost()->GetName()) + "/" + ImportName;
        }
        else
        {
            return nullptr;
        }

        if (ImportArgs.ImportUI->bUseSubDirectory)
        {
            NewPackageName /= InImportName;
        }

        NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);
        LastPackageName = NewPackageName;
        Package = CreatePackage(*NewPackageName);
        Package->FullyLoad();
    }

    return Package;
}

bool FDMXGDTFImporter::AttemptImportFromFile()
{
    if (ImportArgs.ImportUI->bImportXML)
    {
        if (!ParseXML())
        {
            UE_LOG_DMXEDITOR(Warning, TEXT("Error parsing XML"));

            return false;
        }
    }

    return true;
}

UDMXImportGDTF* FDMXGDTFImporter::CreateGDTFDesctription()
{
    UPackage* Package = GetPackage(ImportName);

    // Create new GDTF asset
    UDMXImportGDTF* ImportObject = NewObject<UDMXImportGDTF>(Package, FName(*ImportName), ImportArgs.Flags | RF_Public);

    UDMXImportGDTFFixtureType* GDTFFixtureType = ImportObject->CreateNewObject<UDMXImportGDTFFixtureType>();
    ImportObject->FixtureType = GDTFFixtureType;

    UDMXImportGDTFAttributeDefinitions* GDTFAttributeDefinitions = ImportObject->CreateNewObject<UDMXImportGDTFAttributeDefinitions>();
    ImportObject->AttributeDefinitions = GDTFAttributeDefinitions;

    UDMXImportGDTFWheels* GDTFWheels = ImportObject->CreateNewObject<UDMXImportGDTFWheels>();
    ImportObject->Wheels = GDTFWheels;

    UDMXImportGDTFPhysicalDescriptions* GDTFPhysicalDescriptions = ImportObject->CreateNewObject<UDMXImportGDTFPhysicalDescriptions>();
    ImportObject->PhysicalDescriptions = GDTFPhysicalDescriptions;

    UDMXImportGDTFModels* GDTFModels = ImportObject->CreateNewObject<UDMXImportGDTFModels>();
    ImportObject->Models = GDTFModels;

    UDMXImportGDTFGeometries* GDTFGeometries = ImportObject->CreateNewObject<UDMXImportGDTFGeometries>();
    ImportObject->Geometries = GDTFGeometries;

    UDMXImportGDTFDMXModes* GDTFDMXModes = ImportObject->CreateNewObject<UDMXImportGDTFDMXModes>();
    ImportObject->DMXModes = GDTFDMXModes;

    UDMXImportGDTFProtocols* GDTFProtocols = ImportObject->CreateNewObject<UDMXImportGDTFProtocols>();
    ImportObject->Protocols = GDTFProtocols;

    if (bIsXMLParsedSuccessfully)
    {
        if (FXmlNode* XmlNode = XMLFile->GetRootNode())
        {
            if (const FXmlNode* FixtureTypeNode = XmlNode->FindChildNode("FixtureType"))
            {
                GDTFFixtureType->Description = FixtureTypeNode->GetAttribute("Description");
                GDTFFixtureType->FixtureTypeID = FixtureTypeNode->GetAttribute("FixtureTypeID");
                GDTFFixtureType->LongName = FixtureTypeNode->GetAttribute("LongName");
                GDTFFixtureType->Manufacturer = FixtureTypeNode->GetAttribute("Manufacturer");
                GDTFFixtureType->Name = FName(*FixtureTypeNode->GetAttribute("Name"));
                GDTFFixtureType->ShortName = FixtureTypeNode->GetAttribute("ShortName");
                GDTFFixtureType->RefFT = FixtureTypeNode->GetAttribute("RefFT");

                if (const FXmlNode* AttributeDefinitionsNode = FixtureTypeNode->FindChildNode("AttributeDefinitions"))
                {
                    if (const FXmlNode* ActivationGroupsNode = AttributeDefinitionsNode->FindChildNode("ActivationGroups"))
                    {
                        for (const FXmlNode* ActivationGroupNode : ActivationGroupsNode->GetChildrenNodes())
                        {
                            FDMXImportGDTFActivationGroup ActivationGroup;
                            ActivationGroup.Name = FName(*ActivationGroupNode->GetAttribute("Name"));
                            GDTFAttributeDefinitions->ActivationGroups.Add(ActivationGroup);
                        }
                    }

                    if (const FXmlNode* FeatureGroupsNode = AttributeDefinitionsNode->FindChildNode("FeatureGroups"))
                    {
                        for (const FXmlNode* FeatureGroupNode : FeatureGroupsNode->GetChildrenNodes())
                        {
                            if (FeatureGroupNode != nullptr)
                            {
                                FDMXImportGDTFFeatureGroup FeatureGroup;
                                FeatureGroup.Name = FName(*FeatureGroupNode->GetAttribute("Name"));
                                FeatureGroup.Pretty = FeatureGroupNode->GetAttribute("Pretty");

                                for (const FXmlNode* FeatureNode : FeatureGroupNode->GetChildrenNodes())
                                {
                                    FDMXImportGDTFFeature Feature;
                                    Feature.Name = FName(*FeatureNode->GetAttribute("Name"));
                                    FeatureGroup.Features.Add(Feature);
                                }

                                GDTFAttributeDefinitions->FeatureGroups.Add(FeatureGroup);
                            }
                        }
                    }

                    if (const FXmlNode* AttributesNode = AttributeDefinitionsNode->FindChildNode("Attributes"))
                    {
                        for (const FXmlNode* AttributeNode : AttributesNode->GetChildrenNodes())
                        {
                            if (AttributeNode != nullptr)
                            {
                                FDMXImportGDTFAttribute Attribute ;
                                Attribute.Name = FName(*AttributeNode->GetAttribute("Name"));
                                Attribute.Pretty = AttributeNode->GetAttribute("Pretty");
                                Attribute.ActivationGroup.Name = FName(*AttributeNode->GetAttribute("ActivationGroup"));
                                GDTFAttributeDefinitions->FindFeature(AttributeNode->GetAttribute("Feature"), Attribute.Feature);
                                Attribute.MainAttribute = AttributeNode->GetAttribute("MainAttribute");
                                Attribute.PhysicalUnit = DMXImport::GetEnumValueFromString<EDMXImportGDTFPhysicalUnit>(AttributeNode->GetAttribute("PhysicalUnit"));
                                Attribute.Color = DMXImport::ParseColorCIE(AttributeNode->GetAttribute("Color"));
                                GDTFAttributeDefinitions->Attributes.Add(Attribute);
                            }
                        }
                    }
                }

                if (const FXmlNode* WheelsNode = FixtureTypeNode->FindChildNode("Wheels"))
                {
                    for (const FXmlNode* WheelNode : WheelsNode->GetChildrenNodes())
                    {
                        FDMXImportGDTFWheel ImportWheel;
                        ImportWheel.Name = FName(*WheelNode->GetAttribute("Name"));

                        for (const FXmlNode* SlotNode : WheelNode->GetChildrenNodes())
                        {
                            FDMXImportGDTFWheelSlot ImportWheelSlot;
                            ImportWheelSlot.Name = FName(*SlotNode->GetAttribute("Name"));
                            ImportWheelSlot.Color = DMXImport::ParseColorCIE(SlotNode->GetAttribute("Color"));
                            ImportWheel.Slots.Add(ImportWheelSlot);
                        }

                        GDTFWheels->Wheels.Add(ImportWheel);
                    }
                }

                if (const FXmlNode* PhysicalDescriptionsNode = FixtureTypeNode->FindChildNode("PhysicalDescriptions"))
                {
                    if (const FXmlNode* EmittersNode = PhysicalDescriptionsNode->FindChildNode("Emitters"))
                    {
                        for (const FXmlNode* EmitterNode : EmittersNode->GetChildrenNodes())
                        {
                            FDMXImportGDTFEmitter ImportEmitter;

                            ImportEmitter.Name = FName(*EmitterNode->GetAttribute("Name"));
                            ImportEmitter.Color = DMXImport::ParseColorCIE(EmitterNode->GetAttribute("Color"));
                            ImportEmitter.DiodePart = EmitterNode->GetAttribute("DiodePart");
                            LexTryParseString(ImportEmitter.DominantWaveLength, *EmitterNode->GetAttribute("DominantWaveLength"));
                            ImportEmitter.DiodePart = EmitterNode->GetAttribute("DiodePart");

                            if (const FXmlNode* MeasurementNode = EmitterNode->FindChildNode("Measurement"))
                            {
                                FDMXImportGDTFMeasurement ImportMeasurement;

                                LexTryParseString(ImportMeasurement.Physical, *MeasurementNode->GetAttribute("DominantWaveLength"));
                                LexTryParseString(ImportMeasurement.LuminousIntensity, *MeasurementNode->GetAttribute("LuminousIntensity"));
                                LexTryParseString(ImportMeasurement.Transmission, *MeasurementNode->GetAttribute("Transmission"));
                                ImportMeasurement.InterpolationTo = DMXImport::GetEnumValueFromString<EDMXImportGDTFInterpolationTo>(MeasurementNode->GetAttribute("InterpolationTo"));

                                for (const FXmlNode* MeasurementPointNode : MeasurementNode->GetChildrenNodes())
                                {
                                    FDMXImportGDTFMeasurementPoint ImportMeasurementPoint;

                                    LexTryParseString(ImportMeasurementPoint.Energy, *MeasurementPointNode->GetAttribute("Energy"));
                                    LexTryParseString(ImportMeasurementPoint.WaveLength, *MeasurementPointNode->GetAttribute("WaveLength"));

                                    ImportMeasurement.MeasurementPoints.Add(ImportMeasurementPoint);
                                }

                                ImportEmitter.Measurement = ImportMeasurement;
                            }

                            GDTFPhysicalDescriptions->Emitters.Add(ImportEmitter);
                        }
                    }

                    if (const FXmlNode* ColorSpaceNode = PhysicalDescriptionsNode->FindChildNode("ColorSpace"))
                    {
                        FDMXImportGDTFColorSpace ImportColorSpace;
                        ImportColorSpace.Mode = DMXImport::GetEnumValueFromString<EDMXImportGDTFMode>(ColorSpaceNode->GetAttribute("Mode"));
                        ImportColorSpace.Description = ColorSpaceNode->GetAttribute("Description");
                        ImportColorSpace.Red = DMXImport::ParseColorCIE(ColorSpaceNode->GetAttribute("Red"));
                        ImportColorSpace.Blue = DMXImport::ParseColorCIE(ColorSpaceNode->GetAttribute("Blue"));
                        ImportColorSpace.Green = DMXImport::ParseColorCIE(ColorSpaceNode->GetAttribute("Green"));
                        ImportColorSpace.WhitePoint = DMXImport::ParseColorCIE(ColorSpaceNode->GetAttribute("WhitePoint"));

                        GDTFPhysicalDescriptions->ColorSpace = ImportColorSpace;
                    }
                }

                if (const FXmlNode* ModelsNode = FixtureTypeNode->FindChildNode("Models"))
                {
                    for (const FXmlNode* ModelNode : ModelsNode->GetChildrenNodes())
                    {
                        FDMXImportGDTFModel ImportModel;

                        ImportModel.Name = FName(*ModelNode->GetAttribute("Name"));
                        LexTryParseString(ImportModel.Length, *ModelNode->GetAttribute("Length"));
                        LexTryParseString(ImportModel.Width, *ModelNode->GetAttribute("Width"));
                        LexTryParseString(ImportModel.Height, *ModelNode->GetAttribute("Height"));
                        ImportModel.PrimitiveType = DMXImport::GetEnumValueFromString<EDMXImportGDTFPrimitiveType>(ModelNode->GetAttribute("PrimitiveType"));

                        GDTFModels->Models.Add(ImportModel);
                    }
                }

                if (const FXmlNode* GeometriesNode = FixtureTypeNode->FindChildNode("Geometries"))
                {
                    for (const FXmlNode* GeometryNode : GeometriesNode->GetChildrenNodes())
                    {
                        FDMXImportGDTFGeneralGeometry ImportGeneralGeometry;
                        ImportGeneralGeometry.Name = FName(*GeometryNode->GetAttribute("Name"));
                        ImportGeneralGeometry.Model = FName(*GeometryNode->GetAttribute("Model"));
                        ImportGeneralGeometry.Position = DMXImport::ParseMatrix(GeometryNode->GetAttribute("Position"));

                        if (const FXmlNode* GeneralAxisNode = GeometryNode->FindChildNode("Axis"))
                        {
                            FDMXImportGDTFGeneralAxis ImportGeneralAxis;
                            ImportGeneralAxis.Name = FName(*GeneralAxisNode->GetAttribute("Name"));
                            ImportGeneralAxis.Model = FName(*GeneralAxisNode->GetAttribute("Model"));
                            ImportGeneralAxis.Position = DMXImport::ParseMatrix(GeneralAxisNode->GetAttribute("Position"));

                            for (const FXmlNode* TypeAxisNode : GeneralAxisNode->GetChildrenNodes())
                            {
                                FDMXImportGDTFTypeAxis ImportTypeAxis;
                                ImportTypeAxis.Name = FName(*TypeAxisNode->GetAttribute("Name"));
                                ImportTypeAxis.Model = FName(*TypeAxisNode->GetAttribute("Model"));
                                ImportTypeAxis.Position = DMXImport::ParseMatrix(TypeAxisNode->GetAttribute("Position"));

                                for (const FXmlNode* BeamNode : TypeAxisNode->GetChildrenNodes())
                                {
                                    FDMXImportGDTFBeam ImportBeam;
                                    ImportBeam.Name = FName(*BeamNode->GetAttribute("Name"));
                                    ImportBeam.Model = FName(*BeamNode->GetAttribute("Model"));
                                    ImportBeam.Position = DMXImport::ParseMatrix(BeamNode->GetAttribute("Position"));
                                    ImportBeam.LampType = DMXImport::GetEnumValueFromString<EDMXImportGDTFLampType>(BeamNode->GetAttribute("LampType"));

                                    LexTryParseString(ImportBeam.PowerConsumption, *BeamNode->GetAttribute("PowerConsumption"));
                                    LexTryParseString(ImportBeam.LuminousFlux, *BeamNode->GetAttribute("LuminousFlux"));
                                    LexTryParseString(ImportBeam.ColorTemperature, *BeamNode->GetAttribute("ColorTemperature"));
                                    LexTryParseString(ImportBeam.BeamAngle, *BeamNode->GetAttribute("BeamAngle"));
                                    LexTryParseString(ImportBeam.FieldAngle, *BeamNode->GetAttribute("FieldAngle"));
                                    LexTryParseString(ImportBeam.BeamRadius, *BeamNode->GetAttribute("BeamRadius"));
                                    ImportBeam.BeamType = DMXImport::GetEnumValueFromString<EDMXImportGDTFBeamType>(BeamNode->GetAttribute("BeamType"));
                                    LexTryParseString(ImportBeam.ColorRenderingIndex, *BeamNode->GetAttribute("ColorRenderingIndex"));

                                    ImportTypeAxis.Beams.Add(ImportBeam);
                                }
                                ImportGeneralAxis.Axis.Add(ImportTypeAxis);
                            }
                            ImportGeneralGeometry.Axis = ImportGeneralAxis;
                        }

                        if (const FXmlNode* TypeGeometryNode = GeometryNode->FindChildNode("Geometry"))
                        {
                            FDMXImportGDTFTypeGeometry ImportTypeGeometry;
                            ImportTypeGeometry.Name = FName(*TypeGeometryNode->GetAttribute("Name"));
                            ImportTypeGeometry.Model = FName(*TypeGeometryNode->GetAttribute("Model"));
                            ImportTypeGeometry.Position = DMXImport::ParseMatrix(TypeGeometryNode->GetAttribute("Position"));

                            ImportGeneralGeometry.Geometry = ImportTypeGeometry;
                        }

                        if (const FXmlNode* FilterBeamNode = GeometryNode->FindChildNode("FilterBeam"))
                        {
                            FDMXImportGDTFFilterBeam ImportFilterBeam  ;
                            ImportFilterBeam.Name = FName(*FilterBeamNode->GetAttribute("Name"));
                            ImportFilterBeam.Model = FName(*FilterBeamNode->GetAttribute("Model"));
                            ImportFilterBeam.Position = DMXImport::ParseMatrix(FilterBeamNode->GetAttribute("Position"));

                            ImportGeneralGeometry.FilterBeam = ImportFilterBeam ;
                        }

                        if (const FXmlNode* FilterColorNode = GeometryNode->FindChildNode("FilterColor"))
                        {
                            FDMXImportGDTFFilterColor ImportFilterColor;
                            ImportFilterColor.Name = FName(*FilterColorNode->GetAttribute("Name"));
                            ImportFilterColor.Model = FName(*FilterColorNode->GetAttribute("Model"));
                            ImportFilterColor.Position = DMXImport::ParseMatrix(FilterColorNode->GetAttribute("Position"));

                            ImportGeneralGeometry.FilterColor = ImportFilterColor;
                        }

                        if (const FXmlNode* FilterGoboNode = GeometryNode->FindChildNode("FilterGobo"))
                        {
                            FDMXImportGDTFFilterGobo ImportFilterGobo;
                            ImportFilterGobo.Name = FName(*FilterGoboNode->GetAttribute("Name"));
                            ImportFilterGobo.Model = FName(*FilterGoboNode->GetAttribute("Model"));
                            ImportFilterGobo.Position = DMXImport::ParseMatrix(FilterGoboNode->GetAttribute("Position"));

                            ImportGeneralGeometry.FilterGobo = ImportFilterGobo;
                        }

                        if (const FXmlNode* FilterShaperNode = GeometryNode->FindChildNode("FilterShaper"))
                        {
                            FDMXImportGDTFFilterShaper ImportFilterShaper;
                            ImportFilterShaper.Name = FName(*FilterShaperNode->GetAttribute("Name"));
                            ImportFilterShaper.Model = FName(*FilterShaperNode->GetAttribute("Model"));
                            ImportFilterShaper.Position = DMXImport::ParseMatrix(FilterShaperNode->GetAttribute("Position"));

                            ImportGeneralGeometry.FilterShaper = ImportFilterShaper;
                        }

                        if (const FXmlNode* GeometryReferenceNode = GeometryNode->FindChildNode("GeometryReference"))
                        {
                            FDMXImportGDTFGeometryReference ImportGeometryReference;
                            ImportGeometryReference.Name = FName(*GeometryReferenceNode->GetAttribute("Name"));
                            ImportGeometryReference.Model = FName(*GeometryReferenceNode->GetAttribute("Model"));
                            ImportGeometryReference.Position = DMXImport::ParseMatrix(GeometryReferenceNode->GetAttribute("Position"));

                            for (const FXmlNode* BreakNode : GeometryReferenceNode->GetChildrenNodes())
                            {
                                FDMXImportGDTFBreak ImportBreak;

								// Depending on the GDTF spec the Offset node may be called either 'DMXOffset' or 'Offset'
								const FString DMXOffsetAttribute = UE::DMX::DMXGDTFImporter::Private::FindAttributeEvenIfDMXSubstringIsMissing(*BreakNode, TEXT("DMXOffset"));
								LexTryParseString(ImportBreak.DMXOffset, *DMXOffsetAttribute);

								// Depending on the GDTF spec the Break node may be called either 'DMXBreak' or 'Break'
								const FString DMXBreakAttribute = UE::DMX::DMXGDTFImporter::Private::FindAttributeEvenIfDMXSubstringIsMissing(*BreakNode, TEXT("DMXBreak"));
								LexTryParseString(ImportBreak.DMXBreak, *DMXBreakAttribute);

                                ImportGeometryReference.Breaks.Add(ImportBreak);
                            }

                            ImportGeneralGeometry.GeometryReference = ImportGeometryReference;
                        }

                        GDTFGeometries->GeneralGeometry.Add(ImportGeneralGeometry);
                    }
                }

				// Depending on the GDTF spec the Break node may be called either 'DMXModes' or 'Modes'.
				const FXmlNode* const ModesNode = UE::DMX::DMXGDTFImporter::Private::FindChildNodeEvenIfDMXSubstringIsMissing(*FixtureTypeNode, TEXT("DMXModes"));
                if (ModesNode)
                {
                    for (const FXmlNode* DMXModeNode : ModesNode->GetChildrenNodes())
                    {
                        FDMXImportGDTFDMXMode DMXImportGDTFDMXMode;
                        DMXImportGDTFDMXMode.Name = FName(*DMXModeNode->GetAttribute("Name"));
                        DMXImportGDTFDMXMode.Geometry = FName(*DMXModeNode->GetAttribute("Geometry"));

						// Depending on the GDTF spec in use modes may be stored in the "Modes" or "DMXModes" node.
						const FXmlNode* const ChannelsNode = UE::DMX::DMXGDTFImporter::Private::FindChildNodeEvenIfDMXSubstringIsMissing(*DMXModeNode, TEXT("DMXChannels"));
                        if (ChannelsNode)
                        {
                            for (const FXmlNode* DMXChannelNode : ChannelsNode->GetChildrenNodes())
                            {
								FDMXImportGDTFDMXChannel ImportDMXChannel;

								// Ignore Channels that do not specify a valid offset.
								// E.g. 'Robe Lighting s.r.o.@Robin 800X LEDWash.gdtf' specifies virtual Dimmer channels that have no offset and cannot be accessed.
								if (!ImportDMXChannel.ParseOffset(DMXChannelNode->GetAttribute("Offset")))
								{
									continue;
								}

                                LexTryParseString(ImportDMXChannel.DMXBreak, *DMXChannelNode->GetAttribute("DMXBreak"));
                                ImportDMXChannel.Default = FDMXImportGDTFDMXValue(DMXChannelNode->GetAttribute("Default"));
                                ImportDMXChannel.Highlight = FDMXImportGDTFDMXValue(DMXChannelNode->GetAttribute("Highlight"));
                                ImportDMXChannel.Geometry = FName(*DMXChannelNode->GetAttribute("Geometry"));

                                if (const FXmlNode* LogicalChannelNode = DMXChannelNode->FindChildNode("LogicalChannel"))
                                {
                                    FDMXImportGDTFLogicalChannel ImportLogicalChannel;

                                    GDTFAttributeDefinitions->FindAtributeByName(*LogicalChannelNode->GetAttribute("Attribute"), ImportLogicalChannel.Attribute);
                                    ImportLogicalChannel.Snap = DMXImport::GetEnumValueFromString<EDMXImportGDTFSnap>(LogicalChannelNode->GetAttribute("Snap"));
                                    ImportLogicalChannel.Master = DMXImport::GetEnumValueFromString<EDMXImportGDTFMaster>(LogicalChannelNode->GetAttribute("Master"));

                                    LexTryParseString(ImportLogicalChannel.MibFade, *LogicalChannelNode->GetAttribute("MibFade"));
                                    LexTryParseString(ImportLogicalChannel.DMXChangeTimeLimit, *LogicalChannelNode->GetAttribute("DMXChangeTimeLimit"));

                                    if (const FXmlNode* ChannelFunctionNode = LogicalChannelNode->FindChildNode("ChannelFunction"))
                                    {
                                        FDMXImportGDTFChannelFunction ImportChannelFunction;
                                        ImportChannelFunction.Name = FName(*ChannelFunctionNode->GetAttribute("Name"));
                                        GDTFAttributeDefinitions->FindAtributeByName(*ChannelFunctionNode->GetAttribute("Attribute"), ImportChannelFunction.Attribute);
                                        ImportChannelFunction.OriginalAttribute = *ChannelFunctionNode->GetAttribute("OriginalAttribute");
                                        ImportChannelFunction.DMXFrom = FDMXImportGDTFDMXValue(ChannelFunctionNode->GetAttribute("DMXFrom"));
                                        ImportChannelFunction.DMXValue = FDMXImportGDTFDMXValue(ChannelFunctionNode->GetAttribute("DMXValue"));

                                        LexTryParseString(ImportChannelFunction.PhysicalFrom, *ChannelFunctionNode->GetAttribute("PhysicalFrom"));
                                        LexTryParseString(ImportChannelFunction.PhysicalTo, *ChannelFunctionNode->GetAttribute("PhysicalTo"));
                                        LexTryParseString(ImportChannelFunction.RealFade, *ChannelFunctionNode->GetAttribute("RealFade"));

                                        GDTFWheels->FindWeelByName(*ChannelFunctionNode->GetAttribute("Wheel"), ImportChannelFunction.Wheel);
                                        GDTFPhysicalDescriptions->FindEmitterByName(*ChannelFunctionNode->GetAttribute("Emitter"), ImportChannelFunction.Emitter);

                                        ImportChannelFunction.DMXInvert = DMXImport::GetEnumValueFromString<EDMXImportGDTFDMXInvert>(ChannelFunctionNode->GetAttribute("DMXInvert"));
                                        ImportChannelFunction.ModeMaster = *ChannelFunctionNode->GetAttribute("ModeMaster");
                                        ImportChannelFunction.ModeFrom = FDMXImportGDTFDMXValue(ChannelFunctionNode->GetAttribute("ModeFrom"));
                                        ImportChannelFunction.ModeTo = FDMXImportGDTFDMXValue(ChannelFunctionNode->GetAttribute("ModeTo"));

                                        for (const FXmlNode* ChannelSetNode : ChannelFunctionNode->GetChildrenNodes())
                                        {
                                            FDMXImportGDTFChannelSet ImportChannelSet;

                                            ImportChannelSet.Name = *ChannelSetNode->GetAttribute("Name");
                                            ImportChannelSet.DMXFrom = FDMXImportGDTFDMXValue(ChannelSetNode->GetAttribute("DMXFrom"));
                                            LexTryParseString(ImportChannelSet.PhysicalFrom , *ChannelSetNode->GetAttribute("PhysicalFrom"));
                                            LexTryParseString(ImportChannelSet.PhysicalTo, *ChannelSetNode->GetAttribute("PhysicalTo"));
                                            LexTryParseString(ImportChannelSet.WheelSlotIndex, *ChannelSetNode->GetAttribute("WheelSlotIndex"));

                                            ImportChannelFunction.ChannelSets.Add(ImportChannelSet);
                                        }

                                        ImportLogicalChannel.ChannelFunction = ImportChannelFunction;
                                    }

                                    ImportDMXChannel.LogicalChannel = ImportLogicalChannel;
                                }

                                DMXImportGDTFDMXMode.DMXChannels.Add(ImportDMXChannel);
                            }
                        }

                        if (const FXmlNode* RelationsNode = DMXModeNode->FindChildNode("Relations"))
                        {
                            for (const FXmlNode* RelationNode : RelationsNode->GetChildrenNodes())
                            {
                                FDMXImportGDTFRelation ImportRelation;
                                ImportRelation.Name = RelationNode->GetAttribute("Name");
                                ImportRelation.Master = RelationNode->GetAttribute("Master");
                                ImportRelation.Follower = RelationNode->GetAttribute("Follower");
                                ImportRelation.Type = DMXImport::GetEnumValueFromString<EDMXImportGDTFType>(RelationNode->GetAttribute("Type"));

                                DMXImportGDTFDMXMode.Relations.Add(ImportRelation);
                            }
                        }

                        if (const FXmlNode* FTMacrosNode = DMXModeNode->FindChildNode("FTMacros"))
                        {
                            for (const FXmlNode* FTMacroNode : FTMacrosNode->GetChildrenNodes())
                            {
                                FDMXImportGDTFFTMacro ImportFTMacro;
                                ImportFTMacro.Name = FName(*FTMacroNode->GetAttribute("Name"));
                                DMXImportGDTFDMXMode.FTMacros.Add(ImportFTMacro);
                            }
                        }

                        GDTFDMXModes->DMXModes.Add(DMXImportGDTFDMXMode);
                    }
                }

                if (const FXmlNode* ProtocolsNode = FixtureTypeNode->FindChildNode("Protocols"))
                {
                    for (const FXmlNode* ProtocolNode : ProtocolsNode->GetChildrenNodes())
                    {
                        GDTFProtocols->Protocols.Add(FName(*ProtocolNode->GetTag()));
                    }
                }
            }
        }
    }

    return ImportObject;
}

bool FDMXGDTFImporter::ParseXML()
{
    //Load the Compressed data array
    TArray<uint8> GDTFFileData;
    if (!FFileHelper::LoadFileToArray(GDTFFileData, *ImportArgs.CurrentFilename))
    {
        UE_LOG_DMXEDITOR(Warning, TEXT("InFilename %s"), *ImportArgs.CurrentFilename);
        return false;
    }

    // Convert to readable string
    FString CharString;
    {
        const uint8* DataPtr = GDTFFileData.GetData();
        int32 DataCount = GDTFFileData.Num();
        CharString.Empty(DataCount);
        while (DataCount)
        {
            uint8 Value = *DataPtr;
            CharString += TCHAR(Value);

            ++DataPtr;
            DataCount--;
        }
    }

    FString BeginXMLString("<?xml version=");
    FString EndXMLString("</GDTF>");
    int32 BeginXMLStringIndex = CharString.Find(BeginXMLString, ESearchCase::CaseSensitive);
    int32 EndXMLStringIndex = CharString.Find(EndXMLString, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    FString ResultXMLString = CharString.Mid(BeginXMLStringIndex, EndXMLStringIndex - BeginXMLStringIndex + EndXMLString.Len());

    // @TODO. UE5 XML Parser can't handle symbol < or > inside attribute and just break the parsing
    // This small algorithm remove < and > from the XML line
    TArray<FString> XMLStringLines;
    ResultXMLString.ParseIntoArrayLines(XMLStringLines, /*bCullEmpty*/false);
    FString FinalXMLString;
    for (FString& Line : XMLStringLines)
    {
        FinalXMLString.Append(Line + LINE_TERMINATOR);
    }

    XMLFile = MakeShared<FXmlFile>(FinalXMLString, EConstructMethod::ConstructFromBuffer);
    bIsXMLParsedSuccessfully = XMLFile->IsValid();

    return bIsXMLParsedSuccessfully;
}

void FDMXGDTFImporter::GetImportOptions(const TUniquePtr<FDMXGDTFImporter>& Importer, UDMXGDTFImportUI* ImportUI, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& bOutImportAll, const FString& InFilename)
{
    OutOperationCanceled = false;

    if ( bShowOptionDialog )
    {
        TSharedPtr<SWindow> ParentWindow;

        if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
        {
            IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
            ParentWindow = MainFrame.GetParentWindow();
        }

        // Compute centered window position based on max window size, which include when all categories are expanded
        const float ImportWindowWidth = 410.0f;
        const float ImportWindowHeight = 750.0f;
        FVector2D ImportWindowSize = FVector2D(ImportWindowWidth, ImportWindowHeight); // Max window size it can get based on current slate

        FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
        FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
        FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

        FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - ImportWindowSize) / 2.0f);

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("GDTFImportOpionsTitle", "GDTF Import Options"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::None)
			.ClientSize(ImportWindowSize)
			.ScreenPosition(WindowPosition);

		TSharedPtr<SDMXGDTFOptionWindow> OptionWindow;
		Window->SetContent
		(
			SAssignNew(OptionWindow, SDMXGDTFOptionWindow)
			.ImportUI(ImportUI)
			.WidgetWindow(Window)
			.FullPath(FText::FromString(FullPath))
			.MaxWindowHeight(ImportWindowHeight)
			.MaxWindowWidth(ImportWindowWidth)
		);

		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

        if (OptionWindow->ShouldImport())
        {
            bOutImportAll = OptionWindow->ShouldImportAll();

            return;
        }
        else
        {
            OutOperationCanceled = true;
        }
    }
}

#undef LOCTEXT_NAMESPACE
