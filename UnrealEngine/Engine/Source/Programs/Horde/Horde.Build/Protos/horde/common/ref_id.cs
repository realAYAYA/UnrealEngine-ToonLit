// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeCommon.Wrapper
{
	using NativeRefId = EpicGames.Horde.Storage.RefId;

#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	partial class RefId
	{
		public NativeRefId AsRefId()
		{
			return new NativeRefId(Data);
		}

		public static implicit operator NativeRefId(RefId wrapper)
		{
			return new NativeRefId(wrapper.Data);
		}

		public static implicit operator RefId(NativeRefId source)
		{
			RefId refId = new RefId();
			refId.Data = source.Hash;
			return refId;
		}
	}
#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member
}
