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
 *  PluginLogging.cpp
 *
 *  Created by David Grigsby on 12/31/2007.
 *  Copyright 2007 Yahoo! Inc. All rights reserved.
 *
 */
#include "PluginLogging.h"
#include <iostream>
#include "platform_utils/bpconfig.h"
#include "platform_utils/LogConfigurator.h"
#include "platform_utils/ProductPaths.h"

using namespace std;


namespace bp {
namespace plugin {


bool setupLogging( const boost::filesystem::path& logfilePath )
{
    bp::log::Configurator cfg;
    cfg.loadConfigFile();
    cfg.setPath( bp::paths::getObfuscatedWritableDirectory() / logfilePath );
    cfg.configure();
    
    return true;
}


} // plugin
} // bp

