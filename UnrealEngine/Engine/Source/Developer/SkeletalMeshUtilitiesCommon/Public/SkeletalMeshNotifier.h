// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"
#include "Containers/Array.h"

class FName;
class HHitProxy;

enum class ESkeletalMeshNotifyType
{
	BonesAdded,
	BonesRemoved,
	BonesMoved,
	BonesSelected,
	BonesRenamed,
	HierarchyChanged
};

// A delegate for monitoring to skeletal mesh global notifications.
DECLARE_MULTICAST_DELEGATE_TwoParams(FSkeletalMeshNotifyDelegate, const TArray<FName>& /*InBoneNames*/, const ESkeletalMeshNotifyType /*InNotifType*/);

class SKELETALMESHUTILITIESCOMMON_API ISkeletalMeshNotifier
{
public:
	ISkeletalMeshNotifier() = default;
	virtual ~ISkeletalMeshNotifier() = default;

	FSkeletalMeshNotifyDelegate& Delegate();

	// override this function to react to notifications locally.
	virtual void HandleNotification(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType) = 0;

	void Notify(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType) const;
	bool Notifying() const;

private:
	FSkeletalMeshNotifyDelegate NotifyDelegate;
	mutable bool bNotifying = false;
};

class SKELETALMESHUTILITIESCOMMON_API ISkeletalMeshEditorBinding
{
public:
	ISkeletalMeshEditorBinding() = default;
	virtual ~ISkeletalMeshEditorBinding() = default;

	virtual ISkeletalMeshNotifier& GetNotifier() = 0;

	using NameFunction = TFunction< TOptional<FName>(HHitProxy*) >;
	virtual NameFunction GetNameFunction() = 0;

	virtual TArray<FName> GetSelectedBones() const = 0;
};