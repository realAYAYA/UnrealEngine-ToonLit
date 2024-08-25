// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMUClass.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/Package.h"

IMPLEMENT_CORE_INTRINSIC_CLASS(UVerseVMClass, UClass,
	{
		Class->CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UVerseVMClass);

		UE::GC::DeclareIntrinsicMembers(Class, {UE_GC_MEMBER(UVerseVMClass, Shape)});
	});

/** Default C++ class type information, used for all new UVerseVMClass objects. */
static const FCppClassTypeInfoStatic DefaultCppClassTypeInfoStatic = {false};

UVerseVMClass::UVerseVMClass(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: UClass(ObjectInitializer)
{
	SetCppTypeInfoStatic(&DefaultCppClassTypeInfoStatic);
}

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
