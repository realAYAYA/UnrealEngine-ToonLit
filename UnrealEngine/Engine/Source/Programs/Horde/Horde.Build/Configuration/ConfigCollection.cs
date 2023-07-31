// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Build.Server;
using Microsoft.Extensions.Caching.Memory;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using System;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace Horde.Build.Configuration
{
	/// <summary>
	/// Collection of config documents
	/// </summary>
	public class ConfigCollection
	{
		class ConfigDoc
		{
			[BsonId]
			public string Id { get; set; } = String.Empty;

			[BsonElement("dat")]
			public byte[] Data { get; set; } = Array.Empty<byte>();
		}

		private readonly IMongoCollection<ConfigDoc> _configs;
		private readonly IMemoryCache _memoryCache;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConfigCollection(MongoService mongoService, IMemoryCache memoryCache)
		{
			_configs = mongoService.GetCollection<ConfigDoc>("Configs");
			_memoryCache = memoryCache;
		}

		static string GetDocumentCacheKey(string revision) => $"config:{revision}";
		static string GetTypedCacheKey<T>(string revision) => $"config-typed:{nameof(T)}:{revision}";

		/// <summary>
		/// Adds a config document with a particular revision
		/// </summary>
		/// <param name="revision"></param>
		/// <param name="data"></param>
		/// <returns>Identifier for the config data</returns>
		public async Task AddConfigDataAsync(string revision, ReadOnlyMemory<byte> data)
		{
			ConfigDoc config = new ConfigDoc();
			config.Id = revision;
			config.Data = data.ToArray();
			await _configs.ReplaceOneAsync(x => x.Id == revision, config, new ReplaceOptions { IsUpsert = true });

			AddConfigDataToCache(revision, config.Data);
		}

		/// <summary>
		/// Gets raw config data with the given revision number
		/// </summary>
		/// <param name="revision">The revision number</param>
		/// <returns>Raw config data with the given id</returns>
		public async ValueTask<ReadOnlyMemory<byte>> GetConfigDataAsync(string revision)
		{
			string cacheKey = GetDocumentCacheKey(revision);
			if (!_memoryCache.TryGetValue(cacheKey, out ReadOnlyMemory<byte> data))
			{
				ConfigDoc config = await _configs.Find(x => x.Id == revision).FirstAsync();
				using (ICacheEntry entry = _memoryCache.CreateEntry(GetDocumentCacheKey(config.Id)))
				{
					entry.SetValue(config);
					entry.SetSize(config.Data.Length);
				}
				data = config.Data;
			}
			return data;
		}

		/// <summary>
		/// Adds config data for a given revision to the store, and returns the value of it parsed as a JSON object
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="revision"></param>
		/// <param name="config"></param>
		/// <returns></returns>
		public async Task AddConfigAsync<T>(string revision, T config)
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(options);

			byte[] data = JsonSerializer.SerializeToUtf8Bytes(config, options);
			await AddConfigDataAsync(revision, data);
		}

		/// <summary>
		/// Gets typed config data with a particular id
		/// </summary>
		/// <typeparam name="T">Type of data to return</typeparam>
		/// <param name="revision">The unique revision string</param>
		/// <returns>Parsed config data for the given revision number</returns>
		public async ValueTask<T> GetConfigAsync<T>(string revision)
		{
			string typedCacheKey = GetTypedCacheKey<T>(revision);
			if (!_memoryCache.TryGetValue(typedCacheKey, out T config))
			{
				ReadOnlyMemory<byte> data = await GetConfigDataAsync(revision);

				JsonSerializerOptions options = new JsonSerializerOptions();
				Startup.ConfigureJsonSerializer(options);
				config = JsonSerializer.Deserialize<T>(data.Span, options)!;

				using (ICacheEntry entry = _memoryCache.CreateEntry(typedCacheKey))
				{
					entry.SetValue(config);
					entry.SetSize(data.Length);
				}
			}
			return config;
		}

		void AddConfigDataToCache(string revision, ReadOnlyMemory<byte> data)
		{
			using (ICacheEntry entry = _memoryCache.CreateEntry(GetDocumentCacheKey(revision)))
			{
				entry.SetValue(data);
				entry.SetSize(data.Length);
			}
		}
	}
}
