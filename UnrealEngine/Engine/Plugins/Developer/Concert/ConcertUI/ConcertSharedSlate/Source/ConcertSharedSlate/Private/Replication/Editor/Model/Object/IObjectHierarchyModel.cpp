// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"

#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSharedSlate
{
	void IObjectHierarchyModel::ForEachChildRecursive(
		const FSoftObjectPath& Root,
		TFunctionRef<EBreakBehavior(const FSoftObjectPath& Parent, const FSoftObjectPath& ChildObject, EChildRelationship Relationship)> Callback,
		EChildRelationshipFlags InclusionFlags
		) const
	{
		struct FHelper
		{
			static EBreakBehavior Visit(
				const IObjectHierarchyModel& Model,
				const FSoftObjectPath& Parent,
				const FSoftObjectPath& ChildObject,
				EChildRelationship Relationship,
				TFunctionRef<EBreakBehavior(const FSoftObjectPath&, const FSoftObjectPath&, EChildRelationship)> Callback,
				EChildRelationshipFlags InclusionFlags
				)
			{
				if (Callback(Parent, ChildObject, Relationship) == EBreakBehavior::Break)
				{
					return EBreakBehavior::Break;
				}
				
				EBreakBehavior Result = EBreakBehavior::Continue;
				Model.ForEachDirectChild(ChildObject, [&Model, &ChildObject, &Callback, &InclusionFlags, &Result](const FSoftObjectPath& ChildOfChild, EChildRelationship Relationship)
				{
					Result = Visit(Model, ChildObject, ChildOfChild, Relationship, Callback, InclusionFlags);
					return Result;
				});
				return Result;
			}
		};
			
		ForEachDirectChild(Root, [this, &Root, &Callback, &InclusionFlags](const FSoftObjectPath& ChildObject, EChildRelationship Relationship)
		{
			return FHelper::Visit(*this, Root, ChildObject, Relationship, Callback, InclusionFlags);
		}, InclusionFlags);
	}
}
