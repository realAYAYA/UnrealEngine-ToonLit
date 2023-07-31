// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using JsonSubTypes;
using Newtonsoft.Json;

namespace Jupiter.Implementation
{
    public class AddTransactionEvent : TransactionEvent
    {
        [Required] public BlobIdentifier[] Blobs { get; }

        public Dictionary<string, object>? Metadata { get; }

        [JsonConstructor]
        public AddTransactionEvent(string name, string bucket, BlobIdentifier[] blobs, Dictionary<string, object>? metadata = null, long? identifier = null, long? nextIdentifier = null) 
            : base("Add", identifier, nextIdentifier, name, bucket)
        {
            Blobs = blobs;
            Metadata = metadata;
        }

        public AddTransactionEvent(string name, string bucket, BlobIdentifier[] blobs, List<string> locations, Dictionary<string, object>? metadata = null, long? identifier = null, long? nextIdentifier = null)
            : base("Add", identifier, nextIdentifier, name, bucket, locations)
        {
            Blobs = blobs;
            Metadata = metadata;
        }
    }

    public class RemoveTransactionEvent : TransactionEvent
    {
        [JsonConstructor]
        public RemoveTransactionEvent(string name, string bucket, long? identifier = null, long? nextIdentifier = null) 
            : base("Remove", identifier, nextIdentifier, name, bucket)
        {
        }

        public RemoveTransactionEvent(string name, string bucket, List<string> locations, long? identifier = null, long? nextIdentifier = null)
            : base("Remove", identifier, nextIdentifier, name, bucket, locations)
        {
        }
    }

    public interface IReplicationEvent
    {
        public long? Identifier { get; }
        public long? NextIdentifier { get; }
    }

    [JsonConverter(typeof(JsonSubtypes), "type")]
    [JsonSubtypes.KnownSubType(typeof(AddTransactionEvent), "Add")]
    [JsonSubtypes.KnownSubType(typeof(RemoveTransactionEvent), "Remove")]
    public abstract class TransactionEvent : IReplicationEvent
    {
        /// <summary>
        /// Constructor for use by the controller
        /// </summary>
        /// <param name="type">The type of transaction event, either add or remove</param>
        /// <param name="identifier">The starting offset for this event in the log</param>
        /// <param name="nextIdentifier">The end offset of this event (can be used to fetch the next event)</param>
        /// <param name="name">The name of the ref key that is being added for this event</param>
        /// <param name="bucket">The bucket identifier of this event</param>
        /// <param name="locations">Optional specification of which locations this event has been seen in</param>
        protected TransactionEvent(string type, long? identifier, long? nextIdentifier, string name, string bucket, List<string>? locations = null)
        {
            Type = type;
            Identifier = identifier;
            NextIdentifier = nextIdentifier;
            Name = name;
            Bucket = bucket;
            Locations = locations ?? new List<string>();
        }

        public string Type { get; }
        public long? Identifier { get; }
        public long? NextIdentifier { get; }

        [Required]
        public string Name { get; }
        [Required]
        public string Bucket { get; }

        public List<string> Locations { get; }
    }
}
