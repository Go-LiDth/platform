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
 * InactiveServicesCorelet
 *
 * An instantiated corelet instance upon which functions may be
 * invoked.
 */

#include "InactiveServicesCorelet.h"
#include "BPUtils/bpfile.h"
#include "BPUtils/bpconfig.h"
#include "BPUtils/BPLog.h"
#include "BPUtils/OS.h"
#include "BPUtils/ProductPaths.h"
#include "Permissions/Permissions.h"

using namespace std;
using namespace std::tr1;


InactiveServicesCorelet::InactiveServicesCorelet(
    weak_ptr<CoreletExecutionContext> context)
    : CoreletInstance(context), m_distQuery(NULL)
{
    // Setup a config file reader.
    bp::config::ConfigReader configReader;
    {
        bp::file::Path configFilePath = bp::paths::getConfigFilePath();
        if (!configReader.load(configFilePath)) {
            BPLOG_ERROR_STRM("couldn't read config file at " << configFilePath);
            return;
        }
    }

    // Setup the distribution server url.  Safe to ignore 
    // configReader return value since we couldn't have gotten
    // this far without a DistServerUrl in the config file.
    std::list<std::string> distroServers;
    std::string primaryServer;
    (void) configReader.getStringValue("DistServer", primaryServer);
    (void) configReader.getArrayOfStrings("SecondaryDistServers",
                                          distroServers);
    distroServers.push_front(primaryServer);

    m_distQuery = new DistQuery(distroServers, PermissionsManager::get());
    m_distQuery->setListener(this);
}


InactiveServicesCorelet::~InactiveServicesCorelet()
{
    delete m_distQuery;
}


void
InactiveServicesCorelet::execute(unsigned int tid,
                                 const std::string & function,
                                 const bp::Object & args)
{    
    if (function.empty()) {
        sendFailure(tid, std::string("bp.internalError"),
                    std::string("empty function argument"));
    } else if (0 == function.compare("All")) {
        std::string platform = bp::os::PlatformAsString();

        // extract platform if specified
        if (args.has("platform", BPTString)) {
            std::string s = (std::string) *(args.get("platform"));
            if (s.compare("osx") != 0 && s.compare("win32") != 0) {
                sendFailure(tid,
                            "InactiveServicesCorelet.invalidArguments",
                            " 'platform' must be 'osx' or 'win32'");
                return;
            }

            if (!s.empty()) platform = s;
        }

        unsigned int did = m_distQuery->availableServices(platform);

        if (did == 0) {
            sendFailure(tid,
                        "InactiveServicesCorelet.httpError",
                        " failed to start HTTP transaction with distribution"
                        " server.");
        } else {
            m_distToTransMap[did] = tid;
        }
    }
    else if (0 == function.compare("Describe"))
    {
        std::string service, version, platform;
        platform = bp::os::PlatformAsString();

        std::string s;
        
        // extract platform if specified
        if (args.has("platform", BPTString)) {
            s = (std::string) *(args.get("platform"));
            if (s.compare("osx") != 0 && s.compare("win32") != 0) {
                sendFailure(tid,
                            "InactiveServicesCorelet.invalidArguments",
                            " 'platform' must be 'osx' or 'win32'");
                return;
            }

            if (!s.empty()) platform = s;
        }
        
        // now extract service and version
        service = (std::string) *(args.get("service"));
        version = (std::string) *(args.get("version"));

        unsigned int did = m_distQuery->coreletDetails(service, version, platform);;

        if (did == 0) {
            sendFailure(tid,
                        "InactiveServicesCorelet.httpError",
                        " failed to start HTTP transaction with distribution"
                        " server.");
        } else {
            m_distToTransMap[did] = tid;
        }
    } else {
        sendFailure(tid, std::string("bp.noSuchFunction"), std::string());
    }
}


const bp::service::Description *
InactiveServicesCorelet::getDescription()
{
    static bool ran = false;
    static bp::service::Description desc;
    
    if (!ran) {
        desc.setName("InactiveServices");
        desc.setMajorVersion(1);    
        desc.setMinorVersion(0);    
        desc.setMicroVersion(1);    
        desc.setDocString("Allows for the exploration "
                          " of available services, "
                          "which may be downloaded and activated.");

        std::list<bp::service::Function> funcs;
        {
            bp::service::Function all;
            all.setName("All");
            all.setDocString("Get a list of all available services");

            std::list<bp::service::Argument> args;
            bp::service::Argument arg("platform", bp::service::Argument::String);
            arg.setRequired(false);
            arg.setDocString(
                "The platform (either 'osx' or 'win32') for which you would "
                "like to see available corelets");
            args.push_back(arg);
            all.setArguments(args);
            funcs.push_back(all);
        }
        
        {
            bp::service::Function describe;
            describe.setName("Describe");
            describe.setDocString(
                "Get a data structure describing the interface "
                "of a specified service");

            std::list<bp::service::Argument> args;

            bp::service::Argument arg("service", bp::service::Argument::String);
            arg.setRequired(true);
            arg.setDocString("The name of the service.");
            args.push_back(arg);

            arg.setName("version");
            arg.setType(bp::service::Argument::String);
            arg.setRequired(true);
            arg.setDocString("The exact version of the service.");
            args.push_back(arg);

            arg.setName("platform");
            arg.setType(bp::service::Argument::String);
            arg.setRequired(false);
            arg.setDocString(
                "The platform (either 'osx' or 'win32') for which you would "
                "like a service description.");
            args.push_back(arg);

            describe.setArguments(args);

            funcs.push_back(describe);
        }

        desc.setFunctions(funcs);
    }

    return &desc;
}

void
InactiveServicesCorelet::onTransactionFailed(unsigned int tid)
{
    using namespace bp;
    
    // some request failed, find the SMM tid
    std::map<unsigned int, unsigned int>::iterator it;
    it = m_distToTransMap.find(tid);
    if (it == m_distToTransMap.end()) {
        BPLOG_WARN_STRM("transaction failed with unknown tid "
                        << tid);
    } else {
        unsigned int smmTid = it->second;
        m_distToTransMap.erase(it);
        sendFailure(smmTid, std::string("bp.transactionError"),
                    std::string());
    }
}

void
InactiveServicesCorelet::gotServiceDetails(
    unsigned int tid,
    const bp::service::Description & desc)
{
    std::map<unsigned int, unsigned int>::iterator it;
    it = m_distToTransMap.find(tid);
    if (it == m_distToTransMap.end()) {
        BPLOG_WARN_STRM("transaction completes with unknown tid " << tid);
        return;
    }
    unsigned int smmTid = it->second;
    m_distToTransMap.erase(it);
    
    bp::Object * d = desc.toBPObject();
    sendComplete(smmTid, *d);
    delete d;
}

void
InactiveServicesCorelet::gotAvailableServices(unsigned int tid,
                                              const CoreletList & list)
{
    using namespace bp;
    
    // find the SMM tid
    std::map<unsigned int, unsigned int>::iterator it;
    it = m_distToTransMap.find(tid);
    if (it == m_distToTransMap.end()) {
        BPLOG_WARN_STRM("transaction completes with unknown tid " 
                        << tid);
        return;
    }
    unsigned int smmTid = it->second;
    m_distToTransMap.erase(it);
    List corelets;

    // map from STL into bp::Object
    std::list<std::pair<std::string, std::string> >::const_iterator li;
    for (li = list.begin(); li != list.end(); ++li)
    {
        Map* corelet = new Map;
        corelet->add("name", new String(li->first));
        corelet->add("version", new String(li->second));
        corelets.append(corelet);
    }
    sendComplete(smmTid, corelets);
}