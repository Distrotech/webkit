/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginDatabaseWin.h"

#include "PluginPackageWin.h"
#include "PluginViewWin.h"
#include "FrameWin.h"
#include <windows.h>
#include <shlwapi.h>

namespace WebCore {

PluginDatabaseWin* PluginDatabaseWin::installedPlugins()
{
    static PluginDatabaseWin* plugins = 0;
    
    if (!plugins) {
        plugins = new PluginDatabaseWin;
        plugins->setPluginPaths(PluginDatabaseWin::defaultPluginPaths());
        plugins->refresh();
    }

    return plugins;
}

void PluginDatabaseWin::refresh()
{   
    PluginSet newPlugins;

    // Create a new set of plugins
    newPlugins = getPluginsInPaths();

    if (!m_plugins.isEmpty()) {
        m_registeredMIMETypes.clear();

        PluginSet pluginsToUnload = m_plugins;

        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            pluginsToUnload.remove(*it);

        end = m_plugins.end();
        for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
            newPlugins.remove(*it);

        // Unload plugins
        end = pluginsToUnload.end();
        for (PluginSet::const_iterator it = pluginsToUnload.begin(); it != end; ++it)
            m_plugins.remove(*it);

        // Add new plugins
        end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);
    } else {
        m_plugins = newPlugins;
        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);
    }

    // Register plug-in MIME types
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        // Get MIME types
        MIMEToDescriptionsMap::const_iterator map_end = (*it)->mimeToDescriptions().end();
        for (MIMEToDescriptionsMap::const_iterator map_it = (*it)->mimeToDescriptions().begin(); map_it != map_end; ++map_it) {
            m_registeredMIMETypes.add(map_it->first);
        }
    }
}

Vector<PluginPackageWin*> PluginDatabaseWin::plugins() const
{
    Vector<PluginPackageWin*> result;

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
        result.append((*it).get());

    return result;
}

PluginSet PluginDatabaseWin::getPluginsInPaths() const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;
    PluginSet plugins;

    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findFileData;

    Vector<String>::const_iterator end = m_pluginPaths.end();
    for (Vector<String>::const_iterator it = m_pluginPaths.begin(); it != end; ++it) {
        String pattern = *it + "\\*";

        hFind = FindFirstFileW(pattern.charactersWithNullTermination(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE)
            continue;

        do {
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            String filename = String(findFileData.cFileName, wcslen(findFileData.cFileName));
            if (!filename.startsWith("np", false) || !filename.endsWith(".dll", false))
                continue;

            String fullPath = *it + "\\" + filename;
            if (!uniqueFilenames.add(fullPath).second)
                continue;
        
            PluginPackageWin* pluginPackage = PluginPackageWin::createPackage(fullPath, findFileData.ftLastWriteTime);

            if (pluginPackage)
                plugins.add(pluginPackage);

        } while (FindNextFileW(hFind, &findFileData) != 0);

        FindClose(hFind);
    }

    return plugins;
}

Vector<String> PluginDatabaseWin::defaultPluginPaths()
{
    Vector<String> paths;

    // Enumerate all Mozilla plugin directories in the registry
    HKEY key;
    LONG result;
    
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Mozilla"), 0, KEY_READ, &key);
    if (result == ERROR_SUCCESS) {
        WCHAR name[128];
        FILETIME lastModified;

        // Enumerate subkeys
        for (int i = 0;; i++) {
            DWORD nameLen = sizeof(name) / sizeof(WCHAR);
            result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

            if (result != ERROR_SUCCESS)
                break;

            String extensionsPath = String(name, nameLen) + "\\Extensions";
            HKEY extensionsKey;

            // Try opening the key
            result = RegOpenKeyEx(key, extensionsPath.charactersWithNullTermination(), 0, KEY_READ, &extensionsKey);

            if (result == ERROR_SUCCESS) {
                // Now get the plugins path
                WCHAR pluginsPathStr[_MAX_PATH];
                DWORD pluginsPathSize = sizeof(pluginsPathStr);
                DWORD type;

                result = RegQueryValueEx(extensionsKey, TEXT("Plugins"), 0, &type, (LPBYTE)&pluginsPathStr, &pluginsPathSize);

                if (result == ERROR_SUCCESS && type == REG_SZ)
                    paths.append(String(pluginsPathStr, pluginsPathSize / sizeof(WCHAR) - 1));

                RegCloseKey(extensionsKey);
            }
        }
        
        RegCloseKey(key);
    }

    // The WMP plugin segfaults for some reason so it's disabled for now
#if 0
    // Windows Media Player
    {
        DWORD type;
        WCHAR installationDirectoryStr[_MAX_PATH];
        DWORD installationDirectorySize = sizeof(installationDirectoryStr);

        result = SHGetValue(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\MediaPlayer"), TEXT("Installation Directory"), &type, (LPBYTE)&installationDirectoryStr, &installationDirectorySize);

        if (result == ERROR_SUCCESS && type == REG_SZ)
            paths.append(String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1));
    }
#endif

    // QuickTime
    {
        DWORD type;
        WCHAR installationDirectoryStr[_MAX_PATH];
        DWORD installationDirectorySize = sizeof(installationDirectoryStr);

        result = SHGetValue(HKEY_LOCAL_MACHINE, TEXT("Software\\Apple Computer, Inc.\\QuickTime"), TEXT("InstallDir"), &type, (LPBYTE)&installationDirectoryStr, &installationDirectorySize);

        if (result == ERROR_SUCCESS && type == REG_SZ) {
            String pluginDir = String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1) + "\\plugins";
            paths.append(pluginDir);
        }
    }

    // FIXME: We should get other plugin directories, for example Java, Flash etc. 

    return paths;
}

bool PluginDatabaseWin::isMIMETypeRegistered(const String& mimeType) const
{
    return m_registeredMIMETypes.contains(mimeType);
}

PluginPackageWin* PluginDatabaseWin::pluginForMIMEType(const String& mimeType)
{
    String key = mimeType.lower();

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if ((*it)->mimeToDescriptions().contains(key))
            return (*it).get();
    }

    return 0;
}

PluginPackageWin* PluginDatabaseWin::pluginForExtension(const String& extension)
{
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        MIMEToExtensionsMap::const_iterator mime_end = (*it)->mimeToExtensions().end();

        for (MIMEToExtensionsMap::const_iterator mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            const Vector<String>& extensions = mime_it->second;

            for (unsigned i = 0; i < extensions.size(); i++) {
                if (extensions[i] == extension)
                    return (*it).get();
            }
        }
    }

    return 0;
}

PluginViewWin* PluginDatabaseWin::createPluginView(FrameWin* parentFrame, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
{
    PluginPackageWin* plugin = 0;
    
    if (!mimeType.isNull())
        plugin = pluginForMIMEType(mimeType);
    
    if (!plugin) {
        DeprecatedString path = url.path();
        String extension = path.mid(path.findRev('.') + 1);

        plugin = pluginForExtension(extension);

        // FIXME: if no plugin could be found, query Windows for the mime type 
        // corresponding to the extension.
    }

    if (!plugin || !plugin->load())
        return 0;

    return new PluginViewWin(parentFrame, plugin, element, url, paramNames, paramValues, mimeType);
}

}
