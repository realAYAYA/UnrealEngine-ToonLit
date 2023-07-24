// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System;
using System.Drawing;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace DatasmithSolidworks
{
	public static class MaterialUtils
	{
		public static int GetMaterialID(RenderMaterial RenderMat)
		{
			string MaterialStringID = $"{RenderMat.FileName}_{RenderMat.PrimaryColor.ToString()}";
			MD5 MD5Hasher = MD5.Create();
			byte[] Hashed = MD5Hasher.ComputeHash(Encoding.UTF8.GetBytes(MaterialStringID));
			int ID = Math.Abs(BitConverter.ToInt32(Hashed, 0));
			return ID;
		}

		public static Color ConvertColor(int InABGR)
		{
			int A = (int)(byte)(InABGR >> 24);
			int B = (int)(byte)(InABGR >> 16);
			int G = (int)(byte)(InABGR >> 8);
			int R = (int)(byte)InABGR;
			return Color.FromArgb(A, R, G, B);
		}

		public static string ComputeAssemblySideTexturePath(string InOriginalTexturePath)
		{
			return Path.GetDirectoryName(Addin.Instance.CurrentDocument.GetPathName()) + "\\" + Path.GetFileName(InOriginalTexturePath);
		}

		public static string GetDataPath()
		{
			return Path.Combine(new string[] { Addin.Instance.SolidworksApp.GetExecutablePath(), "data" });
		}
	};
}