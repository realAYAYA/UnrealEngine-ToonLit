// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Jobs.Templates;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;
using MongoDB.Driver;

namespace Horde.Build.Jobs.Graphs
{
	/// <summary>
	/// Collection of graph documents
	/// </summary>
	public class GraphCollection : IGraphCollection
	{
		/// <summary>
		/// Represents a node in the graph
		/// </summary>
		[DebuggerDisplay("{Name}")]
		class Node : INode
		{
			[BsonRequired]
			public string Name { get; set; }

			[BsonIgnoreIfNull]
			public NodeRef[] InputDependencies { get; set; }

			[BsonIgnoreIfNull]
			public NodeRef[] OrderDependencies { get; set; }

			public Priority Priority { get; set; }
			public bool AllowRetry { get; set; } = true;
			public bool RunEarly { get; set; }
			public bool Warnings { get; set; } = true;

			[BsonIgnoreIfNull]
			public Dictionary<string, string>? Credentials { get; set; }

			[BsonIgnoreIfNull]
			public Dictionary<string, string>? Properties { get; set; }

			[BsonIgnoreIfNull]
			[BsonDictionaryOptions(DictionaryRepresentation.ArrayOfDocuments)]
			public NodeAnnotations? Annotations { get; set; }

			IReadOnlyDictionary<string, string>? INode.Credentials => Credentials;
			IReadOnlyDictionary<string, string>? INode.Properties => Properties;
			IReadOnlyNodeAnnotations INode.Annotations => Annotations ?? NodeAnnotations.Empty;

			[BsonConstructor]
			private Node()
			{
				Name = null!;
				InputDependencies = null!;
				OrderDependencies = null!;
			}

			public Node(string name, NodeRef[] inputDependencies, NodeRef[] orderDependencies, Priority priority, bool allowRetry, bool runEarly, bool warnings, Dictionary<string, string>? credentials, Dictionary<string, string>? properties, IReadOnlyNodeAnnotations? annotations)
			{
				Name = name;
				InputDependencies = inputDependencies;
				OrderDependencies = orderDependencies;
				Priority = priority;
				AllowRetry = allowRetry;
				RunEarly = runEarly;
				Warnings = warnings;
				Credentials = credentials;
				Properties = properties;
				if (annotations != null && annotations.Count > 0)
				{
					Annotations = new NodeAnnotations(annotations);
				}
			}
		}

		class NodeGroup : INodeGroup
		{
			public string AgentType { get; set; }
			public List<Node> Nodes { get; set; }

			IReadOnlyList<INode> INodeGroup.Nodes => Nodes;

			/// <summary>
			/// Private constructor for BSON serializer
			/// </summary>
			[BsonConstructor]
			private NodeGroup()
			{
				AgentType = null!;
				Nodes = null!;
			}

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="agentType">The type of agent to execute this group</param>
			/// <param name="nodes">Nodes to execute</param>
			public NodeGroup(string agentType, List<Node> nodes)
			{
				AgentType = agentType;
				Nodes = nodes;
			}
		}

		class Aggregate : IAggregate
		{
			public string Name { get; set; }
			public List<NodeRef> Nodes { get; set; }

			IReadOnlyList<NodeRef> IAggregate.Nodes => Nodes;

			private Aggregate()
			{
				Name = null!;
				Nodes = new List<NodeRef>();
			}

			public Aggregate(string name, List<NodeRef> nodes)
			{
				Name = name;
				Nodes = nodes;
			}
		}

		class Label : ILabel
		{
			[BsonIgnoreIfNull]
			public string? Name { get; set; }

			[BsonIgnoreIfNull]
			public string? Category { get; set; }

			[BsonIgnoreIfNull]
			public string? DashboardName { get; set; }

			[BsonIgnoreIfNull]
			public string? DashboardCategory { get; set; }

			[BsonIgnoreIfNull]
			public string? UgsName { get; set; }

			[BsonIgnoreIfNull]
			public string? UgsProject { get; set; }

			[BsonIgnoreIfNull]
			public LabelChange Change { get; set; }

			public List<NodeRef> RequiredNodes { get; set; }
			public List<NodeRef> IncludedNodes { get; set; }

			string? ILabel.DashboardName => DashboardName ?? Name;
			string? ILabel.DashboardCategory => DashboardCategory ?? Category;

			private Label()
			{
				RequiredNodes = new List<NodeRef>();
				IncludedNodes = new List<NodeRef>();
			}

			public Label(string? dashboardName, string? dashboardCategory, string? ugsName, string? ugsProject, LabelChange change, List<NodeRef> requiredNodes, List<NodeRef> includedNodes)
			{
				DashboardName = dashboardName;
				DashboardCategory = dashboardCategory;
				UgsName = ugsName;
				UgsProject = ugsProject;
				Change = change;
				RequiredNodes = requiredNodes;
				IncludedNodes = includedNodes;
			}
		}

		class GraphDocument : IGraph
		{
			public static GraphDocument Empty { get; } = new GraphDocument(new List<NodeGroup>(), new List<Aggregate>(), new List<Label>());

			[BsonRequired, BsonId]
			public ContentHash Id { get; private set; } = ContentHash.Empty;

			public int Schema { get; set; }
			public List<NodeGroup> Groups { get; private set; } = new List<NodeGroup>();
			public List<Aggregate> Aggregates { get; private set; } = new List<Aggregate>();
			public List<Label> Labels { get; private set; } = new List<Label>();

			[BsonIgnore]
			IReadOnlyDictionary<string, NodeRef>? _cachedNodeNameToRef;

			IReadOnlyList<INodeGroup> IGraph.Groups => Groups;
			IReadOnlyList<IAggregate> IGraph.Aggregates => Aggregates;
			IReadOnlyList<ILabel> IGraph.Labels => Labels;

			[BsonConstructor]
			private GraphDocument()
			{
			}

			public GraphDocument(List<NodeGroup> groups, List<Aggregate> aggregates, List<Label> labels)
			{
				Groups = groups;
				Aggregates = aggregates;
				Labels = labels;
				Id = ContentHash.SHA1(BsonExtensionMethods.ToBson(this));
			}

			public GraphDocument(GraphDocument baseGraph, List<NewGroup>? newGroupRequests, List<NewAggregate>? newAggregateRequests, List<NewLabel>? newLabelRequests)
			{
				Dictionary<string, NodeRef> nodeNameToRef = new Dictionary<string, NodeRef>(baseGraph.GetNodeNameToRef(), StringComparer.OrdinalIgnoreCase);

				// Update the new list of groups
				List<NodeGroup> newGroups = new List<NodeGroup>(baseGraph.Groups);
				if (newGroupRequests != null)
				{
					foreach (NewGroup newGroupRequest in newGroupRequests)
					{
						List<Node> nodes = new List<Node>();
						foreach (NewNode newNodeRequest in newGroupRequest.Nodes)
						{
							int nodeIdx = nodes.Count;

							Priority priority = newNodeRequest.Priority ?? Priority.Normal;
							bool bAllowRetry = newNodeRequest.AllowRetry ?? true;
							bool bRunEarly = newNodeRequest.RunEarly ?? false;
							bool bWarnings = newNodeRequest.Warnings ?? true;

							NodeRef[] inputDependencies = (newNodeRequest.InputDependencies == null) ? Array.Empty<NodeRef>() : newNodeRequest.InputDependencies.Select(x => nodeNameToRef[x]).ToArray();
							NodeRef[] orderDependencies = (newNodeRequest.OrderDependencies == null) ? Array.Empty<NodeRef>() : newNodeRequest.OrderDependencies.Select(x => nodeNameToRef[x]).ToArray();
							orderDependencies = orderDependencies.Union(inputDependencies).ToArray();
							nodes.Add(new Node(newNodeRequest.Name, inputDependencies, orderDependencies, priority, bAllowRetry, bRunEarly, bWarnings, newNodeRequest.Credentials, newNodeRequest.Properties, newNodeRequest.Annotations));

							nodeNameToRef.Add(newNodeRequest.Name, new NodeRef(newGroups.Count, nodeIdx));
						}
						newGroups.Add(new NodeGroup(newGroupRequest.AgentType, nodes));
					}
				}

				// Update the list of aggregates
				List<Aggregate> newAggregates = new List<Aggregate>(baseGraph.Aggregates);
				if (newAggregateRequests != null)
				{
					foreach (NewAggregate newAggregateRequest in newAggregateRequests)
					{
						List<NodeRef> nodes = newAggregateRequest.Nodes.ConvertAll(x => nodeNameToRef[x]);
						newAggregates.Add(new Aggregate(newAggregateRequest.Name, nodes));
					}
				}

				// Update the list of labels
				List<Label> newLabels = new List<Label>(baseGraph.Labels);
				if (newLabelRequests != null)
				{
					foreach (NewLabel newLabelRequest in newLabelRequests)
					{
						List<NodeRef> requiredNodes = newLabelRequest.RequiredNodes.ConvertAll(x => nodeNameToRef[x]);
						List<NodeRef> includedNodes = newLabelRequest.IncludedNodes.ConvertAll(x => nodeNameToRef[x]);
						newLabels.Add(new Label(newLabelRequest.DashboardName, newLabelRequest.DashboardCategory, newLabelRequest.UgsName, newLabelRequest.UgsProject, newLabelRequest.Change, requiredNodes, includedNodes));
					}
				}

				// Create the new arrays
				Groups = newGroups;
				Aggregates = newAggregates;
				Labels = newLabels;

				// Create the new graph, and save the generated node lookup into it
				_cachedNodeNameToRef = nodeNameToRef;

				// Compute the hash
				Id = ContentHash.SHA1(BsonExtensionMethods.ToBson(this));
			}

			public IReadOnlyDictionary<string, NodeRef> GetNodeNameToRef()
			{
				if (_cachedNodeNameToRef == null)
				{
					Dictionary<string, NodeRef> nodeNameToRef = new Dictionary<string, NodeRef>(StringComparer.OrdinalIgnoreCase);
					for (int groupIdx = 0; groupIdx < Groups.Count; groupIdx++)
					{
						List<Node> nodes = Groups[groupIdx].Nodes;
						for (int nodeIdx = 0; nodeIdx < nodes.Count; nodeIdx++)
						{
							Node node = nodes[nodeIdx];
							nodeNameToRef[node.Name] = new NodeRef(groupIdx, nodeIdx);
						}
					}
					_cachedNodeNameToRef = nodeNameToRef;
				}
				return _cachedNodeNameToRef;
			}
		}

		/// <summary>
		/// Stores information about a cached graph
		/// </summary>
		class CachedGraph
		{
			/// <summary>
			/// Time at which the graph was last accessed
			/// </summary>
			public long LastAccessTime => _lastAccessTimePrivate;

			/// <summary>
			/// Backing value for <see cref="LastAccessTime"/>
			/// </summary>
			private long _lastAccessTimePrivate;

			/// <summary>
			/// The graph instance
			/// </summary>
			public IGraph Graph { get; }

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="graph">The graph to store</param>
			public CachedGraph(IGraph graph)
			{
				_lastAccessTimePrivate = Stopwatch.GetTimestamp();
				Graph = graph;
			}

			/// <summary>
			/// Update the last access time
			/// </summary>
			public void Touch()
			{
				for (; ; )
				{
					long time = Stopwatch.GetTimestamp();
					long lastAccessTimeCopy = _lastAccessTimePrivate;
					if (time < lastAccessTimeCopy || Interlocked.CompareExchange(ref _lastAccessTimePrivate, time, lastAccessTimeCopy) == lastAccessTimeCopy)
					{
						break;
					}
				}
			}
		}

		/// <summary>
		/// The jobs collection
		/// </summary>
		readonly IMongoCollection<GraphDocument> _graphs;

		/// <summary>
		/// Maximum number of graphs to keep in the cache
		/// </summary>
		const int MaxGraphs = 1000;

		/// <summary>
		/// Cache of recently accessed graphs
		/// </summary>
		readonly ConcurrentDictionary<ContentHash, CachedGraph> _cachedGraphs = new ConcurrentDictionary<ContentHash, CachedGraph>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		public GraphCollection(MongoService mongoService)
		{
			_graphs = mongoService.GetCollection<GraphDocument>("Graphs");
		}

		/// <summary>
		/// Adds a new graph document
		/// </summary>
		/// <param name="graph">The graph to add</param>
		/// <returns>Async task</returns>
		async Task AddAsync(GraphDocument graph)
		{
			if (!await _graphs.Find(x => x.Id == graph.Id).AnyAsync())
			{
				try
				{
					await _graphs.InsertOneAsync(graph);
				}
				catch (MongoWriteException ex)
				{
					if (ex.WriteError.Category != ServerErrorCategory.DuplicateKey)
					{
						throw;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IGraph> AddAsync(ITemplate template)
		{
			Node node = new Node(IJob.SetupNodeName, Array.Empty<NodeRef>(), Array.Empty<NodeRef>(), Priority.High, true, false, true, null, null, null);
			NodeGroup group = new NodeGroup(template.InitialAgentType ?? "Win64", new List<Node> { node });

			GraphDocument graph = new GraphDocument(new List<NodeGroup> { group }, new List<Aggregate>(), new List<Label>());
			await AddAsync(graph);
			return graph;
		}

		/// <inheritdoc/>
		public async Task<IGraph> AppendAsync(IGraph? baseGraph, List<NewGroup>? newGroupRequests, List<NewAggregate>? newAggregateRequests, List<NewLabel>? newLabelRequests)
		{
			GraphDocument graph = new GraphDocument((GraphDocument?)baseGraph ?? GraphDocument.Empty, newGroupRequests, newAggregateRequests, newLabelRequests);
			await AddAsync(graph);
			return graph;
		}

		/// <inheritdoc/>
		public async Task<IGraph> GetAsync(ContentHash? hash)
		{
			// Special case for an empty graph request
			if (hash == null || hash == ContentHash.Empty || hash == GraphDocument.Empty.Id)
			{
				return GraphDocument.Empty;
			}

			// Try to read the graph from the cache
			CachedGraph? cachedGraph;
			if (_cachedGraphs.TryGetValue(hash, out cachedGraph))
			{
				// Update the last access time
				cachedGraph.Touch();
			}
			else
			{
				// Trim the cache
				while (_cachedGraphs.Count > MaxGraphs)
				{
					ContentHash? removeHash = _cachedGraphs.OrderBy(x => x.Value.LastAccessTime).Select(x => x.Key).FirstOrDefault();
					if (removeHash == null || removeHash == ContentHash.Empty)
					{
						break;
					}
					_cachedGraphs.TryRemove(removeHash, out _);
				}

				// Create the new entry
				cachedGraph = new CachedGraph(await _graphs.Find<GraphDocument>(x => x.Id == hash).FirstAsync());
				_cachedGraphs.TryAdd(hash, cachedGraph);
			}
			return cachedGraph.Graph;
		}

		/// <inheritdoc/>
		public async Task<List<IGraph>> FindAllAsync(ContentHash[]? hashes, int? index, int? count)
		{
			FilterDefinitionBuilder<GraphDocument> filterBuilder = Builders<GraphDocument>.Filter;

			FilterDefinition<GraphDocument> filter = filterBuilder.Empty;
			if (hashes != null)
			{
				filter &= filterBuilder.In(x => x.Id, hashes);
			}

			List<GraphDocument> results;
			IFindFluent<GraphDocument, GraphDocument> search = _graphs.Find(filter);
			if (index != null)
			{
				search = search.Skip(index.Value);
			}
			if (count != null)
			{
				search = search.Limit(count.Value);
			}

			results = await search.ToListAsync();
			return results.ConvertAll<IGraph>(x => x);
		}
	}
}
