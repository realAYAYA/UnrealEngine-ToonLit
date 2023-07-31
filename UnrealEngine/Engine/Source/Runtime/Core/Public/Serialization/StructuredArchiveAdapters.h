// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Concepts/Insertable.h"
#include "Templates/Models.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Templates/UniqueObj.h"

class CORE_API FStructuredArchiveFromArchive
{
	UE_NONCOPYABLE(FStructuredArchiveFromArchive)

	static constexpr uint32 ImplSize = 400;
	static constexpr uint32 ImplAlignment = 8;

	struct FImpl;

public:
	explicit FStructuredArchiveFromArchive(FArchive& Ar);
	~FStructuredArchiveFromArchive();

	FStructuredArchiveSlot GetSlot();

private:
	// Implmented as a pimpl in order to reduce dependencies, but an inline one to avoid heap allocations
	alignas(ImplAlignment) uint8 ImplStorage[ImplSize];
};

#if WITH_TEXT_ARCHIVE_SUPPORT

class CORE_API FArchiveFromStructuredArchiveImpl : public FArchiveProxy
{
	UE_NONCOPYABLE(FArchiveFromStructuredArchiveImpl)

		struct FImpl;

public:
	explicit FArchiveFromStructuredArchiveImpl(FStructuredArchiveSlot Slot);
	virtual ~FArchiveFromStructuredArchiveImpl();

	virtual void Flush() override;
	virtual bool Close() override;

	virtual int64 Tell() override;
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
	virtual bool AtEnd() override;

	using FArchive::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(class FName& Value) override;
	virtual FArchive& operator<<(class UObject*& Value) override;
	virtual FArchive& operator<<(class FText& Value) override;
	//~ End FArchive Interface

	virtual void Serialize(void* V, int64 Length) override;

	virtual FArchive* GetCacheableArchive() override;

	bool ContainsData() const;

protected:
	virtual bool Finalize(FStructuredArchiveRecord Record);
	void OpenArchive();

private:
	void Commit();

	// Implmented as a pimpl in order to reduce dependencies
	TUniqueObj<FImpl> Pimpl;
};

class FArchiveFromStructuredArchive
{
public:
	explicit FArchiveFromStructuredArchive(FStructuredArchiveSlot InSlot)
		: Impl(InSlot)
	{
	}

	FArchive& GetArchive() { return Impl; }
	const FArchive& GetArchive() const { return Impl; }

	void Close() { Impl.Close(); }

private:
	FArchiveFromStructuredArchiveImpl Impl;
};

#else

class FArchiveFromStructuredArchive
{
public:
	explicit FArchiveFromStructuredArchive(FStructuredArchiveSlot InSlot)
		: Ar(InSlot.GetUnderlyingArchive())
	{
	}

	FArchive& GetArchive() { return Ar; }
	const FArchive& GetArchive() const { return Ar; }

	void Close() {}

private:
	FArchive& Ar;
};

#endif

/**
 * Adapter operator which allows a type to stream to an FArchive when it already supports streaming to an FStructuredArchiveSlot.
 *
 * @param  Ar   The archive to read from or write to.
 * @param  Obj  The object to read or write.
 *
 * @return  A reference to the same archive as Ar.
 */
template <typename T>
typename TEnableIf<
	!TModels<CInsertable<FArchive&>, T>::Value&& TModels<CInsertable<FStructuredArchiveSlot>, T>::Value,
	FArchive&
>::Type operator<<(FArchive& Ar, T& Obj)
{
	FStructuredArchiveFromArchive ArAdapt(Ar);
	ArAdapt.GetSlot() << Obj;
	return Ar;
}

/**
 * Adapter operator which allows a type to stream to an FStructuredArchiveSlot when it already supports streaming to an FArchive.
 *
 * @param  Slot  The slot to read from or write to.
 * @param  Obj   The object to read or write.
 */
template <typename T>
typename TEnableIf<
	TModels<CInsertable<FArchive&>, T>::Value &&
	!TModels<CInsertable<FStructuredArchiveSlot>, T>::Value
>::Type operator<<(FStructuredArchiveSlot Slot, T& Obj)
{
#if WITH_TEXT_ARCHIVE_SUPPORT
	FArchiveFromStructuredArchive Adapter(Slot);
	FArchive& Ar = Adapter.GetArchive();
#else
	FArchive& Ar = Slot.GetUnderlyingArchive();
#endif
	Ar << Obj;
#if WITH_TEXT_ARCHIVE_SUPPORT
	Adapter.Close();
#endif
}

