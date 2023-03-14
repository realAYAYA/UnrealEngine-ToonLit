// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Reflection;
using System.Runtime.InteropServices;
using EpicGames.Core;

namespace Horde.Build.Tests;

public abstract class DatabaseRunner
{
	private readonly string _binName;
	private readonly int _defaultPort;
	private readonly string _name;
	private readonly bool _printStdErr;
	private readonly bool _printStdOut;
	private readonly bool _reuseProcess;
	protected string TempDir { get; }
	private Process? _proc;

	protected DatabaseRunner(string name, string binName, int defaultPort, bool reuseProcess, bool printStdOut = false,
		bool printStdErr = true)
	{
		_name = name;
		_binName = binName;
		_defaultPort = defaultPort;
		_reuseProcess = reuseProcess;
		_printStdOut = printStdOut;
		_printStdErr = printStdErr;
		TempDir = GetTemporaryDirectory();
	}

	protected int Port { get; private set; } = -1;

	protected abstract string GetArguments();

	public void Start()
	{
		if (_reuseProcess && !IsPortAvailable(_defaultPort))
		{
			Console.WriteLine($"Re-using already running {_name} process!");
			Port = _defaultPort;
			return;
		}

		if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
		{
			Console.WriteLine($"Unable to find a running {_name} process to use during testing!");
			Console.WriteLine("This is required on any non-Windows as the runner can only start Windows binaries!");
			Console.WriteLine($"Please ensure {_binName} is running on the default port {_defaultPort}.");
			throw new Exception("Failed finding process to re-use! See stdout for info.");
		}

		if (_proc != null)
		{
			return;
		}

		Port = GetAvailablePort();

		Process p = new();
		if (_printStdOut)
		{
			p.OutputDataReceived += (_, args) => Console.WriteLine("{0} stdout: {1}", _name, args.Data);
		}

		if (_printStdErr)
		{
			p.ErrorDataReceived += (_, args) => Console.WriteLine("{0} stderr: {1}", _name, args.Data);
		}

		p.StartInfo.FileName = GetBinaryPath();
		p.StartInfo.WorkingDirectory = TempDir;
		p.StartInfo.Arguments = GetArguments();
		p.StartInfo.UseShellExecute = false;
		p.StartInfo.CreateNoWindow = true;
		p.StartInfo.RedirectStandardOutput = true;
		p.StartInfo.RedirectStandardError = true;

		if (!p.Start())
		{
			throw new Exception("Process start failed!");
		}

		p.BeginOutputReadLine();
		p.BeginErrorReadLine();

		// Try detect when main .NET process exits and kill the runner
		AppDomain.CurrentDomain.ProcessExit += (sender, eventArgs) =>
		{
			Console.WriteLine("Main process exiting!");
			Stop();
		};

		_proc = p;
	}

	public void Stop()
	{
		if (_proc != null)
		{
			_proc.Kill(true);

			// Waiting for exit blocks excessively even though the kill was sent. Anti-virus interfering?
			// Process eventually shuts down but takes 2-3 min in Redis case. ReuseProcess flags circumvents this.
			//Proc.WaitForExit();
			_proc = null;
			DeleteDirectory(TempDir);
		}
	}

	public (string Host, int Port) GetListenAddress()
	{
		return ("localhost", Port);
	}

	private string GetBinaryPath()
	{
		FileReference file = new(new Uri(Assembly.GetExecutingAssembly().Location).LocalPath);
		FileReference binPath = FileReference.Combine(file.Directory, _binName);
		return binPath.FullName;
	}

	private string GetTemporaryDirectory()
	{
		string temp = Path.Join(Path.GetTempPath(), $"horde-{_name}-" + Path.GetRandomFileName());
		Directory.CreateDirectory(temp);
		return temp;
	}

	private static int GetAvailablePort()
	{
		TcpListener listener = new(IPAddress.Loopback, 0);
		listener.Start();
		int port = ((IPEndPoint)listener.LocalEndpoint).Port;
		listener.Stop();
		return port;
	}

	private static bool IsPortAvailable(int port)
	{
		if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
		{
			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();

			IPEndPoint[] listeners = ipGlobalProperties.GetActiveTcpListeners();
			if (listeners.Any(x => x.Port == port))
			{
				return false;
			}

			return true;
		}

		TcpListener? listenerAny = null;
		TcpListener? listenerLoopback = null;
		try
		{
			listenerAny = new TcpListener(IPAddress.Loopback, port);
			listenerAny.Start();
			listenerLoopback = new TcpListener(IPAddress.Any, port);
			listenerLoopback.Start();
			return true;
		}
		catch (SocketException)
		{
		}
		finally
		{
			listenerAny?.Stop();
			listenerLoopback?.Stop();
		}

		return false;
	}

	private static void DeleteDirectory(string path)
	{
		DirectoryInfo dir = new(path) { Attributes = FileAttributes.Normal };
		foreach (FileSystemInfo info in dir.GetFileSystemInfos("*", SearchOption.AllDirectories))
		{
			info.Attributes = FileAttributes.Normal;
		}

		dir.Delete(true);
	}
}

public class MongoDbRunnerLocal : DatabaseRunner
{
	public MongoDbRunnerLocal() : base("mongodb", "ThirdParty/Mongo/mongod.exe", 27017, true)
	{
	}

	protected override string GetArguments()
	{
		return $"--dbpath {TempDir} --noauth --quiet --port {Port}";
	}

	public string GetConnectionString()
	{
		(string host, int listenPort) = GetListenAddress();
		return $"mongodb://{host}:{listenPort}";
	}
}

public class RedisRunner : DatabaseRunner
{
	public RedisRunner() : base("redis", "ThirdParty/Redis/redis-server.exe", 6379, true)
	{
	}

	protected override string GetArguments()
	{
		return $"--port {Port} --save \"\" --appendonly no";
	}
}