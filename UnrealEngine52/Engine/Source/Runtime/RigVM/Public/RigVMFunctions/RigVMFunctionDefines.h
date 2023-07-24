// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "RigVMFunctionDefines.generated.h"

UENUM()
enum class ERigVMTransformSpace : uint8
{
	/** Apply in parent space */
	LocalSpace,

	/** Apply in rig space*/
	GlobalSpace,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

UENUM()
namespace ERigVMClampSpatialMode
{
	enum Type : int
	{
		Plane,
		Cylinder,
		Sphere
	};
}

#if WITH_EDITOR
#define UE_RIGVMSTRUCT_REPORT(Severity, Format, ...) \
if(ExecuteContext.GetLog() != nullptr) \
{ \
ExecuteContext.GetLog()->Report(EMessageSeverity::Severity, ExecuteContext.GetFunctionName(), ExecuteContext.GetInstructionIndex(), FString::Printf((Format), ##__VA_ARGS__)); \
}
#define UE_RIGVMSTRUCT_LOG_MESSAGE(Format, ...) UE_RIGVMSTRUCT_REPORT(Info, (Format), ##__VA_ARGS__)
#define UE_RIGVMSTRUCT_REPORT_WARNING(Format, ...) UE_RIGVMSTRUCT_REPORT(Warning, (Format), ##__VA_ARGS__)
#define UE_RIGVMSTRUCT_REPORT_ERROR(Format, ...) UE_RIGVMSTRUCT_REPORT(Error, (Format), ##__VA_ARGS__)
#else
#define UE_RIGVMSTRUCT_REPORT(Severity, Format, ...)
#define UE_RIGVMSTRUCT_LOG_MESSAGE(Format, ...)
#define UE_RIGVMSTRUCT_REPORT_WARNING(Format, ...)
#define UE_RIGVMSTRUCT_REPORT_ERROR(Format, ...)
#endif
