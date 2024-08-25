// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "ObjectPtrTestClass.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

//can not put the #if inside as the expansion of IMPLEMENT_CORE_INTRINSIC_CLASS fails
#if WITH_EDITORONLY_DATA
IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrTestClass, UObject,
	{
		auto MetaData = Class->GetOutermost()->GetMetaData();
		if (MetaData)
		{
			MetaData->SetValue(Class, TEXT("LoadBehavior"), TEXT("LazyOnDemand"));
		}
	}
);
#else
IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrTestClass, UObject, 
	{ 
	});
#endif


#if WITH_EDITORONLY_DATA
IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrTestClassWithRef, UObject,
	{
		//add reflection info for the two properties
		{
			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtr";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ObjectPtr);
			Params.PropertyFlags = CPF_TObjectPtrWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params );
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}

		{

			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtrNonNullable";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ObjectPtrNonNullable);
			Params.PropertyFlags = EPropertyFlags::CPF_NonNullable | CPF_TObjectPtrWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params);
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}


		{
			UECodeGen_Private::FArrayPropertyParams Params = { };
			Params.NameUTF8 = "ArrayObjPtr";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ArrayObjPtr);
			Params.PropertyFlags = CPF_None;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			auto Property = new FArrayProperty(Class, Params);
			auto InnerProperty = new FObjectProperty(Property, TEXT("Inner"), EObjectFlags::RF_NoFlags);
			InnerProperty->PropertyClass = UObjectPtrTestClass::StaticClass();
			Property->AddCppProperty(InnerProperty);
		}

		auto MetaData = Class->GetOutermost()->GetMetaData();
		if (MetaData)
		{
			MetaData->SetValue(Class, TEXT("LoadBehavior"), TEXT("LazyOnDemand"));
		}
	}
);
#else
IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrTestClassWithRef, UObject,
	{
		{
			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtr";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ObjectPtr);
			Params.PropertyFlags = CPF_TObjectPtrWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params);
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}

		{
			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtrNonNullable";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ObjectPtrNonNullable);
			Params.PropertyFlags = EPropertyFlags::CPF_NonNullable | CPF_TObjectPtrWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params);
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}

		{
			UECodeGen_Private::FArrayPropertyParams Params = { };
			Params.NameUTF8 = "ArrayObjPtr";
			Params.Offset = STRUCT_OFFSET(UObjectPtrTestClassWithRef, ArrayObjPtr);
			Params.PropertyFlags = CPF_None;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			auto Property = new FArrayProperty(Class, Params);
			auto InnerProperty = new FObjectProperty(Property, TEXT("Inner"), EObjectFlags::RF_NoFlags);
			InnerProperty->PropertyClass = UObjectPtrTestClass::StaticClass();
			Property->AddCppProperty(InnerProperty);
		}
	});
#endif

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectWithClassProperty, UObject,
	{
		{
			UECodeGen_Private::FClassPropertyParams Params = { };
			Params.NameUTF8 = "ClassPtr";
			Params.Offset = STRUCT_OFFSET(UObjectWithClassProperty, ClassPtr);
			Params.PropertyFlags = CPF_TObjectPtrWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			Params.MetaClassFunc = []()
			{
				return UClass::StaticClass();
			};
			auto Property = new FClassProperty(Class, Params);
			Property->PropertyClass = UObject::StaticClass();
		}
	
		{
			UECodeGen_Private::FClassPropertyParams Params = { };
			Params.NameUTF8 = "ClassRaw";
			Params.Offset = STRUCT_OFFSET(UObjectWithClassProperty, ClassRaw);
			Params.PropertyFlags = EPropertyFlags::CPF_None;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			Params.MetaClassFunc = []()
			{
				return UClass::StaticClass();
			};
			auto Property = new FClassProperty(Class, Params);
			Property->PropertyClass = UObject::StaticClass();
		}

		{
			UECodeGen_Private::FClassPropertyParams Params = { };
			Params.NameUTF8 = "SubClass";
			Params.Offset = STRUCT_OFFSET(UObjectWithClassProperty, SubClass);
			Params.PropertyFlags = EPropertyFlags::CPF_UObjectWrapper;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			Params.MetaClassFunc = []()
			{
				return UObjectPtrTestClass::StaticClass();
			};
			auto Property = new FClassProperty(Class, Params);
			Property->PropertyClass = UObject::StaticClass();
		}
	}
);

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectWithRawProperty, UObject,
	{
		{
			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtr";
			Params.Offset = STRUCT_OFFSET(UObjectWithRawProperty, ObjectPtr);
			Params.PropertyFlags = CPF_None;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params);
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}

		{
			UECodeGen_Private::FObjectPropertyParams Params = { };
			Params.NameUTF8 = "ObjectPtrNonNullable";
			Params.Offset = STRUCT_OFFSET(UObjectWithRawProperty, ObjectPtrNonNullable);
			Params.PropertyFlags = EPropertyFlags::CPF_NonNullable;
			Params.ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;
			Params.ClassFunc = nullptr;
			auto Property = new FObjectProperty(Class, Params);
			Property->PropertyClass = UObjectPtrTestClass::StaticClass();
		}
	}
);

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrDerrivedTestClass, UObjectPtrTestClass, {});

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrNotLazyTestClass, UObject, {});

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectPtrStressTestClass, UObject, {});

#endif