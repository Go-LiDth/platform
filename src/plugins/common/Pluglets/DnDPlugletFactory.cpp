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

#include "DnDPlugletFactory.h"
#include "BPUtils/BPLog.h"
#include "PluginCommonLib/CommonErrors.h"


#define DEFAULT_DROP_LIMIT 1000

#define _MAKESTRING(x) #x
#define MAKESTRING(x) _MAKESTRING(x)


////////////////////////////////////////////////////////////
// Pluglet Introspection Support

// ----------------  version 1

BPArgumentDefinition s_addArguments[] = {
    {
        (BPString) "id",
        (BPString) "The 'id' of the registered DOM element to operate on",
        BPTString,
        true
    },
    {
        (BPString) "mimeTypes",
        (BPString) "A list of mimetypes to filter against.  Only items which "
        "match one of these mimetypes will be accepted.  Default "
        "is empty (all dropped items will be accepted).",
        BPTList,
        false
    },
    {
        (BPString) "includeGestureInfo",
        (BPString) "Should selection gesture information be included in the argument "
        "to the 'drop' callback?  Default is false.  If false, the argument "
        "is an array of opaque file handles.  If true, the argument is a map "
        "containing keys 'actualSelection' and 'files'.  The 'actualSelection' "
		"value is a list of opaque file handles representing what was actually "
		"selected by the UI gesture.  The 'files' value is an array of maps, "
		"each entry containing keys 'handle' (value is an opaque file handle) "
		"and 'parent' (value is handle id of element in 'actualSelection' list "
		"which resulted in this file being included).",
        BPTBoolean,
        false
    },
    {
        (BPString) "limit",
        (BPString) "Maximum number of items which will be included in a drop. "
		"Default is " MAKESTRING(DEFAULT_DROP_LIMIT),
        BPTInteger,
        false
    }
};

BPArgumentDefinition s_idArgument[] = {
    {
        (BPString) "id",
        (BPString) "The 'id' of the DOM element to operate on",
        BPTString,
        true
    }
};

BPArgumentDefinition s_attachArguments[] = {
    {
        (BPString) "id",
        (BPString) "The 'id' of the registered DOM element to which you wish to attach ",
        BPTString,
        true
    },
    {
        (BPString) "hover",
        (BPString) "A function that will be invoked when the user hovers over "
        "the drop target.  Argument is a boolean which when true means "
        "the user has entered the region, and when false means they have "
        "exited.",
        BPTCallBack,
        false
    },
    {
        (BPString) "drop",
        (BPString) "A function that will be invoked when the user drops files on "
        "your drop target.  Arguments to the callback vary depending on "
        "whether 'includeGestureInfo' was set true for the target. "
        "See the documentation for the 'includeGestureInfo' argument "
        "to 'AddDropTarget()'",
        BPTCallBack,
        false
    }
};

BPArgumentDefinition s_enableArguments[] = {
    {
        (BPString) "id",
        (BPString) "The 'id' of the registered DOM element for which you wish to "
        "enable/disable drag/drop activity.",
        BPTString,
        true
    },
    {
        (BPString) "enable",
        (BPString) "A boolean indicating whether activity should be enabled (true) "
        "or disabled (false).",
        BPTBoolean,
        true
    }
};

BPFunctionDefinition s_dndFunctions[] = {
    {
        (BPString) "AddDropTarget",
        (BPString) "Starts monitoring drag/drop activity for the specified element.",
        sizeof(s_addArguments)/sizeof(s_addArguments[0]),
        s_addArguments
    },
    {
        (BPString) "AttachCallbacks",
        (BPString) "AttachCallbacks to a registered drop target.  This function "
        "will not return until RemoveDropTarget is called, so it should "
        "not be invoked synchronously.",
        sizeof(s_attachArguments)/sizeof(s_attachArguments[0]),
        s_attachArguments
    },
    {
        (BPString) "RemoveDropTarget",
        (BPString) "Stop monitoring an element for drag/drop activity.",
        sizeof(s_idArgument)/sizeof(s_idArgument[0]),
        s_idArgument
    },
    {
        (BPString) "EnableDropTarget",
        (BPString) "Enable/disable an element for drag/drop activity.  AddDropTarget "
        "must have already been called for the element.",
        sizeof(s_enableArguments)/sizeof(s_enableArguments[0]),
        s_enableArguments
    },
    {
        (BPString) "ListTargets",
        (BPString) "Returns a list of the ids of the currently registered drop targets.",
        0,
        NULL
    }
};


// a description of this pluglet.
static BPServiceDefinition s_dndPlugletDef = {
    (BPString) "DragAndDrop",
    1, 0, 1,
    (BPString) "Allow drag and drop of files from desktop to web browser.",
    sizeof(s_dndFunctions)/sizeof(s_dndFunctions[0]),
    s_dndFunctions
};


// ----------------  version 2, shares many args with version 1

BPArgumentDefinition s_addArguments2[] = {
    {
        (BPString) "id",
        (BPString) "The 'id' of the registered DOM element to operate on",
        BPTString,
        true
    }
};

BPFunctionDefinition s_dndFunctions2[] = {
    {
        (BPString) "AddDropTarget",
        (BPString) "Starts monitoring drag/drop activity for the specified element.",
        sizeof(s_addArguments2)/sizeof(s_addArguments2[0]),
        s_addArguments2
    },
    {
        (BPString) "AttachCallbacks",
        (BPString) "AttachCallbacks to a registered drop target.  This function "
        "will not return until RemoveDropTarget is called, so it should "
        "not be invoked synchronously.",
        sizeof(s_attachArguments)/sizeof(s_attachArguments[0]),
        s_attachArguments
    },
    {
        (BPString) "RemoveDropTarget",
        (BPString) "Stop monitoring an element for drag/drop activity.",
        sizeof(s_idArgument)/sizeof(s_idArgument[0]),
        s_idArgument
    },
    {
        (BPString) "EnableDropTarget",
        (BPString) "Enable/disable an element for drag/drop activity.  AddDropTarget "
        "must have already been called for the element.",
        sizeof(s_enableArguments)/sizeof(s_enableArguments[0]),
        s_enableArguments
    },
    {
        (BPString) "ListTargets",
        (BPString) "Returns a list of the ids of the currently registered drop targets.",
        0,
        NULL
    }
};

// a description of this pluglet.
static BPServiceDefinition s_dndPlugletDef2 = {
    (BPString) "DragAndDrop",
    2, 0, 0,
    (BPString) "Allow drag and drop of files from desktop to web browser.",
    sizeof(s_dndFunctions2)/sizeof(s_dndFunctions2[0]),
    s_dndFunctions2
};

DnDPlugletFactory::DnDPlugletFactory()
{
    m_descriptions.push_back(bp::service::Description());
    m_descriptions.back().fromBPServiceDefinition(&s_dndPlugletDef);
    m_descriptions.push_back(bp::service::Description());
    m_descriptions.back().fromBPServiceDefinition(&s_dndPlugletDef2);
}

