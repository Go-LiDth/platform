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


#ifndef __BPPLATFORMUTIL_H__
#define __BPPLATFORMUTIL_H__

#include "BPUtils/bpsemanticversion.h"

namespace bp {
    namespace platformutil {
        // remove a platform if:
        //  a) it is not installed (no .installed file)
        //  b) it is not installing (no .installing file)
        //  c) it is not running
        // force flag ignores these checks
        void removePlatform(const bp::SemanticVersion& version,
                            bool force = false);
    };
};

#endif
