// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;

namespace Jupiter.Implementation;

public class ScyllaSettings : IValidatableObject
{
    public string ConnectionString { get; set; } = "Contact Points=localhost,scylla";
    public long InlineBlobMaxSize { get; set; } = 32 * 1024; // default to 32 kb blobs max
    public int MaxSnapshotsPerNamespace { get; set; } = 10;

    /// <summary>
    /// Set to override the replication strategy used to create keyspace
    /// Note that this only applies when creating the keyspace
    /// To modify a existing keyspace see https://docs.datastax.com/en/dse/6.7/dse-admin/datastax_enterprise/operations/opsChangeKSStrategy.html
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
    public Dictionary<string, string>? KeyspaceReplicationStrategy { get; set; }

    /// <summary>
    /// Set to override the replication strategy used to create the local keyspace
    /// Note that this only applies when creating the keyspace
    /// To modify a existing keyspace see https://docs.datastax.com/en/dse/6.7/dse-admin/datastax_enterprise/operations/opsChangeKSStrategy.html
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
    public Dictionary<string, string>? LocalKeyspaceReplicationStrategy { get; set; }

    /// <summary>
    /// Used to configure the load balancing policy to stick to this specified datacenter
    /// </summary>
    [Required]
    public string LocalDatacenterName { get; set; } = null!;
        
    [Required] 
    public string LocalKeyspaceSuffix { get; set; } = null!;

    /// <summary>
    /// Max number of connections for each scylla host before switching to another host
    /// See https://docs.datastax.com/en/developer/nodejs-driver/4.6/features/connection-pooling/
    /// </summary>
    public int MaxConnectionForLocalHost { get; set; } = 8192;

    /// <summary>
    /// The time for a replication log event to live in the incremental state before being deleted, assumption is that the snapshot will have processed the event within this time
    /// </summary>
    public TimeSpan ReplicationLogTimeToLive { get; set; } = TimeSpan.FromDays(7);

    /// <summary>
    /// Whether a Cosmos DB Cassandra layer from Azure should be used.
    /// </summary>
    public bool UseAzureCosmosDB { get; set; } = false;

    /// <summary>
    /// Set to false to disable use of SSL, this is only used when connecting to a CosmosDB instance
    /// </summary>
    public bool UseSSL { get; set; } = true;

    /// <summary>
    /// Read timeout in milliseconds
    /// Set to -1 to get the default timeout, set to 0 to disable timeouts
    /// </summary>
    public int ReadTimeout { get; set; } = -1; // use the default timeout value by default

    /// <summary>
    /// Enable to scan database per shard, this requires setting more information about your cluster
    /// </summary>
    public bool UsePerShardScanning { get; set; } = false;

    /// <summary>
    /// Count of cores each nodes has, only used when UsePerShardScanning is set
    /// </summary>
    [Range(1, 128)]
    public uint CountOfCoresPerNode { get; set; } = 1;

    /// <summary>
    /// Count of nodes in the DC, only used when UsePerShardScanning is set
    /// </summary>
    [Range(1, 64)]
    public uint CountOfNodes { get; set; } = 1;

    public IEnumerable<ValidationResult> Validate(ValidationContext validationContext)
    {
        List<ValidationResult> results = new List<ValidationResult>();

        if (UsePerShardScanning)
        {
            Validator.TryValidateProperty(CountOfNodes, new ValidationContext(this, null, null) { MemberName = nameof(CountOfNodes) }, results);
            Validator.TryValidateProperty(CountOfCoresPerNode, new ValidationContext(this, null, null) { MemberName = nameof(CountOfCoresPerNode) }, results);
        }
        return results;
    }
}
