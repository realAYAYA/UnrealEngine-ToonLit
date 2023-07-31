// Copyright Epic Games, Inc. All Rights Reserved.

using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;

namespace EpicGames.MongoDB
{
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
		/// Constructor
		/// </summary>
		/// <param name="Id">Unique id for the singleton document</param>
		public SingletonDocumentAttribute(string Id)
		{
			this.Id = Id;
		}
	}

	/// <summary>
	/// Base class for singletons
	/// </summary>
	public class SingletonBase
	{
		/// <summary>
		/// Unique id of the 
		/// </summary>
		public ObjectId Id { get; set; }

		/// <summary>
		/// Current revision number
		/// </summary>
		public int Revision { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		/// <param name="Unused"></param>
		[BsonConstructor]
		protected SingletonBase()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Id">Unique id for the singleton</param>
		public SingletonBase(ObjectId Id)
		{
			this.Id = Id;
		}

		/// <summary>
		/// Base class for singleton documents
		/// </summary>
		class CachedId<T>
		{
			public static ObjectId Value { get; } = GetSingletonId();

			static ObjectId GetSingletonId()
			{
				SingletonDocumentAttribute? Attribute = typeof(T).GetCustomAttribute<SingletonDocumentAttribute>();
				if (Attribute == null)
				{
					throw new Exception($"Type {typeof(T).Name} is missing a {nameof(SingletonDocumentAttribute)} annotation");
				}
				return ObjectId.Parse(Attribute.Id);
			}
		}

		/// <summary>
		/// Gets the id for a singleton type
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		public static ObjectId GetId<T>() where T : SingletonBase
		{
			return CachedId<T>.Value;
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
		/// <param name="Value">New state of the document</param>
		/// <returns>True if the document was updated, false otherwise</returns>
		Task<bool> TryUpdateAsync(T Value);
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
		IMongoCollection<T> Collection;

		/// <summary>
		/// Unique id for the singleton document
		/// </summary>
		ObjectId ObjectId;

		/// <summary>
		/// Static constructor. Registers the document using the automapper.
		/// </summary>
		static SingletonDocument()
		{
			BsonClassMap.RegisterClassMap<T>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="DatabaseService">The database service instance</param>
		public SingletonDocument(IMongoCollection<SingletonBase> Collection)
		{
			this.Collection = Collection.OfType<T>();

			SingletonDocumentAttribute? Attribute = typeof(T).GetCustomAttribute<SingletonDocumentAttribute>();
			if (Attribute == null)
			{
				throw new Exception($"Type {typeof(T).Name} is missing a {nameof(SingletonDocumentAttribute)} annotation");
			}

			ObjectId = new ObjectId(Attribute.Id);
		}

		/// <inheritdoc/>
		public async Task<T> GetAsync()
		{
			for (; ; )
			{
				T? Object = await Collection.Find<T>(x => x.Id == ObjectId).FirstOrDefaultAsync();
				if (Object != null)
				{
					return Object;
				}

				T NewItem = new T();
				NewItem.Id = ObjectId;
				await Collection.InsertOneAsync(NewItem);
			}
		}

		/// <inheritdoc/>
		public async Task<bool> TryUpdateAsync(T Value)
		{
			int PrevRevision = Value.Revision++;
			try
			{
				ReplaceOneResult Result = await Collection.ReplaceOneAsync(x => x.Id == ObjectId && x.Revision == PrevRevision, Value, new ReplaceOptions { IsUpsert = true });
				return Result.MatchedCount > 0;
			}
			catch (MongoWriteException Ex)
			{
				// Duplicate key error occurs if filter fails to match because revision is not the same.
				if (Ex.WriteError != null && Ex.WriteError.Category == ServerErrorCategory.DuplicateKey)
				{
					return false;
				}
				else
				{
					throw;
				}
			}
		}
	}
}
