// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using RestSharp;

namespace Jupiter
{
    public static class ExtensionUtils
    {
        public static Exception ToException(this IRestResponse response, string message)
        {
            return new Exception($"{message}. Status Code: {response.StatusCode}. Error: {response.ErrorMessage}. Body: {response.Content}. Url: {response.ResponseUri}");
        }
    }
}
