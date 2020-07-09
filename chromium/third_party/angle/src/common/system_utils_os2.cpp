//
// Copyright (c) 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_os2.cpp: Implementation of OS-specific functions for OS/2

#include "system_utils.h"

#include <stdlib.h>

#include <array>

namespace angle
{
std::string GetExecutablePath()
{
    char path[PATH_MAX];

    // NOTE: Use realrealpath to get a complete native path w/o any pathrewrite
    // references, with proper casing and Unix slashes.
    if (_execname(path, PATH_MAX) == -1 || !_realrealpath(path, path, PATH_MAX))
        return "";

    return path;
}

std::string GetExecutableDirectory()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}

const char *GetSharedLibraryExtension()
{
    return "dll";
}
}  // namespace angle
