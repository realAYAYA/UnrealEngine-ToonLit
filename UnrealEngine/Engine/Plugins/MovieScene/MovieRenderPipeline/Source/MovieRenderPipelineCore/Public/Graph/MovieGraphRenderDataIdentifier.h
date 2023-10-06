// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MovieGraphRenderDataIdentifier.generated.h"

/**
* This data structure can be used to identify what render data a set of pixels represents
* by knowing what the render layer name is, what renderer produced it, if it's a sub-resource,
* and what camera it is for. Can be used as the key in a TMap.
*/
USTRUCT(BlueprintType)
struct FMovieGraphRenderDataIdentifier
{
	GENERATED_BODY()

	FMovieGraphRenderDataIdentifier()
	{}

	FMovieGraphRenderDataIdentifier(const FName& InRootBranchName, const FString& InRendererName,
		const FString& InSubRenderResourceName, const FString& InCameraName)
		: RootBranchName(InRootBranchName)
		, RendererName(InRendererName)
		, SubResourceName(InSubRenderResourceName)
		, CameraName(InCameraName)
	{
	}

	bool operator == (const FMovieGraphRenderDataIdentifier& InRHS) const
	{
		return RootBranchName == InRHS.RootBranchName && 
			RendererName == InRHS.RendererName &&
			SubResourceName == InRHS.SubResourceName &&
			CameraName == InRHS.CameraName;
	}

	bool operator != (const FMovieGraphRenderDataIdentifier& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend uint32 GetTypeHash(FMovieGraphRenderDataIdentifier InIdentifier)
	{
		return HashCombineFast(GetTypeHash(InIdentifier.RootBranchName),
			HashCombineFast(GetTypeHash(InIdentifier.RendererName),
				HashCombineFast(GetTypeHash(InIdentifier.SubResourceName),
					GetTypeHash(InIdentifier.CameraName))));
	}

	friend FString LexToString(const FMovieGraphRenderDataIdentifier InIdentifier)
	{
		return FString::Printf(TEXT("RootBranch: %s Renderer:%s SubResource: %s Camera: %s"), *InIdentifier.RootBranchName.ToString(), *InIdentifier.RendererName, *InIdentifier.SubResourceName, *InIdentifier.CameraName);
	}

public:
	/** 
	* The root branch name that this render layer exists on. Actual display name comes from a UMovieGraphRenderLayerNode (if found in the branch)
	* otherwise it falls back to just displaying RootBranchName. All of our internal lookups for branches are done based on the path we followed 
	* out from the root so that we can handle overriding a render layer display name via regular node overriding.
	* 
	* Could have a value like "Globals" or a user-provided one "character", "background", etc.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FName RootBranchName;

	/** Which renderer was used to produce this image ("panoramic" "deferred" "path tracer", etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FString RendererName;

	/** A sub-resource name for the renderer (ie: "beauty", "object id", "depth", etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FString SubResourceName;

	/** The name of the camera being used for this render. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FString CameraName;
};