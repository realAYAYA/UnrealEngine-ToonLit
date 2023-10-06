// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace EpicGames.Jupiter
{
	public class JupiterTree
	{
		private readonly JupiterTreeContentProvider ContentProvider;
		private byte[] LastTreeHash = null; // cache the calculated tree hash, reset the cache by setting to null

		public JupiterTree(JupiterTreeContentProvider InContentProvider)
		{
			ContentProvider = InContentProvider;
			ContentProvider.OnContentChanged += OnContentChanged;
		}

		private void OnContentChanged(object Sender, EventArgs EventArgs)
		{
			// invalidate the tree hash when the contents change
			LastTreeHash = null;
		}

		public List<JupiterTree> Trees { get; } = new List<JupiterTree>();

		public IEnumerable<string> ContentHashes
		{
			get { return ContentProvider.GetHashes(); }
		}

		public Task<byte[]> GetContent(string BlobHash)
		{
			return ContentProvider.GetContent(BlobHash);
		}

		public void AddTree(JupiterTree Tree)
		{
			LastTreeHash = null;
			Trees.Add(Tree);
		}

		public void AddTrees(IEnumerable<JupiterTree> InTrees)
		{
			LastTreeHash = null;
			Trees.AddRange(InTrees);
		}

		public byte[] CalculateTreeHash()
		{
			if (LastTreeHash != null)
			{
				return LastTreeHash;
			}

			List<byte> Bytes = new List<byte>();
			foreach (JupiterTree Tree in Trees)
			{
				Bytes.AddRange(Tree.CalculateTreeHash());
			}

			foreach (string Hash in ContentProvider.GetHashes())
			{
				Bytes.AddRange(SHA1Utils.ToSha1FromString(Hash));
			}

			LastTreeHash = SHA1Utils.GetSHA1(Bytes.ToArray());
			return LastTreeHash;
		}

		public List<JupiterTree> GetAllTrees()
		{
			List<JupiterTree> AllTrees = new List<JupiterTree>();
			DoGetAllTrees(AllTrees);
			return AllTrees;
		}

		private void DoGetAllTrees(List<JupiterTree> OutTrees)
		{
			OutTrees.Add(this);
			foreach (JupiterTree Tree in Trees)
			{
				Tree.DoGetAllTrees(OutTrees);
			}
		}

		public async Task UploadToJupiter(string JupiterUrl, string JupiterNamespace, string JupiterTreeKey, Dictionary<string, object> Metadata = null)
		{
			string TreeHashString = SHA1Utils.FormatAsHexString(CalculateTreeHash());
			List<JupiterTree> AllTrees = GetAllTrees();
			using (HttpClient JupiterClient = new HttpClient {BaseAddress = new Uri(JupiterUrl)})
			{
				Log.Logger.LogInformation("Creating a new tree root in Jupiter with id: {Key}", JupiterTreeKey);

				if (Metadata == null)
				{
					Metadata = new Dictionary<string, object>();
				}

				// add a source field if it doesn't already exist to make it known were this tree came from
				if (!Metadata.ContainsKey("source"))
				{
					Metadata["source"] = "BuildGraph";
				}

				{
					var TreeRoot = new
					{
						TreeReferenceKey = JupiterTreeKey,
						TreeHash = TreeHashString,
						Metadata = Metadata,
					};
					string PutTreeRootString = JsonSerializer.Serialize(TreeRoot);
					using StringContent PutTreeRootContent = new StringContent(PutTreeRootString, Encoding.UTF8, "application/json");
					// upload tree information
					HttpResponseMessage PutTreeResult = await JupiterClient.PutAsync(string.Format("api/v1/c/tree-root/{0}", JupiterNamespace), PutTreeRootContent);
					if (!PutTreeResult.IsSuccessStatusCode)
					{
						string ErrorMsg = await PutTreeResult.Content.ReadAsStringAsync();
						throw new Exception(string.Format("Failed to create tree root in Jupiter. Response: {0}", ErrorMsg));
					}
				}

				Log.Logger.LogInformation("Uploading \"{Count}\" trees to Jupiter", AllTrees.Count);

				{
					// upload the trees we have
					var PutTreesRequest = from Tree in AllTrees
						select new
						{
							Hash = SHA1Utils.FormatAsHexString(Tree.CalculateTreeHash()),
							Trees = Tree.Trees.Select(JupiterTree =>
								SHA1Utils.FormatAsHexString(JupiterTree.CalculateTreeHash())),
							Blobs = Tree.ContentHashes,
						};
					string PutTreeString = JsonSerializer.Serialize(PutTreesRequest);
					using StringContent PutTreeContent = new StringContent(PutTreeString, Encoding.UTF8, "application/json");
					HttpResponseMessage PutTreeContentResult = await JupiterClient.PutAsync(string.Format("api/v1/c/tree/{0}", JupiterNamespace), PutTreeContent);
					if (!PutTreeContentResult.IsSuccessStatusCode)
					{
						string ErrorMsg = await PutTreeContentResult.Content.ReadAsStringAsync();
						throw new Exception(string.Format("Failed to upload one or more trees to Jupiter. Response: {0}", ErrorMsg));
					}
				}

				List<string> FoundBlobs;
				{
					// determine which blobs are already present and thus do not need to be uploaded
					List<string> BlobIdentifiers = AllTrees.SelectMany(Tree => Tree.ContentHashes).ToList();

					var FilterBlobRequest = new
					{
						Operations = from Blob in BlobIdentifiers
							select new
							{
								Namespace = JupiterNamespace,
								Id = Blob,
								Op = "HEAD"
							}
					};

					string FilterBlobString = JsonSerializer.Serialize(FilterBlobRequest);
					using StringContent FilterBlobContent = new StringContent(FilterBlobString, Encoding.UTF8, "application/json");
					HttpResponseMessage FilterBlobResponse = await JupiterClient.PostAsync("api/v1/s", FilterBlobContent);
					if (!FilterBlobResponse.IsSuccessStatusCode)
					{
						string ErrorMsg = await FilterBlobResponse.Content.ReadAsStringAsync();
						throw new Exception(string.Format("Failed to determine which blobs were already present in Jupiter. Response: {0}", ErrorMsg));
					}

					string ResponseString = await FilterBlobResponse.Content.ReadAsStringAsync();
					List<string> UnknownBlobs = JsonSerializer.Deserialize<List<string>>(ResponseString);
					FoundBlobs = BlobIdentifiers.Where(Blob => !UnknownBlobs.Contains(Blob)).ToList();

					Log.Logger.LogInformation("Determined that build consist of \"{NumChunks}\" chunks of which \"{NumPresent}\" where already present. Thus uploading \"{Count}\" blobs", BlobIdentifiers.Count, FoundBlobs.Count, BlobIdentifiers.Count - FoundBlobs.Count);
				}

				{
					// upload the blobs
					List<Task> SubmitTasks = new List<Task>();
					foreach (JupiterTree JupiterTree in AllTrees)
					{
						foreach (string BlobHash in JupiterTree.ContentHashes)
						{
							// check if blob is already present
							if (FoundBlobs.Contains(BlobHash))
							{
								continue;
							}

							SubmitTasks.Add(Task.Run(async () =>
								{
									ByteArrayContent Content = new ByteArrayContent(await JupiterTree.GetContent(BlobHash));
									Content.Headers.Remove("Content-Type");
									Content.Headers.Add("Content-Type", "application/octet-stream");
									HttpResponseMessage PutBlobResultResult = await JupiterClient.PutAsync(string.Format("api/v1/s/{0}/{1}", JupiterNamespace, BlobHash), Content);

									if (!PutBlobResultResult.IsSuccessStatusCode)
									{
										string ErrorMsg = await PutBlobResultResult.Content.ReadAsStringAsync();
										throw new Exception(string.Format(
											"Failed to upload content blob to Jupiter. Response: {0}", ErrorMsg));
									}
								}
							));
						}
					}

					Task.WaitAll(SubmitTasks.ToArray());
				}

				// verify the build upload
				{
					HttpResponseMessage FinalizeTreeResult = await JupiterClient.PutAsync(string.Format("api/v1/c/tree-root/{0}/{1}/finalize", JupiterNamespace, JupiterTreeKey), null);
					if (!FinalizeTreeResult.IsSuccessStatusCode)
					{
						string ErrorMsg = await FinalizeTreeResult.Content.ReadAsStringAsync();
						throw new Exception(string.Format("Jupiter tree upload verification failed. Response: {0}",
							ErrorMsg));
					}
				}

				Log.Logger.LogInformation("Build upload and verification complete.");
			}
		}

		public static async Task<JupiterTree> FromJupiterKey(string JupiterUrl, string JupiterNamespace, string JupiterTreeKey)
		{
			using (HttpClient JupiterClient = new HttpClient {BaseAddress = new Uri(JupiterUrl)})
			{
				Log.Logger.LogInformation("Downloading tree with key \"{Key}\" from Jupiter at {Url} in namespace {Namespace}", JupiterTreeKey, JupiterUrl, JupiterNamespace);

				string TopTreeHash;

				{
					// download the tree root
					HttpResponseMessage GetTreeRootResult = await JupiterClient.GetAsync(string.Format("api/v1/c/tree-root/{0}/{1}", JupiterNamespace, JupiterTreeKey));

					string GetTreeRootResultString = await GetTreeRootResult.Content.ReadAsStringAsync();
					if (!GetTreeRootResult.IsSuccessStatusCode)
					{
						throw new Exception(string.Format("Failed to download tree root with key {0}. Response: {1}", JupiterTreeKey, GetTreeRootResultString));
					}

					TreeRootContents TreeRoot = JsonSerializer.Deserialize<TreeRootContents>(GetTreeRootResultString);

					TopTreeHash = TreeRoot.treeHash;
				}

				Dictionary<string, TreeContents> TreeMapping;
				{
					// Flatten this tree, and download the tree descriptions of all those trees
					HttpResponseMessage GetFlattenTreeResult = await JupiterClient.GetAsync(string.Format("api/v1/c/tree/{0}/{1}/flattend", JupiterNamespace, TopTreeHash));

					string GetFlattenTreeResultString = await GetFlattenTreeResult.Content.ReadAsStringAsync();
					if (!GetFlattenTreeResult.IsSuccessStatusCode)
					{
						throw new Exception(string.Format("Failed to download flattened tree {0}. Response: {1}", TopTreeHash, GetFlattenTreeResultString));
					}

					FlattendTreeContents FlattenedTree = JsonSerializer.Deserialize<FlattendTreeContents>(GetFlattenTreeResultString);

					List<string> TreesToDownload = new List<string>(FlattenedTree.allTrees);
					TreesToDownload.Add(TopTreeHash);
					TreeMapping = new Dictionary<string, TreeContents>();
					foreach (string Tree in TreesToDownload)
					{
						// Flatten this tree, and download the tree descriptions of all those trees
						HttpResponseMessage GetTreeResult = await JupiterClient.GetAsync(string.Format("api/v1/c/tree/{0}/{1}", JupiterNamespace, Tree));

						string GetTreeResultString = await GetTreeResult.Content.ReadAsStringAsync();
						if (!GetTreeResult.IsSuccessStatusCode)
						{
							throw new Exception(string.Format("Failed to download tree {0}. Response: {1}", Tree, GetTreeResultString));
						}

						TreeContents TreeContents = JsonSerializer.Deserialize<TreeContents>(GetTreeResultString);
						TreeMapping.Add(Tree, TreeContents);
					}
				}

				{
					return CreateTreeFromTreeDescriptions(JupiterUrl, JupiterNamespace, TopTreeHash, TreeMapping);
				}
			}
		}

		private static JupiterTree CreateTreeFromTreeDescriptions(string JupiterUrl, string JupiterNamespace, string RootTreeHash, Dictionary<string, TreeContents> TreeDescriptions)
		{
			TreeContents RootTreeContents = TreeDescriptions[RootTreeHash];
			JupiterIoContentProvider ContentProvider = new JupiterIoContentProvider();
			ContentProvider.AddContentReference(JupiterUrl, JupiterNamespace, RootTreeContents.blobs);
			JupiterTree NewTree = new JupiterTree(ContentProvider);

			foreach (string Tree in RootTreeContents.trees)
			{
				NewTree.AddTree(CreateTreeFromTreeDescriptions(JupiterUrl, JupiterNamespace, Tree, TreeDescriptions));
			}

			return NewTree;
		}

		public class TreeRootContents
		{
			public string treeHash { get; set; }
			public Dictionary<string, object> metadata { get; set; }
		}

		public class FlattendTreeContents
		{
			public string[] allTrees { get; set; }

			public string[] allBlobs { get; set; }
		}

		public class TreeContents
		{
			public string treeHash { get; set; }
			public List<string> trees { get; set; }
			public List<string> blobs { get; set; }
		}
	}

	public class JupiterIoContentProvider : JupiterTreeContentProvider
	{
		public string BaseUrl { get; private set; }
		public List<string> Blobs { get; private set; }
		public string JupiterNamespace { get; private set; }

		public override async Task<byte[]> GetContent(string Sha1)
		{
			using (HttpClient JupiterClient = new HttpClient {BaseAddress = new Uri(BaseUrl)})
			{
				HttpResponseMessage GetContentsResult = await JupiterClient.GetAsync(string.Format("api/v1/s/{0}/{1}", JupiterNamespace, Sha1));

				if (!GetContentsResult.IsSuccessStatusCode)
				{
					string ErrorMsg = await GetContentsResult.Content.ReadAsStringAsync();
					throw new Exception(string.Format("Failed to download blob {0}. Response: {1}", Sha1, ErrorMsg));
				}

				byte[] Data = await GetContentsResult.Content.ReadAsByteArrayAsync();

				return Data;
			}
		}

		public override IEnumerable<string> GetHashes()
		{
			return Blobs;
		}

		public override event EventHandler OnContentChanged;

		public void AddContentReference(string JupiterUrl, string InJupiterNamespace, List<string> InBlobs)
		{
			BaseUrl = JupiterUrl;
			JupiterNamespace = InJupiterNamespace;
			Blobs = InBlobs;

			OnContentChanged?.Invoke(this, new EventArgs());
		}
	}

	public class JupiterStreamedContentProvider : JupiterTreeContentProvider
	{
		private class ChunkDescription
		{
			public long Offset;
			public long Size;
		}

		private FileReference File { get; set; }
		private Dictionary<string, ChunkDescription> Chunks;

		public override event EventHandler OnContentChanged;

		public void SetFileSource(FileReference InFile)
		{
			File = InFile;
			Chunks = PopulateChunks(File);

			OnContentChanged?.Invoke(this, new EventArgs());
		}

		public override Task<byte[]> GetContent(string Sha1)
		{
			ChunkDescription Chunk;
			if (Chunks.TryGetValue(Sha1, out Chunk))
			{
				using (FileStream FileStream = new FileStream(File.FullName, FileMode.Open, FileAccess.Read, FileShare.Read))
				using (BinaryReader BinaryReader = new BinaryReader(FileStream))
				{
					FileStream.Position = Chunk.Offset;
					return Task.FromResult(BinaryReader.ReadBytes((int)Chunk.Size));
				}
			}
			throw new ArgumentOutOfRangeException(nameof(Sha1), "Unable to find any matching chunk for hash: " + Sha1);
		}

		public override IEnumerable<string> GetHashes()
		{
			return Chunks.Keys;
		}

		private static Dictionary<string, ChunkDescription> PopulateChunks(FileReference File)
		{
			Dictionary<string, ChunkDescription> NewChunks = new Dictionary<string, ChunkDescription>();
			List<Tuple<byte[], byte[]>> FileChunks = JupiterFileUtils.ChunkFile(File);
			long CurrentOffset = 0;
			foreach (Tuple<byte[], byte[]> FileChunk in FileChunks)
			{
				long ChunkLength = FileChunk.Item2.LongLength;
				ChunkDescription ChunkDescription = new ChunkDescription { Offset = CurrentOffset, Size = ChunkLength };
				CurrentOffset += ChunkLength;
				string Hash = SHA1Utils.FormatAsHexString(FileChunk.Item1);
				NewChunks.Add(Hash, ChunkDescription);
			}

			return NewChunks;
		}
	}

	public class JupiterInMemoryContentProvider : JupiterTreeContentProvider
	{
		private readonly Dictionary<string, byte[]> ContentBlobs = new Dictionary<string, byte[]>();

		public void AddContent(byte[] Sha1, byte[] Content)
		{
			if (Sha1.Length != 20)
			{
				throw new ArgumentException("Sha1 argument was not 20 bytes, did you really specify a hash of the contents?", nameof(Sha1));
			}

			string Hash = SHA1Utils.FormatAsHexString(Sha1);
			ContentBlobs.Add(Hash, Content);
			OnContentChanged?.Invoke(this, new EventArgs());
		}

		public override Task<byte[]> GetContent(string Sha1)
		{
			return Task.FromResult(ContentBlobs[Sha1]);
		}

		public override IEnumerable<string> GetHashes()
		{
			return ContentBlobs.Keys;
		}

		public override event EventHandler OnContentChanged;
	}

	public abstract class JupiterTreeContentProvider
	{
		public abstract Task<byte[]> GetContent(string Sha1);
		public abstract IEnumerable<string> GetHashes();
		public abstract event EventHandler OnContentChanged;
	}

	/// <summary>
	/// A merkel tree built from a set of files ready to be uploaded to Jupiter
	/// </summary>
	public class JupiterFileTree
	{
		private readonly DirectoryReference BaseDir;
		private readonly List<FileReference> Files = new List<FileReference>();
		private readonly bool DeferReadingFiles;

		public JupiterFileTree(DirectoryReference InBaseDir, bool InDeferReadingFiles)
		{
			BaseDir = InBaseDir;
			DeferReadingFiles = InDeferReadingFiles;
		}

		public void AddFile(FileReference File)
		{
			Files.Add(File);
		}

		public async Task<Dictionary<FileReference, List<string>>> UploadToJupiter(string JupiterUrl, string JupiterNamespace, string JupiterTreeKey, Dictionary<string, object> Metadata = null)
		{
			Manifest Manifest = new Manifest(BaseDir);

			JupiterTree JupiterTopTree = new JupiterTree(new JupiterInMemoryContentProvider());

			List<JupiterTree> FileTrees = new List<JupiterTree>();
			Dictionary<FileReference, List<string>> FileToChunkMapping = new Dictionary<FileReference, List<string>>();
			foreach (FileReference File in Files)
			{
				JupiterTreeContentProvider ContentProvider;
				if (DeferReadingFiles)
				{
					// deferred reading means we avoid keeping the file in memory, instead reading the contents from disk as we are uploading it.
					JupiterStreamedContentProvider JupiterStreamedContentProvider = new JupiterStreamedContentProvider();
					JupiterStreamedContentProvider.SetFileSource(File);
					ContentProvider = JupiterStreamedContentProvider;
				}
				else
				{
					// In memory use-case - just read all files into memory and hash them.
					JupiterInMemoryContentProvider InMemoryContentProvider = new JupiterInMemoryContentProvider();
					List<Tuple<byte[], byte[]>> FileChunks = JupiterFileUtils.ChunkFile(File);
					foreach (Tuple<byte[], byte[]> Chunk in FileChunks)
					{
						byte[] Sha1 = Chunk.Item1;
						byte[] Content = Chunk.Item2;
						InMemoryContentProvider.AddContent(Sha1, Content);
					}

					ContentProvider = InMemoryContentProvider;

				}

				JupiterTree CurrentTree = new JupiterTree(ContentProvider);

				// populate the manifest as we have prepared the content providers
				Manifest.AddFile(File, CurrentTree.CalculateTreeHash());
				FileTrees.Add(CurrentTree);
				FileToChunkMapping.Add(File, ContentProvider.GetHashes().ToList());
			}

			// The manifest is the first node in the tree so that we can find it later
			JupiterTopTree.AddTree(Manifest.AsTree());
			JupiterTopTree.AddTrees(FileTrees);

			await JupiterTopTree.UploadToJupiter(JupiterUrl, JupiterNamespace, JupiterTreeKey, Metadata);

			return FileToChunkMapping;
		}

		public async Task<List<FileReference>> DownloadFromJupiter(FileReference LocalManifestPath, string JupiterUrl, string JupiterNamespace, string JupiterTreeKey, IProgress<Tuple<float, FileReference>> Progress = null)
		{
			Manifest LocalManifest = null;
			if (LocalManifestPath.ToFileInfo().Exists)
			{
				LocalManifest = Manifest.CreateFromFileReference(BaseDir, LocalManifestPath);
			}
			JupiterTree Tree = await JupiterTree.FromJupiterKey(JupiterUrl, JupiterNamespace, JupiterTreeKey);

			JupiterTree ManifestTree = Tree.Trees.First();
			Manifest NewManifest = Manifest.FromTree(BaseDir, ManifestTree);

			int CountOfFileTrees = Tree.Trees.Count - 1; // all trees except for the first represents files
			JupiterTree[] FileTrees = new JupiterTree[CountOfFileTrees];
			Tree.Trees.CopyTo(1, FileTrees, 0, CountOfFileTrees);

			Log.Logger.LogInformation("Build {Key} consisted of {NumFiles} files. Downloading.", JupiterTreeKey, CountOfFileTrees);

			List<FileReference> FilesWritten = new List<FileReference>();
			int CountOfFilesWritten = 0;
			foreach (JupiterTree FileTree in FileTrees)
			{
				string TreeHash = SHA1Utils.FormatAsHexString(FileTree.CalculateTreeHash());

				KeyValuePair<string, string> ManifestFileReference = NewManifest.Files.First(Pair => Pair.Value == TreeHash);
				FileReference LocalFileReference = FileReference.Combine(BaseDir, ManifestFileReference.Key);

				bool FileAlreadyPresent = false;
				string LocalHash;
				// if we have a local manifest which contains this same file with the same hash we can ignore that file
				if (LocalManifest != null && LocalManifest.Files.TryGetValue(ManifestFileReference.Key, out LocalHash))
				{
					if (FileReference.Exists(LocalFileReference) && LocalHash == TreeHash)
					{
						Log.Logger.LogInformation("File \"{File}\" already present with hash \"{Hash}\". Skipping.", LocalFileReference, LocalHash);
						FileAlreadyPresent = true;
					}
				}

				if (!FileAlreadyPresent)
				{
					Log.Logger.LogInformation("Creating file: {File}", LocalFileReference);

					foreach (string ContentHash in FileTree.ContentHashes)
					{
						byte[] Content = await FileTree.GetContent(ContentHash);
						DirectoryReference.CreateDirectory(LocalFileReference.Directory);
						FileReference.WriteAllBytes(LocalFileReference, Content);
					}
				}

				FilesWritten.Add(LocalFileReference);
				Progress?.Report(new Tuple<float, FileReference>(++CountOfFilesWritten / (float)CountOfFileTrees, LocalFileReference));
			}

			NewManifest.Save(LocalManifestPath);
			return FilesWritten;
		}

		public class Manifest
		{
			private readonly DirectoryReference BaseDir;

			// ReSharper disable once MemberCanBePrivate.Global , fastjson requires this to be public so it can be serialized
			public class ManifestData
			{
				public string SchemaVersion = "v1"; // added in case we need to update this schema in the future
				public Dictionary<string, string> Files = new Dictionary<string, string>();
			}
			private readonly ManifestData Data;

			public Dictionary<string, string> Files
			{
				get { return Data.Files; }
			}

			public Manifest(DirectoryReference InBaseDir)
			{
				BaseDir = InBaseDir;
				Data = new ManifestData();
			}

			private Manifest(DirectoryReference InBaseDir, ManifestData InData)
			{
				BaseDir = InBaseDir;
				Data = InData;
			}

			public void AddFile(FileReference File, byte[] TreeHash)
			{
				// we escape the forward slashes to make it valid json keys
				Files.Add(File.MakeRelativeTo(BaseDir).Replace(@"\", @"\\"), SHA1Utils.FormatAsHexString(TreeHash));
			}

			public JupiterTree AsTree()
			{
				string JsonFiles = JsonSerializer.Serialize(Data);
				byte[] ManifestBytes = Encoding.ASCII.GetBytes(JsonFiles);
				byte[] Hash = SHA1Utils.GetSHA1(ManifestBytes);
				JupiterInMemoryContentProvider ContentProvider = new JupiterInMemoryContentProvider();
				ContentProvider.AddContent(Hash, ManifestBytes);
				return new JupiterTree(ContentProvider);
			}

			public static Manifest FromTree(DirectoryReference BaseDir, JupiterTree ManifestTree)
			{
				// the first hash of the tree is a json serialized form of the manifest, see AsTree
				string FirstHash = ManifestTree.ContentHashes.First();
				byte[] Content = ManifestTree.GetContent(FirstHash).Result;
				string JsonContent = Encoding.ASCII.GetString(Content);
				ManifestData Data = JsonSerializer.Deserialize<ManifestData>(JsonContent);
				return new Manifest(BaseDir, Data);
			}

			public static Manifest CreateFromFileReference(DirectoryReference BaseDir, FileReference FileReference)
			{
				ManifestData Data = JsonSerializer.Deserialize<ManifestData>(FileReference.ReadAllText(FileReference));
				return new Manifest(BaseDir, Data);
			}

			public void Save(FileReference LocalManifestReference)
			{
				DirectoryReference.CreateDirectory(LocalManifestReference.Directory);

				Dictionary<string, string> EscapedFiles = Data.Files.ToDictionary(Pair => JsonWriter.EscapeString(Pair.Key), Pair => Pair.Value);

				ManifestData EscapedData = new ManifestData()
				{
					SchemaVersion = Data.SchemaVersion,
					Files = EscapedFiles
				};
				FileReference.WriteAllText(LocalManifestReference, JsonSerializer.Serialize(EscapedData));
			}
		}
	}
}