// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSyncAutomation
{
	class Program
	{
		enum AutomationRequestType
		{
			SyncProject,
			FindProject,
			OpenProject
		}

		enum AutomationRequestResult
		{
			Ok,
			Invalid,
			Busy,
			Canceled,
			Error,
			NotFound
		}

		static void Main(string[] Arguments)
		{
			// Create the request data
			MemoryStream InputDataStream = new MemoryStream();
			using(BinaryWriter Writer = new BinaryWriter(InputDataStream))
			{
				Writer.Write("//UE4/Main");
				Writer.Write("/Samples/Games/ShooterGame/ShooterGame.uproject");
			}

			Tuple<AutomationRequestResult, byte[]> Response = SendRequest(AutomationRequestType.OpenProject, InputDataStream.ToArray());
		
			string ResponseString = Encoding.UTF8.GetString(Response.Item2);
			Console.WriteLine("{0}: {1}", Response.Item1, ResponseString);
		}

		static Tuple<AutomationRequestResult, byte[]> SendRequest(AutomationRequestType Request, byte[] RequestData)
		{
			int PortNumber = (int)Registry.GetValue("HKEY_CURRENT_USER\\Software\\Epic Games\\UnrealGameSync", "AutomationPort", null);
			using(TcpClient Client = new TcpClient())
			{
				Client.Connect(new IPEndPoint(IPAddress.Loopback, PortNumber));

				using(NetworkStream Stream = Client.GetStream())
				{
					// Send the request
					BinaryWriter Writer = new BinaryWriter(Stream);
					Writer.Write((int)Request);
					Writer.Write(RequestData.Length);
					Writer.Write(RequestData);

					// Read the response
					BinaryReader Reader = new BinaryReader(Stream);
					AutomationRequestResult Result = (AutomationRequestResult)Reader.ReadInt32();
					int ResponseLength = Reader.ReadInt32();
					byte[] ResponseData = Reader.ReadBytes(ResponseLength);

					// Return the response
					return Tuple.Create(Result, ResponseData);
				}
			}
		}
	}
}
