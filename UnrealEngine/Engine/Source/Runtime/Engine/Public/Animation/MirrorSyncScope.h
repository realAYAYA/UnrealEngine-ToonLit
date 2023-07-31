// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNodeMessages.h"
#include "Animation/MirrorDataTable.h"

namespace UE { namespace Anim {

class FAnimSyncGroupScope;

class ENGINE_API FAnimNotifyMirrorContext : public IAnimNotifyEventContextDataInterface
{
	DECLARE_NOTIFY_CONTEXT_INTERFACE(FAnimNotifyMirrorContext)
	public:
	FAnimNotifyMirrorContext() {} 
	FAnimNotifyMirrorContext(const UMirrorDataTable* MirrorDataTable) : MirrorTable(MirrorDataTable) {bAnimationMirrored = MirrorDataTable != nullptr;} 
	bool bAnimationMirrored = true; 

	TWeakObjectPtr<const UMirrorDataTable> MirrorTable; 
};
	
// Scoped graph message used to synchronize mirroring 
class ENGINE_API FMirrorSyncScope : public IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(FMirrorSyncScope);
public:
	FMirrorSyncScope(const FAnimationBaseContext& InContext, const UMirrorDataTable* InMirrorDataTable);
	virtual ~FMirrorSyncScope();
	int32 GetMirrorScopeDepth() const;
	virtual TUniquePtr<const IAnimNotifyEventContextDataInterface> MakeUniqueEventContextData() const override;
private:
	const UMirrorDataTable* MirrorDataTable = nullptr;
	int32 MirrorScopeDepth = 1; 
	const UMirrorDataTable* OuterScopeMirrorDataTable = nullptr;
	FAnimSyncGroupScope* AnimSyncGroupScope = nullptr; 
};

}}	// namespace UE::Anim
