// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/SortedMap.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelTypeID.h"
#include "Templates/SharedPointer.h"
#include "Misc/InlineValue.h"

namespace UE
{
namespace Sequencer
{

class FDynamicExtensionContainer;
class FViewModel;

/**
 * Base class for dynamic extensions that can be added to sequence data models and participate in dynamic casting.
 */
class SEQUENCERCORE_API IDynamicExtension : public TSharedFromThis<IDynamicExtension>
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IDynamicExtension)

	virtual ~IDynamicExtension(){}

	/** Called when the extension is created on a data model */
	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) {}
	/** Called after all extensions have been created on a data model */
	virtual void OnPostInitialize() { OnReinitialize(); }
	/** Called to reinitialize extensions after a major change */
	virtual void OnReinitialize() {}
};

DECLARE_DELEGATE_OneParam(FDynamicExtensionCallback, TSharedRef<FViewModel>);

/**
 * Base class for supporting dynamic extensions that participate in dynamic casting.
 */
class SEQUENCERCORE_API FDynamicExtensionContainer
{
public:

	virtual ~FDynamicExtensionContainer() {}

	FDynamicExtensionContainer() = default;

	FDynamicExtensionContainer(const FDynamicExtensionContainer&) = delete;
	FDynamicExtensionContainer& operator=(const FDynamicExtensionContainer&) = delete;

public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FDynamicExtensionContainer)

	template<typename T>
	const T* CastDynamic() const
	{
		const void* Result = CastDynamic(T::ID);
		return static_cast<const T*>(Result);
	}

	template<typename T>
	const T* CastDynamicChecked() const
	{
		const void* Result = CastDynamicChecked(T::ID);
		return static_cast<const T*>(Result);
	}

	template<typename T>
	T* CastDynamic()
	{
		return const_cast<T*>(const_cast<const FDynamicExtensionContainer*>(this)->CastDynamic<T>());
	}

	template<typename T>
	T* CastDynamicChecked()
	{
		return const_cast<T*>(const_cast<const FDynamicExtensionContainer*>(this)->CastDynamicChecked<T>());
	}

	const void* CastDynamic(FViewModelTypeID Type) const
	{
		for (const TInlineValue<FDynamicExtensionInfo>& DynamicExtension : DynamicExtensions)
		{
			if (const void* Result = DynamicExtension->CastDynamic(Type))
			{
				return Result;
			}
		}
		return nullptr;
	}

	const void* CastDynamicChecked(FViewModelTypeID Type) const
	{
		const void* Result = CastDynamic(Type);
		check(Result);
		return Result;
	}

	void* CastDynamic(FViewModelTypeID Type)
	{
		return const_cast<void*>(const_cast<const FDynamicExtensionContainer*>(this)->CastDynamic(Type));
	}

	void* CastDynamicChecked(FViewModelTypeID Type)
	{
		void* Result = CastDynamic(Type);
		check(Result);
		return Result;
	}

protected:

	template<typename T, typename... InArgTypes>
	T& AddDynamicExtension(TSharedRef<FViewModel> InOwner, InArgTypes&&... Args)
	{
		T* Existing = CastDynamic<T>();
		if (Existing)
		{
			return static_cast<T&>(*Existing);
		}

		static_assert(sizeof(FDynamicExtensionInfo) == sizeof(TDynamicExtensionInfo<T>), "Typed extension wrapper can't contain extra data.");

		TSharedRef<T> NewExtension = MakeShared<T>(Forward<InArgTypes>(Args)...);
		DynamicExtensions.Add(TDynamicExtensionInfo<T>(NewExtension));
		NewExtension->OnCreated(InOwner);
		return NewExtension.Get();
	}

	void PostInitializeExtensions()
	{
		for (const TInlineValue<FDynamicExtensionInfo>& DynamicExtension : DynamicExtensions)
		{
			check(DynamicExtension.IsValid());
			DynamicExtension->Extension->OnPostInitialize();
		}
	}

	void ReinitializeExtensions()
	{
		for (const TInlineValue<FDynamicExtensionInfo>& DynamicExtension : DynamicExtensions)
		{
			check(DynamicExtension.IsValid());
			DynamicExtension->Extension->OnReinitialize();
		}
	}

public:

	struct Implements{};

private:

	struct FDynamicExtensionInfo
	{
		TSharedRef<IDynamicExtension> Extension;
		FDynamicExtensionInfo(TSharedRef<IDynamicExtension> InExtension) : Extension(InExtension) {}
		virtual ~FDynamicExtensionInfo() {}
		virtual const void* CastDynamic(FViewModelTypeID Type) const { return nullptr; }
	};

	template<typename T>
	struct TDynamicExtensionInfo : FDynamicExtensionInfo
	{
		TDynamicExtensionInfo(TSharedRef<T> InExtension) 
			: FDynamicExtensionInfo(StaticCastSharedRef<IDynamicExtension>(InExtension))
		{
		}
		virtual const void* CastDynamic(FViewModelTypeID Type) const
		{
			const void* Result = nullptr;
			TSharedRef<T> TypedExtension = StaticCastSharedRef<T>(Extension);
			FCastableBoilerplate::CastImplementation(&TypedExtension.Get(), Type, Result);
			return Result;
		}
	};

	TArray<TInlineValue<FDynamicExtensionInfo>> DynamicExtensions;
};

template<>
struct TCompositeCast<FDynamicExtensionContainer::Implements>
{
	static void Apply(const FDynamicExtensionContainer* This, FViewModelTypeID ToType, const void*& OutResult)
	{
		if (OutResult == nullptr)
		{
			if (const void* Result = FCastableBoilerplate::DirectCast(This, ToType))
			{
				OutResult = Result;
				return;
			}

			if (const void* Result = This->CastDynamic(ToType))
			{
				OutResult = Result;
			}
		}
	}

	template<typename T>
	static bool IsAny(const ICastable* This)
	{
		return T::ID == FDynamicExtensionContainer::ID;
	}
};

} // namespace Sequencer
} // namespace UE

