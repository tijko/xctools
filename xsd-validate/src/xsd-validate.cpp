/*
 * Copyright (c) 2012 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/EntityResolver.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>

#include <string>
#include <cstdio>

using namespace std;
using namespace xercesc;

static const char *schema_dir = 0;
static const char *ext_schema_loc = 0;
static const char *ext_schema_nons_loc = 0;

class EH: public ErrorHandler {
public:
    int status;
    EH() { status = 0; }
    void warning(const SAXParseException& e) { /*show("warning", e);*/ }
    void error(const SAXParseException& e) { status=1; show("error", e); }
    void fatalError(const SAXParseException& e) { status=1; show("fatal", e); }
    void resetErrors() { status = 0; }
    void show(const char *severity, const SAXParseException& e) {
	int line = e.getLineNumber(), col = e.getColumnNumber();
	char* id  = XMLString::transcode (e.getSystemId());
	char* msg = XMLString::transcode (e.getMessage());
	printf("%s:%d:%d %s: %s\n", id, line, col, severity, msg);
	XMLString::release(&id);
	XMLString::release(&msg);
    }
};

class ER: public EntityResolver {
public:
    virtual InputSource* resolveEntity( const XMLCh* const publicId, const XMLCh *const systemId) {
	char *id = XMLString::transcode(systemId);
	XMLCh *path = 0;
	InputSource *src = 0;
	if (0 == strncmp(id, "http://", 7)) {
	    string buf = id+7;
	    buf = string(schema_dir) + "/" + buf;
	    path = XMLString::transcode(buf.c_str());
	    src = new LocalFileInputSource(path);
	} else {
	    if (strlen(id) > 4) {
		if (0 == strcmp(id + strlen(id)-4, ".xsd")) {
		    int p = strlen(id);
		    while (--p >= 0) {
			if (id[p] == '/') {
			    break;
			}
		    }
		    if (p>=0) {
			string buf = id + p;
			buf = string(schema_dir) + buf;
			path = XMLString::transcode(buf.c_str());
			src = new LocalFileInputSource(path);
		    }
		}
	    }
	}

	//if (!src) {
	//    printf("no resolve %s\n", id);
	//}

	XMLString::release(&id);
	XMLString::release(&path);

	return src;
    };
    
};

/*
 * sample invocation
   xsd-validate /storage/schema "http://schemas.dmtf.org/ovf/envelope/1 dsp8023_2.0.0c.xsd" validate2.ovf
 */
int
main (int argc, char* argv[])
{
  if (argc != 4) {
      printf("usage: %s <schema mirror directory> <ext schema location> test.xml\n", argv[0]);
      return 1;
  }

  schema_dir = argv[1];
  ext_schema_loc = argv[2];
  const char *filename = argv[3];

  XMLPlatformUtils::Initialize();
  
  XercesDOMParser parser;
  parser.setValidationScheme(XercesDOMParser::Val_Always);
  parser.setExternalSchemaLocation(ext_schema_loc);
  parser.setDoNamespaces(true);
  parser.setDoSchema(true);
  parser.setLoadSchema(true);

  EH eh;
  parser.setErrorHandler(&eh);
  ER er;
  parser.setEntityResolver(&er);

  parser.parse(filename);
  return eh.status;
}

