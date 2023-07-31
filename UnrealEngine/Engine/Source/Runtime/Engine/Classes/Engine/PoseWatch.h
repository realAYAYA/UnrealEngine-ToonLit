// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BoneIndices.h"
#include "PoseWatch.generated.h"

struct FCompactHeapPose;
class UAnimBlueprint;
class UBlendProfile;
struct FSlateIcon;

namespace PoseWatchUtil
{
	/** Gets all pose watches that are parented to Folder, if Folder is nullptr then gets orphans */
	TSet<UPoseWatch*> GetChildrenPoseWatchOf(const UPoseWatchFolder* Folder, const UAnimBlueprint* AnimBlueprint);

	/** Gets all pose watches folders that are parented to Folder, if Folder is nullptr then gets orphans */
	TSet<UPoseWatchFolder*> GetChildrenPoseWatchFoldersOf(const UPoseWatchFolder* Folder, const UAnimBlueprint* AnimBlueprint);

	/** Returns a new random color */
	FColor ChoosePoseWatchColor();

	template <typename TParent, typename TItem> FText FindUniqueNameInParent(TParent* InParent, const TItem* InItem);
}

UCLASS()
class ENGINE_API UPoseWatchFolder
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	/** Returns the slash delimited path of this folder (e.g. MyFolder/MyNestedFolder/MyPoseWatch)*/
	const FText GetPath() const;

	/** The default name given to all new folders */
	FText GetDefaultLabel() const;

	/** Returns the display label assigned to this pose watch folder */
	FText GetLabel() const;

	/** Returns the visibility of this pose watch folder */
	bool GetIsVisible() const;

	/** Returns the parent folder this folder belongs to, if any */
	UPoseWatchFolder* GetParent() const;
	
	/**  Attempts to set this folder's parent, returns false if unsuccessful (if moving causes a name clash)
	 *	 Using bForce may change the folder's label to ensure no name clashes
	 */
	bool SetParent(UPoseWatchFolder* Parent, bool bForce = false);

	/** Alias of SetParent */
	void MoveTo(UPoseWatchFolder* InFolder);

	/** Attempts to set the label, returns false if unsuccessful (if there's a name clash with another  folder in the current directory) */
	bool SetLabel(const FText& InLabel);

	/** Sets the visibility of this folder, must contain at least one post watch descendant to become visible */
	void SetIsVisible(bool bInIsVisible, bool bUpdateChildren=true);

	/** Called before the pose watch folder is deleted to cleanup it's children and update it's parent */
	void OnRemoved();

	/** Returns true if InFolder is the parent of this */
	bool IsIn(const UPoseWatchFolder* InFolder) const;

	/** Returns true if this is a descendant of InFolder */
	bool IsDescendantOf(const UPoseWatchFolder* InFolder) const;

	/** Returns true if this folder is inside another folder */
	bool IsAssignedFolder() const;

	/** Returns true if there would be no name clashes when assigning the label InLabel */
	bool ValidateLabelRename(const FText& InLabel, FText& OutErrorMessage);

	/** Returns true if InLabel is a unique folder label among the children of InFolder,  excluding this */
	bool IsLabelUniqueInParent(const FText& InLabel, UPoseWatchFolder* InFolder) const;

	/** Returns true if at least one UPoseWatch/UPoseWatchFolder has this as it's parent */
	bool HasChildren() const;

	/** Takes GetDefaultLabel() and generates unique labels to avoid name clashes (e.g. NewFolder1, NewFolder2, ...) */
	void SetUniqueDefaultLabel();

	/** Called when a child is removed/added to a folder */
	void UpdateVisibility();

	/** Returns the anim blueprint this pose watch folder is stored inside */
	UAnimBlueprint* GetAnimBlueprint() const;

	/** Set whether this should display its children in the pose watch manager window */
	void SetIsExpanded(bool bInIsExpanded);

	/** Returns true if this should display its children in the pose watch manager window */
	bool GetIsExpanded() const;

private:
	/** Returns a unique name for a new folder placed within InParent */
	FText FindUniqueNameInParent(UPoseWatchFolder* InParent) const;

	/** Returns true if there is at least one direct pose watch child */
	bool HasPoseWatchChildren() const;

	/** Returns true if there is at least one pose watch descendant (nested folders) */
	bool HasPoseWatchDescendents() const;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY()
	FText Label;

	UPROPERTY()
	TWeakObjectPtr<UPoseWatchFolder> Parent;

	UPROPERTY()
	bool bIsVisible = false;

	UPROPERTY(Transient)
	bool bIsExpanded = true;
#endif // WITH_EDITORONLY_DATA
};

UCLASS()
class ENGINE_API UPoseWatchElement
	: public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR

	virtual ~UPoseWatchElement() = default;

	/** The default name given to all new pose watch elements */
	FText GetDefaultLabel() const;

	/** Returns the display label assigned to this pose watch */
	FText GetLabel() const;

	/** Returns the visibility of this pose watch */
	bool GetIsVisible() const;

	/** Returns true if the pose watch is able to draw anything in the viewport */
	virtual bool GetIsEnabled() const;

	/** Returns the parent pose watch this element belongs to. */
	UPoseWatch* GetParent() const;

	/**  Sets this element's parent. */
	void SetParent(UPoseWatch* InParent);

	/** Attempts to set the label, returns false if unsuccessful (if there's a name clash with another pose watch in the current directory). */
	bool SetLabel(const FText& InLabel);

	/** Sets whether or not to render this pose watch to the view port. */
	virtual void SetIsVisible(bool bInIsVisible);

	/** Returns true if there would be no name clashes when assigning the label InLabel. */
	bool ValidateLabelRename(const FText& InLabel, FText& OutErrorMessage);

	/** Returns true if InLabel is a unique among the children of this elements parent, excluding this. */
	bool IsLabelUniqueInParent(const FText& InLabel, UPoseWatch* InParent) const;

	/** Takes GetDefaultLabel() and generates unique labels to avoid name clashes (e.g. PoseWatch1, PoseWatch2, ...). */
	void SetUniqueDefaultLabel();

	/** Takes InLabel and generates unique labels to avoid name clashes (e.g. PoseWatch1, PoseWatch2, ...). */
	void SetUniqueLabel(const FText& InLabel);

	/** Returns the color to display the pose watch using. */
	FColor GetColor() const;

	/** Sets the display color of this pose watch in the UI and view port. */
	virtual void SetColor(const FColor& InColor);

	/** Returns true if it is possible to set the color of this element. */
	bool HasColor() const;

	/** Set the can set the color flag. */
	void SetHasColor(const bool bInHasColor);

	/** Get the name of the icon used to represent this element. */
	FName GetIconName() const;

	/** Set the name of the icon used to represent this element. */
	void SetIconName(const FName InIconName);

	/** Toggle's the pose watch's visibility */
	void ToggleIsVisible();

private:
	FText FindUniqueNameInParent(class UPoseWatch* InParent) const;

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
protected:

	// If true will draw the node to the view port.
	UPROPERTY()
	bool bIsVisible = true;

	UPROPERTY()
	bool bHasColor = true;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	FText Label;

	UPROPERTY()
	FName IconName;

	UPROPERTY()
	TWeakObjectPtr<UPoseWatch> Parent;

#endif // WITH_EDITORONLY_DATA
};

UCLASS()
class ENGINE_API UPoseWatchPoseElement : public UPoseWatchElement
{
	GENERATED_BODY()

public:

	UPoseWatchPoseElement(const class FObjectInitializer& PCIP);

#if WITH_EDITOR

	virtual bool GetIsEnabled() const;

	static FSlateIcon StaticGetIcon();

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Optionally select a Blend Mask to control which bones on the skeleton are rendered. Any non-zero entries are rendered. */
	UPROPERTY(EditAnywhere, editfixedsize, Category = Default, meta = (UseAsBlendMask = true))
	TObjectPtr<UBlendProfile> ViewportMask;

	/** Invert which bones are rendered when using a viewport mask */
	UPROPERTY(EditAnywhere, Category = Default, meta = (EditCondition = "ViewportMask != nullptr"))
	bool bInvertViewportMask;

	/** The threshold which each bone's blend scale much surpass to be rendered using the viewport mask */
	UPROPERTY(EditAnywhere, Category = Default, meta = (ClampMin = 0.f, ClampMax = 1.f, EditCondition = "ViewportMask != nullptr"))
	float BlendScaleThreshold;

	/** Offset the rendering of the bones in the viewport. */
	UPROPERTY(EditAnywhere, Category = Default)
	FVector3d ViewportOffset;
#endif // WITH_EDITORONLY_DATA
};

UCLASS()
class ENGINE_API UPoseWatch
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	/** Returns the slash delimited path of this pose watch */
	const FText GetPath() const;

	/** The default name given to all new pose watches */
	FText GetDefaultLabel() const;

	/** Returns the display label assigned to this pose watch */
	FText GetLabel() const;

	/** Returns the visibility of this pose watch */
	bool GetIsVisible() const;

	/** Returns true if any child element is enabled */
	bool GetIsEnabled() const;

	/** Returns true if the this pose watch's node is active in its anim graph */
	bool GetIsNodeEnabled() const;

	/** Returns the color to display the pose watch using */
	UE_DEPRECATED(5.1, "Node watches no longer have colors, use the color of its elements instead.")
	FColor GetColor() const;

	/** Set whether this should display its children in the pose watch manager window */
	void SetIsExpanded(bool bInIsExpanded);

	/** Returns true if this should display its children in the pose watch manager window */
	bool GetIsExpanded() const;

	/** Returns true if this pose watch should be deleted after the user has deselected its assigned node (Editor preference) */
	bool GetShouldDeleteOnDeselect() const;

	/** Returns the parent folder this pose watch belongs to, if any */
	UPoseWatchFolder* GetParent() const;

	/**  Attempts to set this pose watch's parent, returns false if unsuccessful (if moving causes a name clash)
	 *	 Using bForce may change the pose watch's label to ensure no name clashes
	 */
	bool SetParent(UPoseWatchFolder* InParent, bool bForce=false);

	/** If set, denotes the pose watch is able to be drawn to the viewport */
	void SetIsNodeEnabled(const bool bInIsEnabled);

	/** Alias of SetParent */
	void MoveTo(UPoseWatchFolder* InFolder);

	/** Attempts to set the label, returns false if unsuccessful (if there's a name clash with another pose watch in the current directory) */
	bool SetLabel(const FText& InLabel);

	/** Sets whether or not to render this pose watch to the viewport */
	void SetIsVisible(bool bInIsVisible);

	/** Sets the display color of this pose watch in the UI and viewport */
	UE_DEPRECATED(5.1, "Node watches no longer have colors, use the color of its elements instead.")
	void SetColor(const FColor& InColor);

	/** Sets whether this pose watch should delete after deselecting it's assigned node (Editor preference) */
	void SetShouldDeleteOnDeselect(const bool bInDeleteOnDeselection);

	/** Called when a pose watch is deleted to update it's parent */
	void OnRemoved();

	/** Toggle's the pose watch's visibility */
	void ToggleIsVisible();

	/** Returns true if this pose watch is inside InFolder */
	bool IsIn(const UPoseWatchFolder* InFolder) const;

	/** Returns true if this pose watch is inside some pose watch folder */
	bool IsAssignedFolder() const;

	/** Returns true if there would be no name clashes when assigning the label InLabel */
	bool ValidateLabelRename(const FText& InLabel, FText& OutErrorMessage);

	/** Returns true if InLabel is a unique pose watch label among the children of InFolder, excluding this */
	bool IsLabelUniqueInParent(const FText& InLabel, UPoseWatchFolder* InFolder) const;

	/** Takes GetDefaultLabel() and generates unique labels to avoid name clashes (e.g. PoseWatch1, PoseWatch2, ...) */
	void SetUniqueDefaultLabel();

	/** Called when a child element's visibility is changed */
	void UpdateVisibility();

	/** Returns the anim blueprint this pose watch is stored inside */
	UAnimBlueprint* GetAnimBlueprint() const;

	/** Create a new element and add it to this pose watch */
	TObjectPtr<UPoseWatchElement> AddElement(const FText InLabel, const FName IconName);

	/** Create a new element of a specified type and add it to this pose watch */
	template< class TElementType > TObjectPtr<TElementType> AddElement(const FText InLabel, const FName IconName);
	
	/** Returns a reference to the array of elements stored in this */
	TArray<TObjectPtr<UPoseWatchElement>>& GetElements() { return Elements; }

	/** Returns true if this pose watch contains the specified element */
	bool Contains(const TObjectPtr<UPoseWatchElement> InElement) { return Elements.Contains(InElement); }
	bool Contains(const UPoseWatchElement* const InElement) { return Elements.ContainsByPredicate([InElement](const TObjectPtr<UPoseWatchElement>& ContainedElement) { return InElement == ContainedElement.Get(); }); }

	/** Returns the first element with the matching type */
	template< class TElementType > TObjectPtr<TElementType> GetFirstElementOfType()
	{
		for (UPoseWatchElement* Element : Elements)
		{
			if (TElementType* FoundElement = Cast<TElementType>(Element))
			{
				return FoundElement;
			}
		}
		return nullptr;
	}

	
private:
	/** Returns a unique name for a new pose watch placed within InParent */
	FText FindUniqueNameInParent(UPoseWatchFolder* InParent) const;

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
public:
	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY()
	TWeakObjectPtr<class UEdGraphNode> Node;

	UPROPERTY()
	TObjectPtr<UBlendProfile> ViewportMask_DEPRECATED;
	UPROPERTY()
	bool bInvertViewportMask_DEPRECATED;
	UPROPERTY()
	float BlendScaleThreshold_DEPRECATED;
	UPROPERTY()
	FVector3d ViewportOffset_DEPRECATED;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UPoseWatchElement>> Elements;

	UPROPERTY()
	bool bDeleteOnDeselection = false;

	// If true will draw the pose to the viewport
	UPROPERTY()
	bool bIsVisible = true;

	// If true, the node is able to be drawn to the view port.
	UPROPERTY(Transient)
	bool bIsNodeEnabled = false;

	UPROPERTY()
	bool bIsExpanded = true;

	UPROPERTY()
	FColor Color_DEPRECATED;

	UPROPERTY()
	FText Label;

	UPROPERTY()
	FName IconName_DEPRECATED;

	UPROPERTY()
	TWeakObjectPtr<UPoseWatchFolder> Parent;
#endif // WITH_EDITORONLY_DATA
};


#if WITH_EDITOR

template< class TElementType > TObjectPtr<TElementType> UPoseWatch::AddElement(const FText InLabel, const FName InIconName)
{
	TObjectPtr<TElementType> PoseWatchElement(NewObject<TElementType>(this));
	PoseWatchElement->SetParent(this);
	PoseWatchElement->SetUniqueLabel(InLabel);
	PoseWatchElement->SetIconName(InIconName);
	PoseWatchElement->SetColor(FColor::MakeRandomColor());
	Elements.Add(PoseWatchElement);

	return PoseWatchElement;
}

#endif // WITH_EDITOR