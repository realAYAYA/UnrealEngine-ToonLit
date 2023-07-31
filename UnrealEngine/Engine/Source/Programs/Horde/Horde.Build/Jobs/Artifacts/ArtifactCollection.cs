// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Server;
using Horde.Build.Storage;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Jobs.Artifacts
{
	using JobId = ObjectId<IJob>;

	/// <summary>
	/// Wraps functionality for manipulating artifacts
	/// </summary>
	public class ArtifactCollection : IArtifactCollection
	{
		class Artifact : IArtifact
		{
			[BsonRequired, BsonId]
			public ObjectId Id { get; set; }

			[BsonRequired]
			public JobId JobId { get; set; }

			[BsonRequired]
			public string Name { get; set; }

			public SubResourceId? StepId { get; set; }

			[BsonRequired]
			public long Length { get; set; }

			public string MimeType { get; set; }

			[BsonRequired]
			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private Artifact()
			{
				Name = null!;
				MimeType = null!;
			}

			public Artifact(JobId jobId, SubResourceId? stepId, string name, long length, string mimeType)
			{
				Id = ObjectId.GenerateNewId();
				JobId = jobId;
				StepId = stepId;
				Name = name;
				Length = length;
				MimeType = mimeType;
			}
		}

		private readonly IStorageBackend _storageBackend;
		private readonly IMongoCollection<Artifact> _artifacts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		/// <param name="storageBackend">The storage backend</param>
		public ArtifactCollection(MongoService mongoService, IStorageBackend<ArtifactCollection> storageBackend)
		{
			_storageBackend = storageBackend;

			// Initialize Artifacts table
			_artifacts = mongoService.GetCollection<Artifact>("Artifacts", keys => keys.Ascending(x => x.JobId));
		}

		/// <summary>
		/// Creates a new artifact
		/// </summary>
		/// <param name="jobId">Unique id of the job that owns this artifact</param>
		/// <param name="stepId">Optional Step id</param>
		/// <param name="name">Name of artifact</param>
		/// <param name="mimeType">Type of artifact</param>
		/// <param name="data">The data to write</param>
		/// <returns>The new log file document</returns>
		public async Task<IArtifact> CreateArtifactAsync(JobId jobId, SubResourceId? stepId, string name, string mimeType, System.IO.Stream data)
		{
			// upload first
			string artifactName = ValidateName(name);
			await _storageBackend.WriteAsync(GetPath(jobId, stepId, artifactName), data);

			// then create entry
			Artifact newArtifact = new Artifact(jobId, stepId, artifactName, data.Length, mimeType);
			await _artifacts.InsertOneAsync(newArtifact);
			return newArtifact;
		}

		/// <summary>
		/// Gets all the available artifacts for a job
		/// </summary>
		/// <param name="jobId">Unique id of the job to query</param>
		/// <param name="stepId">Unique id of the Step to query</param>
		/// <param name="name">Name of the artifact</param>
		/// <returns>List of artifact documents</returns>
		public async Task<List<IArtifact>> GetArtifactsAsync(JobId? jobId, SubResourceId? stepId, string? name)
		{
			FilterDefinitionBuilder<Artifact> builder = Builders<Artifact>.Filter;

			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			if (jobId != null)
			{
				filter &= builder.Eq(x => x.JobId, jobId.Value);
			}
			if (stepId != null)
			{
				filter &= builder.Eq(x => x.StepId, stepId.Value);
			}
			if (name != null)
			{
				filter &= builder.Eq(x => x.Name, name);
			}

			return await _artifacts.Find(filter).ToListAsync<Artifact, IArtifact>();
		}

		/// <summary>
		/// Gets a specific list of artifacts based on id
		/// </summary>
		/// <param name="artifactIds">The list of artifact Ids</param>
		/// <returns>List of artifact documents</returns>
		public async Task<List<IArtifact>> GetArtifactsAsync(IEnumerable<ObjectId> artifactIds)
		{
			FilterDefinitionBuilder<Artifact> builder = Builders<Artifact>.Filter;

			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			filter &= builder.In(x => x.Id, artifactIds);
			
			return await _artifacts.Find(filter).ToListAsync<Artifact, IArtifact>();
		}

		/// <summary>
		/// Gets an artifact by ID
		/// </summary>
		/// <param name="artifactId">Unique id of the artifact</param>
		/// <returns>The artifact document</returns>
		public async Task<IArtifact?> GetArtifactAsync(ObjectId artifactId)
		{
			return await _artifacts.Find<Artifact>(x => x.Id == artifactId).FirstOrDefaultAsync();
		}

		/// <summary>
		/// Attempts to update a single artifact document, if the update counter is valid
		/// </summary>
		/// <param name="current">The current artifact document</param>
		/// <param name="update">The update definition</param>
		/// <returns>True if the document was updated, false if another writer updated the document first</returns>
		private async Task<bool> TryUpdateArtifactAsync(Artifact current, UpdateDefinition<Artifact> update)
		{
			UpdateResult result = await _artifacts.UpdateOneAsync<Artifact>(x => x.Id == current.Id && x.UpdateIndex == current.UpdateIndex, update.Set(x => x.UpdateIndex, current.UpdateIndex + 1));
			return result.ModifiedCount == 1;
		}

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="newMimeType">New mime type</param>
		/// <param name="newData">New data</param>
		/// <returns>Async task</returns>
		public async Task<bool> UpdateArtifactAsync(IArtifact? artifact, string newMimeType, System.IO.Stream newData)
		{
			while (artifact != null)
			{
				UpdateDefinitionBuilder<Artifact> updateBuilder = Builders<Artifact>.Update;

				List<UpdateDefinition<Artifact>> updates = new List<UpdateDefinition<Artifact>>();
				updates.Add(updateBuilder.Set(x => x.MimeType, newMimeType));
				updates.Add(updateBuilder.Set(x => x.Length, newData.Length));

				// re-upload the data to external
				string artifactName = ValidateName(artifact.Name);
				await _storageBackend.WriteAsync(GetPath(artifact.JobId, artifact.StepId, artifactName), newData);

				if (await TryUpdateArtifactAsync((Artifact)artifact, updateBuilder.Combine(updates)))
				{
					return true;
				}

				artifact = await GetArtifactAsync(artifact.Id);
			}
			return false;
		}

		/// <summary>
		/// gets artifact data
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <returns>The chunk data</returns>
		public async Task<System.IO.Stream> OpenArtifactReadStreamAsync(IArtifact artifact)
		{
			System.IO.Stream? stream = await _storageBackend.ReadAsync(GetPath(artifact.JobId, artifact.StepId, artifact.Name));
			if (stream == null)
			{
				throw new Exception($"Unable to get artifact {artifact.Id}");
			}
			return stream;
		}

		/// <summary>
		/// Get the path for an artifact
		/// </summary>
		/// <param name="jobId"></param>
		/// <param name="stepId"></param>
		/// <param name="name"></param>
		/// <returns></returns>
		private static string GetPath(JobId jobId, SubResourceId? stepId, string name)
		{
			if (stepId == null)
			{
				return $"{jobId}/{name}";
			}
			else
			{
				return $"{jobId}/{stepId.Value}/{name}";
			}
		}

		/// <summary>
		/// Checks that the name given is valid for an artifact
		/// </summary>
		/// <param name="name"></param>
		private static string ValidateName(string name)
		{
			string newName = name.Replace('\\', '/');
			if (newName.Length == 0)
			{
				throw new ArgumentException("Artifact name is empty");
			}
			else if (newName.StartsWith('/'))
			{
				throw new ArgumentException($"Artifact has an absolute path ({newName})");
			}
			else if (newName.EndsWith('/'))
			{
				throw new ArgumentException($"Artifact does not have a file name ({newName})");
			}
			else if (newName.Contains("//", StringComparison.Ordinal))
			{
				throw new ArgumentException($"Artifact name contains an invalid directory ({newName})");
			}

			string invalidChars = ":<>|\"";
			for (int idx = 0; idx < newName.Length; idx++)
			{
				int minIdx = idx;
				for (; idx < newName.Length && newName[idx] != '/'; idx++)
				{
					if (invalidChars.Contains(newName[idx], StringComparison.Ordinal))
					{
						throw new ArgumentException($"Invalid character in artifact name ({newName})");
					}
				}
				if ((idx == minIdx + 1 && newName[minIdx] == '.') || (idx == minIdx + 2 && newName[minIdx] == '.' && newName[minIdx + 1] == '.'))
				{
					throw new ArgumentException($"Artifact may not contain symbolic directory names ({newName})");
				}
			}
			return newName;
		}
	}
}