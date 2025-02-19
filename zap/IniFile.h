//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// IniFile.cpp:  Implementation of the CIniFile class.
// Written by:   Adam Clauss
// Email: cabadam@tamu.edu
// You may use this class/code as you wish in your programs.  Feel free to distribute it, and
// email suggested changes to me.
//
// Rewritten by: Shane Hill
// Date:         21/08/2001
// Email:        Shane.Hill@dsto.defence.gov.au
// Reason:       Remove dependancy on MFC. Code should compile on any
//               platform. Tested on Windows/Linux/Irix
//////////////////////////////////////////////////////////////////////

#ifndef _CIniFile_H_
#define _CIniFile_H_

#include <tnlVector.h>     // For Vector type

#include <string>

#define MAX_KEYNAME    128
#define MAX_VALUENAME  128


using namespace std;
using namespace TNL;

namespace Zap
{

class CIniFile
{
private:
   bool   caseInsensitive;
   string path;

   struct section
   {
      Vector<string> keys;
      Vector<string> values;
      Vector<string> comments;
   };

   Vector<section> sections;         // This is our main Vector that holds all of our INI data
   Vector<string>  sectionNames;     // Holds just the section names
   Vector<string>  headerComments;   // Holds the header comments that aren't part of any section
   bool checkCase(const string &s1, const string &s2) const;

   string section;

public:
   static const S32 noID = -1;

   S32 lineCount;

   explicit CIniFile( const string &iniPath = "");   // Constructor
   virtual ~CIniFile();                              // Destructor

   void processLine(string line);     // Process a line of an input file (CE)


   // Sets whether or not keynames and valuenames should be case sensitive.
   // The default is case insensitive.
   void CaseSensitive();
   void CaseInsensitive();

   // Sets path of ini file to read and write from
   string getPath() const;
   void SetPath(const string &newPath);

   // Reads ini file specified using path.
   // Returns true if successful, false otherwise.
   void readFile();

   // Writes data stored in class to ini file.
   bool writeFile();

   // Deletes all stored ini data.
   void Erase();
   void Clear();
   void Reset();

   // Returns index of specified key, or noID if not found
   S32 findSection(const string &sectionName) const;

   // Returns index of specified key, in the specified section, or noID if not found
   S32 findKey(S32 const sectionID, const string &keyName) const;

   // Returns number of sections currently in the ini
   S32 getNumSections() const;

   // Verify if the key exists
   bool hasKey(const string &section, const string &key) const;

   // Add a section name
   S32 addSection( const string &section);

   // Returns section names by index
   string sectionName( S32 const sectionId) const;
   string getSectionName( S32 const sectionId) const;

   // Returns number of values stored for specified section.
   S32 GetNumEntries(S32 const sectionId) const;
   S32 GetNumEntries(const string &section) const;

   // Returns value name by index for a given keyname or sectionId.
   string valueName(S32 const sectionID, S32 const keyID) const;
   string GetValueName(S32 const sectionID, S32 const keyID) const;
   string valueName(const string &section, S32 const keyID) const;
   string GetValueName(const string &section, S32 const keyID) const;

   // Gets value of [keyname] valuename =.
   // Overloaded to return string, int, and double.
   // Returns defValue if key/value not found.
   string getValue(S32 const sectionId, S32 const keyID, const string &defValue = "") const;
   string getValue(S32 const sectionId, const string &keyName, const string &defValue = "") const;
   string getValue(const string &section, const string &keyName, const string &defValue = "") const;

   // Load up valueList with all values from the section
   void getAllValues(const string &section, Vector<string> &valueList) const;
   void getAllValues(S32 const sectionId, Vector<string> &valueList) const;

   // Load up keyList with all keys from the section
   void getAllKeys(S32 const sectionId, Vector<string> &keyList) const;
   void getAllKeys(const string &section, Vector<string> &keyList) const;

   S32  getValueI(const string &section,  const string &key, S32 const defValue = 0) const;
   bool GetValueB(const string &section,  const string &key, bool const defValue = false) const;
   F32  getValueF(const string &section, const string &key, F32 const defValue = 0.0f) const;
   bool getValueYN(const string &section, const string &key, bool defValue) const;
   bool getValueYN(S32 const sectionId,   const string &keyName, const bool &defValue) const;


   // This is a variable length formatted GetValue routine. All these voids
   // are required because there is no vsscanf() like there is a vsprintf().
   // Only a maximum of 8 variable can be read.
   /*  S32 GetValueV( const string keyname, const string valuename, char *format,
            void *v1 = 0, void *v2 = 0, void *v3 = 0, void *v4 = 0,
            void *v5 = 0, void *v6 = 0, void *v7 = 0, void *v8 = 0,
            void *v9 = 0, void *v10 = 0, void *v11 = 0, void *v12 = 0,
            void *v13 = 0, void *v14 = 0, void *v15 = 0, void *v16 = 0);
    */
   // Sets value of [keyname] valuename =.
   // Specify the optional paramter as false (0) if you do not want it to create
   // the key if it doesn't exist. Returns true if data entered, false otherwise.
   // Overloaded to accept string, int, and double.
   bool setValue(const string &section, const string &key, const string &value, bool const create = true);
   bool setAllValues(const string &section, const string &prefix, const Vector<string> &values);
   bool setValueI(const string &section, const string &key, int const value, bool const create = true);
   bool SetValueB(const string &section, const string &key, bool const value, bool const create = true);
   bool setValueYN(const string section, const string key, bool const value, bool const create = true);
   bool setValueF(const string &section, const string &key, F32 const value, bool const create = true);
   //bool SetValueV(const string &section, const string &key, char *format, ...);
   bool setValue(S32 const sectionId, S32 const valueID, const string &value);

   // Deletes specified value.
   // Returns true if value existed and deleted, false otherwise.
   bool deleteKey(const string &section, const string &key);

   // Deletes specified key and all values contained within.
   // Returns true if key existed and deleted, false otherwise.
   bool deleteSection(const string &section);

   // Header comment functions.
   // Header comments are those comments before the first key.
   //
   // Get number of header comments.
   std::size_t NumHeaderComments();
   // Add a header comment.
   void     headerComment(const string &comment);
   // Return a header comment.
   string   headerComment(S32 const commentID) const;
   // Delete a header comment.
   bool     deleteHeaderComment(S32 commentID);
   // Delete all header comments.
   void     deleteHeaderComments();

   // Key comment functions.
   // Key comments are those comments within a key. Any comments
   // defined within value names will be added to this list. Therefore,
   // these comments will be moved to the top of the key definition when
   // the CIniFile::WriteFile() is called.
   //
   // Number of key comments
   S32 numSectionComments(S32 const sectionId) const;
   S32 numSectionComments(const string keyname) const;
   // Add a key comment
   bool     sectionComment(S32 sectionId, const string &comment);
   bool     sectionComment(const string &keyname, const string &comment, bool const create = true);
   // Return a key comment
   string   sectionComment(S32 const sectionId, S32 const commentID) const;
   string   sectionComment(const string &keyname, S32 const commentID) const;
   // Delete a key comment
   bool     deleteSectionComment(S32 const sectionId, S32 const commentID);
   bool     deleteSectionComment(const string keyname, S32 const commentID);
   // Delete all comments for a key
   bool     deleteSectionComments(S32 const sectionId);
   bool     deleteSectionComments(const string &keyname);

   bool     deleteAllSectionComments();
};

};
#endif

