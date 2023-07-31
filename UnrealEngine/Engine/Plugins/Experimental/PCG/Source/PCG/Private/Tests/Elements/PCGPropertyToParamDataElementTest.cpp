// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Elements/PCGPropertyToParamDataElementTest.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGVolume.h"
#include "Elements/PCGPropertyToParamData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

// To run: automation runtest pcg.tests.PropertyToParamData

#if WITH_EDITOR

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPropertyToParamDataPropertyTypeTest, FPCGTestBaseClass, "pcg.tests.PropertyToParamData.PropertyType", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPropertyToParamDataActorFindTest, FPCGTestBaseClass, "pcg.tests.PropertyToParamData.ActorFind", PCGTestsCommon::TestFlags)

// bShouldFail is a constexpr bool used to cut the "valid" branch
// Because invalid types in expected failed tests could not compile.
template <typename AttributeType, bool bShouldFail>
bool VerifyAttributeValue(FPCGTestBaseClass* TestInstance, PCGTestsCommon::FTestData& TestData, FName PropertyName, AttributeType&& ExpectedValue, FString ExtraTestWhat)
{
	// Use universal reference to either pass an rvalue or lvalue. Use this type below to know the raw type behind.
	using RawAttributeType = std::remove_const_t<std::remove_reference_t<AttributeType>>;

	// Use TestData settings to set the property name
	UPCGPropertyToParamDataSettings* Settings = Cast<UPCGPropertyToParamDataSettings>(TestData.Settings);
	if (!TestInstance->TestNotNull(TEXT("CastToUPCGPropertyToParamDataSettings"), Settings))
	{
		return false;
	}

	Settings->PropertyName = PropertyName;
	Settings->OutputAttributeName = Settings->PropertyName;

	// Add 2 nodes, PropertyToParamDataNode and a Trivial node (just there for the connection)
	UPCGNode* TestNode = TestData.TestPCGComponent->GetGraph()->AddNode(Settings);

	UPCGTrivialSettings* TrivialSettings = NewObject<UPCGTrivialSettings>();
	UPCGNode* TrivialNode = TestData.TestPCGComponent->GetGraph()->AddNode(TrivialSettings);

	TestNode->GetOutputPins()[0]->AddEdgeTo(TrivialNode->GetInputPins()[0]);

	FPCGElementPtr Element = Settings->GetElement();
	TUniquePtr<FPCGContext> Context(Element->Initialize(FPCGDataCollection(), TestData.TestPCGComponent, TestNode));
	Context->NumAvailableTasks = 1;

	// Run the element, can throw an error.
	while (!Element->Execute(Context.Get()))
	{};

	auto FormatWithPropertyName = [&ExtraTestWhat, PropertyName](const FString& InText) -> FString
	{
		return FString::Format(TEXT("{0}_{1}_{2}"), { ExtraTestWhat, PropertyName.ToString(), InText });
	};

	bool bSuccess = true;

	if constexpr (bShouldFail)
	{
		// If it should fail, it will have no output.
		bSuccess = TestInstance->TestEqual(FormatWithPropertyName("NumOutput"), Context->OutputData.GetAllParams().Num(), 0);
	}
	else
	{
		// If it should succeed, we check the number of output and verify that the type and value matches the expected one.
		bSuccess = TestInstance->TestEqual(FormatWithPropertyName("NumOutput"), Context->OutputData.GetAllParams().Num(), 1);

		if (bSuccess)
		{
			UPCGParamData* ParamData = Cast<UPCGParamData>(Context->OutputData.GetAllParams()[0].Data);
			const FPCGMetadataAttributeBase* Attribute = ParamData->ConstMetadata()->GetConstAttribute(Settings->PropertyName);

			bSuccess = TestInstance->TestEqual(FormatWithPropertyName("AttributeType"), Attribute->GetTypeId(), PCG::Private::MetadataTypes<RawAttributeType>::Id);

			if (bSuccess)
			{
				RawAttributeType Value = static_cast<const FPCGMetadataAttribute<RawAttributeType>*>(Attribute)->GetValueFromItemKey(0);
				TestInstance->TestEqual(FormatWithPropertyName("AttributeValue"), Value, ExpectedValue);
			}
		}
	}

	// Cleanup behind ourselves.
	TestData.TestPCGComponent->GetGraph()->RemoveNode(TrivialNode);
	TestData.TestPCGComponent->GetGraph()->RemoveNode(TestNode);

	return bSuccess;
}

// Use aliases for convenience
template <typename AttributeType>
bool VerifyAttributeValueValid(FPCGTestBaseClass* TestInstance, PCGTestsCommon::FTestData& TestData, FName PropertyName, AttributeType&& ExpectedValue, FString ExtraTestWhat)
{
	return VerifyAttributeValue<AttributeType, false>(TestInstance, TestData, PropertyName, std::forward<AttributeType&&>(ExpectedValue), ExtraTestWhat);
}

template <typename AttributeType>
bool VerifyAttributeValueInvalid(FPCGTestBaseClass* TestInstance, PCGTestsCommon::FTestData& TestData, FName PropertyName, AttributeType&& ExpectedValue, FString ExtraTestWhat)
{
	return VerifyAttributeValue<AttributeType, true>(TestInstance, TestData, PropertyName, std::forward<AttributeType&&>(ExpectedValue), ExtraTestWhat);
}

bool FPCGPropertyToParamDataPropertyTypeTest::RunTest(const FString& Parameters)
{
	bool bSuccess = true;

	UPCGPropertyToParamDataSettings* Settings = NewObject<UPCGPropertyToParamDataSettings>();
	Settings->ActorSelection = EPCGActorSelection::ByClass;
	Settings->ActorSelectionClass = APCGUnitTestDummyActor::StaticClass();
	Settings->ActorFilter = EPCGActorFilter::Self;

	static constexpr int32 Seed = 42;
	const FString ExtraTestWhat = "PropertyToParamDataPropertyTypeTest";

	PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());

	// Set all properties
	const FName NameValue = TEXT("HelloWorld");
	const FString StringValue = TEXT("HelloWorld");

	const FVector VectorValue{ 1.0, 2.0, 3.0 };
	const FVector4 Vector4Value{ 1.0, 2.0, 3.0, 4.0 };
	const FRotator RotatorValue{ 45.0, 45.0, 45.0 };
	const FQuat QuatValue = RotatorValue.Quaternion();
	const FTransform TransformValue{ QuatValue, VectorValue, VectorValue };

	APCGVolume* PCGVolume = NewObject<APCGVolume>();

	const FSoftObjectPath SoftObjectPathValue{ PCGVolume };
	const FSoftClassPath SoftClassPathValue{ APCGVolume::StaticClass() };

	const FVector2D Vector2Value = { 1.0, 2.0 };
	const FColor ColorValue = FColor::Cyan;

	APCGUnitTestDummyActor* Actor = Cast<APCGUnitTestDummyActor>(TestData.TestActor);
	Actor->IntProperty = 42;
	Actor->Int64Property = 42ll;
	Actor->FloatProperty = 1.0f;
	Actor->DoubleProperty = 1.0;
	Actor->BoolProperty = true;
	Actor->NameProperty = NameValue;
	Actor->StringProperty = StringValue;
	Actor->EnumProperty = EPCGUnitTestDummyEnum::Three;
	Actor->VectorProperty = VectorValue;
	Actor->Vector4Property = Vector4Value;
	Actor->RotatorProperty = RotatorValue;
	Actor->QuatProperty = QuatValue;
	Actor->TransformProperty = TransformValue;
	Actor->SoftObjectPathProperty = SoftObjectPathValue;
	Actor->SoftClassPathProperty = SoftClassPathValue;
	Actor->ClassProperty = APCGVolume::StaticClass();
	Actor->ObjectProperty = PCGVolume;
	Actor->Vector2Property = Vector2Value;
	Actor->ColorProperty = ColorValue;

	// Basic properties
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, Int64Property), 42ll, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, FloatProperty), 1.0, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, DoubleProperty), 1.0, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, BoolProperty), true, ExtraTestWhat);

	// String/Name Properties
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, NameProperty), NameValue, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, StringProperty), StringValue, ExtraTestWhat);

	// Enum Property
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, EnumProperty), (int64)EPCGUnitTestDummyEnum::Three, ExtraTestWhat);

	// Struct Properties
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, VectorProperty), VectorValue, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, Vector4Property), Vector4Value, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, RotatorProperty), RotatorValue, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, QuatProperty), QuatValue, ExtraTestWhat);
	/** TODO: Uncomment this when the == & != operators are defined for FTransform on FN */
	//bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, TransformProperty), TransformValue, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, SoftObjectPathProperty), SoftObjectPathValue.ToString(), ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, SoftClassPathProperty), SoftClassPathValue.ToString(), ExtraTestWhat);

	// Objects properties
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, ClassProperty), APCGVolume::StaticClass()->GetPathName(), ExtraTestWhat);
	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, ObjectProperty), PCGVolume->GetPathName(), ExtraTestWhat);

	// Unsupported properties
	AddExpectedError(TEXT("Error while creating an attribute. Either the property type is not supported by PCG or attribute creation failed."), EAutomationExpectedErrorFlags::Contains, 2);
	bSuccess &= VerifyAttributeValueInvalid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, Vector2Property), Vector2Value, ExtraTestWhat);
	bSuccess &= VerifyAttributeValueInvalid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, ColorProperty), ColorValue, ExtraTestWhat);

	// Unknown property
	AddExpectedError(TEXT("Property doesn't exist in the found actor."), EAutomationExpectedErrorFlags::Contains, 1);
	bSuccess &= VerifyAttributeValueInvalid(this, TestData, TEXT("DummyMissingProperty"), 42, ExtraTestWhat);

	// Missing property
	AddExpectedError(TEXT("Some parameters are missing, abort."), EAutomationExpectedErrorFlags::Contains, 1);
	bSuccess &= VerifyAttributeValueInvalid(this, TestData, NAME_None, 42, ExtraTestWhat);

	return bSuccess;
}

bool FPCGPropertyToParamDataActorFindTest::RunTest(const FString& Parameters)
{
	bool bSuccess = true;

	static constexpr int32 Seed = 42;
	static const FName Tag = TEXT("PCGUnitTestDummyMyTag");
	static const FString Name = TEXT("MyPCGUnitTestDummyActor");

	UPCGPropertyToParamDataSettings* Settings = NewObject<UPCGPropertyToParamDataSettings>();
	Settings->ActorSelectionClass = APCGUnitTestDummyActor::StaticClass();
	Settings->ActorSelectionTag = Tag;
	Settings->ActorSelectionName = APCGUnitTestDummyActor::StaticClass()->GetFName();
	Settings->ComponentClass = UPCGUnitTestDummyComponent::StaticClass();

	// Self by class
	{
		Settings->ActorSelection = EPCGActorSelection::ByClass;
		Settings->ActorFilter = EPCGActorFilter::Self;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Self_Class");
	}

	// Self by tag
	{
		Settings->ActorSelection = EPCGActorSelection::ByTag;
		Settings->ActorFilter = EPCGActorFilter::Self;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		TestData.TestActor->Tags.Add(Tag);
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Self_Tag");
	}

	// Self by name
	{
		Settings->ActorSelection = EPCGActorSelection::ByName;
		Settings->ActorFilter = EPCGActorFilter::Self;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Self_Name");
	}

	// TODO: Need a good way to spawn actors with parenting relation between them
	//// Parent by class
	//{
	//	Settings->ActorSelection = EPCGActorSelection::ByClass;
	//	Settings->ActorFilter = EPCGActorFilter::Parent;

	//	PCGTestsCommon::FTestData TestData(Seed, Settings, AActor::StaticClass());
	//	TestData.AddActor(APCGUnitTestDummyActor::StaticClass(), true);
	//	Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

	//	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Parent_Class");
	//}

	//// Root by tag
	//{
	//	Settings->ActorSelection = EPCGActorSelection::ByTag;
	//	Settings->ActorFilter = EPCGActorFilter::Root;

	//	PCGTestsCommon::FTestData TestData(Seed, Settings, AActor::StaticClass());
	//	TestData.AddActor(AActor::StaticClass(), true);
	//	TestData.AddActor(APCGUnitTestDummyActor::StaticClass(), true);
	//	TestData.TestActor->Tags.Add(Tag);
	//	Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

	//	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Root_Tag");
	//}

	//// Root by tag including children
	//{
	//	Settings->ActorSelection = EPCGActorSelection::ByTag;
	//	Settings->ActorFilter = EPCGActorFilter::Root;
	//	Settings->bIncludeChildren = true;

	//	PCGTestsCommon::FTestData TestData(Seed, Settings, AActor::StaticClass());
	//	TestData.AddActor(APCGUnitTestDummyActor::StaticClass(), true);
	//	TestData.TestActor->Tags.Add(Tag);
	//	Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

	//	TestData.AddActor(AActor::StaticClass(), true);

	//	bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Root_Tag_Children");
	//	Settings->bIncludeChildren = false;
	//}

	// Self by class including component
	{
		Settings->ActorSelection = EPCGActorSelection::ByTag;
		Settings->ActorFilter = EPCGActorFilter::Self;
		Settings->bSelectComponent = true;
		
		PCGTestsCommon::FTestData TestData(Seed, Settings, AActor::StaticClass());
		TestData.TestActor->Tags.Add(Tag);
		UPCGUnitTestDummyComponent* Component = Cast< UPCGUnitTestDummyComponent>(TestData.TestActor->AddComponentByClass(Settings->ComponentClass, false, FTransform::Identity, false));
		Component->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(UPCGUnitTestDummyComponent, IntProperty), 42ll, "PropertyToParamDataActorFindTest_Self_Tag_Component");
		Settings->bSelectComponent = false;
	}

	// World by class
	{
		Settings->ActorSelection = EPCGActorSelection::ByClass;
		Settings->ActorFilter = EPCGActorFilter::AllWorldActors;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_World_Class");
	}

	// World by tag
	{
		Settings->ActorSelection = EPCGActorSelection::ByTag;
		Settings->ActorFilter = EPCGActorFilter::AllWorldActors;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		TestData.TestActor->Tags.Add(Tag);
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_World_Tag");
	}

	// World by name
	{
		Settings->ActorSelection = EPCGActorSelection::ByName;
		Settings->ActorFilter = EPCGActorFilter::AllWorldActors;

		PCGTestsCommon::FTestData TestData(Seed, Settings, APCGUnitTestDummyActor::StaticClass());
		Cast<APCGUnitTestDummyActor>(TestData.TestActor)->IntProperty = 42;

		bSuccess &= VerifyAttributeValueValid(this, TestData, GET_MEMBER_NAME_CHECKED(APCGUnitTestDummyActor, IntProperty), 42ll, "PropertyToParamDataActorFindTest_World_Name");
	}

	return bSuccess;
}

#endif // WITH_EDITOR