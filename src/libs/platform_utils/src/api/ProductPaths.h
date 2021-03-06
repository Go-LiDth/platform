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

/*
 *  ProductPaths.h
 *  Provides access to bp product-specific paths, etc.
 *
 *  Created by David Grigsby on 8/01/07.
 *  Copyright 2007 Yahoo! Inc. All rights reserved.
 *
 */
#ifndef _PRODUCTPATHS_H_
#define _PRODUCTPATHS_H_

#include <string>
#include <vector>

#include "BPUtils/bpfile.h"

namespace bp {
    
    namespace paths {

        /**
         *   Get company name.
         *   \return   company name
         */
        std::string getCompanyName();
        
        /**
         *   Get product name.
         *   \return   product name
         */
        std::string getProductName();
    
        /**
         *   Get path to "base" directory for all versions.
         *   This is the directory under which versioned platforms
         *   will be kept.  Throws a fatal exception on failure.
         *   \return   path to product base directory
         */
        boost::filesystem::path getProductTopDirectory();

        /**
         *   Get path to product temp directory
         *   \return   path to product temp directory
         */
        boost::filesystem::path getProductTempDirectory();

        /**
         *   Get path to "base" directory for specified version.
         *   This is the directory under which versioned platform 
         *   files will be kept. Letting all args default gets 
         *   current version.  Throws a fatal exception on failure.
         *   \return   path to versioned product base directory 
         */
        boost::filesystem::path getProductDirectory(int major = -1, 
                                                    int minor = -1, 
                                                    int micro = -1);  

        /**
         *  Get path to product permissions directory
         *  Throws a fatal exception on failure.
         *  \return   path to product permissions directory 
         */
        boost::filesystem::path getPermissionsDirectory();
        
        /**
         *  Get path to daemon.
         *  Throws a fatal exception on failure.
         *  \return    path to daemon
         */
        boost::filesystem::path getDaemonPath(int major = -1,
                                              int minor = -1,
                                              int micro = -1);

        /**
         *   Get path to daemon log
         *   Throws a fatal exception on failure.
         *   \return   path to daemon log
         */
        boost::filesystem::path getDaemonLogPath(int major = -1,
                                                 int minor = -1,
                                                 int micro = -1);

        /**
         *  Get path to service runner binary (typically installed
         *  right next to daemon and renamed at install time, the whole
         *  point is to allow the end user to understand the purpose
         *  of all the BrowserPlus proccesses running on their box)
         *  Throws a fatal exception on failure.
         *  \return    path to service runner binary
         */
        boost::filesystem::path getRunnerPath(int major = -1,
                                              int minor = -1,
                                              int micro = -1);
         
        /**
         *  Get path to service installer.
         *  Throws a fatal exception on failure.
         *  \return    path to service installer
         */
        boost::filesystem::path getServiceInstallerPath(int major = -1,
                                                        int minor = -1,
                                                        int micro = -1);

        /**
         *   Get path to service directory.
         *   Throws a fatal exception on failure.
         *   \return   path to service directory
         */
        boost::filesystem::path getServiceDirectory();
        
        /**
         *   Get path to service cache directory.
         *   Throws a fatal exception on failure.
         *   \return   path to service cache directory
         */
        boost::filesystem::path getServiceCacheDirectory();
        
        /**
         *   Get path to cached service interface (stored on disk in json
         *   format).
         *   Throws a fatal exception on failure.
         *   \return path to pid file
         */
        boost::filesystem::path getServiceInterfaceCachePath();
        
        /**
         *   Get path to platform cache directory.
         *   Throws a fatal exception on failure.
         *   \return   path to platform cache directory
         */
        boost::filesystem::path getPlatformCacheDirectory();

        /**
         *   Get path to service data directory.
         *   Throws a fatal exception on failure.
         *   \return   path to service data directory
         */
        boost::filesystem::path getServiceDataDirectory(std::string name,
                                                        unsigned int major_ver);
        
        /**
         *   Get path to sandboxed directory to which plugin can write.
         *   Used to store log files and anything else that plugin must write.
         *   Letting all args default gets current version.
         *   Throws a fatal exception on failure.
         *   \return   path to writable directory
         */
        boost::filesystem::path getPluginWritableDirectory(int major = -1, 
                                                           int minor = -1, 
                                                           int micro = -1);

        /**
         *   Get path to a writable directory whose path is obfuscated
         *   to make it hard to guess by script.
         *   Throws a fatal exception on failure.
         *   \return   path to writable directory
         */
        boost::filesystem::path getObfuscatedWritableDirectory(int major = -1,
                                                               int minor = -1,
                                                               int micro = -1);

        /**
         *   Get path to configfile, as a side effect creates 
         *   product directory.   Letting all args default
         *   gets current version.
         *   Throws a fatal exception on failure.
         *   \return path to pid file
         */
        boost::filesystem::path getConfigFilePath(int major = -1, 
                                                  int minor = -1, 
                                                  int micro = -1);
        
        /**
         *  Get path to platform scoped localized strings file.  
         *  Letting all args default gets current version.
         *  Throws a fatal exception on failure.
         *  Use bp::localization methods to interact with localized strings.
         */
        boost::filesystem::path getLocalizedStringsPath(int major = -1,
                                                        int minor = -1,
                                                        int micro = -1,
                                                        bool useUpdateCache = false);
        
        /**
         *   Get path to certificate file. 
         *   Throws a fatal exception on failure.
         *   \return path to certificates file
         */
        boost::filesystem::path getCertFilePath();
        
        /**
         *   Get path to cached permissions.
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getPermissionsPath();

        /**
         *   Get path to persistent state file.  Usage of the persistent
         *   state hash should go through bpphash.h, you should not
         *   interact with this file directly.  
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getPersistentStatePath(int major = -1,
                                                       int minor = -1,
                                                       int micro = -1);
        
        /**
         *  Get path to domain permissions file.  
         *  Throws a fatal exception on failure.
         */
        boost::filesystem::path getDomainPermissionsPath();
        
        /**
         *  Get path to installation ID file.  
         */
        boost::filesystem::path getInstallIDPath();
        
        /**
         *   Form version string (e.g. "1.0.1")
         *   Letting all args default gets current version.
         */
        std::string versionString(int major = -1, 
                                  int minor = -1, 
                                  int micro = -1);

        /**
         *   "IPC Name" is a user & platform scoped name which is
         *   approriate to use when establishing an IPC listening
         *   server (using bp::ipc::Server, or the higher level abstraction,
         *   using bp::ipc::ChannelServer).  This string is used by both
         *   the listening and the connecting processes.
         *   
         *   This string is platform dependent.  On UNIX its a path in
         *   the product directory.  On windows it's a named pipe 
         *   without '\\.\pipe\'
         *   
         *   This IPC name is used for communication between the BrowserPlus
         *   Daemon and browser plugins.
         *
         */
        std::string getIPCName();

        /**
         *   Another IPC name which is similar to that returned by
         *   getIPCName(), however this IPC name is "ephemeral", which
         *   means it's randomized and hence better for cases where
         *   the name is not used for local addressing or discovery.
         *   This function is intended for use by the communication
         *   mechanism between the BrowserPlus Daemon and Service
         *   processes.
         *
         */
        std::string getEphemeralIPCName();

        /**
         *   "IPC Lock Name" is a user & platform scoped name which is
         *   approriate to use when establishing a lock to guarantee
         *   that no other BrowserPlusCore processes are running.
         *   On Windows this is a Mutex name BrowserPlusCore_<USER>_<platform>
         *   On darwin this is the path to the BrowserPlusCore binary
         *   (on which ftok(3) may be used to get a semaphore "key")  
         *
         */
        std::string getIPCLockName(int major = -1,
                                   int minor = -1,
                                   int micro = -1);

        /** 
         *   Get path to file which is written at the very end of 
         *   a BrowserPlus install.  The presence of this file means
         *   that BrowserPlus is installed and ready to go.  This path
         *   is known to installers.
         *   Letting all args default gets current version.
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getBPInstalledPath(int major = -1, 
                                                   int minor = -1, 
                                                   int micro = -1);
        
        /** 
         *   Get path to file which is written at the very beginning of 
         *   a BrowserPlus install.  The presence of this file means
         *   that BrowserPlus is installing.  This path
         *   is known to installers.
         *   Letting all args default gets current version.
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getBPInstallingPath(int major = -1, 
                                                    int minor = -1, 
                                                    int micro = -1);
        
        /** 
         *   Get path to file whose presence indicates that BrowserPlus
         *   has been disabled.  Deamon will refuse to run.
         *   Letting all args default gets current version.
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getBPDisabledPath(int major = -1, 
                                                  int minor = -1, 
                                                  int micro = -1);
        
        /**
         *   Get path to uninstaller executable
         *   Throws a fatal exception on failure.
         */
        boost::filesystem::path getUninstallerPath();
        
        /** 
         *   Get path to the HTML file which contains the component
         *   install dialog.
         */
        boost::filesystem::path getComponentInstallDialogPath(const std::string & locale);

        /** 
         *   Get path to the HTML file which contains the preference
         *   panel
         */
        boost::filesystem::path getPreferencePanelUIPath(const std::string & locale);
        
        /**
         *   Get paths to plugins
         */
        std::vector<boost::filesystem::path> getPluginPaths(int major = -1,
                                                            int minor = -1,
                                                            int micro = -1);

        /**
         *   Make sure that all needed directories exist, creating
         *   them if necessary.
         */
        void createDirectories(int major = -1,
                               int minor = -1,
                               int micro = -1);
    }
}

#endif
