/*******************************************************************************

Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: P4Directory.cs
 *
 * Author	: Duncan Barbee <dbarbee@perforce.com>
 *
 * Description	: Classes used to abstract a directory in Perforce
 *
 ******************************************************************************/
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;  // for Path class

namespace Perforce.P4
{
	/// <summary>
	/// Class representing a directory in a Perforce depot and/or client workspace.
	/// </summary>
	public class P4Directory
	{
		private Repository _repository = null;

		private P4CommandResult results;
		/// <summary>
		/// Results of the last command run on this directory (if any)
		/// </summary>
		public P4CommandResult LastCommandResults { get { return results; } }

		/// <summary>
		/// The workspace containing this directory
		/// </summary>
		public Client Workspace;

		private String depotPath = String.Empty;

		/// <summary>
		///  The path in the Perforce depot, if known
		/// </summary>
		public String DepotPath
		{
			get { return depotPath; }
		}

		public FileSpec DepotSpec
		{
			get
			{
				return FileSpec.DepotSpec(string.Format("{0}/*", depotPath));
			}
		}

		public FileSpec RecursiveSpec
		{
			get
			{
				return FileSpec.DepotSpec(string.Format("{0}/...", depotPath));
			}
		}

		private String localPath = String.Empty;

		/// <summary>
		/// The path in the local file system, if known
		/// </summary>
		public String LocalPath
		{
			get { return localPath; }
			set { localPath = value; }
		}

		// This directory exists in the depot
		private bool inDepot = false;
		/// <summary>
		/// Is this directory exists in the depot?
		/// </summary>
		public bool InDepot
		{
			get { return inDepot; }
			set { inDepot = value; }
		}

		// This directory exists in the workspace 
		private bool inWorkspace = false;
		/// <summary>
		/// Is this directory in the workspace?
		/// </summary>
		public bool InWorkspace
		{
			get { return inWorkspace; }
			set { inWorkspace = value; }
		}

		private P4Directory parentDirectory = null;

		/// <summary>
		/// The parent directory
		/// </summary>
		public P4Directory ParentDirectory
		{
			get { return parentDirectory; }
		}

		private String name = String.Empty;
		/// <summary>
		/// Directory name
		/// </summary>
		public String Name
		{
			get { return name; }
		}

		private P4DirectoryMap subdirectories;
		/// <summary>
		/// List of child P4Directories 
		/// </summary>
		public IList<P4Directory> Subdirectories
		{
			get { return (List<P4Directory>)subdirectories; }
		}

		private P4FileMap files;
		/// <summary>
		/// List of P4Files in the directory
		/// </summary>
		public IList<FileMetaData> Files
		{
			get { return (List<FileMetaData>)files; }
		}

		private void Init(Repository repository, Client nWorkspace, String nName, String nDepotPath, String nLocalPath, P4Directory parent)
		{
			if (repository == null)
			{
				throw new ArgumentNullException("server", "P4Directory requires a valid Repository");
			}
			if (String.IsNullOrEmpty(nDepotPath) && String.IsNullOrEmpty(nLocalPath))
			{
				throw new ArgumentNullException("nDepotPath/nLocalPath",
					"Must provide either the local path or path in the depot.");
			}

			_repository = repository;
			Workspace = nWorkspace;
			name = nName;
			depotPath = nDepotPath;
			localPath = nLocalPath;

			// We'll determine the unspecified paths when( if) we add files to a 
			// directory, because the server will provide us with enough 
			// information to accurately determine the correct paths, because 
			// depending on the mapping, the structure of the two trees might
			// differ in the levels between the roots and the first directories
			// down ith tree containing files.

			//if( String.IsNullOrEmpty( nLocalPath ) && parent != null && !String.IsNullOrEmpty( parent.LocalPath ) )
			//{
			//    // see if we can determine the local directory if not specified
			//    localPath = String.Format( "{0}/{1}", parent.LocalPath, Path.GetFileName( depotPath ) );
			//}
			//if( String.IsNullOrEmpty( nDepotPath ) && parent != null && !String.IsNullOrEmpty( parent.DepotPath ) )
			//{
			//    // see if we can determine the depot directory if not specified
			//    depotPath = String.Format( "{0}/{1}", parent.DepotPath, Path.GetFileName( localPath ) );
			//}
			parentDirectory = parent;
		}

		/// <summary>
		/// Create a new P4Directory
		/// </summary>
		/// <param name="server">Perforce Server</param>
		/// <param name="nWorkspace">Active workspace(client), can be null</param>
		/// <param name="nName">The name of the directory</param>
		/// <param name="nDepotPath">Full path in the depot, can be null</param>
		/// <param name="nLocalPath">Full path in the local file system, can be null</param>
		/// <param name="parent">Parent directory, can be null</param>
		/// <remarks>
		/// Either the depot or local path can be null, not both.
		/// </remarks>
		public P4Directory(Repository repository, Client nWorkspace, String nName, String nDepotPath, String nLocalPath, P4Directory parent)
		{
			Init(repository, nWorkspace, nName, nDepotPath, nLocalPath, parent);
		}

		/// <summary>
		/// Create a new P4Directory
		/// </summary>
		/// <param name="server">Perforce Server</param>
		/// <param name="nWorkspace">Active workspace(client), can be null</param>
		/// <param name="nDepotPath">Full path in the depot, can be null</param>
		/// <param name="nLocalPath">Full path in the local file system, can be null</param>
		/// <param name="parent">Parent directory, can be null</param>
		/// <remarks>
		/// Either the depot or local path can be null, not both. The file name
		/// will be taken from the depot path if provided, otherwise from the
		/// local path
		/// </remarks>
		public P4Directory(Repository repository, Client nWorkspace, String nDepotPath, String nLocalPath, P4Directory parent)
		{
			String nName;
			if (!String.IsNullOrEmpty(nDepotPath))
				nName = nDepotPath.Substring(nDepotPath.LastIndexOf('/') + 1);
			else if (!String.IsNullOrEmpty(nLocalPath))
				nName = nLocalPath.Substring(nLocalPath.LastIndexOf('/') + 1);
			else
				nName = "????";
			Init(repository, nWorkspace, nName, nDepotPath, nLocalPath, parent);
		}

		/// <summary>
		/// Expand the directory by filling in the list of child directories
		/// and file within the directory.
		/// </summary>
		/// <returns>false if an error prevented completion</returns>
		public bool Expand()
		{
			if (String.IsNullOrEmpty(depotPath))
				return false;

			// if we have the depot path, get a list of the subdirectories from the depot
			if (!String.IsNullOrEmpty(depotPath))
			{
				IList<string> subdirs = _repository.GetDepotDirs(null, String.Format("{0}/*", depotPath));
				if ((subdirs != null) && (subdirs.Count >0))
				{
					subdirectories = P4DirectoryMap.FromDirsOutput(_repository, Workspace, this, subdirs);
					foreach (P4Directory dir in subdirectories.Values)
					{
						dir.InDepot = true;
					}
				}

				IList<FileMetaData> fileList = _repository.GetFileMetaData(null, FileSpec.DepotSpec(String.Format("{0}/*", depotPath)));
				// get a list of the files in the directory

				if (fileList != null)
				{
					files = P4FileMap.FromFstatOutput(fileList);

					// if the directory contains files from the depot, we can use 
					// the local path of one of those files to determine the local 
					// path for this directory
					if ((String.IsNullOrEmpty(localPath)) && (files != null) && (files.Count > 0))
					{

						foreach (FileMetaData f in files.Values)
						{
							if ((f.LocalPath != null) && !String.IsNullOrEmpty(f.LocalPath.Path))
							{
								localPath = f.LocalPath.GetDirectoryName();
								break;
							}
						}
					}
				}
			}
			// if we have a workspace and a local path, match the files and 
			// subdirectories in the depot with the files in the file system
			if ((!String.IsNullOrEmpty(localPath)) && (Workspace != null))
			{
				DirectoryInfo[] directoryList = null;
				FileInfo[] fileList = null;
				try
				{
					DirectoryInfo di = new DirectoryInfo(localPath);

					directoryList = di.GetDirectories();
					fileList = di.GetFiles();

				}
				catch (Exception ex)
				{
					//LogFile.LogException( "Initializing Directory from Workspace", ex );
				}

				// get the subdirectories listed in the file and match them up 
				// with the one in the list from the depot
				if ((directoryList != null) && (directoryList.Length > 0))
				{
					foreach (DirectoryInfo di in directoryList)
					{
						string itemName = di.Name;
						if (subdirectories.ContainsKey(itemName))
						{
							subdirectories[itemName].InWorkspace = true;
						}
						else
						{
							P4Directory subDir = new P4Directory(_repository, Workspace, itemName, null, di.FullName, parentDirectory);
							subDir.InDepot = false;
							subDir.InWorkspace = true;
							subdirectories[itemName] = subDir;
						}
					}
				}

				// get the files listed in the subdirectory and match them up 
				// with the one in the list from the depot
				if ((fileList != null) && (fileList.Length > 0))
				{
					foreach (FileInfo fi in fileList)
					{
						string itemName = fi.Name;
						if (files.ContainsKey(itemName) == false)
						{
							FileMetaData file = new FileMetaData();
							file.LocalPath = new LocalPath(fi.FullName);
							file.DepotPath = null;
							file.FileSize = fi.Length;
							files[itemName] = file;
						}
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Search for a depot file in the sub tree of this directory 
		/// </summary>
		/// <param name="searchStr">Name of file to find</param>
		/// <returns></returns>
		//public IList<File> Search(String searchStr)
		//{
		//    IList<File> files = _repository.GetFiles()

		//    P4Command cmd = new P4Command( _repository, "files", true, 
		//        String.Format( "{0}/.../{1}", DepotPath, searchStr ) );

		//    P4FileList files = null;

		//    results = cmd.Run();
		//    if (results.Success)
		//    {
		//        files = P4FileList.FromFilesOutput(_repository, results.TaggedOutput);
		//    }
		//    return files;
		//}
	}

	/// <summary>
	/// A dictionary of P4Directory objects keyed by the directory name
	/// </summary>
	/// <remarks>
	/// No provision is made for a collision directory of names, so should only create
	/// a mapping for directories that are known to have unique names, such as the 
	/// sub directories within a given directory.
	/// </remarks>
	public class P4DirectoryMap : Dictionary<String, P4Directory>
	{
		/// <summary>
		/// Create a directory map from the output of a 'dirs' command
		/// </summary>
		/// <param name="pserver">Perforce server</param>
		/// <param name="Workspace">Active workspace, if any</param>
		/// <param name="parent">Parent directory for all the directories in the map</param>
		/// <param name="dirsOutput">Output from a 'dirs' command</param>
		/// <returns>THe list of P4Directories</returns>
		public static P4DirectoryMap FromDirsOutput(Repository repository, Client Workspace, P4Directory parent, IList<string> dirsOutput)
		{
			if (dirsOutput == null || dirsOutput.Count < 1)
				return null;

			P4DirectoryMap value = new P4DirectoryMap();

			foreach (string path in dirsOutput)
			{
				P4Directory dir = new P4Directory(repository, Workspace, path, null, parent);
				value[dir.Name] = dir;
				dir.InDepot = true;
				dir.InWorkspace = false; // won't know till later
			}
			return value;
		}

		/// <summary>
		/// Cast a P4Directory map to a P4DirectoryList
		/// </summary>
		/// <param name="m"></param>
		/// <returns></returns>
		public static implicit operator List<P4Directory>(P4DirectoryMap m)
		{
			if (m == null)
				return null;

			List<P4Directory> value = new List<P4Directory>();
			foreach (P4Directory d in m.Values)
				value.Add(d);
			return value;
		}
	}

	/// <summary>
	/// A dictionary of FileMetaData objects keyed by the name of the file.
	/// </summary>
	/// <remarks>
	/// No provision is made for a collision  of file names, so  should only create
	/// a mapping for files that are known to have unique names, such as within a 
	/// given directory.
	/// </remarks>
	public class P4FileMap : Dictionary<String, FileMetaData>
	{
		/// <summary>
		/// Create a P4FileMap from the output of an fstat command
		/// </summary>
		/// <param name="pServer"></param>
		/// <param name="fstatInfo"></param>
		/// <returns></returns>
		public static P4FileMap FromFstatOutput(IList<FileMetaData> fileList)
		{
			if (fileList == null || fileList.Count <= 0)
				return null;

			P4FileMap value = new P4FileMap();
			foreach (FileMetaData fsInfo in fileList)
			{
				value[fsInfo.GetFileName()] = fsInfo;
			}
			return value;
		}

		/// <summary>
		/// Cast the map to IList of FileMetaData
		/// </summary>
		/// <param name="m"></param>
		/// <returns></returns>
		public static implicit operator List<FileMetaData>(P4FileMap m)
		{
			if (m == null)
				return null;

			List<FileMetaData> value = new List<FileMetaData>();
			foreach (FileMetaData d in m.Values)
				value.Add(d);
			return value;
		}
	}

}
