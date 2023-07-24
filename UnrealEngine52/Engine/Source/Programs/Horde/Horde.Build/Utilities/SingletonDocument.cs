// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Reflection;
using System.Threading.Tasks;
using Horde.Build.Server;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Utilities
{
	using SingletonId = StringId<SingletonBase>;

	/// <summary>
	/// Base class for singleton documents
	/// </summary>
	public abstract class SingletonBase
	{
		/// <summary>
		/// Unique id for this singleton
		/// </summary>
		[BsonId]
		public SingletonId Id { get; set; }

		/// <summary>
		/// The revision index of this document
		/// </summary>
		public int Revision { get; set; }

		/// <summary>
		/// Callback to allow the singleton to fix up itself after being read
		/// </summary>
		public virtual void PostLoad()
		{
		}
	}

	/// <summary>
	/// Attribute specifying the unique id for a singleton document
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class SingletonDocumentAttribute : Attribute
	{
		/// <summary>
		/// Unique id for the singleton document
		/// </summary>
		public string Id { get; }

		/// <summary>
		/// ObjectId for the legacy document
		/// </summary>
		public string? LegacyId { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for the singleton document. Should be a valid StringId.</param>
		/// <param name="legacyId">Legacy identifier for the document</param>
		public SingletonDocumentAttribute(string id, string? legacyId = null)
		{
			Id = id;
			LegacyId = legacyId;
		}
	}

	/// <summary>
	/// Interface for the getting and setting the singleton
	/// </summary>
	/// <typeparam name="T">Type of document</typeparam>
	public interface ISingletonDocument<T>
	{
		/// <summary>
		/// Gets the current document
		/// </summary>
		/// <returns>The current document</returns>
		Task<T> GetAsync();

		/// <summary>
		/// Attempts to update the document
		/// </summary>
		/// <param name="value">New state of the document</param>
		/// <returns>True if the document was updated, false otherwise</returns>
		Task<bool> TryUpdateAsync(T value);
	}

	/// <summary>
	/// Concrete implementation of <see cref="ISingletonDocument{T}"/>
	/// </summary>
	/// <typeparam name="T">The document type</typeparam>
	public class SingletonDocument<T> : ISingletonDocument<T> where T : SingletonBase, new()
	{
		/// <summary>
		/// The database service instance
		/// </summary>
		readonly MongoService _mongoService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public SingletonDocument(MongoService mongoService)
		{
			_mongoService = mongoService;
		}

		/// <inheritdoc/>
		public async Task<T> GetAsync()
		{
			return await _mongoService.GetSingletonAsync<T>();
		}

		/// <inheritdoc/>
		public Task<bool> TryUpdateAsync(T value)
		{
			return _mongoService.TryUpdateSingletonAsync<T>(value);
		}
	}

	/// <summary>
	/// Extension methods for singletons
	/// </summary>
	public static class SingletonDocumentExtensions
	{
		/// <summary>
		/// Update a singleton
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="singleton"></param>
		/// <param name="updateAction"></param>
		/// <returns></returns>
		public static async Task<T> UpdateAsync<T>(this ISingletonDocument<T> singleton, Action<T> updateAction) where T : SingletonBase, new()
		{
			for (; ; )
			{
				T value = await singleton.GetAsync();
				updateAction(value);

				if (await singleton.TryUpdateAsync(value))
				{
					return value;
				}
			}
		}
	}
}
