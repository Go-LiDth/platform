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

#import "PluginCommonLib/bppluginutil.h"
#import "PluginCommonLib/CommonErrors.h"
#import "PluginCommonLib/FileBrowsePluglet.h"
#import "BPUtils/bperrorutil.h"
#import "BPUtils/bpfile.h"
#import "BPUtils/bpurl.h"
#import "BPUtils/BPLog.h"
#import "BPUtils/OS.h"
#import "platform_utils/bpbrowserinfo.h"
#import "platform_utils/bplocalization.h"
#import <sstream>
#import <Cocoa/Cocoa.h>

using namespace bp::localization;
using namespace std;

// Ugh, due to a modality bug in 64 bit safari 5 on snow leopard,
// we must play some ugly games to get modality to work.  Hence,
// we need two little subclasses just to keep track of whether
// the "OK" button was pressed on a browse/save dialog.
//
@interface MyOpenPanel : NSOpenPanel {
    bool m_ok;
}
- (id) init;
- (IBAction) ok: (id)sender;
- (IBAction) cancel: (id)sender;
- (bool) okSelected;
@end

@implementation MyOpenPanel
- (id) init
{
    if ((self = [super init])) {
        m_ok = false;
    }
    return self;
}

- (IBAction) ok: (id)sender
{
    m_ok = true;
    return [super ok:sender];
}

- (IBAction) cancel: (id)sender 
{
    m_ok = false;
    return [super cancel:sender];
}

- (bool) okSelected 
{
    return m_ok;
}
@end

@interface MySavePanel : NSSavePanel {
    bool m_ok;
}
- (id) init;
- (IBAction) ok: (id)sender;
- (IBAction) cancel: (id)sender;
- (bool) okSelected;
@end

@implementation MySavePanel
- (id) init
{
    if ((self = [super init])) {
        m_ok = false;
    }
    return self;
}

- (IBAction) ok: (id)sender
{
    m_ok = true;
    return [super ok:sender];
}

- (IBAction) cancel: (id)sender 
{
    m_ok = false;
    return [super cancel:sender];
}

- (bool) okSelected 
{
    return m_ok;
}
@end

// A delegate to apply mimetype filtering for the MyOpenPanel used below
@interface MyDelegate : NSObject {
    set<string>* m_mimetypes;
}
- (id) init;
- (void) setMimetypes: (set<string>*) mimetypes;
- (BOOL) panel: (id)sender shouldShowFilename: (NSString*)filename;
@end

@implementation MyDelegate
- (id) init
{
    if ((self = [super init])) {
        m_mimetypes = 0;
    }
    return self;
}

- (void) setMimetypes: (set<string>*) mimetypes
{
    m_mimetypes = mimetypes;
}

- (BOOL) panel: (id)sender shouldShowFilename: (NSString*)filename
{
    string path([filename UTF8String]);
    if (bp::file::isDirectory(path)) {
        return YES;
    }
    return bp::file::isMimeType(path, *m_mimetypes) ? YES : NO;
}
@end


static bool 
isSafari5OnSnowLeopard(const string& userAgent)
{
    bp::SemanticVersion osVersion, leastOSVersion;
    leastOSVersion.parse("10.6.0");
    osVersion.parse(bp::os::PlatformVersion());
    
    if (osVersion.compare(leastOSVersion) >= 0) {
        try {
            bp::BrowserInfo info(userAgent);
            if (info.browser() == "Safari" && info.version().majorVer() >= 5) {
                return true;
            }
        } catch (const bp::error::Exception& e) {
            BPLOG_ERROR_STRM("caught " << e.what());
            return false;
        }
    }

    return false;
}


static struct ProcessSerialNumber 
getPSN()
{
    struct ProcessSerialNumber psn;
    NSDictionary * dict = [[NSWorkspace sharedWorkspace] activeApplication];
    psn.highLongOfPSN = [[dict objectForKey: @"NSApplicationProcessSerialNumberHigh"] unsignedLongValue];
    psn.lowLongOfPSN = [[dict objectForKey: @"NSApplicationProcessSerialNumberLow"] unsignedLongValue];
    return psn;
}


static vector<boost::filesystem::path>
runPanel(NSSavePanel* panel,
         NSString* file,       // non-nil implies save panel
         const string& userAgent)
{
    MyOpenPanel* openPanel = file ? nil : (MyOpenPanel*)panel;
    MySavePanel* savePanel = file ? (MySavePanel*)panel : nil;
    std::vector<boost::filesystem::path> selection;
    if (isSafari5OnSnowLeopard(userAgent)) {
        BPLOG_DEBUG("Using 10.6+ Safari 5+ modality workaround for file browse");
        
        // let's get the PSN of the current active application for later re-activation
        struct ProcessSerialNumber psn = getPSN();

        [panel _loadPreviousModeAndLayout];
        [panel setDirectoryURL: nil];
        if (file) [panel setNameFieldStringValue: file];

        [NSApp activateIgnoringOtherApps: YES];
        NSModalSession session = [NSApp beginModalSessionForWindow:panel];
        for (;;) {
            if ([NSApp runModalSession:session] != NSRunContinuesResponse) break;
        }
        [NSApp endModalSession:session];

        // now re-activate whoever was active before us
        SetFrontProcess(&psn);
    } else {
        BPLOG_DEBUG("Using runModal[ForDirectory] for panel display");
        if (openPanel) {
            [openPanel runModal];
        } else {
            [savePanel runModalForDirectory: nil file: file];
        }
    }

    bool ok = openPanel ? [openPanel okSelected] : [savePanel okSelected];
    if (ok) {
        if (savePanel) {
            NSURL* url = [savePanel URL];
            if ([url isFileURL]) {
                selection.push_back([[url path] UTF8String]);
            } else {
                BPLOG_WARN_STRM("ignoring non-file url " << [url absoluteString]);
            }
        } else {
            NSArray* urls = [openPanel URLs];
            int count = [urls count];
            for (int i = 0; i < count; i++) {
                NSURL* url = [urls objectAtIndex:i];
                if (![url isFileURL]) {
                    BPLOG_WARN_STRM("ignoring non-file url " << [url absoluteString]);
                    continue;
                }
                selection.push_back([[url path] UTF8String]);
            }
        }
    }
    return selection;
}


void
FileBrowsePluglet::v1Browse(unsigned int tid,
                            const bp::Object* arguments,
                            plugletExecutionSuccessCB successCB,
                            plugletExecutionFailureCB failureCB,
                            void* callbackArgument)
{
    // Dig out args
    bool recurse = true;
    std::set<std::string> mimetypes;
    bool includeGestureInfo = false;
    unsigned int limit = 10000;
    if (!arguments) {
        BPLOG_WARN_STRM("execute called will NULL arguments");
        failureCB(callbackArgument, tid, pluginerrors::InvalidParameters, NULL);
        return;
    }
    if (arguments->has("recurse", BPTBoolean)) {
        recurse = ((bp::Bool*) arguments->get("recurse"))->value();
    }
    
    if (arguments->has("mimeTypes", BPTList)) {
        const bp::List* l = (const bp::List*) arguments->get("mimeTypes");
        for (unsigned int i = 0; l && (i < l->size()); i++) {
            const bp::String* s = dynamic_cast<const bp::String*>(l->value(i));
            if (s) {
                mimetypes.insert(s->value());
            }
        }
    } 
    
    if (arguments->has("includeGestureInfo", BPTBoolean)) {
        includeGestureInfo = ((bp::Bool*) arguments->get("includeGestureInfo"))->value();
    }
    
    if (arguments->has("limit", BPTInteger)) {
        limit = ((bp::Integer*) arguments->get("limit"))->value();
    }

    // Create our panel
    MyDelegate* delegate = [[MyDelegate alloc] init];
    [delegate setMimetypes: &mimetypes];
    MyOpenPanel* panel = [[MyOpenPanel alloc] init];
    [panel setDelegate:delegate];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseFiles:YES];
    string title = dialogTitle(FileBrowsePluglet::kSelectFilesFoldersKey);
    
    // Can folders be selected?
    if (recurse) {
        [panel setCanChooseDirectories:YES];
    } else {
        if (mimetypes.count(bp::file::kFolderMimeType) == 1) {
            [panel setCanChooseDirectories:YES];
            if (mimetypes.size() == 1) {
                [panel setCanChooseFiles:NO];
                title = dialogTitle(FileBrowsePluglet::kSelectFolderKey);
            }
        }
    }
    [panel setTitle: [NSString stringWithUTF8String: title.c_str()]];

    // Run the panel and get the results
    vector<boost::filesystem::path> selection = runPanel(panel, NULL, m_plugin->getUserAgent());
    [panel setDelegate:nil];
    [panel orderOut: nil];
    [panel release];
    [delegate release];

    if (selection.empty()) {
        failureCB(callbackArgument, tid, "FileBrowse.userCanceled",
                  "user canceled browse");
        return;
    }

    bp::Object* obj = NULL;
    unsigned int flags = 0;
    if (recurse) flags |= bp::pluginutil::kRecurse;
    if (includeGestureInfo) flags |= bp::pluginutil::kIncludeGestureInfo;
    obj = bp::pluginutil::applyFilters(selection, mimetypes, flags, limit);
        
    // return results
    successCB(callbackArgument, tid, obj);
    delete obj;
}


void
FileBrowsePluglet::browse(unsigned int tid,
                          plugletExecutionSuccessCB successCB,
                          plugletExecutionFailureCB failureCB,
                          void* callbackArgument)
{
    // Create our panel
    MyOpenPanel* panel = [[MyOpenPanel alloc] init];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:YES];
    string title = dialogTitle(FileBrowsePluglet::kSelectFilesFoldersKey);
    [panel setTitle: [NSString stringWithUTF8String: title.c_str()]];

    // Run the panel and get the results
    vector<boost::filesystem::path> selection = runPanel(panel, NULL, m_plugin->getUserAgent());
    [panel orderOut: nil];
    [panel release];

    if (selection.empty()) {
        failureCB(callbackArgument, tid, "FileBrowse.userCanceled",
                  "user canceled browse");
        return;
    }
    
    // return results
    bp::Map* m = new bp::Map;
    bp::List* l = new bp::List;
    vector<boost::filesystem::path>::const_iterator it;
    for (it = selection.begin(); it != selection.end(); ++it) {
        boost::filesystem::path path(*it);
        l->append(new bp::Path(path));
    }
    m->add("files", l);
    successCB(callbackArgument, tid, m);
    delete m;
}


void
FileBrowsePluglet::save(unsigned int tid,
                        const bp::Object* arguments,
                        plugletExecutionSuccessCB successCB,
                        plugletExecutionFailureCB failureCB,
                        void* callbackArgument)
{
    // dig out arguments
    string fileName;
    if (arguments && arguments->has("name", BPTString)) {
        fileName = ((bp::String*) arguments->get("name"))->value();
    }

    // Create our panel
    MySavePanel* panel = [[MySavePanel alloc] init];
    [panel setCanCreateDirectories: YES];

    // Append current url to default title.
    string title([[panel title] UTF8String]);
    title += string(" (") + currentUrlString() + ")";
    [panel setTitle: [NSString stringWithUTF8String: title.c_str()]];

    // Run the panel and get the results
    vector<boost::filesystem::path> selection = runPanel(panel,
                                                [NSString stringWithUTF8String: fileName.c_str()],
                                                m_plugin->getUserAgent());
    [panel orderOut: nil];
    [panel release];
        
    // return results
    if (selection.size() > 0) {
        bp::Object* obj = new bp::WritablePath(selection[0]);
        successCB(callbackArgument, tid, obj);
        delete obj;
    } else {
        failureCB(callbackArgument, tid, "FileBrowse.userCanceled",
                  "user canceled browse");
    }
}
