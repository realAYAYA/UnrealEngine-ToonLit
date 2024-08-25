// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Jobs.Artifacts
{
	/// <summary>
	/// Wraps functionality for manipulating artifacts
	/// </summary>
	public class ArtifactCollectionV1 : IArtifactCollectionV1
	{
		class Artifact : IArtifactV1
		{
			[BsonRequired, BsonId]
			public ObjectId Id { get; set; }

			[BsonRequired]
			public JobId JobId { get; set; }

			[BsonRequired]
			public string Name { get; set; }

			public JobStepId? StepId { get; set; }

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

			public Artifact(JobId jobId, JobStepId? stepId, string name, long length, string mimeType)
			{
				Id = ObjectId.GenerateNewId();
				JobId = jobId;
				StepId = stepId;
				Name = name;
				Length = length;
				MimeType = mimeType;
			}
		}

		private readonly IObjectStore _objectStore;
		private readonly IMongoCollection<Artifact> _artifacts;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactCollectionV1(MongoService mongoService, IObjectStore<ArtifactCollectionV1> objectStore)
		{
			_objectStore = objectStore;

			// Initialize Artifacts table
			_artifacts = mongoService.GetCollection<Artifact>("Artifacts", keys => keys.Ascending(x => x.JobId));
		}

		/// <inheritdoc/>
		public async Task<IArtifactV1> CreateArtifactAsync(JobId jobId, JobStepId? stepId, string name, string mimeType, System.IO.Stream data, CancellationToken cancellationToken)
		{
			// upload first
			string artifactName = ValidateName(name);
			await _objectStore.WriteAsync(GetObjectKey(jobId, stepId, artifactName), data, cancellationToken);

			// then create entry
			Artifact newArtifact = new Artifact(jobId, stepId, artifactName, data.Length, mimeType);
			await _artifacts.InsertOneAsync(newArtifact, (InsertOneOptions?)null, cancellationToken);
			return newArtifact;
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IArtifactV1>> GetArtifactsAsync(JobId? jobId, JobStepId? stepId, string? name, CancellationToken cancellationToken)
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

			return await _artifacts.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IArtifactV1>> GetArtifactsAsync(IEnumerable<ObjectId> artifactIds, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<Artifact> builder = Builders<Artifact>.Filter;

			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			filter &= builder.In(x => x.Id, artifactIds);

			return await _artifacts.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IArtifactV1?> GetArtifactAsync(ObjectId artifactId, CancellationToken cancellationToken)
		{
			return await _artifacts.Find<Artifact>(x => x.Id == artifactId).FirstOrDefaultAsync(cancellationToken);
		}

		/// <summary>
		/// Attempts to update a single artifact document, if the update counter is valid
		/// </summary>
		/// <param name="current">The current artifact document</param>
		/// <param name="update">The update definition</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the document was updated, false if another writer updated the document first</returns>
		private async Task<bool> TryUpdateArtifactAsync(Artifact current, UpdateDefinition<Artifact> update, CancellationToken cancellationToken)
		{
			UpdateResult result = await _artifacts.UpdateOneAsync<Artifact>(x => x.Id == current.Id && x.UpdateIndex == current.UpdateIndex, update.Set(x => x.UpdateIndex, current.UpdateIndex + 1), cancellationToken: cancellationToken);
			return result.ModifiedCount == 1;
		}

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="newMimeType">New mime type</param>
		/// <param name="newData">New data</param>
		/// <param name="cancellationToken"></param>
		/// <returns>Async task</returns>
		public async Task<bool> UpdateArtifactAsync(IArtifactV1? artifact, string newMimeType, System.IO.Stream newData, CancellationToken cancellationToken)
		{
			while (artifact != null)
			{
				UpdateDefinitionBuilder<Artifact> updateBuilder = Builders<Artifact>.Update;

				List<UpdateDefinition<Artifact>> updates = new List<UpdateDefinition<Artifact>>();
				updates.Add(updateBuilder.Set(x => x.MimeType, newMimeType));
				updates.Add(updateBuilder.Set(x => x.Length, newData.Length));

				// re-upload the data to external
				string artifactName = ValidateName(artifact.Name);
				await _objectStore.WriteAsync(GetObjectKey(artifact.JobId, artifact.StepId, artifactName), newData, cancellationToken);

				if (await TryUpdateArtifactAsync((Artifact)artifact, updateBuilder.Combine(updates), cancellationToken))
				{
					return true;
				}

				artifact = await GetArtifactAsync(artifact.Id, cancellationToken);
			}
			return false;
		}

		/// <summary>
		/// gets artifact data
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The chunk data</returns>
		public async Task<System.IO.Stream> OpenArtifactReadStreamAsync(IArtifactV1 artifact, CancellationToken cancellationToken)
		{
			System.IO.Stream stream = await _objectStore.OpenAsync(GetObjectKey(artifact.JobId, artifact.StepId, artifact.Name), cancellationToken);
			return stream;
		}

		/// <summary>
		/// Get the path for an artifact
		/// </summary>
		/// <param name="jobId"></param>
		/// <param name="stepId"></param>
		/// <param name="name"></param>
		/// <returns></returns>
		private static ObjectKey GetObjectKey(JobId jobId, JobStepId? stepId, string name)
		{
			if (stepId == null)
			{
				return new ObjectKey(new Utf8String($"{jobId}/{name}.blob"), ObjectKey.Validate.None);
			}
			else
			{
				return new ObjectKey(new Utf8String($"{jobId}/{stepId.Value}/{name}.blob"), ObjectKey.Validate.None);
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