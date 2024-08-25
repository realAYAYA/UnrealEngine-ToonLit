// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "ILiveLinkClient.h"
#include "LiveLinkSubject.h"
#include "LiveLinkSubjectSettings.h"
#include "LiveLinkVirtualSubject.h"
#include "UObject/GCObject.h"

class FLiveLinkSourceCollection;

class FLiveLinkSubject;
class ILiveLinkSource;
class ILiveLinkSubject;

class ULiveLinkSourceSettings;
class ULiveLinkSubjectSettings;
class ULiveLinkVirtualSubject;

struct LIVELINK_API FLiveLinkCollectionSourceItem
{
	FLiveLinkCollectionSourceItem() = default;
	FLiveLinkCollectionSourceItem(FLiveLinkCollectionSourceItem&&) = default;
	FLiveLinkCollectionSourceItem& operator=(FLiveLinkCollectionSourceItem&&) = default;

	FLiveLinkCollectionSourceItem(const FLiveLinkCollectionSourceItem&) = delete;
	FLiveLinkCollectionSourceItem& operator=(const FLiveLinkCollectionSourceItem&) = delete;

	FGuid Guid;
	TObjectPtr<ULiveLinkSourceSettings> Setting; // GC by FLiveLinkSourceCollection::AddReferencedObjects
	TSharedPtr<ILiveLinkSource> Source;
	TSharedPtr<FLiveLinkTimedDataInput> TimedData;
	bool bPendingKill = false;
	bool bIsVirtualSource = false;

public:
	bool IsVirtualSource() const;
};


struct LIVELINK_API FLiveLinkCollectionSubjectItem
{
	FLiveLinkCollectionSubjectItem(FLiveLinkSubjectKey InKey, TUniquePtr<FLiveLinkSubject> InLiveSubject, ULiveLinkSubjectSettings* InSettings, bool bInEnabled);
	FLiveLinkCollectionSubjectItem(FLiveLinkSubjectKey InKey, ULiveLinkVirtualSubject* InVirtualSubject, bool bInEnabled);

public:
	FLiveLinkSubjectKey Key;
	bool bEnabled;
	bool bPendingKill;

	ILiveLinkSubject* GetSubject() { return VirtualSubject ? static_cast<ILiveLinkSubject*>(VirtualSubject) : static_cast<ILiveLinkSubject*>(LiveSubject.Get()); }
	FLiveLinkSubject* GetLiveSubject() { return LiveSubject.Get(); }
	ULiveLinkVirtualSubject* GetVirtualSubject() { return VirtualSubject; }
	ILiveLinkSubject* GetSubject() const { return VirtualSubject ? VirtualSubject : static_cast<ILiveLinkSubject*>(LiveSubject.Get()); }
	FLiveLinkSubject* GetLiveSubject() const { return LiveSubject.Get(); }
	ULiveLinkVirtualSubject* GetVirtualSubject() const { return VirtualSubject; }
	UObject* GetSettings() const { return VirtualSubject ? static_cast<UObject*>(VirtualSubject) : static_cast<UObject*>(Setting); }
	ULiveLinkSubjectSettings* GetLinkSettings() const { return Setting; }

private:
	TObjectPtr<ULiveLinkSubjectSettings> Setting; // GC by FLiveLinkSourceCollection::AddReferencedObjects
	TUniquePtr<FLiveLinkSubject> LiveSubject;
	TObjectPtr<ULiveLinkVirtualSubject> VirtualSubject; // GC by FLiveLinkSourceCollection::AddReferencedObjects

public:
	FLiveLinkCollectionSubjectItem(const FLiveLinkCollectionSubjectItem&) = delete;
	FLiveLinkCollectionSubjectItem& operator=(const FLiveLinkCollectionSubjectItem&) = delete;
	FLiveLinkCollectionSubjectItem(FLiveLinkCollectionSubjectItem&&) = default;
	FLiveLinkCollectionSubjectItem& operator=(FLiveLinkCollectionSubjectItem&&) = default;

	friend FLiveLinkSourceCollection;
};


class LIVELINK_API FLiveLinkSourceCollection : public FGCObject
{
public:
	// "source guid" for virtual subjects
	static const FGuid DefaultVirtualSubjectGuid;
	FLiveLinkSourceCollection();

	UE_NONCOPYABLE(FLiveLinkSourceCollection)

public:
	//~ Begin FGCObject implementation
	virtual void AddReferencedObjects(FReferenceCollector & Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FLiveLinkSourceCollection");
	}
	//~ End FGCObject implementation

public:

	
	TArray<FLiveLinkCollectionSourceItem>& GetSources() { return Sources; }
	const TArray<FLiveLinkCollectionSourceItem>& GetSources() const { return Sources; }
	const TArray<FLiveLinkCollectionSubjectItem>& GetSubjects() const { return Subjects; }

	void AddSource(FLiveLinkCollectionSourceItem Source);
	void RemoveSource(FGuid SourceGuid);
	void RemoveAllSources();
	FLiveLinkCollectionSourceItem* FindSource(TSharedPtr<ILiveLinkSource> Source);
	const FLiveLinkCollectionSourceItem* FindSource(TSharedPtr<ILiveLinkSource> Source) const;
	FLiveLinkCollectionSourceItem* FindSource(FGuid SourceGuid);
	const FLiveLinkCollectionSourceItem* FindSource(FGuid SourceGuid) const;
	FLiveLinkCollectionSourceItem* FindVirtualSource(FName VirtualSourceName);
	const FLiveLinkCollectionSourceItem* FindVirtualSource(FName VirtualSourceName) const;

	void AddSubject(FLiveLinkCollectionSubjectItem Subject);
	void RemoveSubject(FLiveLinkSubjectKey SubjectKey);
	FLiveLinkCollectionSubjectItem* FindSubject(FLiveLinkSubjectKey SubjectKey);
	const FLiveLinkCollectionSubjectItem* FindSubject(FLiveLinkSubjectKey SubjectKey) const;
	const FLiveLinkCollectionSubjectItem* FindEnabledSubject(FLiveLinkSubjectName SubjectName) const;

	bool IsSubjectEnabled(FLiveLinkSubjectKey SubjectKey) const;
	void SetSubjectEnabled(FLiveLinkSubjectKey SubjectKey, bool bEnabled);

	void RemovePendingKill();
	bool RequestShutdown();

	FSimpleMulticastDelegate& OnLiveLinkSourcesChanged() { return OnLiveLinkSourcesChangedDelegate; }
	FSimpleMulticastDelegate& OnLiveLinkSubjectsChanged() { return OnLiveLinkSubjectsChangedDelegate; }

	FOnLiveLinkSourceChangedDelegate& OnLiveLinkSourceAdded() { return OnLiveLinkSourceAddedDelegate; }
	FOnLiveLinkSourceChangedDelegate& OnLiveLinkSourceRemoved() { return OnLiveLinkSourceRemovedDelegate; }
	FOnLiveLinkSubjectChangedDelegate& OnLiveLinkSubjectAdded() { return OnLiveLinkSubjectAddedDelegate; }
	FOnLiveLinkSubjectChangedDelegate& OnLiveLinkSubjectRemoved() { return OnLiveLinkSubjectRemovedDelegate; }

private:
	void RemoveSource(int32 Index);

private:
	TArray<FLiveLinkCollectionSourceItem> Sources;

	TArray<FLiveLinkCollectionSubjectItem> Subjects;

	/** Notify when the client sources list has changed */
	FSimpleMulticastDelegate OnLiveLinkSourcesChangedDelegate;

	/** Notify when a client subjects list has changed */
	FSimpleMulticastDelegate OnLiveLinkSubjectsChangedDelegate;

	/** Notify when a client source's is added */
	FOnLiveLinkSourceChangedDelegate OnLiveLinkSourceAddedDelegate;

	/** Notify when a client source's is removed */
	FOnLiveLinkSourceChangedDelegate OnLiveLinkSourceRemovedDelegate;

	/** Notify when a client subject's is added */
	FOnLiveLinkSubjectChangedDelegate OnLiveLinkSubjectAddedDelegate;

	/** Notify when a client subject's is removed */
	FOnLiveLinkSubjectChangedDelegate OnLiveLinkSubjectRemovedDelegate;
};
