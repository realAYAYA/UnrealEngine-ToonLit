// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamDefinition.h"
#include "UObject/Interface.h"
#include "IAnimNextScheduleTermInterface.generated.h"

class UAnimNextSchedule;

namespace UE::AnimNext
{
	struct FScheduleInstanceData;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UENUM()
enum class EScheduleTermDirection : uint8
{
	Input,
	Output,
};

namespace UE::AnimNext
{

struct FScheduleTerm : FParamDefinition
{
	FScheduleTerm(FName InName, const FAnimNextParamType& InType, EScheduleTermDirection InDirection)
		: FParamDefinition(InName, InType)
		, Direction(InDirection)
	{}

	FScheduleTerm(FParamId InId, const FAnimNextParamType& InType, EScheduleTermDirection InDirection)
		: FParamDefinition(InId, InType)
		, Direction(InDirection)
	{}

	EScheduleTermDirection Direction;
};

}

UINTERFACE()
class UAnimNextScheduleTermInterface : public UInterface
{
	GENERATED_BODY()
};

class IAnimNextScheduleTermInterface
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct FAnimNextSchedulePortTask;

	// Get the terms that define the data dependencies of a schedule element
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const PURE_VIRTUAL(IAnimNextScheduleTermInterface::GetTerms, return {};)
};