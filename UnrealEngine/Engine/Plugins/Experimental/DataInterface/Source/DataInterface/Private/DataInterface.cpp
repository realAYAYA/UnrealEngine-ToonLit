// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface.h"
#include "IDataInterface.h"
#include "DataInterfaceContext.h"

namespace UE::DataInterface::Private
{

static bool CheckCompatibility(const IDataInterface* InDataInterface, const UE::DataInterface::FContext& InContext)
{
	check(InDataInterface);
	return InContext.GetResultRaw().GetType().GetName() == InDataInterface->GetReturnTypeName();
}

}

bool IDataInterface::GetDataIfCompatibleInternal(const UE::DataInterface::FContext& InContext) const
{
	if(UE::DataInterface::Private::CheckCompatibility(this, InContext))
	{
		return GetDataRawInternal(InContext);
	}

	return false;
}

bool IDataInterface::GetData(const UE::DataInterface::FContext& Context, UE::DataInterface::FParam& OutResult) const
{
	const UE::DataInterface::FContext CallingContext = Context.WithResult(OutResult);
	return GetDataIfCompatibleInternal(CallingContext);
}

bool IDataInterface::GetDataChecked(const UE::DataInterface::FContext& Context, UE::DataInterface::FParam& OutResult) const
{
	const UE::DataInterface::FContext CallingContext = Context.WithResult(OutResult);
	check(UE::DataInterface::Private::CheckCompatibility(this, CallingContext));
	return GetDataRawInternal(CallingContext);
}

bool IDataInterface::GetDataRawInternal(const UE::DataInterface::FContext& InContext) const
{
	// TODO: debug recording here on context

	const UE::DataInterface::FContext CallContext = InContext.WithCallRaw(this);
	return GetDataImpl(CallContext);
}
