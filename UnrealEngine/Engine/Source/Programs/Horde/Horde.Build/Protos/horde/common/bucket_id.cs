// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeCommon.Wrapper
{
	using NativeBucketId = EpicGames.Horde.Storage.BucketId;

#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	partial class BucketId
	{
		public static implicit operator NativeBucketId(BucketId wrapper)
		{
			return new NativeBucketId(wrapper.Data);
		}

		public static implicit operator BucketId(NativeBucketId source)
		{
			BucketId result = new BucketId();
			result.Data = source.ToString();
			return result;
		}
	}
#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member
}
