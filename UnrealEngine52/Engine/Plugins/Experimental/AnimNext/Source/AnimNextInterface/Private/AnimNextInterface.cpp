// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterface.h"
#include "IAnimNextInterface.h"
#include "AnimNextInterfaceContext.h"

namespace UE::AnimNext::Interface::Private
{

static bool CheckCompatibility(const IAnimNextInterface* InAnimNextInterface, const UE::AnimNext::Interface::FContext& InContext)
{
	check(InAnimNextInterface);
	return InContext.GetResultParam().GetType().GetName() == InAnimNextInterface->GetReturnTypeName();
}

}

bool IAnimNextInterface::GetDataIfCompatibleInternal(const UE::AnimNext::Interface::FContext& InContext) const
{
	if(UE::AnimNext::Interface::Private::CheckCompatibility(this, InContext))
	{
		return GetDataRawInternal(InContext);
	}

	return false;
}

bool IAnimNextInterface::GetData(const UE::AnimNext::Interface::FContext& Context) const
{
	return GetDataIfCompatibleInternal(Context);
}

bool IAnimNextInterface::GetDataChecked(const UE::AnimNext::Interface::FContext& Context) const
{
	check(UE::AnimNext::Interface::Private::CheckCompatibility(this, Context));
	return GetDataRawInternal(Context);
}

bool IAnimNextInterface::GetData(const UE::AnimNext::Interface::FContext& Context, UE::AnimNext::Interface::FParam& OutResult) const
{
	const UE::AnimNext::Interface::FContext CallingContext = Context.WithResult(OutResult);
	return GetDataIfCompatibleInternal(CallingContext);
}

bool IAnimNextInterface::GetDataChecked(const UE::AnimNext::Interface::FContext& Context, UE::AnimNext::Interface::FParam& OutResult) const
{
	const UE::AnimNext::Interface::FContext CallingContext = Context.WithResult(OutResult);
	check(UE::AnimNext::Interface::Private::CheckCompatibility(this, CallingContext));
	return GetDataRawInternal(CallingContext);
}

bool IAnimNextInterface::GetDataRawInternal(const UE::AnimNext::Interface::FContext& InContext) const
{
	// TODO: debug recording here on context

	const UE::AnimNext::Interface::FContext CallContext = InContext.WithCallRaw(this);
	return GetDataImpl(CallContext);
}
