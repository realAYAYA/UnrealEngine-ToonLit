// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Misc/AssertionMacros.h"
#include "Misc/GeneratedTypeName.h"
#include "SequencerCoreFwd.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"

namespace UE
{
namespace Sequencer
{
struct FImplicitViewModelCast;
struct FImplicitWeakViewModelCast;
struct FImplicitWeakViewModelPin;


/**
 * Pointer type that wraps a shared view model pointer to provide implicit dynamic_cast style
 *    casting to any other view-model or extension. Implicit conversion is implemented through
 *    a proxy object so as to make the 'implicit' conversion somewhat explicit:
 *         FViewModelPtr ModelPtr = GetViewModel();
 *         TSharedPtr<IDraggableOutlinerExtension>   Draggable = ModelPtr.ImplicitCast();
 *         TSharedPtr<IDroppableExtension>   Droppable = ModelPtr.ImplicitCast();
 */
struct FViewModelPtr
{
	FViewModelPtr() = default;
	FViewModelPtr(const FViewModelPtr&) = default;
	FViewModelPtr& operator=(const FViewModelPtr&) = default;
	FViewModelPtr(FViewModelPtr&&) = default;
	FViewModelPtr& operator=(FViewModelPtr&&) = default;

	/** Construction from a nullptr */
	FViewModelPtr(nullptr_t)
		: Model(nullptr)
	{}
	/** Assignment from a nullptr */
	FViewModelPtr& operator=(nullptr_t)
	{
		Model = nullptr;
		return *this;
	}

	/** Construction from a shared ptr */
	template<typename ModelType>
	FViewModelPtr(const TSharedPtr<ModelType>& InModel)
		: Model(InModel)
	{}
	/** Assignment from a shared ptr */
	template<typename ModelType>
	FViewModelPtr& operator=(const TSharedPtr<ModelType>& InModel)
	{
		Model = InModel;
		return *this;
	}

	/** Construction from a shared ptr */
	template<typename ModelType>
	FViewModelPtr(const TSharedRef<ModelType>& InModel)
		: Model(InModel)
	{}
	/** Assignment from a shared ptr */
	template<typename ModelType>
	FViewModelPtr& operator=(const TSharedRef<ModelType>& InModel)
	{
		Model = InModel;
		return *this;
	}

	/** Construction from a raw ptr */
	FViewModelPtr(FViewModel* InModel)
		: Model(InModel->AsShared())
	{}
	/** Assignment from a raw ptr */
	FViewModelPtr& operator=(FViewModel* InModel)
	{
		Model = InModel->AsShared();
		return *this;
	}

public:

	/**
	 * Check whether this ptr is valid (non-null)
	 */
	explicit operator bool() const
	{
		return Model.IsValid();
	}

	/**
	 * Reset this view model pointer back to its default (null) state
	 */
	void Reset()
	{
		Model = nullptr;
	}

	/**
	 * Member access operator giving access to the wrapped view model
	 */
	FViewModel* operator->() const
	{
		checkf(Model, TEXT("Attempting to access a nullptr FViewModelPtr."));
		return Model.Get();
	}

	/**
	 * Member access operator giving access to the wrapped view model
	 */
	FViewModel* Get() const
	{
		return Model.Get();
	}

	/**
	 * Dereference operator giving access to the wrapped view model
	 */
	FViewModel& operator*() const
	{
		checkf(Model, TEXT("Attempting to access a nullptr FViewModelPtr."));
		return *Model.Get();
	}

	/**
	 * Implicit conversion to a regular shared pointer
	 */
	operator TSharedPtr<FViewModel>() const
	{
		return Model;
	}

	/**
	 * Explicitly access the view model as a regular shared pointer
	 */
	TSharedPtr<FViewModel> AsModel() const
	{
		return Model;
	}

	/**
	 * Return a proxy type that is able to implicitly convert this view
	 * model to any other TViewModel<IMyExtension> or TSharedPtr<IMyExtension>
	 */
	SEQUENCERCORE_API FImplicitViewModelCast ImplicitCast() const;

	/**
	 * Type hash function for use with the TSet and TMap family of containers
	 */
	friend uint32 GetTypeHash(const FViewModelPtr& In)
	{
		return GetTypeHash(In.Model);
	}

	/**
	 * Equality operator comparing 2 view model pointers
	 */
	friend bool operator==(const FViewModelPtr& A, const FViewModelPtr& B)
	{
		return A.Model == B.Model;
	}
	friend bool operator!=(const FViewModelPtr& A, const FViewModelPtr& B)
	{
		return A.Model != B.Model;
	}

protected:

	TSharedPtr<FViewModel> Model;
};


/**
 * Templated view model pointer that enables conversion and access to an interface implemented by a view model ptr.
 *
 * Since TViewModelPtr is a templated type, it can also be used as a self-documenting mechanism to specify
 *   that a parameter should implement a specific extension interface while only passing a single parameter, for example:
 *   void ExpandRecursively(const TViewModelPtr<IOutlinerExtension>& OutlinerItem);
 *
 * Validity is transitive meaning that an invalid TViewModelPtr<A> is equivalent to a nullptr, and will return an
 * invalid TViewModelPtr<B> even if the originating model implements B but not A.
 */
template<typename ExtensionType>
struct TViewModelPtr : FViewModelPtr
{
	TViewModelPtr() = default;
	TViewModelPtr(const TViewModelPtr<ExtensionType>&) = default;
	TViewModelPtr<ExtensionType>& operator=(const TViewModelPtr<ExtensionType>&) = default;
	TViewModelPtr(TViewModelPtr<ExtensionType>&&) = default;
	TViewModelPtr<ExtensionType>& operator=(TViewModelPtr<ExtensionType>&&) = default;

	/** Construction from a regular view model ptr */
	TViewModelPtr(const FViewModelPtr& InModelPtr)
		: Extension(nullptr)
	{
		Assign(InModelPtr);
	}
	/** Assignment from a regular view model ptr */
	TViewModelPtr& operator=(const FViewModelPtr& InModelPtr)
	{
		Assign(InModelPtr);
		return *this;
	}

	/** Construction from a regular view model ptr */
	TViewModelPtr(const TSharedPtr<FViewModel>& InModel)
		: Extension(nullptr)
	{
		Assign(InModel);
	}
	/** Assignment from a regular view model ptr */
	TViewModelPtr& operator=(const TSharedPtr<FViewModel>& InModel)
	{
		Assign(InModel);
		return *this;
	}

	/** Construction from a regular view model ref */
	TViewModelPtr(const TSharedRef<FViewModel>& InModel)
		: Extension(nullptr)
	{
		Assign(InModel);
	}
	/** Assignment from a regular view model ref */
	TViewModelPtr& operator=(const TSharedRef<FViewModel>& InModel)
	{
		Assign(InModel);
		return *this;
	}

	/** Construction from a regular view model ptr */
	template<typename SharedType>
	TViewModelPtr(const TSharedPtr<SharedType>& InModel)
	{
		Assign(InModel, InModel.Get());
	}
	/** Assignment from a regular view model ptr */
	template<typename SharedType>
	TViewModelPtr& operator=(const TSharedPtr<SharedType>& InModel)
	{
		Assign(InModel, InModel.Get());
		return *this;
	}

	/** Construction from a regular view model ref */
	template<typename SharedType>
	TViewModelPtr(const TSharedRef<SharedType>& InModel)
	{
		Assign(InModel, &InModel.Get());
	}
	/** Assignment from a regular view model ptr */
	template<typename SharedType>
	TViewModelPtr& operator=(const TSharedRef<SharedType>& InModel)
	{
		Assign(InModel, &InModel.Get());
		return *this;
	}

	/** Construction from a raw ptr */
	TViewModelPtr(FViewModel* InModel)
	{
		if (InModel)
		{
			Assign(InModel->AsShared());
		}
	}
	/** Assignment from a raw ptr */
	TViewModelPtr<ExtensionType>& operator=(FViewModel* InModel)
	{
		if (InModel)
		{
			Assign(InModel->AsShared());
		}
		else
		{
			Reset();
		}
		return *this;
	}

	/**
	 * Implicit construction from another view model ptr of a related type (ie, TViewModelPtr<Base>(TViewModelPtr<Derived>) )
	 * We leave the compiler to report errors on the conversion of In.Extension -> Extension for unrelated types
	 */
	template<typename OtherExtensionType>
	TViewModelPtr(const TViewModelPtr<OtherExtensionType>& In)
		: FViewModelPtr(In.Model)
		, Extension(In.Extension)
	{}
	/**
	 * Implicit assignment from another view model ptr of a related type (ie, TViewModelPtr<Base>(TViewModelPtr<Derived>) )
	 * We leave the compiler to report errors on the conversion of In.Extension -> Extension for unrelated types
	 */
	template<typename OtherExtensionType>
	TViewModelPtr<ExtensionType>& operator=(const TViewModelPtr<OtherExtensionType>& In)
	{
		Model = In.Model;
		Extension = In.Extension;
		return *this;
	}

	/**
	 * Construction from nullptr
	 */
	TViewModelPtr(nullptr_t)
		: FViewModelPtr(nullptr)
		, Extension(nullptr)
	{}
	/**
	 * Assignment from nullptr
	 */
	TViewModelPtr<ExtensionType>& operator=(nullptr_t)
	{
		Model = nullptr;
		Extension = nullptr;
		return *this;
	}

	/**
	 * Conversion to a normal shared pointer of this extension type
	 */
	operator TSharedPtr<ExtensionType>() const
	{
		return TSharedPtr<ExtensionType>(Model, Extension);
	}

	/**
	 * Conversion to a normal weak pointer of this extension type
	 */
	operator TWeakPtr<ExtensionType>() const
	{
		return TSharedPtr<ExtensionType>(Model, Extension);
	}

public:

	/**
	 * Check whether this ptr is valid (non-null)
	 * @note: While a TViewModelPtr may contain a valid model ptr, it is only
	 *        considered valid as long as the model implements the templated extension or model type
	 */
	explicit operator bool() const
	{
		return Extension != nullptr;
	}

	/**
	 * Pointer access to the extension pointer. This Ptr must be checked for validity before calling.
	 */
	ExtensionType* operator->() const
	{
		checkf(Extension, TEXT("Attempting to access a nullptr TViewModelPtr<%s>."), GetGeneratedTypeName<ExtensionType>());
		return Extension;
	}

	/**
	 * Dereference operator giving access to the wrapped extension type
	 */
	ExtensionType& operator*() const
	{
		checkf(Extension, TEXT("Attempting to access a nullptr FViewModelPtr."));
		return *Extension;
	}

	/**
	 * Conversion to a normal shared pointer of this extension type
	 */
	ExtensionType* Get() const
	{
		return Extension;
	}

	/**
	 * Reset this ptr back to its default (null) state
	 */
	void Reset()
	{
		Model = nullptr;
		Extension = nullptr;
	}

	friend bool operator==(const TViewModelPtr<ExtensionType>& A, const TViewModelPtr<ExtensionType>& B)
	{
		return A.Model == B.Model && A.Extension == B.Extension;
	}

private:

	void Assign(const TSharedPtr<FViewModel>& InModel, ExtensionType* InExtension = nullptr)
	{
		if (InExtension)
		{
			Extension = InExtension;
			Model = InModel;
			return;
		}

		if (InModel)
		{
			if (ExtensionType* NewExtension = InModel->CastThis<ExtensionType>())
			{
				Extension = NewExtension;
				Model = InModel;
				return;
			}
		}

		Extension = nullptr;
		Model = nullptr;
	}

	template<typename> friend struct TViewModelPtr;

	ExtensionType* Extension = nullptr;
};

/**
 * Weak pointer utility for view models that also provides the same explicit conversion utility as F/TViewModelPtr
 * Using this pointer type allows for expressive null handling and casting in a simple function call for any
 * extension type which is useful for many widgets:
 *
 *    class SMyWidget
 *    {
 *        void OnMouseMove()
 *        {
 *            TViewModelPtr<IDraggable> Draggable = WeakViewModel.ImplicitPin();
 *            if (Draggable)
 *            {
 *                Draggable->BeginDrag();
 *            }
 *        }
 *        FWeakViewModelPtr WeakViewModel;
 *    };
 */
struct FWeakViewModelPtr
{
	FWeakViewModelPtr() = default;

	/**
	 * Construction from a shared ref to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	FWeakViewModelPtr(const TSharedRef<SharedType>& InModel)
		: WeakModel(InModel)
	{}

	/**
	 * Construction from a shared ptr to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	FWeakViewModelPtr(const TSharedPtr<SharedType>& InModel)
		: WeakModel(InModel)
	{}

	/**
	 * Construction from a weak ptr to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	FWeakViewModelPtr(const TWeakPtr<SharedType>& InWeakModel)
		: WeakModel(InWeakModel)
	{}

	/**
	 * Construction from a view model ptr
	 */
	FWeakViewModelPtr(const FViewModelPtr& InViewModelPtr)
		: WeakModel(InViewModelPtr.AsModel())
	{}

	/**
	 * Assignment from a view model ptr
	 */
	FWeakViewModelPtr& operator=(const FViewModelPtr& InViewModelPtr)
	{
		WeakModel = InViewModelPtr.AsModel();
		return *this;
	}

	/**
	 * Pin this weak ptr. The result is guaranteed to be kept alive as long as the variable's scope.
	 */
	FViewModelPtr Pin() const
	{
		TSharedPtr<FViewModel> Pinned = WeakModel.Pin();
		return FViewModelPtr(Pinned);
	}

	/**
	 * Returns whether the weak ptr is valid.
	 */
	bool IsValid() const
	{
		return WeakModel.IsValid();
	}

	/**
	 * Conversion to a normal weak pointer
	 */
	operator TWeakPtr<FViewModel>() const
	{
		return WeakModel;
	}

	/**
	 * Attempt to convert this weak view model ptr to another type implicitly
	 */
	SEQUENCERCORE_API FImplicitWeakViewModelCast ImplicitCast() const;

	/**
	 * Returns a proxy object that will simultaneously Pin() and Cast() this view model
	 * to a particular view-model or extension type
	 */
	SEQUENCERCORE_API FImplicitWeakViewModelPin ImplicitPin() const;

	TWeakPtr<FViewModel> AsModel() const
	{
		return WeakModel;
	}

	friend uint32 GetTypeHash(const FWeakViewModelPtr& In)
	{
		return GetTypeHash(In.WeakModel);
	}

	friend bool operator==(const FWeakViewModelPtr& A, const FWeakViewModelPtr& B)
	{
		return A.WeakModel == B.WeakModel;
	}

	friend bool operator!=(const FWeakViewModelPtr& A, const FWeakViewModelPtr& B)
	{
		return A.WeakModel != B.WeakModel;
	}
protected:

	TWeakPtr<FViewModel> WeakModel;
};

/**
 * Templated version of FWeakViewModelPtr that provides an additional self-documenting extension that should
 * exist on the view model.
 */
template<typename ExtensionType>
struct TWeakViewModelPtr : FWeakViewModelPtr
{
	TWeakViewModelPtr() = default;
	TWeakViewModelPtr(const TWeakViewModelPtr<ExtensionType>&) = default;
	TWeakViewModelPtr<ExtensionType>& operator=(const TWeakViewModelPtr<ExtensionType>&) = default;
	TWeakViewModelPtr(TWeakViewModelPtr<ExtensionType>&&) = default;
	TWeakViewModelPtr<ExtensionType>& operator=(TWeakViewModelPtr<ExtensionType>&&) = default;

	/**
	 * Construction from a shared ref to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	TWeakViewModelPtr(const TSharedRef<SharedType>& InModel)
		: FWeakViewModelPtr(InModel)
	{}

	/**
	 * Construction from a shared ptr to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	TWeakViewModelPtr(const TSharedPtr<SharedType>& InModel)
		: FWeakViewModelPtr(InModel)
	{}

	/**
	 * Construction from a weak ptr to a view model
	 * This is templated to ensure that the overloaded constructors remain unambiguous
	 */
	template<typename SharedType>
	TWeakViewModelPtr(const TWeakPtr<SharedType>& InWeakModel)
		: FWeakViewModelPtr(InWeakModel)
	{}

	/** Construction from a FWeakViewModelPtr */
	TWeakViewModelPtr(const FWeakViewModelPtr& InModel)
		: FWeakViewModelPtr(InModel)
	{}
	/** Assignment from a FWeakViewModelPtr */
	TWeakViewModelPtr<ExtensionType>& operator=(const FWeakViewModelPtr& InModel)
	{
		WeakModel = InModel.AsModel();
		return *this;
	}

	/** Construction from a TViewModelPtr */
	TWeakViewModelPtr(const TViewModelPtr<ExtensionType>& InModel)
		: FWeakViewModelPtr(InModel.AsModel())
	{}
	/** Assignment from a TViewModelPtr */
	TWeakViewModelPtr<ExtensionType>& operator=(const TViewModelPtr<ExtensionType>& InModel)
	{
		WeakModel = InModel.AsModel();
		return *this;
	}

	/** Pin this weak view model ptr */
	TViewModelPtr<ExtensionType> Pin() const
	{
		TSharedPtr<FViewModel> Pinned = WeakModel.Pin();
		return TViewModelPtr<ExtensionType>(Pinned);
	}

	/**
	 * Conversion to a normal weak pointer
	 */
	operator TWeakPtr<ExtensionType>() const
	{
		TViewModelPtr<ExtensionType> Pinned = Pin();
		return TSharedPtr<ExtensionType>(Pinned);
	}
};


/**
 * Proxy type that enables implicit dynamic casting of a view-model to any other view-model
 * or extension type. This is implemented as a proxy type rather than the conversion
 * operators existing on FViewModelPtr so that the implicit cast must be user-directed, while
 * still removing additional template boilerplate at the call-site.
 *
 * @see FViewModelPtr::ImplicitCast
 */
struct FImplicitViewModelCast
{
	/** Construction from a regular shared ptr to a view model */
	explicit FImplicitViewModelCast(const TSharedPtr<FViewModel>& In)
		: ViewModel(In)
	{}

	/**
	 * Implicitly convert this view model to any other view-model or extension type
	 * @return A regular shared pointer to the view-model or extension if it is implemented on the existing model; nullptr otherwise.
	 */
	template<typename T>
	operator TSharedPtr<T>() const
	{
		return ViewModel ? ViewModel->CastThisShared<T>() : nullptr;
	}

	/**
	 * Implicitly convert this view model to any other view-model or extension type
	 * @return A TViewModelPtr to the view-model or extension if it is implemented on the existing model; nullptr otherwise.
	 */
	template<typename ExtensionType>
	operator TViewModelPtr<ExtensionType>() const
	{
		return TViewModelPtr<ExtensionType>(ViewModel);
	}

private:

	TSharedPtr<FViewModel> ViewModel;
};



/**
 * Proxy type that enables implicit dynamic casting of a weak view-model to any other view-model
 * or extension type. This is implemented as a proxy type rather than the conversion
 * operators existing on FWeakViewModelPtr so that the implicit cast must be user-directed, while
 * still removing additional template boilerplate at the call-site.
 *
 * @see FWeakViewModelPtr::ImplicitCast
 * @see FWeakViewModelPtr::ImplicitPin
 */
struct FImplicitWeakViewModelCast
{
	/** Construction from a regular weak ptr to a view model */
	explicit FImplicitWeakViewModelCast(const TWeakPtr<FViewModel>& In)
		: WeakModel(In)
	{}

	/**
	 * Implicit conversion to any TWeakViewModelPtr type
	 */
	template<typename ExtensionType>
	operator TWeakViewModelPtr<ExtensionType>() const
	{
		return TWeakViewModelPtr<ExtensionType>(WeakModel);
	}

private:
	TWeakPtr<FViewModel> WeakModel;
};


/**
 * Proxy type that combines a Pin() and Cast() of a weak view model ptr, gracefully handling null.
 *
 * @see FWeakViewModelPtr::ImplicitPin
 */
struct FImplicitWeakViewModelPin
{
	/** Construction from a regular weak ptr to a view model */
	explicit FImplicitWeakViewModelPin(const TWeakPtr<FViewModel>& In)
		: WeakModel(In)
	{}

	/**
	 * Simultaneously Pin() and Cast() this view model to the templated type
	 * @return A shared pointer to the view-model or extension type if it is implemented on this model, nullptr otherwise.
	 */
	template<typename T>
	operator TSharedPtr<T>() const
	{
		TSharedPtr<FViewModel> Pinned = WeakModel.Pin();
		return Pinned ? Pinned->CastThisShared<T>() : nullptr;
	}

	/**
	 * Simultaneously Pin() and Cast() this view model to the templated type
	 * @return A TViewModelPtr to the view-model or extension type if it is implemented on this model, nullptr otherwise.
	 */
	template<typename ExtensionType>
	operator TViewModelPtr<ExtensionType>() const
	{
		TSharedPtr<FViewModel> Pinned = WeakModel.Pin();
		if (Pinned)
		{
			return TViewModelPtr<ExtensionType>(Pinned);
		}
		return nullptr;
	}

private:

	TWeakPtr<FViewModel> WeakModel;
};


} // namespace Sequencer
} // namespace UE

