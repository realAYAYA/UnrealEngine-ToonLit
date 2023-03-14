// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#define UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(Type) static ::UE::Sequencer::TAutoRegisterViewModelTypeID<Type> ID;
#define UE_SEQUENCER_DEFINE_VIEW_MODEL_TYPE_ID(Type) ::UE::Sequencer::TAutoRegisterViewModelTypeID<Type> Type::ID;

namespace UE
{
namespace Sequencer
{

struct FViewModelTypeID
{
	friend uint32 GetTypeHash(FViewModelTypeID In)
	{
		return In.ID;
	}

	friend bool operator<(FViewModelTypeID A, FViewModelTypeID B)
	{
		return A.ID < B.ID;
	}

	friend bool operator==(FViewModelTypeID A, FViewModelTypeID B)
	{
		return A.ID == B.ID;
	}

protected:

	SEQUENCERCORE_API static FViewModelTypeID RegisterNew();
	SEQUENCERCORE_API static FViewModelTypeID Invalid();

private:

	FViewModelTypeID(uint32 InID)
		: ID(InID)
	{}

	FViewModelTypeID()
		: ID(~0u)
	{}

	uint32 ID;
};

template<typename T>
struct TViewModelTypeID : FViewModelTypeID
{
	static TViewModelTypeID<T> RegisterNew()
	{
		return TViewModelTypeID<T>(FViewModelTypeID::RegisterNew());
	}

	static TViewModelTypeID<T> Invalid()
	{
		return TViewModelTypeID<T>(FViewModelTypeID::Invalid());
	}

protected:
	explicit TViewModelTypeID(FViewModelTypeID In)
		: FViewModelTypeID(In)
	{}
};

template<typename T>
struct TAutoRegisterViewModelTypeID : TViewModelTypeID<T>
{
	TAutoRegisterViewModelTypeID()
		: TViewModelTypeID<T>(TViewModelTypeID<T>::RegisterNew())
	{}
};

} // namespace Sequencer
} // namespace UE

