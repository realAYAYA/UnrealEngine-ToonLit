// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once
#include <filesystem>

namespace FileSystem
{
    std::wstring GetRelativePathWithTrailingSeparator(std::wstring from, std::wstring to);
    std::wstring GetBasePath(const std::wstring& path);
    std::wstring GetFullPath(const std::wstring& path);
    std::wstring CreateSubFolder(const std::wstring& parentPath, const std::wstring& subFolderName);
    std::wstring CreateTempFolder();
};

