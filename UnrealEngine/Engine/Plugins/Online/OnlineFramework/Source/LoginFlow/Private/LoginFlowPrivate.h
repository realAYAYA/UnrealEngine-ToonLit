// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

 /* Dependencies
 *****************************************************************************/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "OnlineSubsystem.h"

 /* Interfaces
 *****************************************************************************/

#include "ILoginFlowModule.h"



#define FACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	static ReturnType Create(__VA_ARGS__); \
}; 

#define IFACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	virtual ReturnType Create(__VA_ARGS__) = 0; \
};

