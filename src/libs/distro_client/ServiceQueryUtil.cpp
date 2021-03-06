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
 * ServiceQueryUtil - logic to perform service matching, and determine
 *                    hot to satisfy requirements.
 */

//#include "BPUtils/BPUtils.h"
#include "ServiceQueryUtil.h"

AvailableServiceList
ServiceQueryUtil::serviceSummaryToACL(
    const std::list<bp::service::Summary> & cs)
{
    AvailableServiceList acl;
    {
        std::list<bp::service::Summary>::const_iterator it;
        for (it = cs.begin(); it != cs.end(); it++)
        {
            AvailableService acp;
            acp.name.append(it->name().c_str());
            std::string verStr(it->version().c_str());
            if (!acp.version.parse(verStr)) {
                BPLOG_WARN_STRM("Error parsing distribution server response! ("
                                << (it->version().empty() ? "empty version string" :
                                    it->version())
                                << ")");
                return AvailableServiceList();
            }
            if (it->type() == bp::service::Summary::Dependent) {
                acp.dependentService = true;
                acp.providerName = it->usesService();
                acp.providerMinversion = it->usesMinversion().asString();
                acp.providerVersion = it->usesVersion().asString();
            } else {
                acp.dependentService = false;
            }
            acl.push_back(acp);
        }
    }

    return acl;
}


bool
ServiceQueryUtil::findBestMatch(std::string name,
                                std::string versionStr,
                                std::string minversionStr,
                                const AvailableServiceList & list,
                                AvailableService & oMatch)
{
    // goal is to find the highest version service that satisfies
    // the specified constraints.  To do this we iterate through
    // the list of all available services as returned from the server
    // and for each service we check
    // 1. if the service name matches
    // 2. if the service version matches
    // 3. it the service version is greater than the minversion
    // 4. if we have a current candidate, if the version of the
    //    new candidate is greater than that of the current candidate.

    // when we find a candidate service, this thing is populated.
    // NULL means we've yet to find a service that satisfies the
    // constraints.
    AvailableService cand;
    bool rv = false;
    
    // when nav is non-null, found is meaningful, and is the version
    // of the service we've found
    bp::SemanticVersion found;
    bp::SemanticVersion version;
    bp::SemanticVersion minversion;

    // parse version string if present
    if (!versionStr.empty()) {
        if (!version.parse(versionStr))
            return false;
    }

    // parse minversion string if present
    if (!minversionStr.empty()) {
        if (!minversion.parse(minversionStr))
            return false;
    }

    // iterate through the list.  
    std::list<AvailableService>::const_iterator it;
    
    for (it = list.begin(); it != list.end(); it++)
    {
        bp::SemanticVersion current = it->version;
        
        // (1)
        if (name.compare(it->name) != 0) continue;
        

        // (2, 3, and 4) is this a newer match than what we've already got?
        if (!bp::SemanticVersion::isNewerMatch(current, found,
                                               version, minversion))
        {
            continue;
        }
        
        // passed our tests!
        found = current;
        cand = *it;
        rv = true;
    }

    oMatch = cand;
    return rv;
}

static bool findServiceSet(
    const std::list<ServiceRequireStatement> & iRequirements,
    const AvailableServiceList & available,
    const AvailableServiceList & installed,
    bool wantNewest,
    AvailableServiceList & need)
{
    // don't modify the const param
    std::list<ServiceRequireStatement> requirements = iRequirements;

    // iterate through all of the require statements
    while (requirements.size() > 0) {
        ServiceRequireStatement curStmt = requirements.front();
        requirements.pop_front();

        AvailableService acp;
        
        // determine if the current statement is already satisfied by
        // something on the need list
        if (ServiceQueryUtil::findBestMatch(
                curStmt.m_name, curStmt.m_version, curStmt.m_minversion,
                need, acp))
        {
            continue;
        }

        // figure out if anything is currently installed which satisfies
        // this requirement
        AvailableService iacp;
        bool haveInstalled = ServiceQueryUtil::findBestMatch(
            curStmt.m_name, curStmt.m_version, curStmt.m_minversion,
            installed, iacp);

        // figure out if anything is available from the distro server
        // which would satisfy this requirement
        bool haveFromDistro = ServiceQueryUtil::findBestMatch(
            curStmt.m_name, curStmt.m_version, curStmt.m_minversion,
            available, acp);

        // now given wantNewest, iacp and acp, we must decide wether to
        // include this service.
        
        AvailableService * dcp = NULL;

        // if nothing is installed, the choice is simple
        if (!haveInstalled) {
            if (!haveFromDistro) {
                //log! unsatisfiable dependencies!
                BPLOG_WARN_STRM("unsatisfiable dependency: "
                                << curStmt.m_name
                                << " v. "
                                << curStmt.m_version
                                << " mv. "
                                << curStmt.m_minversion);
                return false;
            }
            need.push_back(acp);
            dcp = &acp;
        } else if (haveFromDistro) {
            // have something which will work.  only add service to need
            // list if we want newest and a newer one is available
            BPASSERT(!acp.version.isUnset() && !iacp.version.isUnset());
            if (wantNewest && (acp.version.compare(iacp.version) > 0)) {
                need.push_back(acp);
                dcp = &acp; // check deps on new service
            } else {
                // check deps on installed service
                dcp = &iacp;
            }
        } else {
            // nothing on available list satisfies, but something on
            // installed service list satisfies.  
            dcp = &iacp;
        }

        BPASSERT(dcp != NULL);
        
        // if dcp depends on something, push the dependency onto the
        // requirements list 
        if (dcp->dependentService) {
            ServiceRequireStatement dep;
            dep.m_name = dcp->providerName;
            dep.m_version = dcp->providerVersion;
            dep.m_minversion = dcp->providerMinversion;
            requirements.push_back(dep);
        }
    }
        
    return true;
}


bool 
ServiceQueryUtil::findSatisfyingServices(
    const std::list<ServiceRequireStatement> & requirements,
    const std::list<bp::service::Summary> & installedList,
    const AvailableServiceList & available,
    bool wantNewest, AvailableServiceList & need)
{

    // 0. get installedList into a easier to manage format
    AvailableServiceList installed = serviceSummaryToACL(installedList);
    
    // 1. Build a set of services that satisfy the requirements (verticies)
    if (!findServiceSet(requirements, available, installed, wantNewest, need))
    {
        return false;
    }

    // 2. Build a set of start nodes and edges.
    //
    // we do this by building a set of the need list, iterating through need,
    // list, and removing any services that are required by others from
    // the startNodes set
    std::set<AvailableService> startNodes;
    // no multimap, one service may not directly depend on multple others
    std::map<AvailableService, AvailableService> edges;
    {
        std::list<AvailableService>::iterator it;
        for (it = need.begin(); it != need.end(); it++)
        {
            startNodes.insert(*it);
        }

        for (it = need.begin(); it != need.end(); it++)
        {
            if (it->dependentService)
            {
                // find and delete the satisfying dependency
                AvailableService dep;
                if (ServiceQueryUtil::findBestMatch(it->providerName,
                                                    it->providerVersion,
                                                    it->providerMinversion,
                                                    need, dep)) {
                    std::set<AvailableService>::iterator dit;
                    dit = startNodes.find(dep);
                    if (dit != startNodes.end()) startNodes.erase(dit);
                    edges[*it] = dep;
                }
            }
        }
    }

    // 3. perform topological sort
    // from wikipedia
    // L ← Empty list where we put the sorted elements  
    // Q ← Set of all nodes with no incoming edges
    // while Q is non-empty do
    //   remove a node n from Q
    //   insert n into L
    //   for each node m with an edge e from n to m do
    //     remove edge e from the graph
    //     if m has no other incoming edges then
    //       insert m into Q
    // if graph has edges then
    //   output error message (graph has a cycle)
    // else 
    //   output message (proposed topologically sorted order: L)
    // 
    std::list<AvailableService> sorted;
    
    while (startNodes.size() > 0) {
        AvailableService n = *(startNodes.begin());
        startNodes.erase(startNodes.begin());
        sorted.push_back(n);
        
        std::map<AvailableService, AvailableService>::iterator eit;
        eit = edges.find(n);
        if (eit != edges.end()) {
            AvailableService m = eit->second;
            edges.erase(eit);
            
            // n^2 here, oh well
            bool mHasIncomingEdges  = false;
            std::map<AvailableService, AvailableService>::iterator it2;
            for (it2 = edges.begin(); it2 != edges.end(); it2++) {
                if (!(it2->second < m) && !(m < it2->second)) {
                    mHasIncomingEdges = true;
                }
            }

            if (!mHasIncomingEdges) sorted.push_back(m);
        }
    }

    // if edges contains anything, there's a cyclic dependency!
    if (edges.size() != 0) {
        BPLOG_ERROR("!!! cyclic dependency detected in required services");
    }

    // 4. reverse list and return
    sorted.reverse();
    need = sorted;

    return true;
}

bool
ServiceQueryUtil::haveUpdates(
    const std::list<ServiceRequireStatement> & requirements,
    const std::list<bp::service::Summary> & installedList,
    const std::list<bp::service::Summary> & updateList,
    AvailableServiceList & best)
{
    // 0. get installedList and updateList into a easier to manage format
    AvailableServiceList installed = serviceSummaryToACL(installedList);
    AvailableServiceList updates = serviceSummaryToACL(updateList);    

    // 1. get the best satisfaction of requirements, favoring updates
    if (!findServiceSet(requirements, updates, installed, true, best)) {
        // can't satisfy        
        return false;
    }

    // 2. build a set for easy/fast checking of presence on installed list
    std::set<AvailableService> installedSet;
    {
        std::list<AvailableService>::iterator it;
        for (it = installed.begin();
             it != installed.end();
             it++)
        {
            installedSet.insert(*it);
        }
    }

    // 3. now prune anything off the best list that is already installed.
    {
        std::list<AvailableService> pruned;
        std::list<AvailableService>::iterator it;
        for (it = best.begin(); it != best.end(); it++)
        {
            if (installedSet.find(*it) == installedSet.end()) {
                pruned.push_back(*it);
            }
        }

        best = pruned;
    }

    return true;
}

ServiceList
ServiceQueryUtil::reformatAvailableServiceList(const AvailableServiceList & acl)
{
    // turn AvailableServiceList into ServiceList
    ServiceList clp;

    std::list<AvailableService>::const_iterator it;
    for (it = acl.begin(); it != acl.end(); it++)
    {
        std::pair<std::string, std::string>
            clet(it->name, it->version.asString());
            
        clp.push_back(clet);
    }
    
    return clp;
}
