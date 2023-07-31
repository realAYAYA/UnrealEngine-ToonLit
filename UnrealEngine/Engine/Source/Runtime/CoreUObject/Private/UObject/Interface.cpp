// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/Interface.h"
#include "UObject/Class.h"

#if WITH_EDITOR
IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
	{
		Class->CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UInterface);
		Class->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
		Class->SetMetaData(TEXT("CannotImplementInterfaceInBlueprint"), TEXT(""));
	}
);

#else

IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
{
	Class->CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UInterface);
}
);

#endif
