// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkVirtualSubject.h"


#include "LiveLinkAnimationVirtualSubject.generated.h"

struct FLiveLinkSubjectFrameData;
struct FLiveLinkSubjectKey;


// A Skeleton virtual subject is an assembly of different subjects supporting the animation role
UCLASS(meta=(DisplayName="Animation Virtual Subject"))
class LIVELINK_API ULiveLinkAnimationVirtualSubject : public ULiveLinkVirtualSubject
{
	GENERATED_BODY()

public:
	ULiveLinkAnimationVirtualSubject();

	virtual void Update() override;

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

protected:
	// Validates current source subjects
	bool AreSubjectsValid(const TArray<FLiveLinkSubjectKey>& InActiveSubjects) const;

	bool BuildSubjectSnapshot(TArray<FLiveLinkSubjectFrameData>& OutSnapshot);

	// Builds a new ref skeleton based on the current subject state (can early out if ref skeleton is already up to date)
	void BuildSkeleton(const TArray<FLiveLinkSubjectFrameData>& InSubjectSnapshots);

	void BuildFrame(const TArray<FLiveLinkSubjectFrameData>& InSubjectSnapshots);

	// Tests to see if current ref skeleton is up to data
	bool DoesSkeletonNeedRebuilding() const;
	bool bInvalidate;

public:

	//Whether to append SubjectName to each bones part of the virtual hierarchy
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bAppendSubjectNameToBones;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "ILiveLinkClient.h"
#endif
