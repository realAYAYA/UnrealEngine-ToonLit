using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Identifies one or more specific versioned objects in a Perforce repository. 
	/// </summary>
	public class FileSpec
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		public FileSpec() { }

        /// <summary>
        /// Copy Constructor
        /// </summary>
        /// <param name="fs">FileSpec</param>
		public FileSpec(FileSpec fs) 
		{
			DepotPath = fs.DepotPath;
			ClientPath = fs.ClientPath;
			LocalPath = fs.LocalPath;

			Version = fs.Version;
		}

        /// <summary>
        /// Constructor given path and version
        /// </summary>
        /// <param name="path">location of the file</param>
        /// <param name="version">version of the file</param>
		public FileSpec(PathSpec path, VersionSpec version)
		{
			if (path.GetType() == typeof(DepotPath))
			{
				DepotPath = (DepotPath)path;
				ClientPath = null;
				LocalPath = null;
			}
			else if (path.GetType() == typeof(ClientPath))
			{
				DepotPath = null;
				ClientPath = (ClientPath)path;
				LocalPath = null;
			}
			else if (path.GetType() == typeof(LocalPath))
			{
				DepotPath = null;
				ClientPath = null;
				LocalPath = (LocalPath)path;
			}
			Version = version;
		}

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="dp">DepotPath</param>
        /// <param name="cp">ClientPath</param>
        /// <param name="lp">LocalPath</param>
        /// <param name="version">VersionSpec</param>
		public FileSpec(DepotPath dp, ClientPath cp, LocalPath lp, VersionSpec version)
		{
			DepotPath = dp;
			ClientPath = cp;
			LocalPath = lp;

			Version = version;
		}

        /// <summary>
        /// Get a Filespec from a ClientSpec path
        /// </summary>
        /// <param name="path">client path</param>
        /// <returns>FileSpec</returns>
		public static FileSpec ClientSpec(String path)
		{
			return new FileSpec(new ClientPath(path), null);
		}

        /// <summary>
        /// Create a FileSpec given client path and revision
        /// </summary>
        /// <param name="path">Client path</param>
        /// <param name="rev">revision</param>
        /// <returns>FileSpec</returns>
        public static FileSpec ClientSpec(String path, int rev)
        {
            return new FileSpec(new ClientPath(path), new Revision(rev));
        }

        /// <summary>
        /// Create a FileSpec given Client path and VersionSpec
        /// </summary>
        /// <param name="path">client path</param>
        /// <param name="ver">VersionSpec</param>
        /// <returns>FileSpec</returns>
        public static FileSpec ClientSpec(String path, VersionSpec ver)
        {
            return new FileSpec(new ClientPath(path), ver);
        }

        /// <summary>
        /// Create a FileSpec given a depotpath
        /// </summary>
        /// <param name="path">depot path</param>
        /// <returns>FileSpec</returns>
        public static FileSpec DepotSpec(String path)
		{
			return new FileSpec(new DepotPath(path), null);
		}

        /// <summary>
        /// Create a FileSpec given a depot path and revision 
        /// </summary>
        /// <param name="path">depot path</param>
        /// <param name="rev">revision</param>
        /// <returns></returns>
        public static FileSpec DepotSpec(String path, int rev)
        {
            return new FileSpec(new DepotPath(path), new Revision(rev));
        }

        /// <summary>
        /// Create a FileSpec given a depot path and VersionSpec
        /// </summary>
        /// <param name="path">depot path</param>
        /// <param name="ver">VersionSpec</param>
        /// <returns></returns>
        public static FileSpec DepotSpec(String path, VersionSpec ver)
        {
            return new FileSpec(new DepotPath(path), ver);
        }

        /// <summary>
        /// Create a FileSpec given a local file path
        /// </summary>
        /// <param name="path">local file path</param>
        /// <returns></returns>
        public static FileSpec LocalSpec(String path)
		{
			return new FileSpec(new LocalPath(path), null);
		}

        /// <summary>
        /// Create a FileSpec given a local file path and revision
        /// </summary>
        /// <param name="path">local file path</param>
        /// <param name="rev">revision</param>
        /// <returns></returns>
		public static FileSpec LocalSpec(String path, int rev)
		{
			return new FileSpec(new LocalPath(path), new Revision(rev));
		}

        /// <summary>
        /// Create a FileSpec given a local file path and VersionSpec
        /// </summary>
        /// <param name="path">local file path</param>
        /// <param name="ver">VersionSpec</param>
        /// <returns></returns>
        public static FileSpec LocalSpec(String path, VersionSpec ver)
        {
            return new FileSpec(new LocalPath(path), ver);
        }

        /// <summary>
        /// Create a list of FileSpec objects from a list of client paths
        /// </summary>
        /// <param name="paths">List of paths</param>
        /// <returns>List of FileSpec objects</returns>
        public static IList<FileSpec> ClientSpecList(params String[] paths)
		{
			List<FileSpec> val = new List<FileSpec>();
			for (int idx = 0; idx < paths.Length; idx++)
			{
				val.Add(new FileSpec(new ClientPath(paths[idx]), null));
			}
			return val;
		}

		/// <summary>
		/// Create a list of FileSpec objects from a list of depot paths
		/// </summary>
		/// <param name="paths">List of paths</param>
		/// <returns>List of FileSpec objects</returns>
		public static IList<FileSpec> DepotSpecList(params String[] paths)
		{
			List<FileSpec> val = new List<FileSpec>();
			for (int idx = 0; idx < paths.Length; idx++)
			{
				val.Add(new FileSpec(new DepotPath(paths[idx]), null));
			}
			return val;
		}

		/// <summary>
		/// Create a list of FileSpec objects from a list of local paths
		/// </summary>
		/// <param name="paths">List of paths</param>
		/// <returns>List of FileSpec objects</returns>
		public static IList<FileSpec> LocalSpecList(params String[] paths)
		{
			List<FileSpec> val = new List<FileSpec>();
			for (int idx = 0; idx < paths.Length; idx++)
			{
				val.Add(new FileSpec(new LocalPath(paths[idx]), null));
			}
			return val;
		}
		/// <summary>
		/// Create a list of FileSpec objects from a list of local paths
		/// </summary>
		/// <param name="paths">List of paths</param>
		/// <returns>List of FileSpec objects</returns>
		public static IList<FileSpec> LocalSpecList(IList<String> paths)
		{
			List<FileSpec> val = new List<FileSpec>();
            if (paths!=null)
            {
                for (int idx = 0; idx < paths.Count; idx++)
                {
                    val.Add(new FileSpec(new LocalPath(paths[idx]), null));
                }
            }
			return val;
		}

		/// <summary>
		/// Create a copy of a filespec without a version
		/// </summary>
		public FileSpec StripVersion()
		{
			FileSpec fs = new FileSpec(this);
			fs.Version = null;
			return fs;
		}
		/// <summary>
		/// Copies a list of FileSpec objects without vesions
		/// </summary>
		/// <param name="specs">List of FileSpecs</param>
		/// <returns>List of FileSpec objects</returns>
		public static IList<FileSpec> UnversionedSpecList(IList<FileSpec> specs)
		{
			List<FileSpec> val = new List<FileSpec>();
            if (specs!=null)
            {
                for (int idx = 0; idx < specs.Count; idx++)
                { 
					val.Add(specs[idx].StripVersion());
                }
            }
			return val;
		}

		/// <summary>
		/// Create an array of FileSpec objects from a list of local paths
		/// </summary>
		/// <param name="paths">List of paths</param>
		/// <returns>List of FileSpec objects</returns>
		public static FileSpec[] LocalSpecArray(params String[] paths)
		{
			FileSpec[] val = new FileSpec[paths.Length];
			for (int idx = 0; idx < paths.Length; idx++)
			{
				val[idx] = new FileSpec(new LocalPath(paths[idx]), null);
			}
			return val;
		}
		/// <summary>
		/// Create an array of FileSpec objects from a list of local paths
		/// </summary>
		/// <param name="paths">List of paths</param>
		/// <returns>List of FileSpec objects</returns>
		public static FileSpec[] LocalSpecArray(IList<String> paths)
		{
			FileSpec[] val = new FileSpec[paths.Count];
			for (int idx = 0; idx < paths.Count; idx++)
			{
				val[idx] = new FileSpec(new LocalPath(paths[idx]), null);
			}
			return val;
		}

        /// <summary>
        /// Gets or sets the depot path
        /// </summary>
		public DepotPath DepotPath { get; set; }

        /// <summary>
        /// Gets or sets the client path
        /// </summary>
		public ClientPath ClientPath { get; set; }

        /// <summary>
        /// Gets or sets the Local path
        /// </summary>
		public LocalPath LocalPath { get; set; }

        /// <summary>
        /// Gets or sets the VersionSpec
        /// </summary>
		public VersionSpec Version { get; set; }

        /// <summary>
        /// Create a string representing a FileSpec
        /// </summary>
        /// <returns>string representation</returns>
		public override string ToString()
		{
			string value = null;
			if (DepotPath != null)
			{
				value = DepotPath.Path;
			}
			else if (ClientPath != null)
			{
				value = ClientPath.Path;
			}
			else if (LocalPath != null)
			{
				value = LocalPath.Path;
				if ((value.Length > 1) && (value[1] == ':') && char.IsUpper(value, 0))
				{
					value = value.Substring(0, 1).ToLower() + value.Substring(1);
				}
			}
			if (value != null)
			{
				if (Version != null)
				{
					value += Version.ToString();
				}
			}
			return value;
		}

        /// <summary>
        /// Create an Perforce excaped string describing this FileSpec
        /// </summary>
        /// <returns>escaped string</returns>
		public string ToEscapedString()
		{
			string value = null;
			if (DepotPath != null)
			{
				value = DepotPath.Path;
			}
			else if (ClientPath != null)
			{
				value = ClientPath.Path;
			}
			else if (LocalPath != null)
			{
				value = LocalPath.Path;
				if ((value.Length > 1) && (value[1] == ':') && char.IsUpper(value, 0))
				{
					value = value.Substring(0, 1).ToLower() + value.Substring(1);
				}
			}
			value = PathSpec.EscapePath(value);
			if (value != null)
			{
				if (Version != null)
				{
					value += Version.ToString();
				}
			}
			return value;
		}

        /// <summary>
        /// Create a descriptive string for this FileSpec of the specified Type
        /// </summary>
        /// <param name="pathType">Type of path to be returned</param>
        /// <returns>string represented FileSpec of Type</returns>
		public string ToString(Type pathType)
		{
			string value = null;
			if (pathType == typeof(DepotPath))
			{
				value = DepotPath.Path;
			}
			if (pathType == typeof(ClientPath))
			{
				value = ClientPath.Path;
			}
			if (pathType == typeof(LocalPath))
			{
				value = LocalPath.Path;
				if ((value.Length > 1) && (value[1] == ':') && char.IsUpper(value, 0))
				{
					value = value.Substring(0, 1).ToLower() + value.Substring(1);
				}
			}
			value = PathSpec.EscapePath(value);
			if (Version !=null )
			{
				value += Version.ToString();
			}
			return value;
		}

		/// <summary>
		/// Copy the non-null entries into a new string array.
		/// </summary>
		public static string[] ToStrings(IList<FileSpec> list)
		{
			if (list == null)
				return null;

			if ((list.Count == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Count; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Count; idx++)
			{
				if (list[idx] != null)
				{
					value[idx2++] = list[idx].ToString();
				}
			}
			return value;
		}

		/// <summary>
		/// Copy the non-null entries into a new string array
		///   with the paths escaped for special characters
		/// </summary>
		public static string[] ToEscapedStrings(IList<FileSpec> list)
		{
			if (list == null)
				return null;

			if ((list.Count == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Count; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Count; idx++)
			{
				if (list[idx] != null)
				{
					value[idx2++] = list[idx].ToEscapedString();
				}
			}
			return value;
		}
		/// <summary>
		/// Copy the non-null entries into a new string array
		///   with the paths escaped for special characters
		/// </summary>
		public static string[] ToEscapedStrings(params FileSpec[] list)
		{
			if (list == null)
				return null;

			if ((list.Length == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					value[idx2++] = list[idx].ToEscapedString();
				}
			}
			return value;
		}
		/// <summary>
		/// Copy the non-null entries into a new string array.
		/// </summary>
		public static string[] ToStrings(params FileSpec[] list)
		{
			if (list == null)
				return null;

			if ((list.Length == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					value[idx2++] = list[idx].ToString();
				}
			}
			return value;
		}
		/// <summary>
		/// Copy the non-null entries into a new string array containing the paths.
		/// </summary>
		public static string[] ToPaths(params FileSpec[] list)
		{
			if (list == null)
				return null;

			if ((list.Length == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					if (list[idx].DepotPath != null)
					{
						value[idx2++] = list[idx].DepotPath.Path;
					}
					else if (list[idx].LocalPath != null)
					{
						value[idx2++] = list[idx].LocalPath.Path;
					}
					else if (list[idx].ClientPath != null)
					{
						value[idx2] = list[idx].ClientPath.Path;
						if ((value[idx2].Length > 1) && (value[idx2][1] == ':') && char.IsUpper(value[idx2], 0))
						{
							value[idx2] = value[idx2].Substring(0, 1).ToLower() + value[idx2].Substring(1);
						}
						idx2++;
					}
				}
			}
			return value;
		}
		/// <summary>
		/// Copy the non-null entries into a new string array containing the paths
		///   escaped for special characters
		/// </summary>
		public static string[] ToEscapedPaths(params FileSpec[] list)
		{
			if (list == null)
				return null;

			if ((list.Length == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					if (list[idx].DepotPath != null)
					{
						value[idx2++] = PathSpec.EscapePath(list[idx].DepotPath.Path);
					}
					else if (list[idx].LocalPath != null)
					{
						value[idx2++] = PathSpec.EscapePath(list[idx].LocalPath.Path);
					}
					else if (list[idx].ClientPath != null)
					{
						value[idx2] = PathSpec.EscapePath(list[idx].ClientPath.Path);
						if ((value[idx2].Length > 1) && (value[idx2][1] == ':') && char.IsUpper(value[idx2], 0))
						{
							value[idx2] = value[idx2].Substring(0, 1).ToLower() + value[idx2].Substring(1);
						}
						idx2++;
					}
				}
			}
			return value;
		}
		/// <summary>
		/// Copy the non-null entries into a new string array containing the local paths
		///   escaped for special characters
		/// </summary>
		public static string[] ToEscapedLocalPaths(params FileSpec[] list)
		{
			if (list == null)
				return null;

			if ((list.Length == 1) && (list[0] == null))
				return null;

			int count = 0;
			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					count++;
				}
			}

			int idx2 = 0;

			string[] value = new string[count];

			for (int idx = 0; idx < list.Length; idx++)
			{
				if (list[idx] != null)
				{
					if (list[idx].LocalPath != null)
					{
						value[idx2] = PathSpec.EscapePath(list[idx].LocalPath.Path);
						if ((value[idx2].Length > 1) && (value[idx2][1] == ':') && char.IsUpper(value[idx2], 0))
						{
							value[idx2] = value[idx2].Substring(0, 1).ToLower() + value[idx2].Substring(1);
						}
						idx2++;
					}
				}
			}
			return value;
		}

        /// <summary>
        /// Compare this FileSpec with another
        /// </summary>
        /// <param name="obj">Object to compare to</param>
        /// <returns>true if they match</returns>
		public override bool Equals(object obj)
		{
			if ((obj is FileSpec) == false)
			{
				return false;
			}
			FileSpec o = obj as FileSpec;

			if (o.ClientPath != null)
			{
				if (o.ClientPath.Equals(this.ClientPath) == false)
				{ return false; }
			}
			else
			{
				if (this.ClientPath != null)
				{ return false; }
			}
			if (o.DepotPath != null)
			{
				if (o.DepotPath.Equals(this.DepotPath) == false)
				{ return false; }
			}
			else
			{
				if (this.DepotPath != null)
				{ return false; }
			}
				if (o.LocalPath != null)
				{
					if (o.LocalPath.Equals(this.LocalPath) == false)
					{ return false; }
				}
				else
				{
					if (this.LocalPath != null)
					{ return false; }
				}
			if (o.Version != null)
			{
				if (o.Version.Equals(this.Version) == false)
				{ return false; }
			}
			else
			{
				if (this.Version != null)
				{ return false; }
			}
			return true;
		}

        /// <summary>
        /// Get the hashcode for this FileSpec
        /// </summary>
        /// <returns>hash code</returns>
        public override int GetHashCode() {
            if (this.DepotPath != null)
                return this.DepotPath.GetHashCode();
            if (this.LocalPath != null)
                return this.LocalPath.GetHashCode();
            return this.ClientPath.GetHashCode();
        }
    }
}
