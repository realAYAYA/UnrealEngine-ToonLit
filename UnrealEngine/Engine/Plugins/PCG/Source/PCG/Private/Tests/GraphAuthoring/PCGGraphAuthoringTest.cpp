// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Tests/PCGTestsCommon.h"
#include "Tests/GraphAuthoring/PCGGraphAuthoringTestHelperSettings.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGGraphAuthoringTypeSystemTest, FPCGTestBaseClass, "Plugins.PCG.GraphAuthoring.TypeSystem", PCGTestsCommon::TestFlags)

bool FPCGGraphAuthoringTypeSystemTest::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(0);
	UPCGGraph* Graph = TestData.TestPCGComponent->GetGraph();

	UPCGSettings* UpstreamNodeUntypedSettings = nullptr;
	UPCGSettings* DownstreamNodeUntypedSettings = nullptr;
	UPCGNode* UpstreamNode = Graph->AddNodeOfType(UPCGGraphAuthoringTestHelperSettings::StaticClass(), UpstreamNodeUntypedSettings);
	UPCGNode* DownstreamNode = Graph->AddNodeOfType(UPCGGraphAuthoringTestHelperSettings::StaticClass(), DownstreamNodeUntypedSettings);
	UPCGGraphAuthoringTestHelperSettings* UpstreamNodeSettings = CastChecked<UPCGGraphAuthoringTestHelperSettings>(UpstreamNodeUntypedSettings);
	UPCGGraphAuthoringTestHelperSettings* DownstreamNodeSettings = CastChecked<UPCGGraphAuthoringTestHelperSettings>(DownstreamNodeUntypedSettings);

	auto ValidateConversion = [&](
		const FString& InWhat,
		EPCGDataType InUpstreamType,
		EPCGDataType InDownstreamType,
		bool bInExpectedCompatible,
		EPCGTypeConversion InExpectedConversion = EPCGTypeConversion::NoConversionRequired)
	{
		UpstreamNodeSettings->PinType = InUpstreamType;
		UpstreamNode->SetSettingsInterface(UpstreamNodeSettings);

		DownstreamNodeSettings->PinType = InDownstreamType;
		DownstreamNode->SetSettingsInterface(DownstreamNodeSettings);

		const UPCGPin* UpstreamPin = UpstreamNode->GetOutputPin(PCGPinConstants::DefaultOutputLabel);
		const UPCGPin* DownstreamPin = DownstreamNode->GetInputPin(PCGPinConstants::DefaultInputLabel);
		check(UpstreamPin && DownstreamPin);

		const bool bCompatible = UpstreamPin->IsCompatible(DownstreamPin);
		bool bValidationPassed = TestEqual(InWhat, bCompatible, bInExpectedCompatible);

		// If compatibility test was successful, and if types are compatible, check their conversion.
		if (bCompatible && bInExpectedCompatible)
		{
			bValidationPassed &= TestEqual(InWhat, UpstreamPin->GetRequiredTypeConversion(DownstreamPin), InExpectedConversion);
		}

		return bValidationPassed;
	};

	bool bTestPassed = true;

	// Trivial
	bTestPassed &= ValidateConversion("None to none", EPCGDataType::None, EPCGDataType::None, false);
	bTestPassed &= ValidateConversion("Point to none", EPCGDataType::Point, EPCGDataType::None, false);
	bTestPassed &= ValidateConversion("None to point", EPCGDataType::None, EPCGDataType::Point, false);

	// No conversion needed
	bTestPassed &= ValidateConversion("Matching types: Point to Point", EPCGDataType::Point, EPCGDataType::Point, true);
	bTestPassed &= ValidateConversion("Matching types: Landscape to Landscape", EPCGDataType::Landscape, EPCGDataType::Landscape, true);
	bTestPassed &= ValidateConversion("Matching types: Surface to Surface", EPCGDataType::Surface, EPCGDataType::Surface, true);
	bTestPassed &= ValidateConversion("Matching types: Concrete to Concrete", EPCGDataType::Concrete, EPCGDataType::Concrete, true);
	bTestPassed &= ValidateConversion("Matching types: Spatial to Spatial", EPCGDataType::Spatial, EPCGDataType::Spatial, true);
	bTestPassed &= ValidateConversion("Matching types: Other to Other", EPCGDataType::Other, EPCGDataType::Other, true);
	bTestPassed &= ValidateConversion("Matching types: Attribute Set to Attribute Set", EPCGDataType::Param, EPCGDataType::Param, true);
	bTestPassed &= ValidateConversion("Matching types: Any to Any", EPCGDataType::Any, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Landscape to Surface", EPCGDataType::Landscape, EPCGDataType::Surface, true);
	bTestPassed &= ValidateConversion("Compatible types: Point to Concrete", EPCGDataType::Point, EPCGDataType::Concrete, true);
	bTestPassed &= ValidateConversion("Compatible types: Landscape to Concrete", EPCGDataType::Landscape, EPCGDataType::Concrete, true);
	bTestPassed &= ValidateConversion("Compatible types: Surface to Concrete", EPCGDataType::Surface, EPCGDataType::Concrete, true);
	bTestPassed &= ValidateConversion("Compatible types: Point to Spatial", EPCGDataType::Point, EPCGDataType::Spatial, true);
	bTestPassed &= ValidateConversion("Compatible types: Landscape to Spatial", EPCGDataType::Landscape, EPCGDataType::Spatial, true);
	bTestPassed &= ValidateConversion("Compatible types: Surface to Spatial", EPCGDataType::Surface, EPCGDataType::Spatial, true);
	bTestPassed &= ValidateConversion("Compatible types: Concrete to Spatial", EPCGDataType::Concrete, EPCGDataType::Spatial, true);
	bTestPassed &= ValidateConversion("Compatible types: Point to Any", EPCGDataType::Point, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Landscape to Any", EPCGDataType::Landscape, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Surface to Any", EPCGDataType::Surface, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Concrete to Any", EPCGDataType::Concrete, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Spatial to Any", EPCGDataType::Spatial, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Attribute Set to Any", EPCGDataType::Param, EPCGDataType::Any, true);
	bTestPassed &= ValidateConversion("Compatible types: Other to Any", EPCGDataType::Other, EPCGDataType::Any, true);

	// Collapse
	bTestPassed &= ValidateConversion("Collapse: Spline to Point", EPCGDataType::Spline, EPCGDataType::Point, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Landscape to Point", EPCGDataType::Landscape, EPCGDataType::Point, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Surface to Point", EPCGDataType::Surface, EPCGDataType::Point, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Concrete to Point", EPCGDataType::Concrete, EPCGDataType::Point, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Spatial to Point", EPCGDataType::Spatial, EPCGDataType::Point, true, EPCGTypeConversion::CollapseToPoint);

	// Collapse, with Point | Param
	bTestPassed &= ValidateConversion("Collapse: Spline to Point", EPCGDataType::Spline, EPCGDataType::PointOrParam, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Landscape to Point", EPCGDataType::Landscape, EPCGDataType::PointOrParam, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Surface to Point", EPCGDataType::Surface, EPCGDataType::PointOrParam, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Concrete to Point", EPCGDataType::Concrete, EPCGDataType::PointOrParam, true, EPCGTypeConversion::CollapseToPoint);
	bTestPassed &= ValidateConversion("Collapse: Spatial to Point", EPCGDataType::Spatial, EPCGDataType::PointOrParam, true, EPCGTypeConversion::CollapseToPoint);

	// Filter
	bTestPassed &= ValidateConversion("Filter: Any to Point", EPCGDataType::Any, EPCGDataType::Point, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Any to Landscape", EPCGDataType::Any, EPCGDataType::Landscape, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Any to Surface", EPCGDataType::Any, EPCGDataType::Surface, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Any to Spatial", EPCGDataType::Any, EPCGDataType::Spatial, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Any to Attribute Set", EPCGDataType::Any, EPCGDataType::Param, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Spatial to Surface", EPCGDataType::Spatial, EPCGDataType::Surface, true, EPCGTypeConversion::Filter);
	bTestPassed &= ValidateConversion("Filter: Spatial to Landscape", EPCGDataType::Spatial, EPCGDataType::Landscape, true, EPCGTypeConversion::Filter);

	// Make Concrete
	bTestPassed &= ValidateConversion("Make Concrete: Spatial to Concrete", EPCGDataType::Spatial, EPCGDataType::Concrete, true, EPCGTypeConversion::MakeConcrete);
	bTestPassed &= ValidateConversion("Make Concrete: Any to Concrete", EPCGDataType::Any, EPCGDataType::Concrete, true, EPCGTypeConversion::MakeConcrete);

	// Incompatible
	bTestPassed &= ValidateConversion("Incompatible: Attribute Set to Point", EPCGDataType::Param, EPCGDataType::Point, false);
	bTestPassed &= ValidateConversion("Incompatible: Attribute Set to Landscape", EPCGDataType::Param, EPCGDataType::Landscape, false);
	bTestPassed &= ValidateConversion("Incompatible: Attribute Set to Surface", EPCGDataType::Param, EPCGDataType::Surface, false);
	bTestPassed &= ValidateConversion("Incompatible: Attribute Set to Concrete", EPCGDataType::Param, EPCGDataType::Concrete, false);
	bTestPassed &= ValidateConversion("Incompatible: Attribute Set to Spatial", EPCGDataType::Param, EPCGDataType::Spatial, false);
	bTestPassed &= ValidateConversion("Incompatible: Other to Point", EPCGDataType::Other, EPCGDataType::Point, false);
	bTestPassed &= ValidateConversion("Incompatible: Other to Landscape", EPCGDataType::Other, EPCGDataType::Landscape, false);
	bTestPassed &= ValidateConversion("Incompatible: Other to Surface", EPCGDataType::Other, EPCGDataType::Surface, false);
	bTestPassed &= ValidateConversion("Incompatible: Other to Concrete", EPCGDataType::Other, EPCGDataType::Concrete, false);
	bTestPassed &= ValidateConversion("Incompatible: Other to Spatial", EPCGDataType::Other, EPCGDataType::Spatial, false);

	return bTestPassed;
}

#endif // WITH_EDITOR
