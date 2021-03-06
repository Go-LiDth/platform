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

/**
 * BrowserPlusUpdater - a simple binary to manage the update process.
 */

#include <string>
#include <string.h>
 
#include "bpipc/IPCChannel.h"
#include "BPUtils/bpfile.h"
#include "BPUtils/BPLog.h"
#include "BPInstaller/BPInstaller.h"
#include "platform_utils/ARGVConverter.h"
#include "platform_utils/bplocalization.h"

using namespace std;
using namespace std::tr1;
using namespace bp::file;
using namespace bp::install;
namespace bfs = boost::filesystem;

// For platform 2.6 and later, standalone install invokes BrowserPlusUpdater
// and status/progress/error is sent back to installer via IPC.  This listener
// is the proxy.
class IPCProxy : virtual public bp::install::IInstallerListener
{
public:
    IPCProxy(const std::string& ipcName) : m_channel(), m_channelOk(true) {
        bool ok = m_channel.connect(ipcName);
        BPLOG_DEBUG_STRM("connect to " << ipcName << " returns " << ok);
        if (!ok) {
            BPLOG_WARN_STRM("BrowserPlusUpdater unable to connect to "
                            << ipcName);
            m_channelOk = false;
        }
    }
    virtual ~IPCProxy() {
        if (m_channelOk) {
            m_channel.disconnect();
        }
    }
    virtual void onStatus(const std::string& msg) {
        bp::ipc::Message m;
        m.setCommand("status");
        bp::Map* payload = new bp::Map;
        payload->add("message", new bp::String(msg));
        m.setPayload(payload);
        sendMessage(m);
    }
    virtual void onError(const std::string& msg) {
        bp::ipc::Message m;
        m.setCommand("error");
        bp::Map* payload = new bp::Map;
        payload->add("message", new bp::String(msg));
        m.setPayload(payload);
        sendMessage(m);
    }
    virtual void onProgress(unsigned int pct) {
        bp::ipc::Message m;
        m.setCommand("progress");
        bp::Map* payload = new bp::Map;
        payload->add("percent", new bp::Integer(pct));
        m.setPayload(payload);
        sendMessage(m);
    }
    virtual void onDone() {
        bp::ipc::Message m;
        m.setCommand("done");
        bp::Map* payload = new bp::Map;
        m.setPayload(payload);
        sendMessage(m);
    }
    
private:
    void sendMessage(const bp::ipc::Message& m) {
        BPLOG_DEBUG_STRM("IPCProxy sends " << m.serialize());
        if (!m_channelOk) {
            BPLOG_ERROR_STRM("bad channel, unable to send" << m.serialize());
            return;
        }
        if (!m_channel.sendMessage(m)) {
            BPLOG_ERROR_STRM("unable to send" << m.serialize());
        }
    }

    bp::ipc::Channel m_channel;
    bool m_channelOk;
};


void
usage()
{
    string s = "usage: BrowserPlusUpdater [-ipcName=<ipcName>] [-logPath=<logPath>] [-logLevel=<level> dir [lockfile]";
    BPLOG_ERROR(s);
    cout << s << endl;
    exit(-1);
}


int
main(int argc, const char** argv)
{
    int rval = 0;
    try {
        // on win32, may have non-ascii chars in args.  deal with it
        APT::ARGVConverter conv;
        conv.convert(argc, argv);

        // set the appropriate locale for strings generated from the Installer
        string locale = bp::localization::getUsersLocale();
        bfs::path exe(argv[0]);
        bfs::path stringsPath = absolutePath(exe.parent_path() / "strings.json");
        Installer::setLocalizedStringsPath(stringsPath, locale);

        // setup logging, may be overridden by -logPath=<path> and/or -logLevel=<level>
        bfs::path logFile = getTempDirectory().parent_path() / "BrowserPlusUpdater.log";
        bp::log::Level logLevel = bp::log::LEVEL_ALL;

        // crack argv
        // usage is: BrowserPlusUpdater [-ipcName=<ipcName>] [-logPath=<logPath>] 
        //                              [-logLevel=<level> dir [lockfile]
        // we no longer need lockfile arg
        IPCProxy* proxy = NULL;
        if (argc < 2) {
            usage();
        }
        int argIndex = 1;
        string ipcName;
        string curArg(argv[argIndex]);
        if (curArg.find("-ipcName=") == 0) {
            ipcName = curArg.substr(strlen("-ipcName="));
            proxy = new IPCProxy(ipcName);
            curArg = argv[++argIndex];
        }
        if (curArg.find("-logPath=") == 0) {
            logFile = curArg.substr(strlen("-logPath="));
            curArg = argv[++argIndex];
        }
        if (curArg.find("-logLevel=") == 0) {
            logLevel = bp::log::levelFromString(
                           curArg.substr(strlen("-logLevel=")));
            curArg = argv[++argIndex];
        }
        bfs::path dir(argv[argIndex++]);

        if (!logFile.empty()) {
            // TODO: size, layout, time format
            bp::log::setupLogToFile(logFile, logLevel, bp::log::kSizeRollover,
                                    bp::log::TIME_UTC, "standard", 512);
        } else {
            bp::log::setupLogToConsole(logLevel);
        }
    
        // Install out of dir.  We delete dir at completion if we aren't
        // being run on behalf of installer (proxy == NULL).  If we are
        // being run on behalf of installer, it will delete dir.
        shared_ptr<IInstallerListener> p;
        Installer inst(dir, logFile, logLevel, proxy == NULL);
        if (proxy) {
            p.reset(proxy);
            inst.setListener(weak_ptr<IInstallerListener>(p));
        }
        inst.run();
    } catch (const std::exception& e) {
        BP_REPORTCATCH(e);
        rval = -1;
    } catch (...) {
        BP_REPORTCATCH_UNKNOWN;
        rval = -1;
    }
    exit(rval);
}
