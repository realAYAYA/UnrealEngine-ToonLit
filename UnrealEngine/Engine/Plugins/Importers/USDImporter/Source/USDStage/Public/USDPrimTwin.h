// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/SoftObjectPtr.h"

#include "USDPrimTwin.generated.h"

/** The Unreal equivalent (twin) of a USD prim */
UCLASS( Transient, MinimalAPI )
class UUsdPrimTwin final : public UObject
{
	GENERATED_BODY()

public:
	UUsdPrimTwin& AddChild( const FString& InPrimPath );
	void RemoveChild( const TCHAR* InPrimPath );
	const TMap<FString, TObjectPtr<UUsdPrimTwin>>& GetChildren() const { return Children; }

	UUsdPrimTwin* GetParent() const { return Parent.Get(); }

	void Clear();

	void Iterate( TFunction< void( UUsdPrimTwin& ) > Func, bool bRecursive )
	{
		for ( TMap<FString, TObjectPtr<UUsdPrimTwin>>::TIterator It = Children.CreateIterator(); It; ++It )
		{
			UUsdPrimTwin* Child = It->Value;

			Func( *Child );

			if ( bRecursive )
			{
				Child->Iterate( Func, bRecursive );
			}
		}
	}

	UUsdPrimTwin* Find( const FString& InPrimPath );
	UUsdPrimTwin* Find( const USceneComponent* InSceneComponent );

	USceneComponent* GetSceneComponent() const;

	DECLARE_EVENT_OneParam( UUsdPrimTwin, FOnUsdPrimTwinDestroyed, const UUsdPrimTwin& );
	FOnUsdPrimTwinDestroyed OnDestroyed;

public:
	const static FName GetChildrenPropertyName() { return GET_MEMBER_NAME_CHECKED( UUsdPrimTwin, Children ); }

public:
	UPROPERTY()
	FString PrimPath;

	UPROPERTY()
	TSoftObjectPtr< class AActor > SpawnedActor;

	UPROPERTY()
	TWeakObjectPtr< class USceneComponent > SceneComponent;

private:
	UPROPERTY( Transient )
	TMap< FString, TObjectPtr<UUsdPrimTwin> > Children;

	UPROPERTY( Transient )
	TWeakObjectPtr< UUsdPrimTwin > Parent;
};
