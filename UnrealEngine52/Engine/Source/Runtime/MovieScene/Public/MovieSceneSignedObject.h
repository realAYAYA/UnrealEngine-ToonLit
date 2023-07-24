// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EventHandlers/ISignedObjectEventHandler.h"
#include "EventHandlers/MovieSceneDataEventContainer.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneSignedObject.generated.h"

class ITransactionObjectAnnotation;
class UMovieSceneSignedObject;

namespace UE
{
namespace MovieScene
{
class ISignedObjectEventHandler;

struct IDeferredSignedObjectFlushSignal
{
	virtual ~IDeferredSignedObjectFlushSignal(){}

	virtual void OnDeferredModifyFlush() = 0;
};

/**
 * Application-wide utility interface that allows for deferral of UMovieSceneSignedObject::MarkAsChanged calls
 */
struct IDeferredSignedObjectChangeHandler
{
	virtual ~IDeferredSignedObjectChangeHandler(){}

	virtual void Flush() = 0;
	virtual void DeferMarkAsChanged(UMovieSceneSignedObject* SignedObject) = 0;
	virtual bool CreateImplicitScopedModifyDefer() = 0;
	virtual void ResetImplicitScopedModifyDefer() = 0;
};

struct MOVIESCENE_API FScopedSignedObjectModifyDefer
{
	FScopedSignedObjectModifyDefer(bool bInForceFlush = false);
	~FScopedSignedObjectModifyDefer();

private:
	bool bForceFlush;
};


} // namespace MovieScene
} // namespace UE



UCLASS()
class MOVIESCENE_API UMovieSceneSignedObject : public UObject
{
public:
	GENERATED_BODY()

	UMovieSceneSignedObject(const FObjectInitializer& Init);

	static void SetDeferredHandler(TUniquePtr<UE::MovieScene::IDeferredSignedObjectChangeHandler>&& InHandler);
	static void AddFlushSignal(TWeakPtr<UE::MovieScene::IDeferredSignedObjectFlushSignal> Signal);
	static void ResetImplicitScopedModifyDefer();

	/**
	 * Mark this object as having been changed in any way. Will regenerate this object's Signature
	 * and schedule an event to be broadcast to notify subscribers of the change
	 */
	void MarkAsChanged();

	/**
	 * Retrieve this object's signature that uniquely identifies its current state.
	 * Any change to this object will result in a new signature. This is a GUID and
	 * not a hash - equivalent class state does not generate identical signatures.
	 */
	const FGuid& GetSignature() const
	{
		return Signature;
	}

	/**
	 * Immediately broadcast events for this object being changed
	 */
	void BroadcastChanged();

	/** Event that is triggered whenever this object's signature has changed */
	DECLARE_EVENT(UMovieSceneSignedObject, FOnSignatureChanged)
	FOnSignatureChanged& OnSignatureChanged() { return OnSignatureChangedEvent; }

	/** Event that is triggered whenever this object's signature has changed */
	UE::MovieScene::TDataEventContainer<UE::MovieScene::ISignedObjectEventHandler> EventHandlers;

public:

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
#endif

private:

	/** Unique generation signature */
	UPROPERTY()
	FGuid Signature;

	/** Event that is triggered whenever this object's signature has changed */
	FOnSignatureChanged OnSignatureChangedEvent;
};
