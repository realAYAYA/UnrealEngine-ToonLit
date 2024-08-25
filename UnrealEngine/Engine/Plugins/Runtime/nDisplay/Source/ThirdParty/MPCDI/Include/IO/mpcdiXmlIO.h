/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-02-08 11:39:41 -0500 (Wed, 08 Feb 2012) $
  Version:   $Revision: 18341 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */
// .NAME XmlIO - An XML IO helper class
// .SECTION Description
//

#pragma once
#include "tinyxml2.h"
#include "mpcdiHeader.h"
#include <string>

#include "mpcdiErrors.h"
#include "mpcdiXMLStrings.h"
#include "mpcdiUtils.h"

namespace mpcdi 
{
  class XmlIO 
  {
    public:
      // Description:
      // public constructor/destructor
      XmlIO() { }
      ~XmlIO() { m_MPCDxml.Clear(); }

      // Description:
      // static helpers to get an attribute value
      static MPCDI_Error QueryIntAttribute(std::string attrName,tinyxml2::XMLElement *xmlElement, int &value);
      static MPCDI_Error QueryFloatAttribute(std::string attrName,tinyxml2::XMLElement *xmlElement, float &value);
      static MPCDI_Error QueryStringAttribute(std::string attrName,tinyxml2::XMLElement *xmlElement, std::string &value);

      // Description:
      // static helpers to get value from node text <node>thistext</node>
      static MPCDI_Error QueryDoubleText(std::string textName, tinyxml2::XMLElement *xmlParent, double &value);
      static MPCDI_Error QueryIntText(std::string textName, tinyxml2::XMLElement *xmlParent, int &value);
      static MPCDI_Error QueryStringText(std::string textName, tinyxml2::XMLElement *xmlParent, std::string &value);
      
      // Description:
      // add a new element
      tinyxml2::XMLElement *AddNewXMLElement(std::string nodeName, tinyxml2::XMLElement *parent);

      // Description:
      // get a existing element
      tinyxml2::XMLElement *GetXMLElement(std::string nodeName, bool createIfNotFound=false);

      // Description:
      // add a new node with text <nodeName>value</nodeName>
      template <class T> tinyxml2::XMLText *AddNewXMLText(std::string nodeName, T value, tinyxml2::XMLElement* parent);

      // Description:
      // access method to get the underlying document
      tinyxml2::XMLDocument &GetDoc() { return m_MPCDxml; }

      // Description:
      // member variables
      tinyxml2::XMLDocument m_MPCDxml;
  };

  template <class T> 
  tinyxml2::XMLText *XmlIO::AddNewXMLText(std::string nodeName, T value, tinyxml2::XMLElement* parent)
  {
    tinyxml2::XMLElement *textNode = AddNewXMLElement(nodeName.c_str(),parent);
    std::string svalue = NumberToString<T>(value);
    tinyxml2::XMLText *xmlText = m_MPCDxml.NewText(svalue.c_str());
    return (tinyxml2::XMLText *)textNode->LinkEndChild(xmlText);
  }
}// namespace mpcdi
