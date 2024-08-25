// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Security.Cryptography;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Core.Tests
{
	[TestClass]
	public class BuzHashTests
	{
		[TestMethod]
		public void AddSub()
		{
			byte[] bytes = RandomNumberGenerator.GetBytes(4096);

			const int WindowLength = 32;

			uint rollingHash = BuzHash.Add(0, bytes.AsSpan(0, WindowLength - 1));
			for (int idx = WindowLength; idx < bytes.Length; idx++)
			{
				rollingHash = BuzHash.Add(rollingHash, bytes[idx - 1]);

				uint newRollingHash = BuzHash.Add(0, bytes.AsSpan(idx - WindowLength, WindowLength));
				Assert.AreEqual(rollingHash, newRollingHash);

				rollingHash = BuzHash.Sub(rollingHash, bytes[idx - WindowLength], WindowLength);
			}
		}

		[TestMethod]
		public void AddSubStepped()
		{
			byte[] bytes = RandomNumberGenerator.GetBytes(4096);

			const int WindowLength = 32;
			const int WindowStep = 8;

			uint rollingHash = BuzHash.Add(0, bytes.AsSpan(0, WindowLength - WindowStep));
			for (int idx = WindowLength; idx < bytes.Length; idx += WindowStep)
			{
				rollingHash = BuzHash.Add(rollingHash, bytes.AsSpan(idx - WindowStep, WindowStep));

				uint newRollingHash = BuzHash.Add(0, bytes.AsSpan(idx - WindowLength, WindowLength));
				Assert.AreEqual(rollingHash, newRollingHash);

				rollingHash = BuzHash.Sub(rollingHash, bytes.AsSpan(idx - WindowLength, WindowStep), WindowLength);
			}
		}
	}
}
