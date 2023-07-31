// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Datadog.Trace;
using Newtonsoft.Json;

namespace Callisto.Implementation
{
    public class FileTransactionLogIndex
    {
        [JsonProperty]
        private readonly Dictionary<string, int> _locationToIdentifier = new Dictionary<string, int>(StringComparer.InvariantCultureIgnoreCase);

        [JsonProperty]
        private int _nextLocation = 0; 

        [JsonIgnore]
        internal FileInfo? File { get; set; }

        [JsonProperty]
        public Guid LogGeneration { get; set; }

        [JsonProperty]
        public int Version { get; set; }

        [JsonIgnore]
        public List<string> Locations => _locationToIdentifier.Keys.ToList();

        // ReSharper disable once UnusedMember.Local
        public FileTransactionLogIndex()
        {
        }

        public FileTransactionLogIndex(FileInfo indexFile, int version = 1)
        {
            File = indexFile;
            // assign a new guid for this generation when we create the index file
            LogGeneration = Guid.NewGuid();
            Version = version;
            Save();
        }

        private void Save()
        {
            if (File == null)
            {
                throw new ArgumentNullException();
            }

            JsonSerializer js = JsonSerializer.CreateDefault();
            using FileStream fs = File.Open(FileMode.Create, FileAccess.Write, FileShare.None);
            using StreamWriter sw = new StreamWriter(fs);
            js.Serialize(sw, value: this);
        }

        public static FileTransactionLogIndex FromFile(FileInfo file)
        {
            JsonSerializer js = JsonSerializer.CreateDefault();
            using StreamReader sr = file.OpenText();
            using JsonTextReader jsonTextReader = new JsonTextReader(sr);
            FileTransactionLogIndex? index = (FileTransactionLogIndex?) js.Deserialize(jsonTextReader, objectType: typeof(FileTransactionLogIndex));
            index!.File = file;
            return index;
        }

        public int ToIndex(string location)
        {
            lock (_locationToIdentifier)
            {
                if (_locationToIdentifier.TryGetValue(location, value: out int index))
                {
                    return index;
                }

                using IScope _ = Tracer.Instance.StartActive("update.index");
                int newIndex = _nextLocation++;
                _locationToIdentifier.Add(location, newIndex);
                Save();
                return newIndex;
            }
        }

        public string NameOfIndex(in int i)
        {
            lock (_locationToIdentifier)
            {
                KeyValuePair<string, int>[] kvp = _locationToIdentifier.ToArray();
                foreach ((string key, int index) in kvp)
                {
                    if (index == i)
                    {
                        return key;
                    }
                }
            }

            throw new Exception($"Unable to map index {i} to a location");
        }
    }
}
