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
 * BPWebServe - A trivial little webserver mainly to test the http server
 *              implementation in bphttp.
 */
#include <iostream>
#include <sstream>
#include <vector>
#include "bphttp/HttpRequest.h"
#include "bphttp/HttpResponse.h"
#include "bphttp/HttpServer.h"
#include "BPUtils/bpfile.h"
#include "BPUtils/bptime.h"
#include "BPUtils/bpstrutil.h"

#ifdef WIN32
#include <Windows.h>
#define sleep Sleep
#endif

using namespace bp::http::server;
using namespace bp::http;
using namespace bp::file;
namespace bfs = boost::filesystem;

class DirHandler : public IHandler 
{
public:
    DirHandler() { }
    ~DirHandler() { }

    bool processRequest(const Request & request,
                        Response & response)
    {
        // translate request path into fs path
        bfs::path path(".");
        std::string urlPath = request.url.path();
        if (!urlPath.empty()) path /= urlPath;

        if (isRegularFile(path)) {
            std::string mt = *(mimeTypes(path).begin());
            response.headers.add(Headers::ksContentType,mt.c_str());
            std::string sBody;
            if (bp::strutil::loadFromFile(path, sBody)) {
                response.body.assign(sBody);
            }
        } else if (boost::filesystem::is_directory(path)) {
            response.body.append("<html><head><title>");
            response.body.append("Contents of " + path.generic_string());
            response.body.append("</title></head><body>");
            response.body.append("<h2>Index of " + path.generic_string() + "</h2>");
            response.body.append("<hr><pre>\n"); 
            try {
                bfs::recursive_directory_iterator end;
                for (bfs::recursive_directory_iterator it(path); it != end; ++it) {
                    bfs::path pathToKid = it->path();
                    bfs::path relPath = relativeTo(pathToKid, path);
                    std::string relStr = relPath.generic_string();
                    std::string urlToKid = request.url.path() + relStr;
                    
                    unsigned int j;
                    
                    response.body.append("<a href=\"");
                    response.body.append(urlToKid);                
                    response.body.append("\">");
                    response.body.append(relStr);
                    response.body.append("</a>");
                    
                    for (j = relStr.length(); j < 30; j++) {
                        response.body.append(" ");
                    }
                    // files size
                    {
                        boost::uintmax_t sz = size(pathToKid);
                        std::stringstream ss;
                        ss << sz;
                        response.body.append(ss.str());
                        for (j = ss.str().length(); j < 20; j++) {
                            response.body.append(" ");
                        }
                    }
                    
                    // modtime
                    BPTime t;
                    try {
                        t.set(boost::filesystem::last_write_time(pathToKid));
                    } catch (const boost::filesystem::filesystem_error&) {
                        // empty
                    }
                    response.body.append(t.asString());
                    
                    // next line
                    response.body.append("\n");
                }
                
                
            } catch (const boost::filesystem::filesystem_error& ) {
                // empty
            }
            
            response.body.append("</pre><hr></body></html>");        
        } else {
            response.status.setCode(Status::NOT_FOUND);
            response.body.append("<html><head><title>");
            response.body.append("Not Found:" + path.generic_string());
            response.body.append("</title></head><body>");
            response.body.append("Cannot find file: " + path.generic_string());
            response.body.append("</body></html>");        
        }
        
        return true;
    }
};


class InfoHandler : public IHandler 
{
public:
    InfoHandler() { }
    ~InfoHandler() { }


    bool processRequest(const Request & request,
                        Response & response)
    {
        response.body.append("<html><head><title>");
        response.body.append("Basic request information");
        response.body.append("</title></head><body>");
        response.body.append("<h2> request properties & headers </h2>");        

        response.body.append("<ul>");        

        // Virtual headers
        response.body.append("<li> Method: ");                            
        response.body.append(request.method.toString());
        response.body.append("<li> Path: ");                            
        response.body.append(request.url.path());
        response.body.append("<li> Query String: ");                            
        response.body.append(request.url.query());

        for (Headers::const_iterator it = request.headers.begin();
             it != request.headers.end(); ++it) {
            std::stringstream ss;
            ss << "<li>" << it->first << ": " << it->second;
            response.body.append(ss.str());
        }

        response.body.append("</ul>");                

        response.body.append("</body></html>");        

        return true;
    }
};

int
main(int /*argc*/, char ** /*argv*/)
{
    InfoHandler ih;
    DirHandler dh;
    Server s;

    unsigned short int port = 0;
    if (!s.bind(port)) {
        std::cerr << "error binding ephemeral port\n" << std::endl;
        exit(1);
    }
    std::cout << "bound to localhost:" << port << std::endl;
    
    s.mount("/info", &ih);
    s.mount("*", &dh);
    s.start();
    
    sleep((unsigned int) -1);

    return 0;
}
