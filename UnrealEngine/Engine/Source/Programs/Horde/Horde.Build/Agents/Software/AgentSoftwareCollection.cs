// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Agents.Software
{
	/// <summary>
	/// Collection of agent software documents
	/// </summary>
	public class AgentSoftwareCollection : IAgentSoftwareCollection
	{
		/// <summary>
		/// Information about agent software
		/// </summary>
		class AgentSoftwareDocument
		{
			[BsonId]
			public string Id { get; set; }

			[BsonIgnoreIfNull]
			public byte[]? Data { get; set; }

			[BsonIgnoreIfNull]
            public List<string>? ChunkIds { get; set; }

            [BsonConstructor]
			private AgentSoftwareDocument()
			{
				Id = null!;
				Data = null;
                ChunkIds = null;			
            }

			public AgentSoftwareDocument(string id, byte[]? data, List<string>? chunkIds = null)
			{
				Id = id;
				Data = data;
                ChunkIds = chunkIds;
            }
		}

		class ChunkDocument
		{
			[BsonId]
			public string Id { get; set; }

			public byte[] Data { get; set; } = null!;

			public string Version { get; set; }

            [BsonConstructor]
			private ChunkDocument()
			{
				Id = null!;
				Data = null!;
                Version = null!;
            }

			public ChunkDocument(string id, string version, byte[] data)
			{
				Id = id;
				Data = data;
                Version = version;
            }
		}

		// MongoDB document size limit is 16 megs, we chunk at this with a 1k buffer for anything else in document
		readonly int _chunkSize = 16 * 1024 * 1024 - 1024;

		/// <summary>
		/// Template documents
		/// </summary>
		readonly IMongoCollection<AgentSoftwareDocument> _collection;

		/// <summary>
		/// Agent template chunks
		/// </summary>
		readonly IMongoCollection<ChunkDocument> _agentChunks;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		public AgentSoftwareCollection(MongoService mongoService)
		{
			_collection = mongoService.GetCollection<AgentSoftwareDocument>("AgentSoftware");
			_agentChunks = mongoService.GetCollection<ChunkDocument>("AgentSoftwareChunks");
		}

		/// <inheritdoc/>
		public async Task<bool> AddAsync(string version, byte[] data)
		{

			if (await ExistsAsync(version))
			{
                return false;
            }

            AgentSoftwareDocument document;

            // Check if agent will fit in document, otherwise we need to chunk
            if (data.Length <= _chunkSize)
			{
				document = new AgentSoftwareDocument(version, data);
				await _collection.InsertOneAsync(document);
                return true;
            }

			int totalRead = 0;            
            byte[] buffer = new byte[_chunkSize];            
			List<ChunkDocument> chunks = new List<ChunkDocument>();
            using (MemoryStream chunkStream = new MemoryStream(data))
            {
                for (; ; )
                {
					int bytesRead = await chunkStream.ReadAsync(buffer, 0, _chunkSize);
					chunks.Add(new ChunkDocument(ObjectId.GenerateNewId().ToString(), version, buffer.Take(bytesRead).ToArray()));
					totalRead += bytesRead;
					if (totalRead == data.Length)
					{
						break;
					}
                }
            }

            // insert the chunks
            await _agentChunks.InsertManyAsync(chunks);

            document = new AgentSoftwareDocument(version, null, chunks.Select( chunk => chunk.Id).ToList());
			await _collection.InsertOneAsync(document);
            return true;
        }

		/// <inheritdoc/>
		public async Task<bool> ExistsAsync(string version)
		{
			return await _collection.Find(x => x.Id == version).AnyAsync();
		}

		/// <inheritdoc/>
		public async Task<bool> RemoveAsync(string version)
		{
			// Delete any chunks
			await _agentChunks.DeleteManyAsync(x => x.Version == version);

			DeleteResult result = await _collection.DeleteOneAsync(x => x.Id == version);			
			return result.DeletedCount > 0;
		}

		/// <inheritdoc/>
		public async Task<byte[]?> GetAsync(string version)
		{			
			IFindFluent<AgentSoftwareDocument, AgentSoftwareDocument> query = _collection.Find(x => x.Id == version);
			AgentSoftwareDocument? software = await query.FirstOrDefaultAsync();

			if (software == null)
			{
                return null;
            }

			if (software.Data != null && software.Data.Length > 0)
			{
                return software.Data;
            }

			if (software.ChunkIds != null)
			{
				FilterDefinitionBuilder<ChunkDocument> filterBuilder = Builders<ChunkDocument>.Filter;
				FilterDefinition<ChunkDocument> filter = filterBuilder.Empty;
				filter &= filterBuilder.In(x => x.Id, software.ChunkIds);

				IFindFluent<ChunkDocument, ChunkDocument> chunkQuery = _agentChunks.Find(x => software.ChunkIds.Contains(x.Id));				
				List<ChunkDocument> chunks = await chunkQuery.ToListAsync();

				using (MemoryStream dataStream = new MemoryStream())
				{
					foreach (string chunkId in software.ChunkIds)
					{
                        dataStream.Write(chunks.First(x => x.Id == chunkId).Data);
                    }

                    return dataStream.ToArray();
                }
			}

            return null;
        }
	}
}
