// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "PropertyBag.h"
#include "StructUtilsTestTypes.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "StructUtilsTests"

PRAGMA_DISABLE_OPTIMIZATION

namespace FPropertyBagTest
{

struct FTest_CreatePropertyBag : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName IsHotName(TEXT("bIsHot"));
		static const FName TemperatureName(TEXT("Temperature"));
		static const FName CountName(TEXT("Count"));

		FInstancedPropertyBag Bag;

		Bag.AddProperty(IsHotName, EPropertyBagPropertyType::Bool);
		AITEST_TRUE(TEXT("Should have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Set bIsHot should succeed"), Bag.SetValueBool(IsHotName, true) == EPropertyBagResult::Success);

		// Amend the bag with new properties.
		Bag.AddProperties({
			{ TemperatureName, EPropertyBagPropertyType::Float },
			{ CountName, EPropertyBagPropertyType::Int32 }
			});
		AITEST_TRUE(TEXT("Set Temperature should succeed"), Bag.SetValueFloat(TemperatureName, 451.0f) == EPropertyBagResult::Success);
		AITEST_TRUE(TEXT("Set Count should succeed"), Bag.SetValueFloat(CountName, 42) == EPropertyBagResult::Success);

		Bag.RemovePropertyByName(IsHotName);
		AITEST_TRUE(TEXT("Should not have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) == nullptr);
		AITEST_TRUE(TEXT("Set bIsHot should not succeed"), Bag.SetValueBool(IsHotName, true) != EPropertyBagResult::Success);
		
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_CreatePropertyBag, "System.StructUtils.PropertyBag.CreateBag");

struct FTest_MovePropertyBag : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName IsHotName(TEXT("bIsHot"));
		static const FName TemperatureName(TEXT("Temperature"));

		FInstancedPropertyBag Bag;

		Bag.AddProperty(IsHotName, EPropertyBagPropertyType::Bool);
		AITEST_TRUE(TEXT("Bag should have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag set bIsHot should succeed"), Bag.SetValueBool(IsHotName, true) == EPropertyBagResult::Success);

		FInstancedPropertyBag Bag2(Bag);
		Bag2.AddProperty(TemperatureName, EPropertyBagPropertyType::Float);
		AITEST_TRUE(TEXT("Bag should have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag2 should have bIsHot property"), Bag2.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag2 should have Temperature property"), Bag2.FindPropertyDescByName(TemperatureName) != nullptr);

		FInstancedPropertyBag Bag3(MoveTemp(Bag));
		AITEST_TRUE(TEXT("Bag should not have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) == nullptr);
		AITEST_TRUE(TEXT("Bag3 should have bIsHot property"), Bag3.FindPropertyDescByName(IsHotName) != nullptr);

		Bag = Bag2;
		AITEST_TRUE(TEXT("Bag should have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag should have Temperature property"), Bag.FindPropertyDescByName(TemperatureName) != nullptr);
		AITEST_TRUE(TEXT("Bag2 should have bIsHot property"), Bag2.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag2 should have Temperature property"), Bag2.FindPropertyDescByName(TemperatureName) != nullptr);

		Bag = MoveTemp(Bag2);
		AITEST_TRUE(TEXT("Bag should have bIsHot property"), Bag.FindPropertyDescByName(IsHotName) != nullptr);
		AITEST_TRUE(TEXT("Bag should have Temperature property"), Bag.FindPropertyDescByName(TemperatureName) != nullptr);
		AITEST_TRUE(TEXT("Bag2 should not have bIsHot property"), Bag2.FindPropertyDescByName(IsHotName) == nullptr);
		AITEST_TRUE(TEXT("Bag2 should not have Temperature property"), Bag2.FindPropertyDescByName(TemperatureName) == nullptr);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_MovePropertyBag, "System.StructUtils.PropertyBag.MoveBag");

struct FTest_MigrateProperty : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName TemperatureName(TEXT("Temperature"));

		FInstancedPropertyBag Bag;
		Bag.AddProperty(TemperatureName, EPropertyBagPropertyType::Float);
		AITEST_TRUE(TEXT("Bag should have Temperature property"), Bag.FindPropertyDescByName(TemperatureName) != nullptr);

		TValueOrError<float, EPropertyBagResult> FloatDefaultRes = Bag.GetValueFloat(TemperatureName);
		AITEST_TRUE(TEXT("Bag getting Temperature default value should succeed"), FloatDefaultRes.IsValid());
		AITEST_TRUE(TEXT("Bag Temperature default value should be 0"), FMath::IsNearlyEqual(FloatDefaultRes.GetValue(), 0.0f));
		
		AITEST_TRUE(TEXT("Bag set Temperature as float should succeed"), Bag.SetValueFloat(TemperatureName, 451.0f) == EPropertyBagResult::Success);
		TValueOrError<float, EPropertyBagResult> FloatRes = Bag.GetValueFloat(TemperatureName);
		AITEST_TRUE(TEXT("Bag Temperature as float should be 451"), FloatRes.IsValid() && FMath::IsNearlyEqual(FloatRes.GetValue(), 451.0f));

		AITEST_TRUE(TEXT("Bag set Temperature as int should succeed"), Bag.SetValueInt32(TemperatureName, 451) == EPropertyBagResult::Success);
		FloatRes = Bag.GetValueFloat(TemperatureName);
		AITEST_TRUE(TEXT("Bag Temperature as float should be 451"), FloatRes.IsValid() && FMath::IsNearlyEqual(FloatRes.GetValue(), 451.0f));
		TValueOrError<int64, EPropertyBagResult> Int64Res = Bag.GetValueInt64(TemperatureName);
		AITEST_TRUE(TEXT("Bag Temperature as int64 should be 451"), Int64Res.IsValid() && Int64Res.GetValue() == 451);

		Bag.AddProperty(TemperatureName, EPropertyBagPropertyType::Int32);
		const FPropertyBagPropertyDesc* TempDesc = Bag.FindPropertyDescByName(TemperatureName);
		AITEST_TRUE(TEXT("Temperature property should be int32"), TempDesc != nullptr && TempDesc->ValueType == EPropertyBagPropertyType::Int32);

		TValueOrError<int32, EPropertyBagResult> Int32Res = Bag.GetValueInt32(TemperatureName);
		AITEST_TRUE(TEXT("Bag Temperature as int32 should be 451"), Int32Res.IsValid() && Int32Res.GetValue() == 451);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_MigrateProperty, "System.StructUtils.PropertyBag.MigrateProperty");

struct FTest_Object : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName ObjectName(TEXT("Object"));

		UBagTestObject1* Test1 = NewObject<UBagTestObject1>();
		UBagTestObject2* Test2 = NewObject<UBagTestObject2>();
		UBagTestObject1Derived* Test1Derived = NewObject<UBagTestObject1Derived>();

		FInstancedPropertyBag Bag;
		Bag.AddProperty(ObjectName, EPropertyBagPropertyType::Object, UBagTestObject1::StaticClass());
		AITEST_TRUE(TEXT("Bag should have Object property"), Bag.FindPropertyDescByName(ObjectName) != nullptr);

		AITEST_TRUE(TEXT("Bag set Object to Test1Derived should succeed"), Bag.SetValueObject(ObjectName, Test1Derived) == EPropertyBagResult::Success);
		AITEST_FALSE(TEXT("Bag set Object to Test2 should fail"), Bag.SetValueObject(ObjectName, Test2) == EPropertyBagResult::Success);
		AITEST_TRUE(TEXT("Bag set Object to Test1 should succeed"), Bag.SetValueObject(ObjectName, Test1) == EPropertyBagResult::Success);

		TValueOrError<UBagTestObject1*, EPropertyBagResult> Test1Res = Bag.GetValueObject<UBagTestObject1>(ObjectName);
		TValueOrError<UBagTestObject1Derived*, EPropertyBagResult> Test1DerivedRes = Bag.GetValueObject<UBagTestObject1Derived>(ObjectName);
		
		AITEST_TRUE(TEXT("Bag get Object as Test1 should succeed"), Test1Res.IsValid());
		AITEST_FALSE(TEXT("Bag get Object as Test1Derived should fail"), Test1DerivedRes.IsValid()); // Note: the current value is Test1, and Cast should fail.

		// Test conversion from Object to SoftObject
		Bag.AddProperty(ObjectName, EPropertyBagPropertyType::SoftObject, UBagTestObject1::StaticClass());
		TValueOrError<UBagTestObject1*, EPropertyBagResult> Test1Res2 = Bag.GetValueObject<UBagTestObject1>(ObjectName);
		AITEST_TRUE(TEXT("Bag get Object as Test1 should succeed after migration soft object"), Test1Res2.IsValid());
		AITEST_TRUE(TEXT("Bag get Object Test1 should be Test1 after migration soft object"), Test1Res2.GetValue() == Test1);

		// Test conversion from SoftObject to Object
		Bag.AddProperty(ObjectName, EPropertyBagPropertyType::Object, UBagTestObject1::StaticClass());
		TValueOrError<UBagTestObject1*, EPropertyBagResult> Test1Res3 = Bag.GetValueObject<UBagTestObject1>(ObjectName);
		AITEST_TRUE(TEXT("Bag get Object as Test1 should succeed after migration object"), Test1Res3.IsValid());
		AITEST_TRUE(TEXT("Bag get Object Test1 should be Test1 after migration object"), Test1Res3.GetValue() == Test1);

		// Test conversion from different type
		Bag.AddProperty(ObjectName, EPropertyBagPropertyType::Object, UBagTestObject2::StaticClass());
		TValueOrError<UBagTestObject1*, EPropertyBagResult> Test1Res4 = Bag.GetValueObject<UBagTestObject1>(ObjectName);
		TValueOrError<UBagTestObject2*, EPropertyBagResult> Test2Res = Bag.GetValueObject<UBagTestObject2>(ObjectName);
		AITEST_FALSE(TEXT("Bag get Object as Test1 should fail after migration to test2"), Test1Res4.IsValid());
		AITEST_TRUE(TEXT("Bag get Object as Test1 should succeed after migration to test2"), Test2Res.IsValid());
		AITEST_TRUE(TEXT("Bag get Object Test2 should be null after migration to test2"), Test2Res.GetValue() == nullptr);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_Object, "System.StructUtils.PropertyBag.Object");

struct FTest_Struct : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName StructName(TEXT("Struct"));

		FInstancedPropertyBag Bag;
		Bag.AddProperty(StructName, EPropertyBagPropertyType::Struct, FTestStructSimple::StaticStruct());
		AITEST_TRUE(TEXT("Bag should have Struct property"), Bag.FindPropertyDescByName(StructName) != nullptr);

		FTestStructSimple Value;
		Value.Float = 42.0f;

		FTestStructComplex Value2;

		AITEST_TRUE(TEXT("Bag set Struct as struct view should succeed"), Bag.SetValueStruct(StructName, FConstStructView::Make(Value)) == EPropertyBagResult::Success);
		AITEST_TRUE(TEXT("Bag set Struct with template should succeed"), Bag.SetValueStruct(StructName, Value) == EPropertyBagResult::Success);
		AITEST_FALSE(TEXT("Bag set Struct to complex as struct view should succeed"), Bag.SetValueStruct(StructName, FConstStructView::Make(Value2)) == EPropertyBagResult::Success);
		AITEST_FALSE(TEXT("Bag set Struct to complex  with template should succeed"), Bag.SetValueStruct(StructName, Value2) == EPropertyBagResult::Success);
		
		TValueOrError<FStructView, EPropertyBagResult> Res1 = Bag.GetValueStruct(StructName);
		TValueOrError<FTestStructSimple*, EPropertyBagResult> Res2 = Bag.GetValueStruct<FTestStructSimple>(StructName);
		TValueOrError<FTestStructSimpleBase*, EPropertyBagResult> Res3 = Bag.GetValueStruct<FTestStructSimpleBase>(StructName);
		TValueOrError<FTestStructComplex*, EPropertyBagResult> Res4 = Bag.GetValueStruct<FTestStructComplex>(StructName);
		
		AITEST_TRUE(TEXT("Bag get Struct as struct view should succeed"), Res1.IsValid());
		AITEST_TRUE(TEXT("Bag get Struct as simple should succeed"), Res2.IsValid());
		AITEST_TRUE(TEXT("Bag result value should be 42"), FMath::IsNearlyEqual(Res2.GetValue()->Float, 42.0f));
		AITEST_TRUE(TEXT("Bag get Struct as simple base should succeed"), Res3.IsValid());
		AITEST_FALSE(TEXT("Bag get Struct as complex should succeed"), Res4.IsValid());

		Bag.AddProperty(StructName, EPropertyBagPropertyType::Bool);
		TValueOrError<FStructView, EPropertyBagResult> MigRes1 = Bag.GetValueStruct(StructName);
		TValueOrError<FTestStructSimple*, EPropertyBagResult> MigRes2 = Bag.GetValueStruct<FTestStructSimple>(StructName);
		
		AITEST_FALSE(TEXT("Bag get Struct as struct view should fail after migration"), MigRes1.IsValid());
		AITEST_FALSE(TEXT("Bag get Struct as simple should succeed after migration"), MigRes2.IsValid());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_Struct, "System.StructUtils.PropertyBag.Struct");

struct FTest_Class : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName ClassName(TEXT("Class"));

		FInstancedPropertyBag Bag;
		Bag.AddProperty(ClassName, EPropertyBagPropertyType::Class, UBagTestObject1::StaticClass());
		AITEST_TRUE(TEXT("Bag should have Class property"), Bag.FindPropertyDescByName(ClassName) != nullptr);

		AITEST_TRUE(TEXT("Bag set Class to UBagTestObject1 should succeed"), Bag.SetValueClass(ClassName, UBagTestObject1::StaticClass()) == EPropertyBagResult::Success);
		AITEST_FALSE(TEXT("Bag set Class to UBagTestObject2 should fail"), Bag.SetValueClass(ClassName, UBagTestObject2::StaticClass()) == EPropertyBagResult::Success);
		AITEST_TRUE(TEXT("Bag set Class to UBagTestObject1Derived should succeed"), Bag.SetValueClass(ClassName, UBagTestObject1Derived::StaticClass()) == EPropertyBagResult::Success);

		TValueOrError<UClass*, EPropertyBagResult> Res1 = Bag.GetValueClass(ClassName);
		AITEST_TRUE(TEXT("Bag get Class should succeed"), Res1.IsValid());
		AITEST_TRUE(TEXT("Bag Class result should be UBagTestObject1Derived"), Res1.GetValue() == UBagTestObject1Derived::StaticClass());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_Class, "System.StructUtils.PropertyBag.Class");

struct FTest_Enum : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName EnumName(TEXT("Enum"));

		FInstancedPropertyBag Bag;
		Bag.AddProperty(EnumName, EPropertyBagPropertyType::Enum, StaticEnum<EPropertyBagTest1>());
		AITEST_TRUE(TEXT("Bag should have Enum property"), Bag.FindPropertyDescByName(EnumName) != nullptr);

		AITEST_TRUE(TEXT("Bag set Enum to Foo should succeed"), Bag.SetValueEnum(EnumName, EPropertyBagTest1::Foo) == EPropertyBagResult::Success);
		AITEST_FALSE(TEXT("Bag set Enum to Bongo should fail"), Bag.SetValueEnum(EnumName, EPropertyBagTest2::Bongo) == EPropertyBagResult::Success);
		
		TValueOrError<EPropertyBagTest1, EPropertyBagResult> Res1 = Bag.GetValueEnum<EPropertyBagTest1>(EnumName);
		TValueOrError<EPropertyBagTest2, EPropertyBagResult> Res2 = Bag.GetValueEnum<EPropertyBagTest2>(EnumName);
		
		AITEST_TRUE(TEXT("Bag get Enum should succeed"), Res1.IsValid());
		AITEST_TRUE(TEXT("Bag Enum result should be Byte"), Res1.GetValue() == EPropertyBagTest1::Foo);
		AITEST_FALSE(TEXT("Bag get Enum with different type should fail"), Res2.IsValid());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_Enum, "System.StructUtils.PropertyBag.Enum");

struct FTest_GC : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName EnumName(TEXT("Enum"));

		UTestObjectWithPropertyBag* Obj = NewObject<UTestObjectWithPropertyBag>();
		Obj->Bag.AddProperty(EnumName, EPropertyBagPropertyType::Enum, StaticEnum<EPropertyBagTest1>());

		const UPropertyBag* BagStruct = Obj->Bag.GetPropertyBagStruct();
		check(BagStruct);
		
		const FString BagStructName = BagStruct->GetName();
		const FString ObjName = Obj->GetName();

		// Obj is unreachable, it should be collected by the GC.
		Obj = nullptr;
		CollectGarbage(RF_NoFlags);

		// The used property bag struct should exists after the GC.
		const UPropertyBag* ExistingObj = FindObject<UPropertyBag>(GetTransientPackage(), *ObjName);
		const UPropertyBag* ExistingBagStruct1 = FindObject<UPropertyBag>(GetTransientPackage(), *BagStructName);

		AITEST_NULL(TEXT("Obj should have been released"), ExistingObj);
		AITEST_NOT_NULL(TEXT("Bag struct should exists after Obj released"), ExistingBagStruct1);

		// The next GC should collect the bag struct
		CollectGarbage(RF_NoFlags);

		const UPropertyBag* ExistingBagStruct2 = FindObject<UPropertyBag>(GetTransientPackage(), *BagStructName);
		AITEST_NULL(TEXT("Bag struct should not exists after second GC"), ExistingBagStruct2);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_GC, "System.StructUtils.PropertyBag.GC");

struct FTest_Arrays : FAITestBase
{
	virtual bool InstantTest() override
	{
		static const FName FloatArrayName(TEXT("FloatArray"));

		FInstancedPropertyBag Bag;
		Bag.AddProperties({
			{ FloatArrayName, EPropertyBagContainerType::Array, EPropertyBagPropertyType::Float },
		});

		TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> FloatArrayRes = Bag.GetArrayRef(FloatArrayName);
		AITEST_TRUE(TEXT("Get float array should succeed"), FloatArrayRes.IsValid());

		FPropertyBagArrayRef FloatArray = FloatArrayRes.GetValue();
		const int32 FloatIndex = FloatArray.AddValue();
		AITEST_TRUE(TEXT("Float array should have 1 item"), FloatArray.Num() == 1);

		const TValueOrError<float, EPropertyBagResult> GetDefaultFloatRes = FloatArray.GetValueFloat(FloatIndex);
		AITEST_TRUE(TEXT("Get float should succeed immediatelly after add"), GetDefaultFloatRes.IsValid());
		AITEST_TRUE(TEXT("Default value for Float should be 0.0f"), FMath::IsNearlyEqual(GetDefaultFloatRes.GetValue(), 0.0f));

		const EPropertyBagResult SetFloatRes = FloatArray.SetValueFloat(FloatIndex, 123.0f);
		AITEST_TRUE(TEXT("Set float should succeed"), SetFloatRes == EPropertyBagResult::Success);

		const TValueOrError<float, EPropertyBagResult> GetFloatRes = FloatArray.GetValueFloat(FloatIndex);
		AITEST_TRUE(TEXT("Get float should succeed"), GetFloatRes.IsValid());
		AITEST_TRUE(TEXT("Float value should be 123.0f"), FMath::IsNearlyEqual(GetFloatRes.GetValue(), 123.0f));

		const TValueOrError<float, EPropertyBagResult> GetFloatOOBRes = FloatArray.GetValueFloat(42);
		AITEST_FALSE(TEXT("Get float out of bounds should not succeed"), GetFloatOOBRes.IsValid());
		AITEST_TRUE(TEXT("Error should be our of bounds"), GetFloatOOBRes.GetError() == EPropertyBagResult::OutOfBounds);

		const EPropertyBagResult SetFloatOOBRes = FloatArray.SetValueFloat(-1, 0.0);
		AITEST_TRUE(TEXT("Set float out of bounds should return out of bounds"), SetFloatOOBRes == EPropertyBagResult::OutOfBounds);
		
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_Arrays, "System.StructUtils.PropertyBag.Arrays");


} // FPropertyBagTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
