// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeRWLock.h"
#include "PrimitiveSceneInfo.h"

#if !UE_BUILD_SHIPPING // TODO: Decide whether or not the struct should be entirely stripped out of shipping

class UMaterialInterface;
class FScene;
class FViewInfo;
class FViewCommands;

DECLARE_MULTICAST_DELEGATE(FOnUpdateViewDebugInfo);

/**
 * A collection of debug data associated with the current on screen view.
 */
struct FViewDebugInfo
{
	friend class FDrawPrimitiveDebuggerModule;

private:

	static RENDERER_API FViewDebugInfo Instance;

	RENDERER_API FViewDebugInfo();
	
public:

	/**
	 * Gets a reference to the view debug information that is used by the renderer.
	 * @returns The debug information that is used by the renderer.
	 */
	static inline FViewDebugInfo& Get()
	{
		return Instance;
	}

	/**
	 * Data collected about a single primitive being drawn to the screen.
	 */
	struct FPrimitiveInfo
	{
		UObject* Owner;
		FPrimitiveComponentId ComponentId;
		IPrimitiveComponent* ComponentInterface;
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

	RENDERER_API void ProcessPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& View, FScene* Scene, const IPrimitiveComponent* DebugComponent);

	RENDERER_API void CaptureNextFrame();

	RENDERER_API void EnableLiveCapture();

	RENDERER_API void DisableLiveCapture();

	static RENDERER_API void DumpPrimitives(FScene* Scene, const FViewCommands& ViewCommands);

public:
	RENDERER_API void ProcessPrimitives(FScene* Scene, const FViewInfo& View, const FViewCommands& ViewCommands);

	/**
	 * Writes the currently stored information out to a CSV file.
	 */
	RENDERER_API void DumpToCSV() const;

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
	RENDERER_API bool HasEverUpdated() const;

	/**
	 * Checks if current information is from an older frame.
	 * @returns True if the data in this object is outdated.
	 */
	RENDERER_API bool IsOutOfDate() const;

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