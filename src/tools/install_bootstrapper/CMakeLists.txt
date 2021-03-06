# ***** BEGIN LICENSE BLOCK *****
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is BrowserPlus (tm).
# 
# The Initial Developer of the Original Code is Yahoo!.
# Portions created by Yahoo! are Copyright (c) 2010 Yahoo! Inc.
# All rights reserved.
# 
# Contributor(s): 
# ***** END LICENSE BLOCK *****
SET(BrowserPlusBootstrapper_LINK_STATIC ArchiveLib BPUtils)

# Specify sources manually - Build templates for BINARY WIN32 doesn't add 
# .rc file properly.
SET(BrowserPlusBootstrapper_SOURCES
      bootstrap.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.rc)

# make sure we can find "resource.h" over on alpha centauri (in the binary output directory)
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

# Inhibit BuildTemplates from adding its own version resources.
SET(BrowserPlusBootstrapper_NO_VERSION_RESOURCES 1)

# The WIN32 arg causes linker subsytem GUI instead of CONSOLE.
YBT_BUILD(BINARY BrowserPlusBootstrapper WIN32)

ADD_DEPENDENCIES(BrowserPlusBootstrapper ArchiveLib_s BPUtils_s)

SET_TARGET_PROPERTIES(BrowserPlusBootstrapper PROPERTIES LINK_FLAGS
                      "/MACHINE:X86")

# copy the icon over to binary dir so the silly rc compiler can find it
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/bpicon.ico"
               "${CMAKE_CURRENT_BINARY_DIR}/bpicon.ico" COPYONLY)
