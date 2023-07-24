// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataSet.h"
#include "NiagaraDataChannelCommon.generated.h"

class UWorld;
class UActorComponentc;
class FNiagaraWorldManager;
class FNiagaraDataBuffer;
class FNiagaraSystemInstance;
class UNiagaraDataChannel;
class UNiagaraDataChannelHandler;
class UNiagaraDataChannelDefinitions;
class UNiagaraDataInterfaceDataChannelWrite;
class UNiagaraDataInterfaceDataChannelRead;
class UNiagaraDataChannelWriter;
class UNiagaraDataChannelReader;
struct FNiagaraDataChannelGameData;

typedef TSharedPtr<FNiagaraDataChannelGameData> FNiagaraDataChannelGameDataPtr;

/** Wrapper struct for FNames referencing Niagara Data Channels allowing a type customization. */
USTRUCT(BlueprintType)
struct FNiagaraDataChannelReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Channel")
	FName ChannelName = NAME_None;
};

/** A request to publish data into a Niagara Data Channel.  */
struct FNiagaraDataChannelPublishRequest
{
	/** The buffer containing the data to be published. This can come from a data channel DI or can be the direct contents on a Niagara simulation. */
	FNiagaraDataBuffer* Data = nullptr;

	/** Game level data if this request comes from the game code. */
	FNiagaraDataChannelGameDataPtr GameData;

	/** If true, data in this request will be made visible to BP and C++ game code.*/
	bool bVisibleToGame = false;

	/** If true, data in this request will be made visible to Niagara CPU simulations. */
	bool bVisibleToCPUSims = false;

	/** If true, data in this request will be made visible to Niagara GPU simulations. */
	bool bVisibleToGPUSims = false;

	/** 
	LWC Tile for the originator system of this request.
	Allows us to convert from the Niagara Simulation space into LWC coordinates.
	*/
	FVector3f LwcTile = FVector3f::ZeroVector;

	FNiagaraDataChannelPublishRequest() = default;
	explicit FNiagaraDataChannelPublishRequest(FNiagaraDataBuffer* InData)
		: Data(InData)
	{
		Data->AddReadRef();
	}

	explicit FNiagaraDataChannelPublishRequest(FNiagaraDataBuffer* InData, bool bInVisibleToGame, bool bInVisibleToCPUSims, bool bInVisibleToGPUSims, FVector3f InLwcTile)
	: Data(InData), bVisibleToGame(bInVisibleToGame), bVisibleToCPUSims(bInVisibleToCPUSims), bVisibleToGPUSims(bInVisibleToGPUSims), LwcTile(InLwcTile)
	{
		Data->AddReadRef();
	}

	explicit FNiagaraDataChannelPublishRequest(const FNiagaraDataChannelPublishRequest& Other)
	{
		*this = Other;
	}

	FNiagaraDataChannelPublishRequest& operator=(const FNiagaraDataChannelPublishRequest& Other)
	{
		Data = Other.Data;
		if(Data)
		{
			Data->AddReadRef();
		}
		GameData = Other.GameData;
		bVisibleToGame = Other.bVisibleToGame;
		bVisibleToCPUSims = Other.bVisibleToCPUSims;
		bVisibleToGPUSims = Other.bVisibleToGPUSims;
		LwcTile = Other.LwcTile;
		return *this;
	}

	~FNiagaraDataChannelPublishRequest()
	{
		if(Data)
		{
			Data->ReleaseReadRef();
		}
	}
};
