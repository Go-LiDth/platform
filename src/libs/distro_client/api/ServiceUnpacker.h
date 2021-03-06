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
 * ServiceUnpacker.h
 *
 * Created by Gordon Durand on 07/23/07.
 * Copyright 2007 Yahoo! Inc.  All rights reservered.
 */

#ifndef __SERVICEUNPACKER_H__
#define __SERVICEUNPACKER_H__

#include <string>
#include <map>
#include "Unpacker.h"

class ServiceUnpacker : virtual public Unpacker
{
 public:
    /** 
     * Create an instance to unpack a bpkg file
     */
    ServiceUnpacker(const boost::filesystem::path& pkgFile,
                    const boost::filesystem::path& certFile = boost::filesystem::path());
    
    /** 
     * Create an instance to unpack a buffer containing
     * a bpkg
     */
    ServiceUnpacker(const std::vector<unsigned char> & buf,
                    const boost::filesystem::path& certFile = boost::filesystem::path());

    /**
     * Create an instance from a directory.  Second arg
     * is ignored, just lets us disambiguate constructors
     */
    ServiceUnpacker(const boost::filesystem::path& dir,
                    int);
                
    virtual ~ServiceUnpacker(); 

    // unpacks, returns true on success
    bool unpack(std::string& errMsg);

    // after an unpack(), installs service
    // On error, error msg returned in errMsg    
    bool install(std::string& errMsg);

    // unpacks to dir, returns true on success
    bool unpackTo(const boost::filesystem::path& dir,
                  std::string& errMsg);

 private:
    boost::filesystem::path m_tmpDir;
    boost::filesystem::path m_dir;
};

#endif
