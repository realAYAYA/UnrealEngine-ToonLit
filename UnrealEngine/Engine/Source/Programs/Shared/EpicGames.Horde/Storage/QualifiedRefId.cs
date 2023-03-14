// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Qualified locator for a ref
	/// </summary>
	public class QualifiedRefId
	{
		/// <summary>
		/// The namespace containing the ref
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Bucket containing the ref
		/// </summary>
		public BucketId BucketId { get; }

		/// <summary>
		/// The ref itself
		/// </summary>
		public RefId RefId { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="namespaceId"></param>
		/// <param name="bucketId"></param>
		/// <param name="refId"></param>
		public QualifiedRefId(NamespaceId namespaceId, BucketId bucketId, RefId refId)
		{
			NamespaceId = namespaceId;
			BucketId = bucketId;
			RefId = refId;
		}
	}
}
