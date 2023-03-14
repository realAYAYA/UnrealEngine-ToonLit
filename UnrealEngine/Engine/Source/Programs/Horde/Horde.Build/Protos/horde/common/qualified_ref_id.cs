// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeCommon.Wrapper
{
	using System.Diagnostics.CodeAnalysis;
	using NativeQualifiedRefId = EpicGames.Horde.Storage.QualifiedRefId;

#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	partial class QualifiedRefId
	{
		[return: NotNullIfNotNull("wrapper")]
		public static implicit operator NativeQualifiedRefId?(QualifiedRefId? wrapper)
		{
			if (wrapper == null)
			{
				return null;
			}
			else
			{
				return new NativeQualifiedRefId(wrapper.NamespaceId, wrapper.BucketId, wrapper.RefId);
			}
		}

		[return: NotNullIfNotNull("source")]
		public static implicit operator QualifiedRefId?(NativeQualifiedRefId? source)
		{
			if (source == null)
			{
				return null;
			}
			else
			{
				QualifiedRefId result = new QualifiedRefId();
				result.NamespaceId = source.NamespaceId;
				result.BucketId = source.BucketId;
				result.RefId = source.RefId;
				return result;
			}
		}
	}
#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member
}
