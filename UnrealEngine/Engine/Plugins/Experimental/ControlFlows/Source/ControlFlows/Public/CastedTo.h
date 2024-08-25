// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/SharedPointer.h"

/* A Pointer Container for a 'T' (UObject xOr TSharedFromThis) that can be statically casted to 'CastTo'
 * These classes are useful if you want your system to support both memory management systems: (1) UObject Garbage Collection and (2) Smart Pointer Reference counting
 */
template<typename CastTo>
class TPointerContainerBase : public TSharedFromThis<TPointerContainerBase<CastTo>>
{
public:
	virtual ~TPointerContainerBase() {}

	virtual CastTo* Cast() const = 0;
	virtual bool IsValid() const = 0;
	virtual void Reset() = 0;

public:
	CastTo* Get() const { return Cast(); }
	bool IsPtrValid() const { return IsValid(); }
	void ResetPtr() { Reset(); }
};

template<typename CastTo>
class TPointerContainer : public TPointerContainerBase<CastTo>
{
public:
	virtual ~TPointerContainer() {}

	virtual CastTo* Cast() const override { return nullptr; }
	virtual bool IsValid() const override { return false; }
	virtual void Reset() override {}
};

/* A "Weak" Pointer Reference Container for a 'T' (UObject xOr TSharedFromThis) that can be statically casted to 'CastTo' */
template<typename CastTo>
class TWeakContainerTo : public TPointerContainer<CastTo>
{
public:
	virtual ~TWeakContainerTo() {}
};

template<typename T, typename CastTo, bool /*bDerivedFromUObject*/, bool /*bDerivedFromTSharedFromThis*/>
class TWeakCastableImpl;

template<typename T, typename CastTo>
class TWeakCastableImpl<T, CastTo, true, false> : public TWeakContainerTo<CastTo>
{
public:
	TWeakCastableImpl() {}
	TWeakCastableImpl(T* InObject) : Object(InObject) {}
	virtual ~TWeakCastableImpl() {}

public:
	virtual CastTo* Cast() const override final { return static_cast<CastTo*>(Object.Get()); }
	virtual bool IsValid() const override final { return Object.IsValid(); }
	virtual void Reset() override final { Object.Reset(); }

private:
	TWeakObjectPtr<T> Object;
};

template<typename T, typename CastTo>
class TWeakCastableImpl<T, CastTo, false, true> : public TWeakContainerTo<CastTo>
{
public:
	TWeakCastableImpl() {}
	TWeakCastableImpl(T* InObject) : Object(StaticCastSharedRef<T>(InObject->AsShared())) {}
	virtual ~TWeakCastableImpl() {}

public:
	virtual CastTo* Cast() const override final { return static_cast<CastTo*>(IsValid() ? Object.Pin().Get() : nullptr); }
	virtual bool IsValid() const override final { return Object.IsValid(); }
	virtual void Reset() override final { Object.Reset(); }

private:
	TWeakPtr<T> Object;
};

#define TWeakCastableImpl_Decl TWeakCastableImpl<CastFrom, CastTo, TIsDerivedFrom<CastFrom, UObject>::IsDerived, IsDerivedFromSharedFromThis<CastFrom>()>
template<typename CastFrom, typename CastTo>
class TWeakCastable : public TWeakCastableImpl_Decl
{
public:
	TWeakCastable() {}
	TWeakCastable(CastFrom* InObject) : TWeakCastableImpl_Decl(InObject) {}
};
#undef TWeakCastableImpl_Decl

template<typename T>
class TWeakContainer : public TWeakCastable<T, T>
{
public:
	TWeakContainer() : TWeakCastable<T, T>() {}
	TWeakContainer(T* InObject) : TWeakCastable<T, T>(InObject) {}
};

/* A "Strong" Pointer Reference Container for a 'T' (UObject xOr TSharedFromThis) that can be statically casted to 'CastTo' */
template<typename CastTo>
class TStrongContainerTo : public TPointerContainer<CastTo>
{
public:
	virtual ~TStrongContainerTo() {}
};

template<typename T, typename CastTo, bool /*bDerivedFromUObject*/, bool /*bDerivedFromTSharedFromThis*/>
class TStrongContainerToImpl;

template<typename T, typename CastTo>
class TStrongContainerToImpl<T, CastTo, true, false> : public TStrongContainerTo<CastTo>
{
public:
	TStrongContainerToImpl() {}
	TStrongContainerToImpl(T* InObject) : Object(InObject) {}
	virtual ~TStrongContainerToImpl() {}

public:
	virtual CastTo* Cast() const override final { return static_cast<CastTo*>(Object.Get()); }
	virtual bool IsValid() const override final { return Object.IsValid(); }
	virtual void Reset() override final { Object.Reset(); }

private:
	TStrongObjectPtr<T> Object;
};

template<typename T, typename CastTo>
class TStrongContainerToImpl<T, CastTo, false, true> : public TStrongContainerTo<CastTo>
{
public:
	TStrongContainerToImpl() {}
	TStrongContainerToImpl(T* InObject) : Object(StaticCastSharedRef<T>(InObject->AsShared())) {}
	virtual ~TStrongContainerToImpl() {}

public:
	virtual CastTo* Cast() const override final { return static_cast<CastTo*>(Object.IsValid() ? Object.Get() : nullptr); }
	virtual bool IsValid() const override final { return Object.IsValid(); }
	virtual void Reset() override final { Object.Reset(); }

private:
	TSharedPtr<T> Object;
};

#define TStrongContainerToImpl_Decl TStrongContainerToImpl<CastFrom, CastTo, TIsDerivedFrom<CastFrom, UObject>::IsDerived, IsDerivedFromSharedFromThis<CastFrom>()>
template<typename CastFrom, typename CastTo>
class TStrongCastable : public TStrongContainerToImpl_Decl
{
public:
	TStrongCastable() : TStrongContainerToImpl_Decl() {}
	TStrongCastable(CastFrom* InObject) : TStrongContainerToImpl_Decl(InObject) {}
};
#undef TStrongContainerToImpl_Decl

template<typename T>
class TStrongContainer : public TStrongCastable<T, T>
{
public:
	TStrongContainer() : TStrongCastable<T, T>() {}
	TStrongContainer(T* InObject) : TStrongCastable<T, T>(InObject) {}
};