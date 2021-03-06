/**
 * ***** BEGIN LICENSE BLOCK *****
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is BrowserPlus (tm).
 * 
 * The Initial Developer of the Original Code is Yahoo!.
 * Portions created by Yahoo! are Copyright (c) 2010 Yahoo! Inc.
 * All rights reserved.
 * 
 * Contributor(s): 
 * ***** END LICENSE BLOCK *****
 */

#include "Utils.h"
#include "BPUtils/BPLog.h"
#include "BPUtils/bpstrutil.h"
#include "platform_utils/bpconfig.h"
#include "platform_utils/ProductPaths.h"

using namespace std;
using namespace bp::paths;
using namespace bp::strutil;
using namespace bp::error;
using namespace bp::install;
namespace bpf = bp::file;
namespace bfs = boost::filesystem;


static vector<string> s_mimeTypes;
static vector<bp::SemanticVersion> s_installedVersions;
#ifdef WIN32
static string s_activeXGuid;
static string s_typeLibGuid;
static string s_controlPanelGuid;
#endif

void 
bp::install::utils::readPlatformInfo(const bfs::path& path)
{
    bp::config::ConfigReader reader;
    if (!reader.load(path)) {
        BP_THROW("Unable to load " + path.string());
    }
    list<string> strList;
    if (!reader.getArrayOfStrings("mimeTypes", strList)) {
        BP_THROW("Unable to read mimeTypes from " + path.string());
    }
    list<string>::const_iterator it;
    for (it = strList.begin(); it != strList.end(); ++it) {
        if (it->length() > 0) {
            s_mimeTypes.push_back(*it);
        }
    }
#ifdef WIN32
    if (!reader.getStringValue("activeXGuid", s_activeXGuid)) {
        BP_THROW("Unable to read activeXGuid from " + path.string());
    }
    if (!reader.getStringValue("typeLibGuid", s_typeLibGuid)) {
        BP_THROW("Unable to read typeLibGuid from " + path.string());
    }
    if (!reader.getStringValue("controlPanelGuid", s_controlPanelGuid)) {
        BP_THROW("Unable to read controlPanelGuid from " + path.string());
    }
#endif

    // determine what other versions of this major rev exist on disk
    bfs::path dir = getProductTopDirectory();
    if (bpf::isDirectory(dir)) {
        bfs::directory_iterator end;
        try {
            for (bfs::directory_iterator it(dir); it != end; ++it) {
                bp::SemanticVersion version;
                if (version.parse(it->path().filename().string())) {
                    bfs::path installedPath = getBPInstalledPath(version.majorVer(),
                                                                 version.minorVer(),
                                                                 version.microVer());
                    if (bpf::pathExists(installedPath)) {
                        s_installedVersions.push_back(version);
                    }
                }
            }
        } catch (const bfs::filesystem_error& e) {
            BPLOG_WARN_STRM("unable to iterate thru " << dir
                            << ": " << e.what());
        }
    }
}


vector<string> 
bp::install::utils::mimeTypes()
{
    return s_mimeTypes;
}



vector<bp::SemanticVersion>
bp::install::utils::installedVersions()
{
    return s_installedVersions;
}


#ifdef WIN32
string
bp::install::utils::activeXGuid()
{
    return s_activeXGuid;
}


string
bp::install::utils::typeLibGuid()
{
    return s_typeLibGuid;
}


string
bp::install::utils::controlPanelGuid()
{
    return s_controlPanelGuid;
}

#endif
