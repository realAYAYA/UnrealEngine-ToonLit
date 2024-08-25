// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading.Tasks;

namespace Jupiter
{
	internal static class TaskUtils
	{
		public static byte[] CombineIntoSingleBuffer(Task<byte[]>[] tasks)
		{
			int totalLength = tasks.Sum(task => task.Result.Length);
			byte[] buffer = new byte[totalLength];
			int index = 0;
			foreach (byte[] partialObject in tasks.Select(t => t.Result))
			{
				Array.Copy(partialObject, 0, buffer, index, partialObject.Length);
				index += partialObject.Length;
			}

			return buffer;
		}
	}
}
