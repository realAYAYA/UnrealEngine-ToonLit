// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Security.Claims;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Acls
{
	/// <summary>
	/// Set of actions that can be performed by a user. NOTE: This enum is sensitive to ordering. Do not change values.
	/// </summary>
	public enum AclAction
	{
		//// PROJECTS ////

        #region Projects

        /// <summary>
        /// Allows the creation of new projects
        /// </summary>
        CreateProject,

		/// <summary>
		/// Allows deletion of projects.
		/// </summary>
		DeleteProject,
		
		/// <summary>
		/// Modify attributes of a project (name, categories, etc...)
		/// </summary>
		UpdateProject,

		/// <summary>
		/// View information about a project
		/// </summary>
		ViewProject,

        #endregion

		//// STREAMS ////

        #region Streams

        /// <summary>
        /// Allows the creation of new streams within a project
        /// </summary>
        CreateStream,

		/// <summary>
		/// Allows updating a stream (agent types, templates, schedules)
		/// </summary>
		UpdateStream,

		/// <summary>
		/// Allows deleting a stream
		/// </summary>
		DeleteStream,

		/// <summary>
		/// Ability to view a stream
		/// </summary>
		ViewStream,

		/// <summary>
		/// View changes submitted to a stream. NOTE: this returns responses from the server's Perforce account, which may be a priviledged user.
		/// </summary>
		ViewChanges,

		/// <summary>
		/// Update records in commit queues for a stream
		/// </summary>
		UpdateCommitQueues,

		/// <summary>
		/// View commit queues for a stream
		/// </summary>
		ViewCommitQueues,

        #endregion

		//// JOBS ////

        #region Jobs

        /// <summary>
        /// Ability to start new jobs
        /// </summary>
        CreateJob,

		/// <summary>
		/// Rename a job, modify its priority, etc...
		/// </summary>
		UpdateJob,

		/// <summary>
		/// Delete a job properties
		/// </summary>
		DeleteJob,

		/// <summary>
		/// Allows updating a job metadata (name, changelist number, step properties, new groups, job states, etc...). Typically granted to agents. Not user facing.
		/// </summary>
		ExecuteJob,

		/// <summary>
		/// Ability to retry a failed job step
		/// </summary>
		RetryJobStep,

		/// <summary>
		/// Ability to view a job
		/// </summary>
		ViewJob,

        #endregion

		//// EVENTS ////

        #region Events

        /// <summary>
        /// Ability to create events
        /// </summary>
        CreateEvent,

		/// <summary>
		/// Ability to view events
		/// </summary>
		ViewEvent,

        #endregion

		//// AGENTS ////

        #region Agents

        /// <summary>
        /// Ability to create an agent. This may be done explicitly, or granted to agents to allow them to self-register.
        /// </summary>
        CreateAgent,

		/// <summary>
		/// Update an agent's name, pools, etc...
		/// </summary>
		UpdateAgent,

		/// <summary>
		/// Soft-delete an agent
		/// </summary>
		DeleteAgent,

		/// <summary>
		/// View an agent
		/// </summary>
		ViewAgent,

		/// <summary>
		/// List the available agents
		/// </summary>
		ListAgents,

        #endregion

		//// POOLS ////

        #region Pools

        /// <summary>
        /// Create a global pool of agents
        /// </summary>
        CreatePool,

		/// <summary>
		/// Modify an agent pool
		/// </summary>
		UpdatePool,

		/// <summary>
		/// Delete an agent pool
		/// </summary>
		DeletePool,

		/// <summary>
		/// Ability to view a pool
		/// </summary>
		ViewPool,

		/// <summary>
		/// View all the available agent pools
		/// </summary>
		ListPools,

        #endregion

		//// SESSIONS ////

        #region Sessions

        /// <summary>
        /// Granted to agents to call CreateSession, which returns a bearer token identifying themselves valid to call UpdateSesssion via gRPC.
        /// </summary>
        CreateSession,

		/// <summary>
		/// Allows viewing information about an agent session
		/// </summary>
		ViewSession,

        #endregion

        //// CREDENTIALS ////

        #region Credentials

        /// <summary>
        /// Create a new credential
        /// </summary>
        CreateCredential,

		/// <summary>
		/// Delete a credential
		/// </summary>
		DeleteCredential,

		/// <summary>
		/// Modify an existing credential
		/// </summary>
		UpdateCredential,

		/// <summary>
		/// Enumerates all the available credentials
		/// </summary>
		ListCredentials,

		/// <summary>
		/// View a credential
		/// </summary>
		ViewCredential,

        #endregion

        //// LEASES ////

        #region Leases

        /// <summary>
        /// View all the leases that an agent has worked on
        /// </summary>
        ViewLeases,

        #endregion

        //// COMMITS ////

        #region Commits

        /// <summary>
        /// Add a new commit
        /// </summary>
        AddCommit,

		/// <summary>
		/// List all the commits that have been added
		/// </summary>
		FindCommits,

		/// <summary>
		/// View the commits for a particular stream
		/// </summary>
		ViewCommits,

        #endregion

        //// ISSUES ////

        #region Issues

        /// <summary>
        /// View a build health issue
        /// </summary>
        ViewIssue,

		/// <summary>
		/// Mark an issue as fixed via the p4fix endpoint
		/// </summary>
		IssueFixViaPerforce,

        #endregion

        //// TEMPLATES ////

        #region Templates

        /// <summary>
        /// View template associated with a stream
        /// </summary>
        ViewTemplate,

        #endregion

        //// LOGS ////

        #region Logs

        /// <summary>
        /// Ability to create a log. Implicitly granted to agents.
        /// </summary>
        CreateLog,

		/// <summary>
		/// Ability to update log metadata
		/// </summary>
		UpdateLog,

		/// <summary>
		/// Ability to view a log contents
		/// </summary>
		ViewLog,

		/// <summary>
		/// Ability to write log data
		/// </summary>
		WriteLogData,

        #endregion

        //// ARTIFACTS ////

        #region Artifacts

        /// <summary>
        /// Ability to create an artifact. Typically just for debugging; agents have this access for a particular session.
        /// </summary>
        UploadArtifact,

		/// <summary>
		/// Ability to download an artifact
		/// </summary>
		DownloadArtifact,

        #endregion

        //// SOFTWARE ////

        #region Software

        /// <summary>
        /// Ability to upload new versions of the agent software
        /// </summary>
        UploadSoftware,

		/// <summary>
		/// Ability to download the agent software
		/// </summary>
		DownloadSoftware,

		/// <summary>
		/// Ability to delete agent software
		/// </summary>
		DeleteSoftware,

		#endregion

		//// TOOLS ////

		#region Tools

		/// <summary>
		/// Ability to download a tool
		/// </summary>
		DownloadTool,

		/// <summary>
		/// Ability to upload new tool versions
		/// </summary>
		UploadTool,

		#endregion


		//// ADMIN ////

		#region Admin

		/// <summary>
		/// Ability to read any data from the server. Always inherited.
		/// </summary>
		AdminRead,

		/// <summary>
		/// Ability to write any data to the server.
		/// </summary>
		AdminWrite,

		/// <summary>
		/// Ability to impersonate another user
		/// </summary>
		Impersonate,

		/// <summary>
		/// View estimated costs for particular operations
		/// </summary>
		ViewCosts,

        #endregion

        //// PERMISSIONS ////

        #region Permissions

        /// <summary>
        /// Ability to view permissions on an object
        /// </summary>
        ViewPermissions,

		/// <summary>
		/// Ability to change permissions on an object
		/// </summary>
		ChangePermissions,

		/// <summary>
		/// Issue bearer token for the current user
		/// </summary>
		IssueBearerToken,

        #endregion

        //// NOTIFICATIONS ////

        #region Notifications

        /// <summary>
        /// Ability to subscribe to notifications
        /// </summary>
        CreateSubscription,

        #endregion

        //// DEVICES ////

        #region Devices

        /// <summary>
        /// Ability to read devices
        /// </summary>
        DeviceRead,

		/// <summary>
		/// Ability to write devices
		/// </summary>
		DeviceWrite,

        #endregion

        //// COMPUTE ////

        #region Compute

        /// <summary>
        /// User can add tasks to the compute cluster
        /// </summary>
        AddComputeTasks,

		/// <summary>
		/// User can poll for compute results
		/// </summary>
		ViewComputeTasks,

        #endregion

        //// STORAGE ////

        #region Storage

        /// <summary>
        /// Ability to read blobs from the storage service
        /// </summary>
        ReadBlobs,

		/// <summary>
		/// Ability to write blobs to the storage service
		/// </summary>
		WriteBlobs,

		/// <summary>
		/// Ability to read refs from the storage service
		/// </summary>
		ReadRefs,

		/// <summary>
		/// Ability to write refs to the storage service
		/// </summary>
		WriteRefs, 

		/// <summary>
		/// Ability to delete refs
		/// </summary>
		DeleteRefs,

        #endregion
    }

    #region V1

    /// <summary>
    /// Stores information about a claim
    /// </summary>
    public class AclClaim
	{
		/// <summary>
		/// The claim type, typically a URI
		/// </summary>
		public string Type { get; set; }

		/// <summary>
		/// The claim value
		/// </summary>
		public string Value { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="claim">The claim object</param>
		public AclClaim(Claim claim)
			: this(claim.Type, claim.Value)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The claim type</param>
		/// <param name="value">The claim value</param>
		public AclClaim(string type, string value)
		{
			Type = type;
			Value = value;
		}

		/// <summary>
		/// Constructs a claim from a request object
		/// </summary>
		/// <param name="request">The request object</param>
		public AclClaim(CreateAclClaimRequest request)
			: this(request.Type, request.Value)
		{
		}
	}

	/// <summary>
	/// Describes an entry in the ACL for a particular claim
	/// </summary>
	public class AclEntry
	{
		/// <summary>
		/// Claim for this entry
		/// </summary>
		public AclClaim Claim { get; set; }

		/// <summary>
		/// List of allowed operations
		/// </summary>
		[BsonSerializer(typeof(AclActionSetSerializer))]
		public HashSet<AclAction> Actions { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		[BsonConstructor]
		private AclEntry()
		{
			Claim = new AclClaim(String.Empty, String.Empty);
			Actions = new HashSet<AclAction>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="claim">The claim this entry applies to</param>
		/// <param name="actions">List of allowed operations</param>
		internal AclEntry(AclClaim claim, IEnumerable<AclAction> actions)
		{
			Claim = claim;
			Actions = new HashSet<AclAction>(actions);
		}

		/// <summary>
		/// Constructs an ACL entry from a request
		/// </summary>
		/// <param name="request">Request instance</param>
		/// <returns>New ACL entry</returns>
		public static AclEntry FromRequest(CreateAclEntryRequest request)
		{
			return new AclEntry(new AclClaim(request.Claim.Type, request.Claim.Value), ParseActionNames(request.Actions).ToArray());
		}

		/// <summary>
		/// Parses a list of names as an AclAction bitmask
		/// </summary>
		/// <param name="actionNames">Array of names</param>
		/// <returns>Action bitmask</returns>
		public static List<AclAction> ParseActionNames(string[]? actionNames)
		{
			List<AclAction> actions = new();
			if (actionNames != null)
			{
				foreach (string name in actionNames)
				{
					actions.Add(Enum.Parse<AclAction>(name, true));
				}
			}
			return actions;
		}

		/// <summary>
		/// Build a list of action names from an array of flags
		/// </summary>
		/// <param name="actions"></param>
		/// <returns></returns>
		public static List<string> GetActionNames(IEnumerable<AclAction> actions)
		{
            List<string> actionNames = new();
            foreach(AclAction action in actions)
			{
				string name = Enum.GetName(typeof(AclAction), action)!;
				actionNames.Add(name);
			}
			return actionNames;
		}
	}

	/// <summary>
	/// Represents an access control list for an object in the database
	/// </summary>
	public class Acl
	{
		/// <summary>
		/// List of entries for this ACL
		/// </summary>
		public List<AclEntry> Entries { get; set; }

		/// <summary>
		/// Whether to inherit permissions from the parent ACL by default
		/// </summary>
		public bool Inherit { get; set; } = true;

		/// <summary>
		/// Specifies a list of exceptions to the inheritance setting
		/// </summary>
		public List<AclAction>? Exceptions { get; set; }

		/// <summary>
		/// Default constructor
		/// </summary>
		public Acl()
		{
			Entries = new List<AclEntry>();
			Inherit = true;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="entries">List of entries for this ACL</param>
		/// <param name="inherit">Whether to inherit permissions from the parent object by default</param>
		public Acl(List<AclEntry> entries, bool inherit)
		{
			Entries = entries;
			Inherit = inherit;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="entries">List of entries for this ACL</param>
		public Acl(params AclEntry[] entries)
			: this(entries.ToList(), true)
		{
		}

		/// <summary>
		/// Tests whether a user is authorized to perform the given actions
		/// </summary>
		/// <param name="action">Action that is being performed. This should be a single flag.</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True/false if the action is allowed or denied, null if there is no specific setting for this user</returns>
		public bool? Authorize(AclAction action, ClaimsPrincipal user)
		{
			// Check if there's a specific entry for this action
			foreach (AclEntry entry in Entries)
			{
				if(entry.Actions.Contains(action) && user.HasClaim(entry.Claim.Type, entry.Claim.Value))
				{
					return true;
				}
			}

			// Otherwise check if we're prevented from inheriting permissions
			if(Inherit)
			{
				if(Exceptions != null && Exceptions.Contains(action))
				{
					return false;
				}
			}
			else
			{
				if (Exceptions == null || !Exceptions.Contains(action))
				{
					return false;
				}
			}

			// Otherwise allow to propagate up the hierarchy
			return null;
		}

		/// <summary>
		/// Merge new settings into an ACL
		/// </summary>
		/// <param name="baseAcl">The current acl</param>
		/// <param name="update">The update to apply</param>
		/// <returns>The new ACL value. Null if the ACL has all default settings.</returns>
		public static Acl? Merge(Acl? baseAcl, UpdateAclRequest? update)
		{
			Acl? newAcl = null;
			if(update != null)
			{
				newAcl = new Acl();

				if(update.Entries != null)
				{
					newAcl.Entries = update.Entries.ConvertAll(x => AclEntry.FromRequest(x));
				}
				else if(baseAcl != null)
				{
					newAcl.Entries = baseAcl.Entries;
				}

				if(update.Inherit != null)
				{
					newAcl.Inherit = update.Inherit.Value;
				}
				else if(baseAcl != null)
				{
					newAcl.Inherit = baseAcl.Inherit;
				}
			}
			return newAcl;
		}

		/// <summary>
		/// Creates an update definition for the given ACL. Clears the ACL property if it's null.
		/// </summary>
		/// <typeparam name="T">Type of document containing the ACL</typeparam>
		/// <param name="field">Selector for the ACL property</param>
		/// <param name="newAcl">The new ACL value</param>
		/// <returns>Update definition for the document</returns>
		public static UpdateDefinition<T> CreateUpdate<T>(Expression<Func<T, object>> field, Acl newAcl)
		{
			if (newAcl.Entries.Count == 0 && newAcl.Inherit)
			{
				return Builders<T>.Update.Unset(field);
			}
			else
			{
				return Builders<T>.Update.Set(field, newAcl);
			}
		}
	}

    #endregion

    #region V2

    /// <summary>
    /// Describes an entry in the ACL for a particular claim
    /// </summary>
    public class AclEntryV2 : IEquatable<AclEntryV2>
    {
        /// <summary>
        /// The claim type, typically a URI
        /// </summary>
        [BsonElement("t")]
        public string Type { get; set; }

        /// <summary>
        /// The claim value
        /// </summary>
        [BsonElement("v")]
        public string Value { get; set; }

        /// <summary>
        /// List of allowed operations
        /// </summary>
        [BsonElement("a")]
        [BsonSerializer(typeof(AclActionListSerializer))]
        public List<AclAction> Actions { get; set; }

        /// <summary>
        /// Private constructor for serialization
        /// </summary>
        [BsonConstructor]
        private AclEntryV2()
        {
            Type = String.Empty;
            Value = String.Empty;
            Actions = new List<AclAction>();
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="type">The claim type</param>
        /// <param name="value">The claim value</param>
        /// <param name="actions">List of allowed operations</param>
        public AclEntryV2(string type, string value, IEnumerable<AclAction> actions)
        {
            Type = type;
            Value = value;
            Actions = new List<AclAction>(actions);
        }

        /// <inheritdoc/>
        public override bool Equals(object? obj) => Equals(obj as AclEntryV2);

        /// <inheritdoc/>
        public override int GetHashCode() => throw new NotSupportedException();

        /// <inheritdoc/>
        public bool Equals(AclEntryV2? other) => other != null && String.Equals(Type, other.Type, StringComparison.Ordinal) && String.Equals(Value, other.Value, StringComparison.Ordinal) && Actions.SequenceEqual(other.Actions);
    }

    /// <summary>
    /// Represents an access control list for an object in the database
    /// </summary>
    public class AclV2 : IEquatable<AclV2>
    {
        /// <summary>
        /// List of entries for this ACL
        /// </summary>
        [BsonElement("al")]
        public List<AclEntryV2> Allow { get; set; } = new List<AclEntryV2>();

        /// <summary>
        /// Whether to inherit permissions from the parent ACL by default
        /// </summary>
        [BsonElement("in")]
        public bool Inherit { get; set; } = true;

        /// <summary>
        /// Specifies a list of exceptions to the inheritance setting
        /// </summary>
        [BsonElement("ex")]
        [BsonSerializer(typeof(AclActionListSerializer))]
        public List<AclAction> Exceptions { get; set; } = new List<AclAction>();

        /// <summary>
        /// Default constructor
        /// </summary>
        public AclV2()
        {
            Allow = new List<AclEntryV2>();
            Inherit = true;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="allow">List of entries for this ACL</param>
        /// <param name="inherit">Whether to inherit permissions from the parent object by default</param>
        public AclV2(IEnumerable<AclEntryV2> allow, bool inherit)
        {
            Allow.AddRange(allow);
            Inherit = inherit;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="allow">List of entries for this ACL</param>
        public AclV2(params AclEntryV2[] allow)
            : this(allow.ToList(), true)
        {
        }

        /// <summary>
        /// Checks whether the ACL is set to its default value
        /// </summary>
        /// <returns>True if the ACL is its default value</returns>
        public bool IsDefault() => Allow.Count == 0 && Inherit && Exceptions.Count == 0;

		/// <summary>
		/// Checks whether an ACL is null or set to its default value
		/// </summary>
		/// <returns>True if the ACL is its default value</returns>
		public static bool IsNullOrDefault(AclV2? acl) => acl is null || acl.IsDefault();

		/// <summary>
		/// Tests whether a user is authorized to perform the given actions
		/// </summary>
		/// <param name="action">Action that is being performed. This should be a single flag.</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True/false if the action is allowed or denied, null if there is no specific setting for this user</returns>
		public bool? Authorize(AclAction action, ClaimsPrincipal user)
        {
            // Check if there's a specific entry for this action
            foreach (AclEntryV2 entry in Allow)
            {
                if (entry.Actions.Contains(action) && user.HasClaim(entry.Type, entry.Value))
                {
                    return true;
                }
            }

            // Otherwise check if we're prevented from inheriting permissions
            if (Inherit)
            {
                if (Exceptions != null && Exceptions.Contains(action))
                {
                    return false;
                }
            }
            else
            {
                if (Exceptions == null || !Exceptions.Contains(action))
                {
                    return false;
                }
            }

            // Otherwise allow to propagate up the hierarchy
            return null;
        }

        /// <inheritdoc/>
        public override bool Equals(object? obj) => Equals(obj as AclV2);

        /// <inheritdoc/>
        public bool Equals(AclV2? other) => other != null && Allow.SequenceEqual(other.Allow) && Inherit == other.Inherit && Exceptions.SequenceEqual(other.Exceptions);

		/// <summary>
		/// Checks whether two acls are equal
		/// </summary>
		/// <param name="lhs"></param>
		/// <param name="rhs"></param>
		/// <returns></returns>
		public static bool Equals(AclV2? lhs, AclV2? rhs)
		{
			if (lhs is null)
			{
				return rhs is null || rhs.IsDefault();
			}
			else
			{
				return lhs.Equals(rhs);
			}
		}

        /// <inheritdoc/>
        public override int GetHashCode() => throw new NotSupportedException();

        /// <summary>
        /// Register this type's class map
        /// </summary>
        internal static void ConfigureClassMap(BsonClassMap<AclV2> cm)
        {
            cm.AutoMap();
            cm.MapMember(x => x.Allow).SetIgnoreIfDefault(true).SetDefaultValue(new List<AclEntryV2>());
            cm.MapMember(x => x.Exceptions).SetIgnoreIfDefault(true).SetDefaultValue(new List<AclAction>());
        }
    }

    #endregion

    /// <summary>
    /// Serializer for a list of unique values
    /// </summary>
    public sealed class AclActionListSerializer : IBsonSerializer<List<AclAction>>
    {
        /// <inheritdoc/>
        public Type ValueType => typeof(List<AclAction>);

        /// <inheritdoc/>
        void IBsonSerializer.Serialize(BsonSerializationContext context, BsonSerializationArgs args, object value) => Serialize(context, args, (List<AclAction>)value);

        /// <inheritdoc/>
        object IBsonSerializer.Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args) => ((IBsonSerializer<List<AclAction>>)this).Deserialize(context, args);

        /// <inheritdoc/>
        public List<AclAction> Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
        {
            List<AclAction> values = new();

            context.Reader.ReadStartArray();
            for (; ; )
            {
                BsonType type = context.Reader.ReadBsonType();
                if (type == BsonType.EndOfDocument)
                {
                    break;
                }
                else
                {
                    values.Add((AclAction)Enum.Parse(typeof(AclAction), context.Reader.ReadString()));
                }
            }
            context.Reader.ReadEndArray();

            return values;
        }

        /// <inheritdoc/>
        public void Serialize(BsonSerializationContext context, BsonSerializationArgs args, List<AclAction> values)
        {
            context.Writer.WriteStartArray();
            if (values.Count > 0)
            {
                for (int idx = 0; idx < values.Count; idx++)
                {
                    AclAction value = values[idx];
                    if (values.IndexOf(value) == idx)
                    {
                        context.Writer.WriteString(value.ToString());
                    }
                }
            }
            context.Writer.WriteEndArray();
        }
    }

    /// <summary>
    /// Serializer for JobStepRefId objects
    /// </summary>
    public sealed class AclActionSetSerializer : IBsonSerializer<HashSet<AclAction>>
	{
		/// <inheritdoc/>
		public Type ValueType => typeof(HashSet<AclAction>);

		/// <inheritdoc/>
		void IBsonSerializer.Serialize(BsonSerializationContext context, BsonSerializationArgs args, object value)
		{
			Serialize(context, args, (HashSet<AclAction>)value);
		}

		/// <inheritdoc/>
		object IBsonSerializer.Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			return ((IBsonSerializer<HashSet<AclAction>>)this).Deserialize(context, args);
		}

		/// <inheritdoc/>
		public HashSet<AclAction> Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
            HashSet<AclAction> values = new();

			context.Reader.ReadStartArray();
			for(; ;)
			{
				BsonType type = context.Reader.ReadBsonType();
				if(type == BsonType.EndOfDocument)
				{
					break;
				}
				else
				{
					values.Add((AclAction)Enum.Parse(typeof(AclAction), context.Reader.ReadString()));
				}
			}
			context.Reader.ReadEndArray();

			return values;
		}

		/// <inheritdoc/>
		public void Serialize(BsonSerializationContext context, BsonSerializationArgs args, HashSet<AclAction> values)
		{
			context.Writer.WriteStartArray();
			foreach (AclAction value in values)
			{
				context.Writer.WriteString(value.ToString());
			}
			context.Writer.WriteEndArray();
		}
	}
}
