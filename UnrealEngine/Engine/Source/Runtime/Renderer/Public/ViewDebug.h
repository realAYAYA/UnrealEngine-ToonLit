// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneInfo.h"

#if !UE_BUILD_SHIPPING // TODO: Decide whether or not the struct should be entirely stripped out of shipping

DECLARE_MULTICAST_DELEGATE(FOnUpdateViewDebugInfo);

/**
 * A collection of debug data associated with the current on screen view.
 */
struct RENDERER_API FViewDebugInfo
{
	friend class FSceneRenderer;
	friend class FDrawPrimitiveDebuggerModule;

private:

	static FViewDebugInfo Instance;

	FViewDebugInfo();
	
public:

	/**
	 * Gets a reference to the view debug information that is used by the renderer.
	 * @returns The debug information that is used by the renderer.
	 */
	static inline const FViewDebugInfo& Get()
	{
		return Instance;
	}

	/**
	 * Data collected about a single primitive being drawn to the screen.
	 */
	struct FPrimitiveInfo
	{
		AActor* Owner;
		FPrimitiveComponentId ComponentId;
		UPrimitiveComponent* Component;
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		TArray<UMaterialInterface*> Materials;
		FString Name;
		uint32 DrawCount;
		int32 TriangleCount;
		int32 LOD;

		bool operator<(const FPrimitiveInfo& Other) const
		{
			// Sort by name to group similar assets together, then by exact primitives so we can ignore duplicates
			const int32 NameCompare = Name.Compare(Other.Name);
			if (NameCompare != 0)
			{
				return NameCompare < 0;
			}

			return PrimitiveSceneInfo < Other.PrimitiveSceneInfo;
		}
	};

private:

	bool bHasEverUpdated;
	bool bIsOutdated;
	bool bShouldUpdate;
	bool bShouldCaptureSingleFrame;

	FOnUpdateViewDebugInfo OnUpdate;

	mutable FRWLock Lock;
	
	TArray<FPrimitiveInfo> Primitives;

	void ProcessPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& View, FScene* Scene, const UPrimitiveComponent* DebugComponent);

	void CaptureNextFrame();

	void EnableLiveCapture();

	void DisableLiveCapture();

public:

	/**
	 * Writes the currently stored information out to a CSV file.
	 */
	void DumpToCSV() const;

	/**
	 * Performs an operation for each primitive currently tracked.
	 * @param Action The action to perform for each primitive.
	 */
	template <typename CallableT>
	void ForEachPrimitive(CallableT Action) const
	{
		const FPrimitiveSceneInfo* LastPrimitiveSceneInfo = nullptr;
		FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
		for (const FPrimitiveInfo& Primitive : Primitives)
		{
			if (Primitive.PrimitiveSceneInfo != LastPrimitiveSceneInfo)
			{
				Invoke(Action, Primitive);
				LastPrimitiveSceneInfo = Primitive.PrimitiveSceneInfo;
			}
		}
	}

	/**
	 * Checks if this debug information has ever been updated.
	 * @returns True if the information has been updated at least once.
	 */
	bool HasEverUpdated() const;

	/**
	 * Checks if current information is from an older frame.
	 * @returns True if the data in this object is outdated.
	 */
	bool IsOutOfDate() const;

	template <typename UserClass>
	FDelegateHandle AddUpdateHandler(UserClass* UserObject, void (UserClass::*Func)())
	{
		return OnUpdate.AddRaw(UserObject, Func);
	}

	FDelegateHandle AddUpdateHandler(void (*Func)())
	{
		return OnUpdate.AddStatic(Func);
	}

	void RemoveUpdateHandler(const FDelegateHandle& Handle)
	{
		OnUpdate.Remove(Handle);
	}
};
#endif