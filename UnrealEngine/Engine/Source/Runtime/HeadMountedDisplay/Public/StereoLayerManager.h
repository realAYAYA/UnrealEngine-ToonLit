// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IStereoLayers.h"
#include "IXRLoadingScreen.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "StereoLayerShapes.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/TypeHash.h"

/**
	Partial implementation of the Layer management code for the IStereoLayers interface.
	Implements adding, deleting and updating layers regardless of how they are rendered.

	A class that wishes to implement the IStereoLayer interface can extend this class instead.
	The template argument should be a type for storing layer data. It should have a constructor matching the following:
		LayerType(const FLayerDesc& InLayerDesc);
	... and implement the following function overloads:
		void SetLayerId(uint32 InId),
		uint32 GetLayerId() const,
		bool GetLayerDescMember(LayerType& Layer, FLayerDesc& OutLayerDesc),
		void SetLayerDescMember(LayerType& Layer, const FLayerDesc& Desc), and
		void MarkLayerTextureForUpdate(LayerType& Layer)
	
	To perform additional bookkeeping each time individual layers are changed, you can override the following protected method:
		UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid)
	It is called whenever CreateLayer, DestroyLayer, SetLayerDesc and MarkTextureForUpdate are called.

	Simple implementations that do not to track additional data per layer may use FLayerDesc directly.
	The FSimpleLayerManager subclass can be used in that case and it implements all the required glue functions listed above.

	To access the layer data from your subclass, you have the following protected interface:
		bool GetStereoLayersDirty() -- Returns true if layer data have changed since the status was last cleared
		ForEachLayer(...) -- pass in a lambda to iterate through each existing layer.
		CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true) -- Copies the layers into OutArray.
		CopySortedLayers(TArray<LayerType>& OutArray, bool bMarkClean = true) -- Copies the layers into OutArray sorted by their priority.
		WithLayer(uint32 LayerId, TFunction<void(LayerType*)> Func) -- Finds the layer by Id and passes it to Func or nullptr if not found.
	The last two methods will clear the layer dirty flag unless you pass in false as the optional final argument.
	
	Thread safety:
	Updates and the two protected access methods use a critical section to ensure atomic access to the layer structures.
	Therefore, it is probably better to copy layers before performing time consuming operations using CopyLayers and reserve
	ForEachLayer for simple processing or operations where you need to know the user-facing layer id. The WithLayer method
	is useful if you already know the id of a layer you need to access in a thread safe manner.

*/

template<typename LayerType>
class TStereoLayerManager : public IStereoLayers
{
private:
	mutable FCriticalSection LayerCritSect;
	bool bStereoLayersDirty;

	struct FLayerComparePriority
	{
		bool operator()(const LayerType& A, const LayerType& B) const
		{
			FLayerDesc DescA, DescB;
			if (GetLayerDescMember(A, DescA) && GetLayerDescMember(B, DescB))
			{
				if (DescA.Priority < DescB.Priority)
				{
					return true;
				}
				if (DescA.Priority > DescB.Priority)
				{
					return false;
				}
				return DescA.Id < DescB.Id;
			}
			return false;
		}
	};

	struct FLayerData {
		TMap<uint32, LayerType> Layers;
		uint32 NextLayerId;
		bool bShowBackground;

		FLayerData(uint32 InNext, bool bInShowBackground = true) : Layers(), NextLayerId(InNext), bShowBackground(bInShowBackground) {}
		FLayerData(const FLayerData& In) : Layers(In.Layers), NextLayerId(In.NextLayerId), bShowBackground(In.bShowBackground) {}
	};
	TArray<FLayerData> LayerStack;

	FLayerData& LayerState(int Level=0) { return LayerStack.Last(Level); }
	//const FLayerData& LayerState(int Level = 0) const { return LayerStack.Last(Level); }
	TMap<uint32, LayerType>& StereoLayers(int Level = 0) { return LayerState(Level).Layers; }
	uint32 MakeLayerId() { return LayerState().NextLayerId++; }
	
	LayerType* FindLayerById(uint32 LayerId, int32& OutLevel)
	{
		if (LayerId == FLayerDesc::INVALID_LAYER_ID || LayerId >= LayerState().NextLayerId)
		{
			return nullptr;
		}

		// If the layer id does not exist in the current state, we'll search up the stack for the last version of the layer.
		for (int32 I = 0; I < LayerStack.Num(); I++)
		{
			LayerType* Found = StereoLayers(I).Find(LayerId);
			if (Found)
			{
				OutLevel = I;
				return Found;
			}
		}
		return nullptr;
	}

protected:

	virtual void UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid)
	{}

	bool GetStereoLayersDirty()
	{
		return bStereoLayersDirty;
	}


	void ForEachLayer(TFunction<void(uint32, LayerType&)> Func, bool bMarkClean = true)
	{
		FScopeLock LockLayers(&LayerCritSect);
		for (auto& Pair : StereoLayers())
		{
			Func(Pair.Key, Pair.Value);
		}

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

	void CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true)
	{
		FScopeLock LockLayers(&LayerCritSect);
		StereoLayers().GenerateValueArray(OutArray);

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

	void CopySortedLayers(TArray<LayerType>& OutArray, bool bMarkClean = true)
	{
		CopyLayers(OutArray, bMarkClean);
		OutArray.Sort(FLayerComparePriority());
	}

	void WithLayer(uint32 LayerId, TFunction<void(LayerType*)> Func)
	{
		FScopeLock LockLayers(&LayerCritSect);
		int32 FoundLevel;
		Func(FindLayerById(LayerId, FoundLevel));
	}

public:

	TStereoLayerManager()
		: bStereoLayersDirty(false)
		, LayerStack{ 1 }
	{
	}

	virtual ~TStereoLayerManager()
	{}

	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) override
	{
		FScopeLock LockLayers(&LayerCritSect);

		uint32 LayerId = MakeLayerId();
		check(LayerId != FLayerDesc::INVALID_LAYER_ID);
		LayerType& NewLayer = StereoLayers().Emplace(LayerId, InLayerDesc);
		NewLayer.SetLayerId(LayerId);
		UpdateLayer(NewLayer, LayerId, InLayerDesc.IsVisible());
		bStereoLayersDirty = true;
		return LayerId;
	}

	virtual void DestroyLayer(uint32 LayerId) override
	{
		FScopeLock LockLayers(&LayerCritSect);
		if (LayerId == FLayerDesc::INVALID_LAYER_ID || LayerId >= LayerState().NextLayerId)
		{
			return;
		}

		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);

		// Destroy layer will delete the last active copy of the layer even if it's currently not active
		if (Found)
		{
			if (FoundLevel == 0)
			{
				UpdateLayer(*Found, LayerId, false);
				bStereoLayersDirty = true;
			}

			StereoLayers(FoundLevel).Remove(LayerId);
		}
	}

	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) override
	{
		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return;
		}
		FScopeLock LockLayers(&LayerCritSect);

		// SetLayerDesc layer will update the last active copy of the layer.
		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			SetLayerDescMember(*Found, InLayerDesc);
			Found->SetLayerId(LayerId);

			// If the layer is currently active, update layer state
			if (FoundLevel == 0)
			{
				UpdateLayer(*Found, LayerId, InLayerDesc.IsVisible());
				bStereoLayersDirty = true;
			}
		}
	}

	virtual bool GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc) override
	{
		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return false;
		}
		FScopeLock LockLayers(&LayerCritSect);

		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			return GetLayerDescMember(*Found, OutLayerDesc);
		}
		else
		{
			return false;
		}
	}

	virtual void MarkTextureForUpdate(uint32 LayerId) override 
	{
		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return;
		}
		FScopeLock LockLayers(&LayerCritSect);
		// If the layer id does not exist in the current state, we'll search up the stack for the last version of the layer.
		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			FLayerDesc LayerDesc;
			GetLayerDescMember(*Found, LayerDesc);
			MarkLayerTextureForUpdate(*Found);
			UpdateLayer(*Found, LayerId, true);
		}
	}

	virtual void PushLayerState(bool bPreserve = false) override
	{
		FScopeLock LockLayers(&LayerCritSect);
		const auto& CurrentState = LayerState();

		if (bPreserve)
		{
			// If bPreserve is true, copy the entire state.
			LayerStack.Emplace(CurrentState);
			// We don't need to mark stereo layers as dirty as the new state is a copy of the existing one.
		}
		else
		{
			// Else start with an empty set of layers, but preserve NextLayerId.
			for (auto& Pair : StereoLayers())
			{
				// We need to mark the layers going out of scope as invalid, so implementations will remove them from the screen.
				UpdateLayer(Pair.Value, Pair.Key, false);
			}

			// New layers should continue using unique layer ids
			LayerStack.Emplace(CurrentState.NextLayerId, CurrentState.bShowBackground);
			bStereoLayersDirty = true;
		}
	}

	virtual void PopLayerState() override
	{
		FScopeLock LockLayers(&LayerCritSect);

		// Ignore if there is only one element on the stack
		if (LayerStack.Num() <= 1)
		{
			return;
		}

		// First mark all layers in the current state as invalid if they did not exist previously.
		for (auto& Pair : StereoLayers(0))
		{
			if (!StereoLayers(1).Contains(Pair.Key))
			{
				UpdateLayer(Pair.Value, Pair.Key, false);
			}
		}

		// Destroy the top of the stack
		LayerStack.Pop();

		// Update the layers in the new current state to mark them as valid and restore previous state
		for (auto& Pair : StereoLayers(0))
		{
			FLayerDesc LayerDesc;
			GetLayerDescMember(Pair.Value, LayerDesc);
			UpdateLayer(Pair.Value, Pair.Key, LayerDesc.IsVisible());
		}

		bStereoLayersDirty = true;
	}

	virtual void GetAllocatedTexture(uint32 LayerId, FTextureRHIRef &Texture, FTextureRHIRef &LeftTexture) override
	{
		Texture = LeftTexture = nullptr;
		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return;
		}
		FScopeLock LockLayers(&LayerCritSect);

		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			FLayerDesc LayerDesc;
			GetLayerDescMember(*Found, LayerDesc);
			if (LayerDesc.Texture)
			{
				if (LayerDesc.HasShape<FCubemapLayer>())
				{
					Texture = LayerDesc.Texture->GetTextureCube();
					LeftTexture = LayerDesc.LeftTexture ? LayerDesc.LeftTexture->GetTextureCube() : nullptr;
				}
				else
				{
					Texture = LayerDesc.Texture->GetTexture2D();
					LeftTexture = LayerDesc.LeftTexture ? LayerDesc.LeftTexture->GetTexture2D() : nullptr;
				}
			}
		}
	}

	virtual bool SupportsLayerState() override { return true; }

	virtual void HideBackgroundLayer() { LayerState().bShowBackground = false; }
	virtual void ShowBackgroundLayer() { LayerState().bShowBackground = true; }
	virtual bool IsBackgroundLayerVisible() const { return LayerStack.Last().bShowBackground; }

	virtual void UpdateSplashScreen() override { IXRLoadingScreen::ShowLoadingScreen_Compat(bSplashIsShown, (bSplashShowMovie && SplashMovie.IsValid()) ? SplashMovie : SplashTexture, SplashOffset, SplashScale); }

};

class HEADMOUNTEDDISPLAY_API FSimpleLayerManager : public TStereoLayerManager<IStereoLayers::FLayerDesc>
{
protected:
	virtual void MarkTextureForUpdate(uint32 LayerId) override
	{}
};

HEADMOUNTEDDISPLAY_API bool GetLayerDescMember(const IStereoLayers::FLayerDesc& Layer, IStereoLayers::FLayerDesc& OutLayerDesc);
HEADMOUNTEDDISPLAY_API void SetLayerDescMember(IStereoLayers::FLayerDesc& OutLayer, const IStereoLayers::FLayerDesc& InLayerDesc);
HEADMOUNTEDDISPLAY_API void MarkLayerTextureForUpdate(IStereoLayers::FLayerDesc& Layer);
