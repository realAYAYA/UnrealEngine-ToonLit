// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace HordeCommon.Wrapper
{
	using NativeNamespaceId = EpicGames.Horde.Storage.NamespaceId;

#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	partial class NamespaceId
	{
		public static implicit operator NativeNamespaceId(NamespaceId wrapper)
		{
			return new NativeNamespaceId(new Utf8String(wrapper.Data));
		}

		public static implicit operator NamespaceId(NativeNamespaceId source)
		{
			NamespaceId result = new NamespaceId();
			result.Data = source.ToString();
			return result;
		}
	}
#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member
}
