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
 * PermissionsManager.cpp - A singleton responsible for maintaining up to
 *                         date permissions data, including blacklist,
 *                         approved signeers, and revoked signers.
 *
 * Created by Gordon Durand on Mon 22 Oct 2007
 * Copyright (c) 2007 Yahoo!, Inc. All rights reserved.
 */

#include "PermissionsManager.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "ArchiveLib/ArchiveLib.h"
#include "BPUtils/bpfile.h"
#include "BPUtils/BPLog.h"
#include "BPUtils/OS.h"
#include "platform_utils/bpconfig.h"
#include "platform_utils/bpdns.h"
#include "platform_utils/bplocalization.h"
#include "platform_utils/bpphash.h"
#include "platform_utils/ProductPaths.h"


using namespace std;
 
// bp::phash keys 
static const char* kPermsTSKey = "PermissionsManager::kPermsTSKey";   
static const char* kLastCheckTSKey = "PermissionsManager::kLastCheckTSKey";

// domain permission key
const char* PermissionsManager::kAllowDomain = "AllowBrowserPlus";

// autoUpdate key
const char* PermissionsManager::kAutoUpdate = "AutoUpdate";

const char* kSilentPlatformUpdate = "SilentPlatformUpdate";
const char* kSilentServiceUpdate = "SilentServiceUpdate";

PermissionsManager* PermissionsManager::m_singleton = NULL;


PermissionsManager*
PermissionsManager::get(int major,
                        int minor,
                        int micro)
{
    if (m_singleton == NULL) {
        bp::config::ConfigReader configReader;
        boost::filesystem::path configFilePath = bp::paths::getConfigFilePath(major, minor, micro);
        if (!configReader.load(configFilePath)) {
            BP_THROW_FATAL("couldn't read config file at: " + configFilePath.string());
        }
        string baseURL;
        if (!configReader.getStringValue("DistServer", baseURL)) {
            BP_THROW_FATAL("No DistServerUrl key in config file at: "
                           + configFilePath.string());
        }
        m_singleton = new PermissionsManager(baseURL);
    }
    return m_singleton;
}


bool
PermissionsManager::upToDateCheck(IPermissionsManagerListener * listener)
{
    // TODO:  would eventually like to do an "am I online" check
    
    bool mustCheck = true;
    string lastCheck;
    if (bp::phash::get(kLastCheckTSKey, lastCheck)) {
        BPTime last(lastCheck);
        BPTime now;
        long diff = now.diffInSeconds(lastCheck);
        double days = diff / 60.0 / 60.0 / 24.0;
        mustCheck = days > m_checkDays;
    }
    if (m_badPermissionsOnDisk) {
        mustCheck = true;
    }
    if (mustCheck) {
        unsigned int tid = m_distQuery->permissions();
        if (tid != 0) {
            m_listeners[tid] = listener;
        } else {
            hop((void *) listener);
        }
    }
    return(mustCheck == false);
}
 
 
bool
PermissionsManager::mayRun()
{  
    string version = bp::paths::versionString();
    bp::SemanticVersion ourVersion;
    (void) ourVersion.parse(version);
    
    vector<string>::const_iterator it;
    for (it = m_platformBlacklist.begin(); it != m_platformBlacklist.end(); ++it) {
        bp::SemanticVersion bad;
        if (bad.parse(*it) && ourVersion.match(bad)) {
            m_error = true;
            return false;
        }
    }
    return true;
}


bool
PermissionsManager::lastCheck(BPTime& t) const
{
    bool rval = false;
    string lastCheck;
    if (bp::phash::get(kLastCheckTSKey, lastCheck)) {
        BPTime last(lastCheck);
        t = last;
        rval = true;
    }
    return rval;
}


bool 
PermissionsManager::isOSPlatformDeprecated() const
{
    string os = bp::os::PlatformAsString();
    string osVersion = bp::os::PlatformVersion();
    map<string, set<string> >::const_iterator it;
    it = m_deprecatedOSPlatforms.find(os);
    if (it != m_deprecatedOSPlatforms.end()) {
        for (set<string>::const_iterator vi = it->second.begin();
             vi != it->second.end(); ++vi) {
            if (osVersion.find(*vi) == 0) {
                BPLOG_DEBUG_STRM(os << " / " << osVersion
                                 << " deprecated by entry "
                                 << it->first << " / " << *vi);
                return true;
            }
        }
    }
    return false;
}


bool
PermissionsManager::requireDomainApproval() const
{
    return m_requireDomainApproval;
}


void
PermissionsManager::setRequireDomainApproval(bool b) 
{
    if (b != m_requireDomainApproval) {
        m_requireDomainApproval = b;
        saveDomainPermissions();
    }
}

 
PermissionsManager::Permission
PermissionsManager::domainMayUseBrowserPlus(const string& domain) const
{
    if (!m_requireDomainApproval) {
        return eAllowed;
    }
    
    Permission rval = eUnknown;
    BPLOG_DEBUG_STRM("check domain " << domain);
    string resolvedDomain = normalizeDomain(domain);
    if (resolvedDomain.compare(domain) != 0) {
        BPLOG_DEBUG_STRM("URL '" << domain << "' normalized to '"
                         << resolvedDomain << "'");
    }
    map<string, PermissionInfo> perms = queryDomainPermissions(resolvedDomain);
    map<string, PermissionInfo>::iterator it = perms.find(kAllowDomain);
    if (it != perms.end()) {
        rval = it->second.m_allowed ? eAllowed : eNotAllowed;
    }
    BPLOG_DEBUG_STRM("permission = " << rval);
    return rval;
}


void 
PermissionsManager::allowDomain(const string& domain)
{
    string resolvedDomain = normalizeDomain(domain);
    addDomainPermission(resolvedDomain, kAllowDomain);
}


void 
PermissionsManager::disallowDomain(const string& domain)
{
    string resolvedDomain = normalizeDomain(domain);
    revokeAllDomainPermissions(resolvedDomain);
    revokeDomainPermission(resolvedDomain, kAllowDomain);
}

 
bool 
PermissionsManager::serviceMayRun(const string& name, 
                                  const string& version) const
{
    map<string, vector<string> >::const_iterator it = m_blacklist.find(name);
    if (it == m_blacklist.end()) {
        return true;
    }
    bp::SemanticVersion ourVersion;
    if (!ourVersion.parse(version)) {
        BPLOG_ERROR_STRM("serviceMayRun(" << name
                         << "," << version << "), bad version");
        return false;
    }
    
    for (unsigned int i = 0; i < it->second.size(); i++) {
        const string& v = it->second[i];
        bp::SemanticVersion bad;
        if (bad.parse(v) && ourVersion.match(bad)) {
            return false;
        }
    }
    return true;
}


bool 
PermissionsManager::isBusy() const
{
    return !m_listeners.empty();
}


bool 
PermissionsManager::error() const
{
    return m_error;
}


void 
PermissionsManager::addDomainPermission(const string& domain,
                                        const string& permission)
{
    string resolvedDomain = normalizeDomain(domain);
    if (permission == kSilentPlatformUpdate) {
        setAutoUpdatePlatform(resolvedDomain, eAllowed);
    } else if (permission == kSilentServiceUpdate) {
        // all existing silentserviceupdate perms for domain get allowed
        map<string, AutoUpdateInfo> m = queryAutoUpdate();
        map<string, Permission> servicesCopy;
        map<string, AutoUpdateInfo>::iterator it = m.find(resolvedDomain);
        if (it != m.end()) {
            servicesCopy = it->second.m_services;
        }
        for (map<string, Permission>::const_iterator sit = servicesCopy.begin();
             sit != servicesCopy.end(); ++sit) {
            setAutoUpdateService(resolvedDomain, sit->first, eAllowed);
        }
    } else {
        PermissionInfo p(true);
        m_domainPermissions[resolvedDomain].insert(make_pair(permission, p));
    }
    saveDomainPermissions();
}


void 
PermissionsManager::revokeDomainPermission(const string& domain,
                                           const string& permission)
{
    string resolvedDomain = normalizeDomain(domain);
    if (permission == kSilentPlatformUpdate) {
        setAutoUpdatePlatform(resolvedDomain, eNotAllowed);
    } else if (permission == kSilentServiceUpdate) {
        map<string, AutoUpdateInfo> m = queryAutoUpdate();
        map<string, Permission> servicesCopy;
        map<string, AutoUpdateInfo>::iterator it = m.find(resolvedDomain);
        if (it != m.end()) {
            servicesCopy = it->second.m_services;
        }
        for (map<string, Permission>::const_iterator sit = servicesCopy.begin();
             sit != servicesCopy.end(); ++sit) {
            setAutoUpdateService(resolvedDomain, sit->first, eNotAllowed);
        }
    } else {
        PermissionInfo p(false);
        m_domainPermissions[resolvedDomain].insert(make_pair(permission, p));
    }
    saveDomainPermissions();
}


void 
PermissionsManager::resetDomainPermission(const string& domain,
                                          const string& permission,
                                          Permission currentState)
{
    string resolvedDomain = normalizeDomain(domain);
    if (permission == kSilentPlatformUpdate) {
        setAutoUpdatePlatform(resolvedDomain, eUnknown);
    } else if (permission == kSilentServiceUpdate) {
        map<string, AutoUpdateInfo> m = queryAutoUpdate();
        map<string, Permission> servicesCopy;
        map<string, AutoUpdateInfo>::iterator it = m.find(resolvedDomain);
        if (it != m.end()) {
            servicesCopy = it->second.m_services;
        }
        for (map<string, Permission>::const_iterator sit = servicesCopy.begin();
             sit != servicesCopy.end(); ++sit) {
            if (currentState == eUnknown || sit->second == currentState) {
                setAutoUpdateService(resolvedDomain, sit->first, eUnknown);
            }
        }
    } else {
        m_domainPermissions[resolvedDomain].erase(permission);
    }
    saveDomainPermissions();
}


void 
PermissionsManager::revokeAllDomainPermissions(const string& domain)
{
    string resolvedDomain = normalizeDomain(domain);
    map<string, PermissionInfo>::iterator it;
    for (it = m_domainPermissions[resolvedDomain].begin();
         it != m_domainPermissions[resolvedDomain].end(); ++it) {
        revokeDomainPermission(resolvedDomain, it->first);
    }
}


void 
PermissionsManager::resetAllDomainPermissions(const string& domain)
{
    string resolvedDomain = normalizeDomain(domain);
    if (m_domainPermissions.erase(resolvedDomain) > 0) {
        saveDomainPermissions();
    }
}


void 
PermissionsManager::revokePermission(const string& permission)
{
    map<string, map<string, PermissionInfo> >::iterator it;
    PermissionInfo p(false);
    for (it = m_domainPermissions.begin(); it != m_domainPermissions.end(); ++it) {
        m_domainPermissions[it->first].insert(make_pair(permission, p));
    }
    saveDomainPermissions();
}


void 
PermissionsManager::resetPermission(const string& permission)
{
    map<string, map<string, PermissionInfo> >::iterator it;
    for (it = m_domainPermissions.begin(); it != m_domainPermissions.end(); ++it) {
        resetDomainPermission(it->first, permission);
    }
}


map<string, PermissionsManager::PermissionInfo>
PermissionsManager::queryDomainPermissions(const string& domain) const
{
    string resolvedDomain = normalizeDomain(domain);
    map<string, PermissionInfo> rval;
    
    // TODO: consider this: Is this the correct set of dirs to
    // disallow?  Basically, where can evil.org use us to write a
    // bad page?

    // Disallow any local page coming from any product dir.  We don't want 
    // evil.org somehow writing a page and using it against us.
    if (domain.find(bp::paths::getProductTopDirectory().generic_string()) == 0) {
        return rval;
    }

    // Disallow any local page coming from plugin writable dir.
    if (domain.find(bp::paths::getPluginWritableDirectory().generic_string()) == 0) {
        return rval;
    }

    // First go for exact match, then check patterns.  First
    // pattern which matches wins.
    map<string, map<string, PermissionInfo> >::const_iterator it;
    it = m_domainPermissions.find(resolvedDomain);
    if (it != m_domainPermissions.end()) {
        rval = it->second;
    } else {
        for (it = m_domainPermissions.begin(); it != m_domainPermissions.end(); ++it) {
            string pattern = it->first;
            if (!domainPatternValid(pattern)) {
                continue;
            }
            if (bp::strutil::matchesWildcard(resolvedDomain, pattern)) {
                rval = it->second;
                break;
            }
        }
    }
    return rval;
}


PermissionsManager::Permission 
PermissionsManager::queryDomainPermission(const string& domain,
                                          const string& permission) const
{
    if (!m_requireDomainApproval) {
        return eAllowed;
    }
    
    Permission rval = eUnknown;
    map<string, PermissionInfo> perms = queryDomainPermissions(domain);
    map<string, PermissionInfo>::const_iterator i = perms.find(permission);
    if (i != perms.end()) {
        rval = i->second.m_allowed ? eAllowed : eNotAllowed;
    }
    return rval;
}


set<string> 
PermissionsManager::queryPermissionDomains(const string& permission) const
{
    set<string> rval;
    map<string, map<string, PermissionInfo> >::const_iterator it;
    for (it = m_domainPermissions.begin(); it != m_domainPermissions.end(); ++it) {
        if (queryDomainPermission(it->first, permission) == eAllowed) {
            rval.insert(it->first);
        }
    }
    return rval;
}


map<string, map<string, PermissionsManager::PermissionInfo> > 
PermissionsManager::queryAllDomainPermissions() const
{
    return m_domainPermissions;
}


string 
PermissionsManager::getLocalizedPermission(const string& permission,
                                           const string& locale) const
{
    map<string, map<string, string> >::const_iterator it;
    it = m_permLocalizations.find(permission);
    if (it == m_permLocalizations.end()) {
        return permission;
    }
    const map<string, string>& m = it->second;
    vector<string> candidates = bp::localization::getLocaleCandidates(locale);
    for (unsigned int i = 0; i < candidates.size(); ++i) {
        map<string, string>::const_iterator mi = m.find(candidates[i]);
        if (mi != m.end()) {
            return mi->second;
        }
    }
    return permission;
}


PermissionsManager::Permission
PermissionsManager::queryAutoUpdatePlatform(const std::string& domain) const
{
    map<string, AutoUpdateInfo>::const_iterator it;
    for (it = m_autoUpdatePermissions.begin(); it != m_autoUpdatePermissions.end(); ++it) {
        BPLOG_DEBUG_STRM(it->first << ": " << it->second.toString());
    }
    
    Permission rval = eUnknown;
    map<string, AutoUpdateInfo>::const_iterator iter;
    for (iter = m_autoUpdatePermissions.begin(); 
         iter != m_autoUpdatePermissions.end(); ++iter) {
        std::string pattern = iter->first;
        if (!domainPatternValid(pattern)) {
            continue;
        }
        if (bp::strutil::matchesWildcard(domain, pattern)) {
            rval = iter->second.m_platform;
            break;
        }
    }
    return rval;
}


PermissionsManager::Permission
PermissionsManager::queryAutoUpdateService(const std::string& domain,
                                           const std::string& service) const
{
    Permission rval = eUnknown;
    map<string, AutoUpdateInfo>::const_iterator iter;
    for (iter = m_autoUpdatePermissions.begin(); 
         iter != m_autoUpdatePermissions.end(); ++iter) {
        std::string pattern = iter->first;
        if (!domainPatternValid(pattern)) {
            continue;
        }
        if (bp::strutil::matchesWildcard(domain, pattern)) {
            map<string, Permission>::const_iterator it;
            it = iter->second.m_services.find(service);
            if (it != iter->second.m_services.end()) {
                rval = it->second;
                break;
            }
        }
    }
    return rval;
}


void 
PermissionsManager::setAutoUpdatePlatform(const std::string& domain,
                                          PermissionsManager::Permission value)
{
    map<string, AutoUpdateInfo>::iterator iter;
    iter = m_autoUpdatePermissions.find(domain);
    if (iter != m_autoUpdatePermissions.end()) {
        iter->second.m_platform = value;
    } else {
        AutoUpdateInfo info;
        info.m_platform = value;
        m_autoUpdatePermissions[domain] = info;
    }
    saveDomainPermissions();
}


void 
PermissionsManager::setAutoUpdateService(const std::string& domain,
                                         const std::string& service,
                                         PermissionsManager::Permission value)
{
    map<string, AutoUpdateInfo>::iterator iter;
    iter = m_autoUpdatePermissions.find(domain);
    if (iter != m_autoUpdatePermissions.end()) {
        iter->second.m_services[service] = value;
    } else {
        map<string, Permission> m;
        m[service] = value;
        AutoUpdateInfo info;
        info.m_services = m;
        m_autoUpdatePermissions[domain] = info;
    }
    saveDomainPermissions();
}


std::map<std::string, PermissionsManager::AutoUpdateInfo> 
PermissionsManager::queryAutoUpdate() const
{
    return m_autoUpdatePermissions;
}


PermissionsManager::PermissionsManager(const string& baseURL)
: m_checkDays(1.0), m_url(baseURL), m_error(false),
  m_badPermissionsOnDisk(false), m_requireDomainApproval(true),
  m_domainPermissions(), m_autoUpdatePermissions()
{
    list<string> distroServer;
    distroServer.push_back(baseURL);
    m_distQuery = new DistQuery(distroServer, this);
    m_distQuery->setListener(this);
    load();
}


PermissionsManager::~PermissionsManager()
{
    delete m_distQuery;
}


class AutoUpdatePerms {
public:
    AutoUpdatePerms() : m_platform(PermissionsManager::eUnknown) {
    }
    AutoUpdatePerms(const string& s)
      : m_domain(s), m_platform(PermissionsManager::eUnknown) {
    }
    string m_domain;
    PermissionsManager::Permission m_platform;
    map<string, PermissionsManager::Permission> m_services;
};

class DomainPerms {
public:
    DomainPerms() {
    }
    DomainPerms(const string& s) : m_domain(s) {
    }
    string m_domain;
    map<string, PermissionsManager::Permission> m_perms;
};

void
PermissionsManager::load()
{
    using namespace bp::file;
    using namespace bp::paths;
    using namespace bp::strutil;;

    m_blacklist.clear();
    m_platformBlacklist.clear();
    m_permLocalizations.clear();
    m_permMigrations.clear();
    m_appliedPermMigrations.clear();
    m_badPermissionsOnDisk = false;

    // must get new autoupdateperms and domainperms from permissions file,
    // then apply them after we've loaded domain permissions
    map<string, AutoUpdatePerms> newAutoUpdatePerms;
    map<string, DomainPerms> newDomainPerms;

    // All file contents are JSON.  Errors
    // within file cause us to ignore it
    string json;
    string jsonErrors;
    try {
        // permissions file is a bunch 'o json
        // with a map for blacklist, platform blacklist,
        // and permission localizations
        boost::filesystem::path inFile = getPermissionsPath();
        if (!loadFromFile(inFile, json)) {
            BP_THROW("unable to read permissions file");
        }
        boost::scoped_ptr<bp::Object> obj(
                    bp::Object::fromPlainJsonString(json, &jsonErrors));

        if (obj == NULL) {
            string errMsg("permissions file not valid json: ");
            errMsg += jsonErrors;
            BP_THROW(errMsg);
        }
        
        // get obj into an std::map
        map<string, const bp::Object*> m = *obj;
                    
        // service blacklist is a list of servicename/version pairs
        // gets turned into a map whose key is servicename and whose value
        // is list of versions
        const bp::Object* objPtr;
        objPtr = m["blacklist"];
        if (objPtr) {
            vector<const bp::Object*> v = *objPtr;
            for (unsigned int i = 0; i < v.size(); i++) {
                vector<const bp::Object*> p = *(v[i]);
                if (p.size() != 2) {
                    BP_THROW("invalid blacklist entry");
                }
                string name = *(p[0]);
                string vers = *(p[1]);
                m_blacklist[name].push_back(vers);
            }
        }
        
        // platform blacklist is a list of platform strings
        objPtr = m["platformBlacklist"];
        if (objPtr) {
            vector<const bp::Object*> v = *objPtr;
            for (unsigned int i = 0; i < v.size(); i++) {
                string s = *(v[i]);
                m_platformBlacklist.push_back(s);
            }
        }

        // deprecated OS platforms
        objPtr = m["deprecatedOSPlatforms"];
        if (objPtr) {
            map<string, const bp::Object*> osMap = *objPtr;
            map<string, const bp::Object*>::const_iterator it;
            for (it = osMap.begin(); it != osMap.end(); ++it) {
                string os = it->first;
                BPLOG_INFO_STRM("OS " << os << " has deprecated versions:");
                vector<const bp::Object*> versions = *(it->second);
                set<string> versionSet;
                for (size_t i = 0; i < versions.size(); ++i) {
                    string s = *(versions[i]);
                    versionSet.insert(s);
                    BPLOG_INFO_STRM("\t" << s);
                }
                m_deprecatedOSPlatforms[it->first] = versionSet;
                
            }
        }

        // publicKeys is a list of public keys to be added to certs
        objPtr = m["publicKeys"];
        if (objPtr) {
            boost::filesystem::path certPath = getCertFilePath();
            string certs;
            if (!loadFromFile(certPath, certs)) {
                BP_THROW("unable to read " + certPath.string());
            }
            bool dirty = false;
            vector<const bp::Object*>v = *objPtr;
            for (size_t i = 0; i < v.size(); i++) {
                string s = *(v[i]);
                if (certs.find(s) == string::npos) {
                    BPLOG_INFO_STRM("add cert " << s.substr(0, 50) << "...");
                    certs += s;
                    dirty = true;
                }
            }
            if (dirty){
                if (!storeToFile(certPath, certs)) {
                    BP_THROW("unable to write " + certPath.string());
                }
            }
        }
        
        // permission localizations are a map
        objPtr = m["servicePermissionLocalizations"];
        if (objPtr) {
            // key is permission, value is map of locale->localizations
            map<string, const bp::Object*> m2 = *objPtr;
            map<string, const bp::Object*>::const_iterator it;
            for (it = m2.begin(); it != m2.end(); ++it) {
                string perm = it->first;
                map<string, string> localizations;
                map<string, const bp::Object*> lm = *(it->second);
                map<string, const bp::Object*>::const_iterator li;
                for (li = lm.begin(); li != lm.end(); ++li) {
                    localizations[li->first] = string(*(li->second));
                }
                m_permLocalizations[perm] = localizations;
            }
        }

        // autoupdate permission migration.  Format is:
        // { <guid1>:
        //    // list of maps for each migration in this group
        //    [
        //      {  "domain": "foo.com",
        //         "operator": "or"|"and"
        //         "old": ["service1", "service2",...]
        //         "new": ["service3", "service4",...]
        //      },
        //      ...
        //    ],
        //   <guid2>:
        //     ...
        //  }
        // For each entry, if any/all of the services in 'old' have
        // autoupdate set, then autoupdate is set on the services in 'new' 
        // The guid is persisted as part of 'appliedAutoUpdatePermissionMigrations'
        // in order to prevent us from repeatedly applying the migration.
        //
        objPtr = m["autoUpdatePermissionMigration"];
        if (objPtr) {
            map<string, const bp::Object*> migrationMap = *objPtr;
            map<string, const bp::Object*>::const_iterator migrationMapIter;
            for (migrationMapIter = migrationMap.begin();
                 migrationMapIter != migrationMap.end(); ++migrationMapIter) {
                string guid = migrationMapIter->first;
                vector<const bp::Object*> migrationVec = *(migrationMapIter->second);
                vector<const bp::Object*>::const_iterator migrationVecIter;
                for (migrationVecIter = migrationVec.begin();
                     migrationVecIter != migrationVec.end(); ++migrationVecIter) {
                    map<string, const bp::Object*> entryMap = *((const bp::Map*)*migrationVecIter);
                    MigrationEntry entry;
                    entry.m_guid = guid;
                    const bp::String* s = dynamic_cast<const bp::String*>(entryMap["domain"]);
                    if (!s) {
                        BPLOG_WARN_STRM("bad migration domain entry for guid " << guid);
                        continue;
                    }
                    entry.m_domain = s->value();
                    s = dynamic_cast<const bp::String*>(entryMap["operator"]);
                    if (!s) {
                        BPLOG_WARN_STRM("bad migration operator entry for guid/domain "
                                        << guid << "/" << entry.m_domain);
                        continue;
                    }
                    entry.m_operator = s->value();
                    vector<const bp::Object*> oldVec = *(const bp::List*) entryMap["old"];
                    for (size_t i = 0; i < oldVec.size(); i++) {
                        entry.m_old.push_back(string(*oldVec[i]));
                    }
                    vector<const bp::Object*> newVec = *(const bp::List*) entryMap["new"];
                    for (size_t i = 0; i < newVec.size(); i++) {
                        entry.m_new.push_back(string(*newVec[i]));
                    }
                    if (entry.m_domain.empty() || entry.m_operator.empty()
                        || entry.m_old.empty() || entry.m_new.empty()) {
                        continue;
                    }
                    m_permMigrations.push_back(entry);
                }
            }
        }

        // auto update perms to set IF user hasn't already set them
        // example entry:
        //    "autoUpdatePermissions": {
        //       "*.yahoo.com": { 
        //          "platform": true,
        //          "services": {
        //             "YahooVoiceAndVideo": true
        //          }
        //       }
        //    }
        objPtr = m["autoUpdatePermissions"];
        if (objPtr) {
            map<string, const bp::Object*> m2 = *objPtr;
            map<string, const bp::Object*>::const_iterator it;
            for (it = m2.begin(); it != m2.end(); ++it) {
                AutoUpdatePerms perms(it->first);
                map<string, const bp::Object*> pm = *(it->second);
                map<string, const bp::Object*>::const_iterator pi;
                for (pi = pm.begin(); pi != pm.end(); ++pi) {
                    string key = pi->first;
                    if (key == "platform") {
                        const bp::Bool* bObj = dynamic_cast<const bp::Bool*>(pi->second);
                        if (bObj) {
                            perms.m_platform = bObj->value() ? eAllowed : eNotAllowed;
                        }
                    } else if (key == "services") {
                        map<string, const bp::Object*> sm = *(pi->second);
                        map<string, const bp::Object*>::const_iterator si;
                        for (si = sm.begin(); si != sm.end(); ++si) {
                            string service = si->first;
                            const bp::Bool* bObj = dynamic_cast<const bp::Bool*>(si->second);
                            if (bObj) {
                                perms.m_services[service] = bObj->value() ? eAllowed : eNotAllowed;
                            }
                        }
                    }
                }
                newAutoUpdatePerms[it->first] = perms;
            }
        }

        // domain perms to set IF user hasn't already set them
        // example entry:
        //    "domainPermissions": {
        //       "*.yahoo.com": { 
        //          "AllowBrowserPlus": true
        //       }
        //    }
        objPtr = m["domainPermissions"];
        if (objPtr) {
            map<string, const bp::Object*> m2 = *objPtr;
            map<string, const bp::Object*>::const_iterator it;
            for (it = m2.begin(); it != m2.end(); ++it) {
                DomainPerms perms(it->first);
                map<string, const bp::Object*> pm = *(it->second);
                map<string, const bp::Object*>::const_iterator pi;
                for (pi = pm.begin(); pi != pm.end(); ++pi) {
                    string permission = pi->first;
                    const bp::Bool* bObj = dynamic_cast<const bp::Bool*>(pi->second);
                    if (bObj) {
                        perms.m_perms[permission] = bObj->value() ? eAllowed : eNotAllowed;
                    }
                }
                newDomainPerms[it->first] = perms;
            }
        }

    } catch (const bp::error::Exception& e) {
        BPLOG_ERROR(e.what());
        m_blacklist.clear();
        m_platformBlacklist.clear();
        m_permLocalizations.clear();
        m_permMigrations.clear();
        m_deprecatedOSPlatforms.clear();
        m_badPermissionsOnDisk = true;
    }
    
    // domain permissions are a map of maps
    // not a fatal error if it's missing or hosed
    bool domainPermsBad = false;
    boost::filesystem::path domainPermsFile = getDomainPermissionsPath();
    if (loadFromFile(domainPermsFile, json)) {
        try {
            boost::scoped_ptr<bp::Object> obj(
                bp::Object::fromPlainJsonString(json, &jsonErrors));

            if (obj == NULL) {
                BP_THROW("domain permissions file not valid json: " + jsonErrors);
            }
            map<string, const bp::Object*> pm = *obj;
            const bp::Bool* bObj = dynamic_cast<const bp::Bool*>(pm["requireDomainApproval"]);
            if (bObj) {
                m_requireDomainApproval = bObj->value();
            }

            // get domain permissions
            const bp::Map* mObj = dynamic_cast<const bp::Map*>(pm["domainPermissions"]);
            if (mObj) {
                map<string, const bp::Object*> m = *mObj;
                map<string, const bp::Object*>::const_iterator it;
                for (it = m.begin(); it != m.end(); ++it) {
                    string domain = it->first;
                    map<string, const bp::Object*> perms = (*it->second);
                    map<string, const bp::Object*>::const_iterator pit;
                    for (pit = perms.begin(); pit != perms.end(); ++pit) {
                        vector<const bp::Object*> pair = *(pit->second);
                        const bp::Bool* bObj = dynamic_cast<const bp::Bool*>(pair[0]);
                        if (!bObj) {
                            BPLOG_WARN_STRM("bad domainPermissions for " << domain);
                            continue;
                        }
                        bool b = bObj->value();
                        const bp::String* sObj = dynamic_cast<const bp::String*>(pair[1]);
                        if (!sObj) {
                            BPLOG_WARN_STRM("bad domainPermissions for " << domain);
                            continue;
                        }
                        string ts = *sObj;
                        PermissionInfo info(b, ts);
                        m_domainPermissions[domain].insert(make_pair(pit->first, info));
                    }
                }
            }

            // get autoupdate permissions
            mObj = dynamic_cast<const bp::Map*>(pm["autoUpdatePermissions"]);
            if (mObj) {
                map<string, const bp::Object*> m = *mObj;
                map<string, const bp::Object*>::const_iterator it;
                for (it = m.begin(); it != m.end(); ++it) {
                    AutoUpdateInfo info;
                    string domain = it->first;
                    map<string, const bp::Object*> perms = (*it->second);
                    map<string, const bp::Object*>::const_iterator pit;
                    for (pit = perms.begin(); pit != perms.end(); ++pit) {
                        if (pit->first.compare("platform") == 0) {
                            const bp::Bool* bVal = dynamic_cast<const bp::Bool*>(pit->second);
                            if (!bVal) {
                                BPLOG_WARN_STRM("bad platform value in map: "
                                            << mObj->toPlainJsonString());
                                continue;
                            }
                            info.m_platform = bVal->value() ? eAllowed : eNotAllowed;
                            BPLOG_DEBUG_STRM(domain << " silent platform update "
                                             << info.m_platform);
                        } else if (pit->first.compare("services") == 0) {
                            const bp::Map* mVal = dynamic_cast<const bp::Map*>(pit->second);
                            if (!mVal) {
                                BPLOG_WARN_STRM("bad services value in map: "
                                                << mObj->toPlainJsonString());
                                continue;
                            }
                            map<string, const bp::Object*> sMap = *mVal;
                            map<string, const bp::Object*>::const_iterator sit;
                            for (sit = sMap.begin(); sit != sMap.end(); ++sit) {
                                const bp::Bool* bVal = dynamic_cast<const bp::Bool*>(sit->second);
                                if (bVal) {
                                    info.m_services[sit->first] = 
                                        bVal->value() ? eAllowed : eNotAllowed;
                                    BPLOG_DEBUG_STRM(domain << " silent service "
                                                     << sit->first << " update "
                                                     << info.m_services[sit->first]);
                                } else {
                                    BPLOG_WARN_STRM("bad service value: "
                                                    << sit->first << " in map: "
                                                    << mObj->toPlainJsonString());
                                    continue;
                                }
                            }
                        } else if (pit->first.compare("timeStamp") == 0) {
                            const bp::String* sVal = dynamic_cast<const bp::String*>(pit->second);
                            if (!sVal) {
                                BPLOG_WARN_STRM("bad timeStamp value in map: "
                                                << mObj->toPlainJsonString());
                                continue;
                            }
                            info.m_time = BPTime(sVal->value());
                        } else {
                            BPLOG_WARN_STRM("bad autoupdatePermissions key: " 
                                            << pit->first << " in map "
                                            << mObj->toPlainJsonString());
                            continue;
                        }
                    }
                    BPLOG_DEBUG_STRM(domain << " AutoUpdateInfo: " << info.toString());
                    m_autoUpdatePermissions[domain] = info;
                }
            }

            // applied permission migrations
            const bp::List* lObj = dynamic_cast<const bp::List*>(pm["appliedAutoUpdatePermissionMigrations"]);
            if (lObj) {
                vector<const bp::Object*> v = *lObj;
                for (size_t i = 0; i < v.size(); ++i) {
                    const bp::String* s = dynamic_cast<const bp::String*>(v[i]);
                    if (s) {
                        m_appliedPermMigrations.insert(string(*s));
                    }
                }
            }

            // Now apply newAutoUpdatePerms and newDomainPerms
            map<string, AutoUpdatePerms>::const_iterator it;
            for (it = newAutoUpdatePerms.begin(); it != newAutoUpdatePerms.end(); ++it) {
                const AutoUpdatePerms& perms = it->second;
                if (perms.m_platform != eUnknown) {
                    if (queryAutoUpdatePlatform(perms.m_domain) == eUnknown) {
                        BPLOG_DEBUG_STRM("newAutoUpdatePerms sets silent platform update for "
                                         << perms.m_domain);
                        setAutoUpdatePlatform(perms.m_domain, perms.m_platform);
                    }
                }
                map<string, Permission>::const_iterator si;
                for (si = it->second.m_services.begin();
                     si != it->second.m_services.end(); ++si) {
                    if (si->second == eUnknown) continue;
                    if (queryAutoUpdateService(perms.m_domain, si->first) == eUnknown) {
                        BPLOG_DEBUG_STRM("newAutoUpdatePerms "
                                         << (si->second == eAllowed ? "sets" : "revokes")
                                         << " silent service update for "
                                         << perms.m_domain << " / " << si->first);
                        setAutoUpdateService(perms.m_domain, si->first, si->second);
                    }
                }
            }

            map<string, DomainPerms>::const_iterator it2;
            for (it2 = newDomainPerms.begin(); it2 != newDomainPerms.end(); ++it2) {
                const DomainPerms& perms = it2->second;
                map<string, Permission>::const_iterator pi;
                for (pi = perms.m_perms.begin(); pi != perms.m_perms.end(); ++pi) {
                    if (pi->second == eUnknown) continue;
                    if (queryDomainPermission(perms.m_domain, pi->first) == eUnknown) {
                        BPLOG_DEBUG_STRM("newDomainPerms "
                                         << (pi->second == eAllowed ? "sets" : "revokes")
                                         << " permission " << pi->first
                                         << " for " << perms.m_domain);
                        if (pi->second == eAllowed) {
                            addDomainPermission(perms.m_domain, pi->first);
                        } else {
                            revokeDomainPermission(perms.m_domain, pi->first);
                        }
                    }
                }
            }
            
        } catch (const bp::error::Exception& e) {
            BPLOG_WARN_STRM("error parsing domainPermissions: " << e.what());
            domainPermsBad = true;
        } catch (const runtime_error&) {
            BPLOG_WARN_STRM("bad timestamp format in domainPermissions");
            domainPermsBad = true;
        }
    }
    
    if (domainPermsBad) {
        m_domainPermissions.clear();
        boost::filesystem::path badFile(domainPermsFile.string() + "_bad");
        (void) safeMove(domainPermsFile, badFile);
        (void) bp::file::safeRemove(domainPermsFile);
    }

    try {
        applyPermissionMigrations();
    } catch (const bp::error::Exception& e) {
        BPLOG_WARN_STRM("error applying permission migrations: " << e.what());
    }
}


string 
PermissionsManager::normalizeDomain(const string& domain) const
{
    // make a url
    bp::url::Url u;

    // can we successfully parse domain?  does it have a non empty host?
    if (!u.parse(domain) || u.host().size() == 0) {
        return domain;
    }

    // is host portion an ip address?
    vector<string> names = bp::dns::ipToNames(u.host());
    if (names.size()) {
        u.setHost(names[0]);
    }

    return u.toString();
}


void
PermissionsManager::saveDomainPermissions()
{
    using namespace bp::file;
    using namespace bp::paths;
    using namespace bp::strutil;
    
    boost::filesystem::path path = getDomainPermissionsPath();
    
    if (m_domainPermissions.empty()) {
        bp::file::safeRemove(path);
        return;
    }

    // form json map from m_requireDomainApproval, m_domainPermissions,
    // m_autoUpdatePermissions, and m_appliedPermMigrations
    bp::Map* pm = new bp::Map;
    if (!pm) {
        BP_THROW_FATAL("unable to allocate Map");
    }
    pm->add("requireDomainApproval", new bp::Bool(m_requireDomainApproval));
    
    // add any domain permissions
    if (!m_domainPermissions.empty()) {
        bp::Map* m = new bp::Map;
        if (!m) {
            BP_THROW_FATAL("unable to allocate Map");
        }
        map<string, map<string, PermissionInfo> >::const_iterator it;
        for (it = m_domainPermissions.begin(); it != m_domainPermissions.end(); ++it) {
            if (it->second.empty()) {
                continue;
            }
            bp::Map* dm = new bp::Map;
            if (!dm) {
                BP_THROW_FATAL("unable to allocate Map");
            }
            const string& domain = it->first;
            const map<string, PermissionInfo>& perms = it->second;
            map<string, PermissionInfo>::const_iterator mit;
            for (mit = perms.begin(); mit != perms.end(); ++mit) {
                bp::List* l = new bp::List;
                l->append(new bp::Bool(mit->second.m_allowed));
                l->append(new bp::String(mit->second.m_time.asString()));
                dm->add(mit->first, l);
            }
            m->add(domain, dm);
        }
        pm->add("domainPermissions", m);
    }
    
    // add any autoupdate permissions
    if (!m_autoUpdatePermissions.empty()) {
        bp::Map* m = new bp::Map;
        if (!m) {
            BP_THROW_FATAL("unable to allocate Map");
        }
        map<string, AutoUpdateInfo>::const_iterator it;
        for (it = m_autoUpdatePermissions.begin(); it != m_autoUpdatePermissions.end(); ++it) {
            bp::Map* dm = new bp::Map;
            if (!dm) {
                BP_THROW_FATAL("unable to allocate Map");
            }
            const string& domain = it->first;
            const AutoUpdateInfo& info = it->second;
            switch (info.m_platform) {
                case eAllowed:
                    dm->add("platform", new bp::Bool(true));
                    break;
                case eNotAllowed:
                    dm->add("platform", new bp::Bool(false));
                    break;
                case eUnknown:
                    break;
            }
            
            if (!info.m_services.empty()) {
                bp::Map* sm = new bp::Map;
                if (!sm) {
                    BP_THROW_FATAL("unable to allocate Map");
                }
                map<string, Permission>::const_iterator svcIter;
                for (svcIter = info.m_services.begin(); 
                     svcIter != info.m_services.end(); ++svcIter) {
                    switch (svcIter->second) {
                        case eAllowed:
                            sm->add(svcIter->first, new bp::Bool(true));
                            break;
                        case eNotAllowed:
                            sm->add(svcIter->first, new bp::Bool(false));
                            break;
                        case eUnknown:
                            break;
                    }
                }
                dm->add("services", sm);
            }
            dm->add("timeStamp", new bp::String(info.m_time.asString()));
            m->add(domain, dm);
        }
        pm->add("autoUpdatePermissions", m);
    }
    
    // add any applied permission migrations
    if (!m_appliedPermMigrations.empty()) {
        bp::List* l = new bp::List;
        set<string>::const_iterator it;
        for (it = m_appliedPermMigrations.begin();
             it != m_appliedPermMigrations.end(); ++it) {
            l->append(new bp::String(*it));
        }
        pm->add("appliedAutoUpdatePermissionMigrations", l);
    }

    // stringify and store
    string json = pm->toPlainJsonString(true);
    if (!storeToFile(path, json)) {
        BPLOG_WARN("store to file failed");
    }

    delete pm;
}


bool 
PermissionsManager::domainPatternValid(const string& pattern) const
{
    // sanity check
    if (pattern.empty()) {
        return false;
    }

    // If pattern starts with '/', it is a filename pattern
    if (pattern[0] == '/') {
        // TODO: What restrictions do we want on filename patterns?
        return true;
    }

    // Pattern must be a domain pattern.  Make sure it ends with 
    // ".somehost.somedomain" and that the "somehost" and
    // "somedomain" do not contain '*'
    vector<string> edges = bp::strutil::split(pattern, ".");
    if (edges.size() < 2) {
        BPLOG_DEBUG_STRM("pattern " << pattern << " doesn't contain "
                         << "at least two edges, ignored");
        return false;
    }
    if (edges[edges.size()-2].find('*') != string::npos
        || edges[edges.size()-1].find('*') != string::npos) {
        BPLOG_DEBUG_STRM("pattern " << pattern << " may not contain wildcard"
                         << "in last two edges, ignored");
        return false;
    }
    return true;
}


void
PermissionsManager::applyPermissionMigrations()
{
    // Do we have autoupdate permission migration to apply?
    if (m_permMigrations.empty() || m_domainPermissions.empty()) {
        return;
    }

    set<string> newMigrations;
    vector<pair<string, string> > toApply;
    map<string, AutoUpdateInfo> info = queryAutoUpdate();
    for (size_t pi = 0; pi < m_permMigrations.size(); ++pi) {
        // If we've already applied this migration, skip it.
        // Otherwise, apply it and remember the guid.
        string guid = m_permMigrations[pi].m_guid;
        if (m_appliedPermMigrations.count(guid) > 0) {
            BPLOG_DEBUG_STRM("skipping applied migration " << guid);
            continue;
        }
        newMigrations.insert(guid);

        const string& domain = m_permMigrations[pi].m_domain;
        const vector<string>& oldVec = m_permMigrations[pi].m_old;
        const vector<string>& newVec = m_permMigrations[pi].m_new;
        map<string, AutoUpdateInfo>::const_iterator iter;
        for (iter = info.begin(); iter != info.end(); ++iter) {
            if (domain != "*"
                && !bp::strutil::matchesWildcard(iter->first, domain)) {
                continue;
            }
            bool anyAllowed = false;
            bool allAllowed = true;
            for (size_t i = 0; i < oldVec.size(); i++) {
                Permission p = queryAutoUpdateService(iter->first, oldVec[i]);
                if (p == eAllowed) {
                    anyAllowed = true; 
                } else {
                    allAllowed = false;
                }
            }

            bool doUpdate = m_permMigrations[pi].m_operator == "or" 
                            ? anyAllowed : allAllowed;
            if (doUpdate) {
                for (size_t i = 0; i < newVec.size(); i++) {
                    toApply.push_back(pair<string, string>(iter->first,
                                                           newVec[i]));
                }
            }
        }
    }

    for (size_t i = 0; i < toApply.size(); ++i) {
        BPLOG_DEBUG_STRM("autoupdate migrate sets " << toApply[i].first
                         << " / " << toApply[i].second);
        setAutoUpdateService(toApply[i].first, toApply[i].second, eAllowed);
    }

    // persist applied migrations
    set<string>::const_iterator si;
    for (si = newMigrations.begin(); si != newMigrations.end(); ++si) {
        m_appliedPermMigrations.insert(*si);
    }
    saveDomainPermissions();
}


void
PermissionsManager::onTransactionFailed(unsigned int tid,
                                        const std::string& msg)
{
    BPLOG_WARN_STRM("PermissionsManager transaction failed: " << msg);
    map<unsigned int, IPermissionsManagerListener *>::iterator it =
        m_listeners.find(tid);
    if (it == m_listeners.end()) {
        BPLOG_WARN_STRM("PermissionsManager sees event "
                        << tid << " with no listener");
        return;
    }
    IPermissionsManagerListener *  listener = it->second;
    m_listeners.erase(it);
    m_error = true;
    listener->cantGetUpToDate();
}

void
PermissionsManager::gotPermissions(unsigned int tid,
                                   vector<unsigned char> permBundle)
{
    using namespace bp::file;
    using namespace bp::paths;
  
    bool ok = false;
    map<unsigned int, IPermissionsManagerListener *>::iterator it =
        m_listeners.find(tid);
    if (it == m_listeners.end()) {
        BPLOG_WARN_STRM("PermissionsManager sees event "
                        << tid << " with no listener");
        return;
    }
    IPermissionsManagerListener * listener = it->second;
    m_listeners.erase(it);

    boost::filesystem::path tmpPath = getTempPath(getTempDirectory(), "perms_pkg");
    try {
        // extract perms
        if (permBundle.size() == 0) {
            throw runtime_error("empty permissions bpkg");
        }
        string bpkgStr((const char *) &(permBundle[0]), permBundle.size());
        if (!bp::strutil::storeToFile(tmpPath, bpkgStr)) {
            throw runtime_error("unable to save perms to " + tmpPath.string());
        }
        string permsStr;
        string errMsg;
        BPTime timestamp;
        if (!bp::pkg::unpackToString(tmpPath, permsStr, timestamp, errMsg)) {
            throw runtime_error("unable to unpack permissions bpkg: " + errMsg);
        }
        
        // no replay attacks.  timestamp cannot be older
        string oldTSStr;
        if (bp::phash::get(kPermsTSKey, oldTSStr)) {
            BPTime oldTS(oldTSStr);
            if (timestamp.compare(oldTS) < 0) {
                throw runtime_error("bad timestamp in permissions bpkg");
            }
        }
            
        // replace cache and update our internal state
        if (!bp::strutil::storeToFile(getPermissionsPath(), permsStr)) {
            throw runtime_error("unable to update permissions");
        }
                        
        // load 'em up
        load();
        if (m_badPermissionsOnDisk) {
            throw runtime_error("bad permissions file after fetch");
        }
            
        // update timestamps
        bp::phash::set(kPermsTSKey, timestamp.asString());
        BPTime now;
        bp::phash::set(kLastCheckTSKey, now.asString());
            
        ok = true;

        // certificates taken care of by bp::sign
            
    } catch (exception& e) {
        cerr << "perms oopsie: " << e.what() << endl;
        BPLOG_ERROR_STRM("Error getting updated permissions: " << e.what());
        ok = false;
    }   
    (void) bp::file::safeRemove(tmpPath);
    
    // Whew!  We're done
    if (ok) {
        listener->gotUpToDate();
    } else {
        m_error = true;
        listener->cantGetUpToDate();
    }
}

void
PermissionsManager::onHop(void * context)
{
    IPermissionsManagerListener * l = (IPermissionsManagerListener *) context;
    l->cantGetUpToDate();
}

std::vector<PermissionsManager::PermissionDesc>
PermissionsManager::queryAllPermissions() const
{
    std::vector<PermissionsManager::PermissionDesc> allPerms;

    // first merge all domain permissions into the allperms vector
    std::map<std::string, std::map<std::string, PermissionInfo> >::const_iterator dpi;
    for (dpi = m_domainPermissions.begin(); dpi != m_domainPermissions.end(); dpi++) {
        std::map<std::string, PermissionInfo>::const_iterator permit;
        for (permit = dpi->second.begin();
             permit != dpi->second.end();
             permit++) {
            PermissionsManager::PermissionDesc pd;
            pd.type = permit->first;
            pd.domain = dpi->first;
            pd.allowed = permit->second.m_allowed ? eAllowed : eNotAllowed;
            pd.time = permit->second.m_time;
            allPerms.push_back(pd);
        }
    }

    // now merge all autoupdate permissions into the allperms vector         
    std::map<std::string, AutoUpdateInfo>::const_iterator aupi;
    for (aupi = m_autoUpdatePermissions.begin(); aupi != m_autoUpdatePermissions.end(); aupi++) {
        // first add the "SlentPlatformUpdate" perm if appropriate
        if (aupi->second.m_platform != eUnknown) {
            PermissionsManager::PermissionDesc pd;
            pd.type.append(kSilentPlatformUpdate);
            pd.domain = aupi->first;
            pd.allowed = aupi->second.m_platform;
            pd.time = aupi->second.m_time;
            allPerms.push_back(pd);
        }


        // now the "SilentServiceUpdate" perm if appropriate
        if (aupi->second.m_services.size() > 0) {
            std::map<std::string, Permission>::const_iterator sit;

            std::set<std::string> allowed;
            std::set<std::string> denied;
            
            for (sit = aupi->second.m_services.begin(); 
                 sit != aupi->second.m_services.end(); sit++) {
                switch (sit->second) {
                    case eAllowed:
                        allowed.insert(sit->first);
                        break;
                    case eNotAllowed:
                        denied.insert(sit->first);
                        break;
                    case eUnknown:
                        // empty
                        break;
                    default:
                        BPLOG_ERROR_STRM("unknown perm(" << sit->second
                                         << ") for domain/service "
                                         << aupi->first << "/" << sit->first);
                        break;
                }
            }

            PermissionsManager::PermissionDesc pd;
            pd.type.append(kSilentServiceUpdate);
            pd.domain = aupi->first;
            pd.allowed = eUnknown;
            pd.time = aupi->second.m_time;

            if (allowed.size()) {
                pd.allowed = eAllowed;
                pd.extra = allowed;
                allPerms.push_back(pd);
            }

            if (denied.size()) {
                pd.allowed = eNotAllowed;
                pd.extra = denied;
                allPerms.push_back(pd);
            }
        }
    }

    return allPerms;
}
