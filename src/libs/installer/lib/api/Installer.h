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

#include <memory>
#include <string>
#include "BPUtils/bpsemanticversion.h"
#include "BPUtils/bptr1.h"
#include "BPUtils/bpfile.h"
#include "BPUtils/BPLog.h"


// The installer/updater depends upon having the following directory
// structure in "dir", wher "dir" is a version number (e.g. "2.7.0")
//  dir
//      daemon
//          <the bits that will be copied to the versioned daemon dir>
//      permissions
//          BrowserPlus.crt
//          Permissions
//          configAutoUpatePermissions [optional]
//          updateDomainPermissions [optional]
//          configDomainPermissions [optional]
//      plugins
//          IE (windows only)
//              <IE plugin>
//          NPAPI (windows only)
//              <npapi plugin>
//          BrowserPlus_<version>.plugin (OSX only)
//              <OSX npapi plugin>
//      prefPane
//          <preference panel bits>
//      services [optional]
//          <any services to be pre-installed, e.g. TextToSpeech]/1.0.1
//      BrowserPlusUninstaller
//      BrowserPlusUpdater
//      platformInfo.json
//      strings.json
//      ybang.ico (windows only)
//
// The installation order is:
//      preflight
//      daemon
//      permissions
//      plugins
//      prefPanel
//      services
//      uninstaller
//      makeLinks
//      postflight
//      allDone

namespace bp {
namespace install {
    
class IInstallerListener
{
 public:
    // Listeners will get a stream on status/error/progress
    // callbacks.  Prior to version 2.6, a progress of 100%
    // indicates that the install is done.  In 2.6 and later,
    // onDone() will be called when the installer is finished, 
    // both successfully and with errors.
    virtual ~IInstallerListener() {};
    virtual void onStatus(const std::string& msg) = 0;
    virtual void onError(const std::string& msg) = 0;
    // progress callbacks are invoked during run()
    virtual void onProgress(unsigned int pct) = 0;
    virtual void onDone() = 0;
};

class Installer 
{
 public:
    static const char* kProductName;
    static const char* kProductNameShort;
    static const char* kInstallerTitle;
    static const char* kNotAdmin; 
    static const char* kInstallerAlreadyRunning;
    static const char* kStartingInstallation;
    static const char* kCopyingDaemonBits;
    static const char* kUpdatingPermissions;
    static const char* kInstallingPlugins;
    static const char* kInstallingPreferencePane;
    static const char* kInstallingBundledServices;
    static const char* kInstallingUninstaller;
    static const char* kFinishingUp;
    static const char* kErrorEncountered;
    static const char* kConfigLink;
    static const char* kUninstallLink;
    static const char* kPlatformDownloading;
    static const char* kServicesDownloading;
    static const char* kNewerVersionInstalled;

    Installer(const boost::filesystem::path& dir,
			  const boost::filesystem::path& logFile,
			  bp::log::Level logLevel,
              bool deleteWhenDone = false);
    virtual ~Installer();

    void setListener(std::tr1::weak_ptr<IInstallerListener> listener) {
        m_listener = listener;
    }

    // begin installation, inside this call the listener's 
    void run(); // throws std::string

    static void setLocalizedStringsPath(const boost::filesystem::path& path,
                                        const std::string& locale) {
        s_locale = locale;
        s_stringsPath = path;
    }
    static std::string getString(const char* key);

 protected:
    void preflight();
    void installDaemon();
    void installPermissions();
    void installServices();
    void installPlugins();
    void installPrefPanel();
    void installUninstaller();
    void makeLinks();
    void postflight();
    void allDone();
    void sendProgress(unsigned int pct);
    void sendStatus(const std::string& s);
    void sendError(const std::string&s);
    void sendDone();
    void doCopy(const boost::filesystem::path& src,
                const boost::filesystem::path& dest);  // throws
    void doSingleFileCopy(const boost::filesystem::path& src,
                          const boost::filesystem::path& dest);
    void removePlatform(const bp::SemanticVersion& version);
    void disablePlugins(const bp::SemanticVersion& version);
    bool filesAreIdentical(const boost::filesystem::path& f1,
                           const boost::filesystem::path& f2);

    boost::filesystem::path m_dir;
	boost::filesystem::path m_logFile;
	bp::log::Level m_logLevel;
    bool m_deleteWhenDone;
    bp::SemanticVersion m_version;
    std::tr1::weak_ptr<IInstallerListener> m_listener;

    static std::string s_locale;
    static boost::filesystem::path s_stringsPath;
};

}}
