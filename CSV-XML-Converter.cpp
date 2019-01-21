// CSV-XML-Converter.cpp
/*
CSV-XML-Converter Version 0.8 from 21.01.2019

Command line tool for conversion of data files from CSV to XML and from XML to CSV via mapping definition

Source code is licenced under the MIT licence on GitHub: https://github.com/peterraf/csv-xml-converter

Copyright (c) 2019 by Peter Raffelsberger (peter@raffelsberger.net)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>  // for Windows
#include <windows.h>  // for Windows
//#include <dirent.h>  // for Linux
//#include <regex.h>
//#include "stdafx.h"

#include "libxml/tree.h"
#include "libxml/parser.h"
//#include "tree.h"
//#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_NAME_SIZE  256
#define MAX_FILE_NAME_LEN  (MAX_FILE_NAME_SIZE - 1)

#define MAX_PATH_SIZE  240
#define MAX_PATH_LEN  (MAX_PATH_SIZE - 1)

#define MAX_HEADER_SIZE  65536
#define MAX_HEADER_LEN  (MAX_HEADER_SIZE - 1)

#define MAX_LINE_SIZE  262144
#define MAX_LINE_LEN  (MAX_LINE_SIZE - 1)

#define MAX_COUNTER_NAME_SIZE  128
#define MAX_COUNTER_NAME_LEN  (MAX_COUNTER_NAME_SIZE - 1)

#define MAX_ERROR_MESSAGE_SIZE  2048
#define MAX_ERROR_MESSAGE_LEN  (MAX_ERROR_MESSAGE_SIZE - 1)

#define MAX_MAPPING_COLUMNS  25
#define MAX_FIELD_MAPPINGS  500
#define MAX_CSV_COLUMNS  500
#define MAX_LOOPS 100

#define MAX_VALUE_SIZE  16384
#define MAX_VALUE_LEN  (MAX_VALUE_SIZE - 1)

#define MAX_CONDITION_SIZE  1024
#define MAX_CONDITION_LEN  (MAX_CONDITION_SIZE - 1)

//#define MAX_FORMAT_SIZE  1024
//#define MAX_FORMAT_LEN  (MAX_FORMAT_SIZE - 1)

#define MAX_TIMESTAMP_FORMAT_SIZE  32
#define MAX_TIMESTAMP_FORMAT_LEN  (MAX_TIMESTAMP_FORMAT_SIZE - 1)

#define MAX_FORMAT_ERROR_SIZE  128
#define MAX_FORMAT_ERROR_LEN  (MAX_FORMAT_ERROR_SIZE - 1)

#define MAX_UNIQUE_DOCUMENT_ID_SIZE  129
#define MAX_UNIQUE_DOCUMENT_ID_LEN  (MAX_UNIQUE_DOCUMENT_ID_SIZE - 1)

#define MAX_NODE_NAME_SIZE  256
#define MAX_NODE_NAME_LEN  (MAX_NODE_NAME_SIZE - 1)

#define MAX_XPATH_SIZE  1024
#define MAX_XPATH_LEN  (MAX_XPATH_SIZE - 1)

#define MAX_NODE_INDEX_SIZE  10
#define MAX_NODE_INDEX_LEN  (MAX_NODE_INDEX_SIZE - 1)

//#define MAX_NODE_INDEX  10

// allow usage of original stdio functions like strcpy, memcpy, ...
//#pragma warning(suppress : 4996)

// type definitions

typedef char *pchar;
typedef char const *cpchar;

typedef enum { CSV2XML, XML2CSV } ConversionDirection;

typedef struct {
  char cOperation;  // Fix, Var, Map, Line/Loop
  const char *szContent;  // Constant, Variable, Column Name or XPath
  const char *szAttribute;
  const char *szShortName;
  bool bMandatory;
  bool bAddEmptyNodes;
  char cType;  // Text, Integer, Number, Date, Boolean
  int nMinLen;
  int nMaxLen;
  const char *szFormat;
  const char *szTransform;
  const char *szDefault;
  const char *szCondition;
  const char *szValue;
} FieldDefinition;

typedef FieldDefinition const *CPFieldDefinition;

typedef struct {
  FieldDefinition csv;
  FieldDefinition xml;
  int nCsvIndex;
  int nSourceMapIndex;
  int nLoopIndex;
  int nRefUniqueLoopIndex;
} FieldMapping;

typedef FieldMapping *PFieldMapping;
typedef FieldMapping const *CPFieldMapping;

const char szEmptyString[] = "";
const char szCsvDefaultDateFormat[] = "DD.MM.YYYY";
const char szXmlShortDateFormat[] = "YYYY-MM-DD";
const char szDefaultBooleanFormat[] = ",true,false,";  // internal version of regular expression "(true,false)"

const char szDigits[] = "0123456789";
const char szSmallLetters[] = "abcdefghijklmnopqrstuvwxyz";
const char szBigLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

ConversionDirection convDir;
int nFieldMappings = 0;
FieldMapping aFieldMapping[MAX_FIELD_MAPPINGS];
int nFieldMappingBufferSize = 0;
char *pFieldMappingsBuffer = NULL;

int nCsvDataBufferSize = 0;
char *pCsvDataBuffer = NULL;
char cColumnDelimiter = ';';  // default column delimiter is semicolon
char cDecimalPoint = '.';  // default decimal point is point
char szIgnoreChars[8] = " \t";  // default space-like characters to be ignored during parsing a csv line
int nCsvDataColumns = 0;
int nCsvDataLines = 0;
int nCsvDataFieldsBufferSize = 0;
pchar *aCsvDataFields = NULL;
int nRealCsvDataLines = 0;
int nCurrentCsvLine = 0;
int nLoops = 0;
int anLoopIndex[MAX_LOOPS];
int anLoopSize[MAX_LOOPS];
PFieldMapping apLoopFieldMapping[MAX_LOOPS];
char szCsvHeader[MAX_HEADER_SIZE] = "";
char szLineBuffer[MAX_LINE_SIZE];
char szFieldValueBuffer[MAX_LINE_SIZE];
char szUniqueDocumentID[MAX_UNIQUE_DOCUMENT_ID_SIZE];
char szCounterPath[MAX_FILE_NAME_SIZE] = "";
char szLastError[MAX_ERROR_MESSAGE_SIZE];
char szLastFieldMappingError[MAX_ERROR_MESSAGE_SIZE];
char szErrorFileName[MAX_FILE_NAME_SIZE];
FILE *pErrorFile = NULL;
int nErrors = 0;
bool abColumnQuoted[MAX_CSV_COLUMNS];
bool abColumnError[MAX_CSV_COLUMNS];
xmlDocPtr pXmlDoc = NULL;
bool bTrace = false; //true; //false;
char cPathSeparator = '\\';  // change to '/' for linux

//--------------------------------------------------------------------------------------------------------

bool IsEmptyString(cpchar pszString)
{
  return (!pszString || !*pszString);
}

//--------------------------------------------------------------------------------------------------------

void mystrncpy(char *dest, const char *src, size_t maxSize)
{
  // similar to strncpy, but ensuring null terminating character at the end

  if (!dest || !src || !maxSize)
    return;

  if (strlen(src) >= maxSize) {
    size_t len = maxSize - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
  }
  else
    strcpy(dest, src);
}

//--------------------------------------------------------------------------------------------------------

char GetLastChar(cpchar pString)
{
  char cResult = '\0';
	size_t len = strlen(pString);
	if (len > 0)
	  cResult = pString[len-1];
	return cResult;
}

//--------------------------------------------------------------------------------------------------------

void CopyStringTill(char *pszDestination, cpchar pszSource, char cDelimiter)
{
  char *pszDest = pszDestination;
  cpchar pszSrc = pszSource;

  while (*pszSrc && *pszSrc != cDelimiter)
    *pszDest++ = *pszSrc++;

  *pszDest = '\0';
}

//--------------------------------------------------------------------------------------------------------

void RemoveEndChars(char *pszString, cpchar pszIgnoreChars)
{
  char *pLastChar = pszString + strlen(pszString) - 1;

  while (pLastChar >= pszString && strchr(pszIgnoreChars, *pLastChar))
    *pLastChar-- = '\0';
}

//--------------------------------------------------------------------------------------------------------

int SplitString(char *pszString, char cSeparator, pchar *aszValue, int nMaxValues, cpchar pszIgnoreChars)
{
  int nValues = 0;
  char *pPos = pszString;
  char *pSep = strchr(pszString, cSeparator);
  char *pEnd = NULL;

  while (*pPos && strchr(pszIgnoreChars, *pPos))
    pPos++;

  if (*pPos) {
    while (pSep) {
      *pSep = '\0';
      RemoveEndChars(pPos, pszIgnoreChars);
      aszValue[nValues++] = pPos;
      pPos = pSep + 1;
      while (*pPos && strchr(pszIgnoreChars, *pPos))
        pPos++;
      pSep = strchr(pPos, cSeparator);
    }

    aszValue[nValues++] = pPos;
    RemoveEndChars(pPos, pszIgnoreChars);
  }

  return nValues;
}

//--------------------------------------------------------------------------------------------------------

void ExtractPath(char *pszPath, cpchar pszFileName)
{
  cpchar pBackSlash = strchr(pszFileName, '\\');
  cpchar pSlash = strchr(pszFileName, '/');
  cpchar pPos = strchr(pszFileName, ':');

  if (pBackSlash) {
    pPos = pBackSlash;
    pBackSlash = strchr(pPos+1, '\\');
    // search last back-slash in file name
    while (pBackSlash) {
      pPos = pBackSlash;
      pBackSlash = strchr(pPos+1, '\\');
    }
  }
  else
    if (pSlash) {
      pPos = pSlash;
      pSlash = strchr(pPos+1, '/');
      // search last slash in file name
      while (pSlash) {
        pPos = pSlash;
        pSlash = strchr(pPos+1, '/');
      }
    }

  if (pPos) {
    int len = pPos - pszFileName + 1;
    memcpy(pszPath, pszFileName, len);
    pszPath[len] = '\0';
  }
  else
    *pszPath = '\0';
}

//--------------------------------------------------------------------------------------------------------

void ReplaceExtension(char *szFileName, cpchar szExtension)
{
  int nLen = strlen(szFileName);
  int i = nLen - 1;
  char ch;

  while (i > 0) {
    ch = szFileName[i];
    if (ch == '.') {
      strcpy(szFileName + i + 1, szExtension);
      return;
    }
    if (strchr("/\\:", ch))
      exit;
    i--;
  }

  strcat(szFileName, ".");
  strcat(szFileName, szExtension);
}

//--------------------------------------------------------------------------------------------------------

bool FileExists(cpchar szFileName)
{
  FILE *pFile = NULL;

  errno_t error_code = fopen_s(&pFile, szFileName, "r");

  if (pFile) {
    fclose(pFile);
    return true;
  }

  return false;
}

//--------------------------------------------------------------------------------------------------------

bool FindUniqueFileName(char *szFileName)
{
  char szUniqueFileName[MAX_FILE_NAME_SIZE];
  int nLen = strlen(szFileName);
  int i = nLen - 1;
  int nPos = nLen;  // default for index is end of file name
  char ch;

  if (!FileExists(szFileName))
    return true;  // file name already unique (not existing at the moment)

  if (nLen + 7 > MAX_FILE_NAME_LEN)
    return false;  // source file name too long (cannot insert seven additional characters)

  // search start of file extension (dot character)
  while (i > 0) {
    ch = szFileName[i];
    if (ch == '.') {
      nPos = i;
      exit;
    }
    if (strchr("/\\:", ch))
      exit;
    i--;
  }

  strcpy(szUniqueFileName, szFileName);

  for (i = 1; i <= 999999; i++) {
    // insert counter just before file extension
    sprintf(szUniqueFileName + nPos, "-%06d%s", i, szFileName + nPos);
    // is generated file name unique ?
    if (!FileExists(szUniqueFileName)) {
      // generated file name is unique (not existing at the moment)
      strcpy(szFileName, szUniqueFileName);
      return true;
    }
  }

  // no unique file name found
  return false;
}

//--------------------------------------------------------------------------------------------------------

void AddLog(cpchar szFileName, cpchar szLogType, cpchar szConversion, cpchar szLastError, int nErrors, int nCsvDataLines, cpchar szInputFileName, cpchar szMappingFileName, cpchar szTemplateFileName, 
            cpchar szOutputFileName, cpchar szProcessedFileName, cpchar szUniqueDocumentID, cpchar szErrorFileName)
{
  FILE *pFile = NULL;
  errno_t error_code;
  char szNow[20];

  error_code = fopen_s(&pFile, szFileName, "a");
  if (pFile) {
    time_t now = time(NULL);
    struct tm* tm_info;
    tm_info = localtime(&now);
    strftime(szNow, 20, "%Y-%m-%d %H:%M:%S", tm_info);  // e.g. "2018-12-08 12:30:00"

    fprintf(pFile, "%s;%s;%s;%s;%d;%d;%s;%s;%s;%s;%s;%s;%s\n", szNow, szLogType, szConversion, szLastError, nErrors, nCsvDataLines, szInputFileName, szMappingFileName, szTemplateFileName,
            szOutputFileName, szProcessedFileName, szUniqueDocumentID, szErrorFileName);

    fclose(pFile);
  }
  else
    printf("Cannot append to file %s (error %d)\n", szFileName, error_code);
}

//--------------------------------------------------------------------------------------------------------

int CheckLogFileHeader(cpchar szFileName)
{
  FILE *pFile = NULL;
  errno_t error_code;

  if (!FileExists(szFileName)) {
    error_code = fopen_s(&pFile, szFileName, "w");
    if (pFile) {
      fputs("TIMESTAMP;LOG_TYPE;CONVERSION;ERROR;CONTENT_ERRORS;CSV_DATA_RECORDS;INPUT_FILE_NAME;MAPPING_FILE_NAME;TEMPLATE_FILE_NAME;OUTPUT_FILE_NAME;PROCESSED_FILE_NAME;UNIQUE_DOCUMENT_ID;ERROR_FILE_NAME\n", pFile);
      fclose(pFile);
      return 0;
    }
    sprintf(szLastError, "Cannot create log file %s (error %d)", szFileName, error_code);
    puts(szLastError);
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------------

int LogXmlError(int nLine, int nColumnIndex, cpchar szColumnName, cpchar szXPath, cpchar szValue, cpchar szError)
{
  int nReturnCode = 0;
  errno_t error_code;

  if (nColumnIndex >= 0 && nColumnIndex < nCsvDataColumns && abColumnError[nColumnIndex])
    return 0;  // report only first error for each column

  nErrors++;

  if (convDir == CSV2XML && nColumnIndex >= 0 && nColumnIndex < nCsvDataColumns)
    abColumnError[nColumnIndex] = true;

  if (!pErrorFile) {
    // open file in write mode
    error_code = fopen_s(&pErrorFile, szErrorFileName, "w");
    if (!pErrorFile) {
      printf("Cannot open file '%s' (error code %d)\n", szErrorFileName, error_code);
      return -2;
    }

    // write header of error file
    nReturnCode = fputs("LINE;COLUMN_NR;COLUMN_NAME;XPATH;VALUE;ERROR\n", pErrorFile);
  }

  if (*szXPath)
    nReturnCode = fprintf(pErrorFile, "%d;%d;\"%s\";\"%s\";\"%s\";\"%s\"\n", nLine, nColumnIndex + 1, szColumnName, szXPath, szValue, szError);
  else
    nReturnCode = fprintf(pErrorFile, "%d;%d;\"%s\";;\"%s\";\"%s\"\n", nLine, nColumnIndex + 1, szColumnName, szValue, szError);

  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int LogCsvError(int nLine, int nColumnIndex, cpchar szColumnName, cpchar szValue, cpchar szError)
{
  return LogXmlError(nLine, nColumnIndex, szColumnName, szEmptyString, szValue, szError);
}

//--------------------------------------------------------------------------------------------------------

int MyLoadFile(cpchar pszFileName, char *pBuffer, int nMaxSize)
{
  int nBytesRead = 0;
  FILE *pFile = NULL;
  errno_t error_code;

  error_code = fopen_s(&pFile, pszFileName, "r");
  if (!pFile) {
    sprintf(szLastError, "Cannot open file '%s' (error code %d)", pszFileName, error_code);
    puts(szLastError);
    return -1;
  }

  // clear read buffer
  memset(pBuffer, 0, nMaxSize);

  // read content of file
  nBytesRead = fread(pBuffer, nMaxSize-1, 1, pFile);

  // close file
  fclose(pFile);

  return nBytesRead;
}

//--------------------------------------------------------------------------------------------------------

int MyWriteFile(cpchar pszFileName, cpchar pszBuffer)
{
  int nReturnCode = 0;
  FILE *pFile = NULL;
  errno_t error_code;

  error_code = fopen_s(&pFile, pszFileName, "w");
  if (!pFile) {
    sprintf(szLastError, "Cannot open file '%s' (error code %d)", pszFileName, error_code);
    puts(szLastError);
    return -1;
  }

  // write content of file
  nReturnCode = fputs(pszBuffer, pFile);
  if (nReturnCode == EOF) {
    sprintf(szLastError, "Cannot open file '%s' (error code %d)", pszFileName, nReturnCode);
    puts(szLastError);
    return -2;
  }

  // close file
  nReturnCode = fclose(pFile);

  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

xmlNodePtr GetNode(xmlNodePtr pParentNode, const char *pXPath, bool bCreateIfNotFound = false, cpchar pszConditionAttributeName = NULL, cpchar pszConditionAttributeValue = NULL)
{
  char szNodeName[MAX_NODE_NAME_SIZE];
  cpchar pChildXPath = NULL;
	cpchar pszAttributeValue = NULL;
  cpchar pSlash = NULL;
  char *pPos = NULL;
  xmlNodePtr pChildNode = NULL;
  int nRequestedNodeIndex = 1;
  int nFoundNodeIndex = 0;

  // Extract node name from xpath (first position of slash)
  pSlash = strchr(pXPath, '/');
  if (pSlash == NULL)
    strcpy_s(szNodeName, MAX_NODE_NAME_SIZE, pXPath);
  else {
    pChildXPath = pSlash + 1;
    if (!*pChildXPath)
      pChildXPath = NULL;
    int len = min(pSlash - pXPath, MAX_NODE_NAME_SIZE);
    mystrncpy(szNodeName, pXPath, len + 1);
    //szNodeName[len] = '\0';
  }

  // Extract node index (if existing)
  pPos = strchr(szNodeName, '[');
  if (pPos != NULL) {
    *pPos++ = '\0';
    nRequestedNodeIndex = atoi(pPos);
    if (nRequestedNodeIndex < 1)
      nRequestedNodeIndex = 1;
    //if (nRequestedNodeIndex > MAX_NODE_INDEX)
    //  nRequestedNodeIndex = MAX_NODE_INDEX;
  }

  // search for <szNodeName> (with requested node index) within children of pParentNode
  pChildNode = pParentNode->children;

	if (pszConditionAttributeName && *pszConditionAttributeName && pszConditionAttributeValue && pChildXPath == NULL) {
		// search for right node with node name and condition for attribute value
		while (pChildNode != NULL) {
			if (strcmp((cpchar)pChildNode->name, szNodeName) == 0) {
				nFoundNodeIndex++;
				// xmlChar *xmlGetProp (const xmlNode *node, const xmlChar *name)
				pszAttributeValue = (cpchar)xmlGetProp(pChildNode, (const xmlChar *)pszConditionAttributeName);

				if (pszAttributeValue && strcmp(pszAttributeValue, pszConditionAttributeValue) == 0) {
					// node with matching attribute value found
					return pChildNode;
				}
			}
			pChildNode = pChildNode->next;
		}
	}
	else {
		// search for right node with node name and node index
		while (pChildNode != NULL) {
			if (strcmp((const char*)pChildNode->name, szNodeName) == 0) {
				nFoundNodeIndex++;
				if (nFoundNodeIndex == nRequestedNodeIndex) {
					// <szNodeName> with requested node index found
					if (pChildXPath)
						return GetNode(pChildNode, pChildXPath, bCreateIfNotFound, pszConditionAttributeName, pszConditionAttributeValue);
					else
						return pChildNode;
				}
			}
			pChildNode = pChildNode->next;
		}
	}

  // <szNodeName> with requested node index or attribute value not found
  if (nFoundNodeIndex < nRequestedNodeIndex && !bCreateIfNotFound)
    return NULL;

	if (pszConditionAttributeName && *pszConditionAttributeName && pszConditionAttributeValue && pChildXPath == NULL) {
		// create <szNodeName> with requested attribute value
		pChildNode = xmlNewChild(pParentNode, NULL, (const xmlChar *)szNodeName, NULL);
		// xmlAttrPtr	xmlSetProp(xmlNodePtr node, const xmlChar *name, const xmlChar *value)
		xmlSetProp(pChildNode, (const xmlChar *)pszConditionAttributeName, (const xmlChar *)pszConditionAttributeValue);
	}
	else {
		// create <szNodeName> with requested node index
		while (nFoundNodeIndex < nRequestedNodeIndex) {
			pChildNode = xmlNewChild(pParentNode, NULL, (const xmlChar *)szNodeName, NULL);
			nFoundNodeIndex++;
		}
	}

  // end of XPath reached ?
  if (pChildXPath)
    return GetNode(pChildNode, pChildXPath, bCreateIfNotFound, pszConditionAttributeName, pszConditionAttributeValue);
  else
    return pChildNode;
}

//--------------------------------------------------------------------------------------------------------

xmlNodePtr GetCreateNode(xmlNodePtr pParentNode, const char *pXPath, cpchar pszConditionAttributeName = NULL, cpchar pszConditionAttributeValue = NULL)
{
  return GetNode(pParentNode, pXPath, true, pszConditionAttributeName, pszConditionAttributeValue);
}

//--------------------------------------------------------------------------------------------------------

int GetNodeCount(xmlNodePtr pParentNode, const char *pXPath)
{
  char szNodeName[MAX_NODE_NAME_SIZE];
  const char *pChildXPath = NULL;
  const char *pSlash = NULL;
  char *pPos = NULL;
  xmlNodePtr pChildNode = NULL;
  int nRequestedNodeIndex = 1;
  int nFoundNodeIndex = 0;
  int nCount = 0;

  // Extract node name from xpath (first position of slash)
  pSlash = strchr(pXPath, '/');
  if (pSlash == NULL)
    strcpy_s(szNodeName, MAX_NODE_NAME_SIZE, pXPath);
  else {
    pChildXPath = pSlash + 1;
    if (!*pChildXPath)
      pChildXPath = NULL;
    int len = min(pSlash - pXPath, MAX_NODE_NAME_SIZE);
    mystrncpy(szNodeName, pXPath, len + 1);
    //szNodeName[len] = '\0';
  }

  // Extract node index (if existing)
  pPos = strchr(szNodeName, '[');
  if (pPos != NULL) {
    *pPos++ = '\0';
    nRequestedNodeIndex = atoi(pPos);
    if (nRequestedNodeIndex < 1)
      nRequestedNodeIndex = 1;
    //if (nRequestedNodeIndex > MAX_NODE_INDEX)
    //  nRequestedNodeIndex = MAX_NODE_INDEX;
  }
  // end of xpath reached ?
  if (!pChildXPath)
    nRequestedNodeIndex = -1;

  // search for <szNodeName> within children of pParentNode
  pChildNode = pParentNode->children;

  while (pChildNode != NULL) {
    if (strcmp((const char*)pChildNode->name, szNodeName) == 0) {
      if (pChildXPath) {
        nFoundNodeIndex++;
        if (nFoundNodeIndex == nRequestedNodeIndex)
          // <szNodeName> with requested node index found
          return GetNodeCount(pChildNode, pChildXPath);
      }
      else
        nCount++;
    }
    pChildNode = pChildNode->next;
  }

  return nCount;
}

//--------------------------------------------------------------------------------------------------------

int GetNodeTextValue(xmlNodePtr pParentNode, cpchar pXPath, cpchar *ppszValue, cpchar pszConditionAttributeName = NULL, cpchar pszConditionAttributeValue = NULL)
{
  int nError = 0;

  xmlNodePtr pNode = GetNode(pParentNode, pXPath, false, pszConditionAttributeName, pszConditionAttributeValue);

  if (pNode)
    // xmlChar *xmlNodeGetContent	(const xmlNode * cur)
    *ppszValue = (cpchar)xmlNodeGetContent(pNode);
  else
    nError = 1;

  return nError;
}

//--------------------------------------------------------------------------------------------------------

xmlNodePtr SetNodeValue(xmlDocPtr pDoc, xmlNodePtr pParentNode, const char * pXPath, const char *pValue, FieldMapping const *pFieldMapping, cpchar pszConditionAttributeName = NULL, cpchar pszConditionAttributeValue = NULL)
{
  xmlNodePtr pRealParentNode = NULL;
  xmlNodePtr pNode = NULL;
  /*
  documentation: http://xmlsoft.org/html/libxml-tree.html
	// xmlNewProp() creates attributes, which is "attached" to an node. It returns xmlAttrPtr, which isn't used here.
	node = xmlNewChild(root_node, NULL, BAD_CAST "node3", BAD_CAST "this node has attributes");
	xmlNewProp(node, BAD_CAST "attribute", BAD_CAST "yes");
  */

  if (*pValue || pFieldMapping->xml.bAddEmptyNodes)
  {
    if (pParentNode)
      pRealParentNode = pParentNode;
    else
      pParentNode = xmlDocGetRootElement(pDoc);

    xmlNodePtr pNode = GetCreateNode(pParentNode, pXPath, pszConditionAttributeName, pszConditionAttributeValue);

    if (strlen(pFieldMapping->xml.szAttribute) == 0)
      xmlNodeSetContent(pNode, xmlEncodeSpecialChars(pDoc, (const xmlChar *)pValue));
      // xmlChar *xmlNodeGetContent	(const xmlNode * cur)
    else {
      // xmlChar *xmlGetProp (const xmlNode *node, const xmlChar *name)
      // xmlAttrPtr	xmlSetProp(xmlNodePtr node, const xmlChar *name, const xmlChar *value)
      xmlSetProp(pNode, (const xmlChar *)pFieldMapping->xml.szAttribute, xmlEncodeSpecialChars(pDoc, (const xmlChar *)pValue));
      //xmlNewProp(pNode, (const xmlChar *)pFieldMapping->xml.szAttribute, xmlEncodeSpecialChars(pDoc, (const xmlChar *)pValue));
    }
  }

  return pNode;
}

//--------------------------------------------------------------------------------------------------------

#define POSSIBLE_COLUMN_DELIMITERS  11
const char szPossibleColumnDelimiters[POSSIBLE_COLUMN_DELIMITERS+1] = ",;:\t|/^!%$#";

char GetColumnDelimiter(cpchar szLine)
{
  int i, nResultIndex, nResultCount, anDelimiters[POSSIBLE_COLUMN_DELIMITERS];
  cpchar pPos, pTemp;

  // initialize occurrence counts of possible column delimiters
  for (i = 0; i < POSSIBLE_COLUMN_DELIMITERS; i++)
    anDelimiters[i] = 0;

  // count occurrences of possible column delimiters
  for (cpchar pPos = szLine; *pPos; pPos++) {
    pTemp = strchr(szPossibleColumnDelimiters, *pPos);
    if (pTemp) {
      i = pTemp - szPossibleColumnDelimiters;
      if (i >= 0 && i < POSSIBLE_COLUMN_DELIMITERS)
        anDelimiters[i]++;
    }
  }

  // find column delimiter with maximum occurence count
  nResultIndex = 0;
  nResultCount = anDelimiters[nResultIndex];

  for (i = 1; i < POSSIBLE_COLUMN_DELIMITERS; i++)
    if (anDelimiters[i] > nResultCount) {
      nResultIndex = i;
      nResultCount = anDelimiters[nResultIndex];
    }

  return szPossibleColumnDelimiters[nResultIndex];
}

//--------------------------------------------------------------------------------------------------------

void CopyIgnoreString(pchar pszDest, cpchar pszSource, cpchar pszIgnoreChars)
{
  // copy source string to destination buffer skipping specified list of characters
  cpchar pSource = pszSource;
  pchar pDest = pszDest;

  while (*pSource) {
    if (!strchr(pszIgnoreChars, *pSource))
      *pDest = *pSource;
    pSource++;
    pDest++;
  }

  *pDest = '\0';
}

//--------------------------------------------------------------------------------------------------------

bool ValidChars(cpchar pszString, cpchar pszValidChars)
{
  for (cpchar p = pszString; *p; p++)
    if (!strchr(pszValidChars, *p))
      return false;

  return true;
}

//--------------------------------------------------------------------------------------------------------

bool IsValidNumber(cpchar pszString, char cDecimalPoint)
{
  cpchar pszTest = pszString;
  char szValidChars[32];
  bool bResult;

  // empty string ?
  if (!*pszTest)
    return false;

  // number with plus or minus sign at the beginning ?
  if (strchr("+-", *pszTest))
    pszTest++;

  // check valid characters (digits plus decimal point)
  sprintf(szValidChars, "%s%c", szDigits, cDecimalPoint);
  bResult = ValidChars(pszTest, szValidChars);

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

bool CheckRegex(cpchar pszString, cpchar pszRegex)
{
  // Sample regular expressions: "[A-Z]{3}", "[0-9a-zA-Z]{18}[0-9]{2}"
  // Enumeration: "(AIF,UCITS)"
  cpchar pRegexPos = pszRegex;
  cpchar pElemEnd = NULL;
  char szValidChars[256] = "";
  char szSearch[MAX_VALUE_SIZE+2];
  bool bResult = true;

  if (*pszRegex == ',' /*was originally '(', but already changed during loading of mapping file*/) {
    // Definition of allowed strings (xml enumeration)
    if (strlen(pszString) > MAX_VALUE_SIZE)
      bResult = false;
    else {
      sprintf(szSearch, ",%s,", pszString);
      // Example: searching for ",UCITS," in ",AIF,UCITS,"
      bResult = (strstr(pszRegex, szSearch) != NULL);
    }
  }

  if (*pszRegex == '[') {
    // Definition of allowed pattern with allowed characters
    if (strncmp(pszRegex, "[0-9]", 5) == 0) strcpy(szValidChars, szDigits);
    if (strncmp(pszRegex, "[a-z]", 5) == 0) strcpy(szValidChars, szSmallLetters);
    if (strncmp(pszRegex, "[A-Z]", 5) == 0) strcpy(szValidChars, szBigLetters);
    if (strncmp(pszRegex, "[0-9A-Z]", 8) == 0) sprintf(szValidChars, "%s%s", szDigits, szBigLetters);
    if (strncmp(pszRegex, "[a-zA-Z]", 8) == 0) sprintf(szValidChars, "%s%s", szSmallLetters, szBigLetters);
    if (strncmp(pszRegex, "[0-9a-zA-Z]", 11) == 0) sprintf(szValidChars, "%s%s%s", szDigits, szSmallLetters, szBigLetters);

    if (*szValidChars)
      bResult = ValidChars(pszString, szValidChars);
  }

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

bool IsValidText(cpchar pszString, CPFieldMapping pFieldMapping)
{
  bool bResult = true;

  if (!*pszString && !pFieldMapping->csv.bMandatory)
    return true;  // empty string allowed for optional fields

  if (!*pszString && pFieldMapping->csv.bMandatory)
    return false;  // empty string not allowed for mandatory fields

  if (pFieldMapping->csv.nMaxLen > 0 && strlen(pszString) > pFieldMapping->csv.nMaxLen)
    return false;  // string is too long

  if (pFieldMapping->csv.nMinLen > 0 && strlen(pszString) < pFieldMapping->csv.nMinLen)
    return false;  // string is too short

  if (*pFieldMapping->csv.szFormat)
    bResult = CheckRegex(pszString, pFieldMapping->csv.szFormat);

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

int MapIntFormat(cpchar pszSourceValue, cpchar pszSourceFormat, pchar pszDestValue, cpchar pszDestFormat, int nMaxLen)
{
  int nErrorCode = 0;
  cpchar pSourceValue = pszSourceValue;
  pchar pDestValue = pszDestValue;

  if (strlen(pszSourceValue) > nMaxLen)
    return -1;  // destination value buffer is too small
 
  if (*pSourceValue == '+')
    pSourceValue++;  // remove plus sign at the start

  CopyIgnoreString(pszDestValue, pSourceValue, " ");

  if (*pDestValue == '-')
    pDestValue++;  // skip minus sign at the start

  if (strlen(pDestValue) > 15) {
    sprintf(szLastFieldMappingError, "Number too long (maximum digits %d)", 15);
    return 2;
  }
  if (!ValidChars(pDestValue, szDigits)) {
    strcpy(szLastFieldMappingError, "Invalid number (invalid characters found)");
    return 3;
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int MapNumberFormat(cpchar pszSourceValue, char cSourceDecimalPoint, cpchar pszSourceFormat, pchar pszDestValue, char cDestDecimalPoint, cpchar pszDestFormat, int nMaxLen)
{
  int nErrorCode = 0;
  char szIgnoreChars[3];
  cpchar pSourceValue = pszSourceValue;
  pchar pDestValue = pszDestValue;
  char *pPos = NULL;

  if (strlen(pszSourceValue) > nMaxLen)
    return -1;  // destination value buffer is too small
 
  if (*pSourceValue == '+')
    pSourceValue++;  // ignore plus sign at the start

  if (convDir == CSV2XML) {
    // copy source number to destination buffer without 1000-delimiters
    strcpy(szIgnoreChars, (cSourceDecimalPoint == '.') ? ", " : ". ");
    CopyIgnoreString(pszDestValue, pSourceValue, szIgnoreChars);
  }
  else
    strcpy(pszDestValue, pSourceValue);

  // check number format
  if (*pDestValue == '-')
    pDestValue++;  // skip minus sign at the start

  pPos = strchr(pszDestValue, cSourceDecimalPoint);
  if (pPos) {
    // check integer part of number (before comma/dot)
    *pPos = '\0';
    if (strlen(pDestValue) > 15) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d before decimal point)", 15);
      return 2;
    }
    if (!ValidChars(pDestValue, szDigits)) {
      sprintf(szLastFieldMappingError, "Invalid number (invalid characters found, decimal point is '%c')", cSourceDecimalPoint);
      return 3;
    }
    // change decimal character to destination format
    *pPos++ = cDestDecimalPoint;
    // check fraction part of number (behind comma/dot)
    pDestValue = pPos;
    if (strlen(pDestValue) > 15) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d after decimal point)", 15);
      return 2;
    }
    if (!ValidChars(pDestValue, szDigits)) {
      sprintf(szLastFieldMappingError, "Invalid number (invalid characters found, decimal point is '%c')", cSourceDecimalPoint);
      return 3;
    }
  }
  else {
    // check integer number
    if (strlen(pDestValue) > 15) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d)", 15);
      return 2;
    }
    if (!ValidChars(pDestValue, szDigits)) {
      sprintf(szLastFieldMappingError, "Invalid number (invalid characters found, decimal point is '%c')", cSourceDecimalPoint);
      return 3;
    }
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int MapDateFormat(cpchar pszSourceValue, cpchar pszSourceFormat, pchar pszDestValue, cpchar pszDestFormat)
{
  int nErrorCode = 0;
  int nDay = 1;
  int nMonth = 1;
  int nYear = 2000;
  cpchar pPos = NULL;
  char szDay[3];
  char szMonth[3];
  char szYear[5];

  if (strlen(pszSourceValue) != strlen(pszSourceFormat))
    return 1;

  // parse source date
  pPos = strstr(pszSourceFormat, "DD");
  if (pPos) {
    memcpy(szDay, pszSourceValue + (pPos - pszSourceFormat), 2);
    szDay[2] = '\0';
    nDay = atoi(szDay);
    if (nDay < 1) { nDay = 1; nErrorCode = 2; }
    if (nDay > 31) { nDay = 31; nErrorCode = 2; }
  }

  pPos = strstr(pszSourceFormat, "MM");
  if (pPos) {
    memcpy(szMonth, pszSourceValue + (pPos - pszSourceFormat), 2);
    szMonth[2] = '\0';
    nMonth = atoi(szMonth);
    if (nMonth < 1) { nMonth = 1; nErrorCode = 2; }
    if (nMonth > 12) { nMonth = 12; nErrorCode = 2; }
  }

  pPos = strstr(pszSourceFormat, "YYYY");
  if (pPos) {
    memcpy(szYear, pszSourceValue + (pPos - pszSourceFormat), 4);
    szYear[4] = '\0';
    nYear = atoi(szYear);
    if (nYear < 1900) { nYear = 1900; nErrorCode = 2; }
    if (nYear > 2150) { nYear = 2150; nErrorCode = 2; }
  }
  else {
    pPos = strstr(pszSourceFormat, "YY");
    if (pPos) {
      memcpy(szYear, pszSourceValue + (pPos - pszSourceFormat), 2);
      szYear[2] = '\0';
      nYear = 2000 + atoi(szYear);
    }
  }

  // check delimiters
  for (int i = strlen(pszSourceFormat) - 1; i >= 0; i--)
    if (!strchr("DMY", pszSourceFormat[i]) && pszSourceFormat[i] != pszSourceValue[i])
      nErrorCode = 2;  // Invalid delimiter found

  // build result date
  strcpy(pszDestValue, pszDestFormat);

  pPos = strstr(pszDestFormat, "DD");
  if (pPos) {
    sprintf(szDay, "%02d", nDay);
    memcpy(pszDestValue + (pPos - pszDestFormat), szDay, 2);
  }

  pPos = strstr(pszDestFormat, "MM");
  if (pPos) {
    sprintf(szMonth, "%02d", nMonth);
    memcpy(pszDestValue + (pPos - pszDestFormat), szMonth, 2);
  }

  pPos = strstr(pszDestFormat, "YYYY");
  if (pPos) {
    sprintf(szYear, "%04d", nYear);
    memcpy(pszDestValue + (pPos - pszDestFormat), szYear, 4);
  }
  else {
    pPos = strstr(pszDestFormat, "YY");
    if (pPos) {
      sprintf(szYear, "%04d", nYear);
      memcpy(pszDestValue + (pPos - pszDestFormat), szYear + 2, 2);
    }
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int MapTextFormat(cpchar pszSourceValue, CPFieldDefinition pSourceFieldDef, pchar pszDestValue, CPFieldDefinition pDestFieldDef, int nMaxLen)
{
  char szShortFormat[MAX_FORMAT_ERROR_SIZE+4];
  int nLen, nErrorCode = 0;

  *szLastFieldMappingError = '\0';

  //if (strcmp(pDestFieldDef->szContent, "CCY") == 0)
  //  nLen = 0;  // for debugging purposes only!

  if (!*pszSourceValue && !pSourceFieldDef->bMandatory)
    return 0;  // empty value is allowed for optional fields

  if (strlen(pszSourceValue) > nMaxLen)
    return -1;  // destination value buffer is too small

  if (strlen(pszSourceValue) < pSourceFieldDef->nMinLen) {
    sprintf(szLastFieldMappingError, "Content too short (minimum length %d)", pSourceFieldDef->nMinLen);
    return 1;
  }

  if (pSourceFieldDef->nMaxLen >= 0 && strlen(pszSourceValue) > pSourceFieldDef->nMaxLen) {
    sprintf(szLastFieldMappingError, "Content too long (maximum length %d)", pSourceFieldDef->nMaxLen);
    return 2;
  }

  if (!CheckRegex(pszSourceValue, pSourceFieldDef->szFormat)) {
    nLen = strlen(pSourceFieldDef->szFormat);
    if (nLen > MAX_FORMAT_ERROR_LEN) {
      nLen = MAX_FORMAT_ERROR_LEN;
      mystrncpy(szShortFormat, pSourceFieldDef->szFormat, nLen + 1);
      strcpy(szShortFormat + nLen, " ...");
    }
    else
      strcpy(szShortFormat, pSourceFieldDef->szFormat);

    if (*szShortFormat == ',') {
      *szShortFormat = '(';
      nLen = strlen(szShortFormat);
      if (nLen > 2 && szShortFormat[nLen-1] == ',')
        szShortFormat[nLen-1] = ')';
      sprintf(szLastFieldMappingError, "Value not found in list %s", szShortFormat);
    }
    else
      sprintf(szLastFieldMappingError, "Invalid characters found (does not match %s)", szShortFormat);

    return 3;
  }

  // source value is valid, so copy source value to destination buffer
  strcpy(pszDestValue, pszSourceValue);

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int MapBoolFormat(cpchar pszSourceValue, CPFieldDefinition pSourceFieldDef, pchar pszDestValue, CPFieldDefinition pDestFieldDef)
{
  cpchar pszSourceFormat = pSourceFieldDef->szFormat;
  cpchar pszDestFormat = pDestFieldDef->szFormat;
  cpchar pPos = NULL;
  char szShortFormat[MAX_FORMAT_ERROR_SIZE+4];

  if (!pszSourceFormat || !*pszSourceFormat)
    pszSourceFormat = szDefaultBooleanFormat;

  if (!pszDestFormat || !*pszDestFormat)
    pszDestFormat = szDefaultBooleanFormat;

  *szLastFieldMappingError = '\0';

  if (!*pszSourceValue && !pSourceFieldDef->bMandatory)
    return 0;  // empty value is allowed for optional fields

  if (!CheckRegex(pszSourceValue, pszSourceFormat)) {
    int nLen = strlen(pSourceFieldDef->szFormat);
    if (nLen > MAX_FORMAT_ERROR_LEN) {
      nLen = MAX_FORMAT_ERROR_LEN;
      mystrncpy(szShortFormat, pSourceFieldDef->szFormat, nLen + 1);
      strcpy(szShortFormat + nLen, " ...");
    }
    else
      strcpy(szShortFormat, pSourceFieldDef->szFormat);

    *szShortFormat = '(';
    nLen = strlen(szShortFormat);
    if (nLen > 2 && szShortFormat[nLen-1] == ',')
      szShortFormat[nLen-1] = ')';
    sprintf(szLastFieldMappingError, "Value not found in list %s", szShortFormat);
    return 3;
  }

  // source value is valid
  pPos = strchr(pszDestFormat+1, ',');
  if (!pPos) {
    sprintf(szLastFieldMappingError, "Second string missing in destination format %s", pszDestFormat);
    return 4;
  }

  // source value matching first or second string defined in source format ?
  if (strncmp(pszSourceValue, pszSourceFormat + 1, strlen(pszSourceValue)) == 0)
    CopyStringTill(pszDestValue, pszDestFormat + 1, ',');  // copy first string defined in destination format (e.g. "true")
  else
    CopyStringTill(pszDestValue, pPos + 1, ',');  // copy second string defined in destination format (e.g. "false")

  return 0;  // success
}

//--------------------------------------------------------------------------------------------------------

int MapCsvToXmlValue(FieldMapping const *pFieldMapping, cpchar pCsvValue, pchar pXmlValue, int nMaxLen)
{
  int nErrorCode = 0;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  cpchar pszFormat = pFieldMapping->xml.szFormat;

  // mandatory field without content ?
  if (!*pCsvValue && pFieldMapping->csv.bMandatory) {
    LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, "Mandatory field empty");
    return 1;
  }

  // optional field without content ?
  if (!*pCsvValue && !pFieldMapping->csv.bMandatory)
    return 0;  // nothing to do

  if (pFieldMapping->csv.cType == 'B'/*BOOLEAN*/ && pFieldMapping->xml.cType == 'B'/*BOOLEAN*/) {
    // map boolean value
    nErrorCode = MapBoolFormat(pCsvValue, &pFieldMapping->csv, pXmlValue, &pFieldMapping->xml);
    if (nErrorCode > 0)
      LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'D'/*DATE*/ && pFieldMapping->xml.cType == 'D'/*DATE*/) {
    // map date value
    if (pszFormat == NULL || strlen(pszFormat) < 10)
      pszFormat = szXmlShortDateFormat;

    if (strlen(pszFormat) <= nMaxLen)
      nErrorCode = MapDateFormat(pCsvValue, pFieldMapping->csv.szFormat, pXmlValue, pszFormat);
    else
      nErrorCode = -1;

    if (nErrorCode > 0) {
      sprintf(szErrorMessage, "Invalid date (does not match '%s')", pFieldMapping->csv.szFormat);
      LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, szErrorMessage);
    }
  }

  if (pFieldMapping->csv.cType == 'I'/*INTEGER*/ && pFieldMapping->xml.cType == 'I'/*INTEGER*/) {
    nErrorCode = MapIntFormat(pCsvValue, pFieldMapping->csv.szFormat, pXmlValue, pszFormat, nMaxLen);
    if (nErrorCode > 0)
      LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->xml.cType == 'N'/*NUMBER*/) {
    nErrorCode = MapNumberFormat(pCsvValue, cDecimalPoint, pFieldMapping->csv.szFormat, pXmlValue, '.', pszFormat, nMaxLen);
    if (nErrorCode > 0)
      LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'T'/*TEXT*/ && pFieldMapping->xml.cType == 'T'/*TEXT*/) {
    nErrorCode = MapTextFormat(pCsvValue, &pFieldMapping->csv, pXmlValue, &pFieldMapping->xml, nMaxLen);
    if (nErrorCode > 0)
      LogCsvError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pCsvValue, szLastFieldMappingError);
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int MapXmlToCsvValue(FieldMapping const *pFieldMapping, cpchar pXmlValue, cpchar pXPath, pchar pCsvValue, int nMaxLen)
{
  int nErrorCode = 0;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  cpchar pszFromFormat = pFieldMapping->xml.szFormat;
  cpchar pszToFormat = pFieldMapping->csv.szFormat;

  // mandatory field without content ?
  if (!*pXmlValue && pFieldMapping->xml.bMandatory) {
    LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, "Mandatory field missing or empty");
    return 1;
  }

  if (pFieldMapping->csv.cType == 'B'/*BOOLEAN*/ && pFieldMapping->xml.cType == 'B'/*BOOLEAN*/) {
    // map boolean value
    nErrorCode = MapBoolFormat(pXmlValue, &pFieldMapping->xml, pCsvValue, &pFieldMapping->csv);
    if (nErrorCode > 0)
      LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'D'/*DATE*/ && pFieldMapping->xml.cType == 'D'/*DATE*/) {
    // map date value
    if (pszFromFormat == NULL || strlen(pszFromFormat) < 10)
      pszFromFormat = szXmlShortDateFormat;
    if (pszToFormat == NULL)
      pszToFormat = szCsvDefaultDateFormat;

    if (strlen(pszToFormat) <= nMaxLen)
      nErrorCode = MapDateFormat(pXmlValue, pszFromFormat, pCsvValue, pszToFormat);
    else
      nErrorCode = -1;

    if (nErrorCode > 0) {
      sprintf(szErrorMessage, "Invalid date (does not match '%s')", pszFromFormat);
      LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szErrorMessage);
    }
  }

  if (pFieldMapping->csv.cType == 'I'/*INTEGER*/ && pFieldMapping->xml.cType == 'I'/*INTEGER*/) {
    // map integer value
    nErrorCode = MapIntFormat(pXmlValue, pFieldMapping->xml.szFormat, pCsvValue, pFieldMapping->csv.szFormat, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->xml.cType == 'N'/*NUMBER*/) {
    // map xml number to csv number format
    nErrorCode = MapNumberFormat(pXmlValue, '.', pFieldMapping->xml.szFormat, pCsvValue, cDecimalPoint, pszToFormat, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'T'/*TEXT*/ && pFieldMapping->xml.cType == 'T'/*TEXT*/) {
    // map text value
    nErrorCode = MapTextFormat(pXmlValue, &pFieldMapping->xml, pCsvValue, &pFieldMapping->csv, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

/*
CSV_OP CSV_CONTENT    CSV_MO CSV_TYPE	CSV_MIN_LEN	CSV_MAX_LEN	CSV_FORMAT	CSV_DEFAULT	XML_OP	XML_CONTENT	XML_MO	XML_TYPE	XML_MIN_LEN	XML_MAX_LEN	XML_FORMAT	XML_DEFAULT
FIX    Constant Value M      TEXT	3	3			FIX	Constant Value	M	TEXT	3	3
VAR    Variables      O      INTEGER					VAR	Macro	O	INTEGER
MAP    Column Name    M NUMBER					MAP	XPATH	M	NUMBER
      DATE			DD.MM.YYYY					DATE
      DATETIME								DATETIME
      BOOLEAN			Ja/Nein					BOOLEAN
*/

//--------------------------------------------------------------------------------------------------------

char *GetNextLine(pchar *ppReadPos)
{
  char *pReadPos = *ppReadPos;
  char *pResult = pReadPos;

  if (pReadPos && *pReadPos) {
    while (strchr("\r\n", *pReadPos) == NULL)
      pReadPos++;
    if (*pReadPos) {
      *pReadPos++ = '\0';
      if (*pReadPos == '\n')
        pReadPos++;
    }
    *ppReadPos = pReadPos;
  }
  else
    pResult = NULL;

  return pResult;
}

//--------------------------------------------------------------------------------------------------------

int GetFields(char *pLine, char cDelimiter, pchar *aField, int nMaxFields, bool *pbColumnQuoted = NULL)
{
  char *pPos = pLine;
  int nFields = 0;
  char *pDelimiter = NULL;
  char *pEnd = NULL;
  char *pNext = NULL;

  // skip spaces and tabs at the beginning of the field
  while (*pPos && strchr(szIgnoreChars, *pPos))
    pPos++;

  while (*pPos && nFields < nMaxFields) {
    if (*pPos == '"') {
      // store flag that value has been quoted
      if (pbColumnQuoted)
        pbColumnQuoted[nFields] = true;

      // store start address of current field
      aField[nFields++] = ++pPos;

      // search for end of string
      pEnd = strchr(pPos, '"');
      if (pEnd) {
        *pEnd = '\0';
        pPos = pEnd + 1;

        // search for delimiter
        pDelimiter = strchr(pPos, cDelimiter);
        if (pDelimiter)
          pPos = pDelimiter + 1;
        else
          pPos = strchr(pPos, '\0');  // end of line
      }
      else
        pPos = strchr(pPos, '\0');  // end of line
    }
    else {
      // store start address of current field
      aField[nFields++] = pPos;

      // search for next delimiter
      pDelimiter = strchr(pPos, cDelimiter);

      // get end of current field
      if (pDelimiter) {
        pEnd = pDelimiter;
        pNext = pDelimiter + 1;
      }
      else {
        pEnd = strchr(pPos, '\0');  // end of line
        pNext = pEnd;
      }

      // skip spaces and tabs at the end of the field
      while (pEnd > pPos && strchr(szIgnoreChars, *(pEnd-1)))
        pEnd--;
      *pEnd = '\0';

      pPos = pNext;
    }

    // skip spaces and tabs at the beginning of the next field
    while (*pPos && strchr(szIgnoreChars, *pPos))
      pPos++;
  }

  return nFields;
}

//--------------------------------------------------------------------------------------------------------

pchar LoadTextMappingField(char const *szFieldName, pchar const *aField, int nFields, int nFieldIndex, char const *szValidContent)
{
  pchar pResult = (pchar)szEmptyString;

  if (nFieldIndex >= 0 && nFieldIndex < nFields) {
    pResult = aField[nFieldIndex];

    // TODO: check content

    int nLen = strlen(pResult);
    if ((strcmp(szFieldName, "CSV_FORMAT") == 0 || strcmp(szFieldName, "XML_FORMAT") == 0) && nLen >= 3 && *pResult == '(' && pResult[nLen-1] == ')') {
      // change first and last character to comma for easier check of content (comma+text+comma must part of this string)
      *pResult = ',';
      pResult[nLen-1] = ',';
    }
  }

  return pResult;
}

//--------------------------------------------------------------------------------------------------------

bool LoadBoolMappingField(char const *szFieldName, pchar const *aField, int nFields, int nFieldIndex, char const *szTrue, char const *szFalse, bool bDefault)
{
  bool bResult = bDefault;
  pchar pContent = NULL;

  if (nFieldIndex >= 0 && nFieldIndex < nFields) {
    pContent = aField[nFieldIndex];
    if (strnicmp(pContent, szTrue, strlen(szTrue)) == 0)
      bResult = true;
    if (strnicmp(pContent, szFalse, strlen(szFalse)) == 0)
      bResult = false;
  }

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

char LoadCharMappingField(char const *szFieldName, pchar const *aField, int nFields, int nFieldIndex, char const *szValidContent, char cDefault)
{
  char cResult = cDefault;
  pchar pContent = NULL;

  if (nFieldIndex >= 0 && nFieldIndex < nFields) {
    pContent = aField[nFieldIndex];
    // TODO: check content
    cResult = toupper(*pContent);
  }

  return cResult;
}

//--------------------------------------------------------------------------------------------------------

int LoadIntMappingField(char const *szFieldName, pchar const *aField, int nFields, int nFieldIndex, int nDefault)
{
  int nResult = nDefault;
  pchar pContent = NULL;

  if (nFieldIndex >= 0 && nFieldIndex < nFields) {
    pContent = aField[nFieldIndex];
    // TODO: check content
    if (*pContent >= '0' && *pContent <= '9')
      nResult = atoi(pContent);
  }

  return nResult;
}

//--------------------------------------------------------------------------------------------------------

int ReadFieldMappings(const char *szFileName)
{
  int i, nMapIndex, nMapIndex2;
  char *pColumnName = NULL;
  FieldMapping *pFieldMapping, *pFieldMapping2;
  pchar aField[MAX_MAPPING_COLUMNS];
  FILE *pFile = NULL;
  errno_t error_code;

  // open csv file for input in binary mode (cr/lf are not changed)
  error_code = fopen_s(&pFile, szFileName, "rb");
  if (!pFile) {
    sprintf(szLastError, "Cannot open mapping file '%s' (error code %d)", szFileName, error_code);
    puts(szLastError);
    return -2;
  }

  // get file size
  //int fseek(FILE *stream, long offset, int whence);
  int iReturnCode = fseek(pFile, 0, SEEK_END);
  int nFileSize = ftell(pFile);

  // allocate reading buffer
  nFieldMappingBufferSize = nFileSize + 1;
  pFieldMappingsBuffer = (char*)malloc(nFieldMappingBufferSize);

  if (!pFieldMappingsBuffer) {
    sprintf(szLastError, "Not enough memory for reading mapping file '%s' (%d bytes)", szFileName, nFileSize);
    puts(szLastError);
    return -1;  // not enough free memory
  }

  // clear read buffer
  memset(pFieldMappingsBuffer, 0, nFieldMappingBufferSize);

  // go to start of file
  iReturnCode = fseek(pFile, 0, SEEK_SET);

  // read file content
  int nBytesRead = fread(pFieldMappingsBuffer, nFieldMappingBufferSize, 1, pFile);

  // close file
  fclose(pFile);

  // initialize reading position
  char *pReadPos = pFieldMappingsBuffer;

  // get header line with column names
  char *pLine = GetNextLine(&pReadPos);

  // parse header line
  int nColumns = GetFields(pLine, ';', aField, MAX_MAPPING_COLUMNS);
  int nCsvOpIdx = -1;
  int nCsvContentIdx = -1;
  int nCsvShortNameIdx = -1;
  int nCsvMoIdx = -1;
  int nCsvTypeIdx = -1;
  int nCsvMinLenIdx = -1;
  int nCsvMaxLenIdx = -1;
  int nCsvFormatIdx = -1;
  int nCsvTransformIdx = -1;
  int nCsvDefaultIdx = -1;
  int nCsvConditionIdx = -1;
  int nXmlOpIdx = -1;
  int nXmlContentIdx = -1;
  int nXmlAttributeIdx = -1;
  int nXmlShortNameIdx = -1;
  int nXmlMoIdx = -1;
  int nXmlTypeIdx = -1;
  int nXmlMinLenIdx = -1;
  int nXmlMaxLenIdx = -1;
  int nXmlFormatIdx = -1;
  int nXmlTransformIdx = -1;
  int nXmlDefaultIdx = -1;
  int nXmlConditionIdx = -1;

  // find position of mapping columns
  for (i = 0; i < nColumns; i++) {
    pColumnName = aField[i];
    // search for csv columns
    if (strcmp(pColumnName, "CSV_OP") == 0) nCsvOpIdx = i;
    if (strcmp(pColumnName, "CSV_CONTENT") == 0) nCsvContentIdx = i;
    if (strcmp(pColumnName, "CSV_SHORT_NAME") == 0) nCsvShortNameIdx = i;
    if (strcmp(pColumnName, "CSV_MO") == 0) nCsvMoIdx = i;
    if (strcmp(pColumnName, "CSV_TYPE") == 0) nCsvTypeIdx = i;
    if (strcmp(pColumnName, "CSV_MIN_LEN") == 0) nCsvMinLenIdx = i;
    if (strcmp(pColumnName, "CSV_MAX_LEN") == 0) nCsvMaxLenIdx = i;
    if (strcmp(pColumnName, "CSV_FORMAT") == 0) nCsvFormatIdx = i;
    if (strcmp(pColumnName, "CSV_TRANSFORM") == 0) nCsvTransformIdx = i;
    if (strcmp(pColumnName, "CSV_DEFAULT") == 0) nCsvDefaultIdx = i;
    if (strcmp(pColumnName, "CSV_CONDITION") == 0) nCsvConditionIdx = i;
    // search for xml columns
    if (strcmp(pColumnName, "XML_OP") == 0) nXmlOpIdx = i;
    if (strcmp(pColumnName, "XML_CONTENT") == 0) nXmlContentIdx = i;
    if (strcmp(pColumnName, "XML_ATTRIBUTE") == 0) nXmlAttributeIdx = i;
    if (strcmp(pColumnName, "XML_SHORT_NAME") == 0) nXmlShortNameIdx = i;
    if (strcmp(pColumnName, "XML_MO") == 0) nXmlMoIdx = i;
    if (strcmp(pColumnName, "XML_TYPE") == 0) nXmlTypeIdx = i;
    if (strcmp(pColumnName, "XML_MIN_LEN") == 0) nXmlMinLenIdx = i;
    if (strcmp(pColumnName, "XML_MAX_LEN") == 0) nXmlMaxLenIdx = i;
    if (strcmp(pColumnName, "XML_FORMAT") == 0) nXmlFormatIdx = i;
    if (strcmp(pColumnName, "XML_TRANSFORM") == 0) nXmlTransformIdx = i;
    if (strcmp(pColumnName, "XML_DEFAULT") == 0) nXmlDefaultIdx = i;
    if (strcmp(pColumnName, "XML_CONDITION") == 0) nXmlConditionIdx = i;
  }

  if (nCsvOpIdx < 0 || nCsvContentIdx < 0 || nCsvTypeIdx < 0 || nXmlOpIdx < 0 || nXmlContentIdx < 0)
    return -2;  // missing mandatory columns in mapping file

  pFieldMapping = aFieldMapping;

  // load field mapping definitions into mapping array
  while (pLine = GetNextLine(&pReadPos)) {
    nColumns = GetFields(pLine, ';', aField, MAX_MAPPING_COLUMNS);
		// line with real content ?
    if (*pLine != '#' /*Comment*/ && nColumns >= 4 && nFieldMappings < MAX_FIELD_MAPPINGS) {
      //pFieldMapping->csv = EmptyFieldDefinition;
      pFieldMapping->csv.cOperation = LoadCharMappingField("CSV_OP", aField, nColumns, nCsvOpIdx, "FIX,VAR,MAP,CHANGE,UNIQUE,IF", 0);
      pFieldMapping->csv.szContent = LoadTextMappingField("CSV_CONTENT", aField, nColumns, nCsvContentIdx, NULL);
      pFieldMapping->csv.szAttribute = NULL; // not used at the moment
      pFieldMapping->csv.szShortName = LoadTextMappingField("CSV_SHORT_NAME", aField, nColumns, nCsvShortNameIdx, NULL);
      pFieldMapping->csv.bMandatory = LoadBoolMappingField("CSV_MO", aField, nColumns, nCsvMoIdx, "M", "O", false);
      pFieldMapping->csv.cType = LoadCharMappingField("CSV_TYPE", aField, nColumns, nCsvTypeIdx, "BOOLEAN,DATE,INTEGER,NUMBER,TEXT", 'T');
      pFieldMapping->csv.nMinLen = LoadIntMappingField("CSV_MIN_LEN", aField, nColumns, nCsvMinLenIdx, 0);
      pFieldMapping->csv.nMaxLen = LoadIntMappingField("CSV_MAX_LEN", aField, nColumns, nCsvMaxLenIdx, -1);
      pFieldMapping->csv.szFormat = LoadTextMappingField("CSV_FORMAT", aField, nColumns, nCsvFormatIdx, NULL);
      pFieldMapping->csv.szTransform = LoadTextMappingField("CSV_TRANSFORM", aField, nColumns, nCsvTransformIdx, NULL);
      pFieldMapping->csv.szDefault = LoadTextMappingField("CSV_DEFAULT", aField, nColumns, nCsvDefaultIdx, NULL);
      pFieldMapping->csv.szCondition = LoadTextMappingField("CSV_CONDITION", aField, nColumns, nCsvConditionIdx, NULL);

      //pFieldMapping->xml = EmptyFieldDefinition;
      pFieldMapping->xml.cOperation = LoadCharMappingField("XML_OP", aField, nColumns, nXmlOpIdx, "FIX,VAR,MAP,LOOP,IF", 0);
      pFieldMapping->xml.szContent = LoadTextMappingField("XML_CONTENT", aField, nColumns, nXmlContentIdx, NULL);
      pFieldMapping->xml.szAttribute = LoadTextMappingField("XML_ATTRIBUTE", aField, nColumns, nXmlAttributeIdx, NULL);
      pFieldMapping->xml.szShortName = LoadTextMappingField("XML_SHORT_NAME", aField, nColumns, nXmlShortNameIdx, NULL);
      pFieldMapping->xml.bMandatory = LoadBoolMappingField("XML_MO", aField, nColumns, nXmlMoIdx, "M", "O", pFieldMapping->csv.bMandatory);
      pFieldMapping->xml.bAddEmptyNodes = false;
      pFieldMapping->xml.cType = LoadCharMappingField("XML_TYPE", aField, nColumns, nXmlTypeIdx, "BOOLEAN,DATE,INTEGER,NUMBER,TEXT", pFieldMapping->csv.cType);
      pFieldMapping->xml.nMinLen = LoadIntMappingField("XML_MIN_LEN", aField, nColumns, nXmlMinLenIdx, pFieldMapping->csv.nMinLen);
      pFieldMapping->xml.nMaxLen = LoadIntMappingField("XML_MAX_LEN", aField, nColumns, nXmlMaxLenIdx, pFieldMapping->csv.nMaxLen);
      pFieldMapping->xml.szFormat = LoadTextMappingField("XML_FORMAT", aField, nColumns, nXmlFormatIdx, NULL);
      pFieldMapping->xml.szTransform = LoadTextMappingField("XML_TRANSFORM", aField, nColumns, nXmlTransformIdx, NULL);
      pFieldMapping->xml.szDefault = LoadTextMappingField("XML_DEFAULT", aField, nColumns, nXmlDefaultIdx, NULL);
      pFieldMapping->xml.szCondition = LoadTextMappingField("XML_CONDITION", aField, nColumns, nXmlConditionIdx, NULL);

      if (pFieldMapping->csv.cOperation == 'M' && pFieldMapping->xml.cOperation == 'M' && IsEmptyString(pFieldMapping->xml.szFormat) &&
          strchr("BT", pFieldMapping->xml.cType) && pFieldMapping->csv.cType == pFieldMapping->xml.cType)
        pFieldMapping->xml.szFormat = pFieldMapping->csv.szFormat;

      pFieldMapping->nCsvIndex = -1;
      pFieldMapping->nSourceMapIndex = -1;
      pFieldMapping++;
      nFieldMappings++;
    }
  }

  // search for field content mapping of source columns
  for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++)
    if (strchr("CIU"/*CHANGE,IF,UNIQUE*/, pFieldMapping->csv.cOperation)) {
      // search for content mapping of source column
      for (nMapIndex2 = 0, pFieldMapping2 = aFieldMapping; nMapIndex2 < nFieldMappings; nMapIndex2++, pFieldMapping2++)
        if (pFieldMapping2->csv.cOperation == 'M' && pFieldMapping2->xml.cOperation == 'M' && strcmp(pFieldMapping->csv.szContent, pFieldMapping2->csv.szContent) == 0) {
          pFieldMapping->nSourceMapIndex = nMapIndex2;
          nMapIndex2 = nFieldMappings;
        }
    }

  if (nFieldMappings == 0) {
    sprintf(szLastError, "No field mappings found in '%s'", szFileName);
    puts(szLastError);
  }

  return nFieldMappings;
} // ReadFieldMappings

//--------------------------------------------------------------------------------------------------------

void FreeCsvFileBuffers()
{
  if (pCsvDataBuffer != NULL) {
    free(pCsvDataBuffer);
    pCsvDataBuffer = NULL;
  }

  if (aCsvDataFields != NULL) {
    free(aCsvDataFields);
    aCsvDataFields = NULL;
  }
}

//--------------------------------------------------------------------------------------------------------

int ReadCsvData(const char *szFileName)
{
  int i, nColumnIndex, nMapIndex, nColumns, nNonEmptyColumns;
  char *pColumnName = NULL;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  pchar aField[MAX_CSV_COLUMNS];
  FieldMapping *pFieldMapping = NULL;
  FILE *pFile = NULL;
  errno_t error_code;

  // initialize number of columns and csv data lines
  nCsvDataColumns = 0;
  nRealCsvDataLines = 0;

  // open csv file for input in binary mode (cr/lf are not changed)
  error_code = fopen_s(&pFile, szFileName, "rb");
  if (!pFile) {
    sprintf(szLastError, "Cannot open input file '%s' (error code %d)", szFileName, error_code);
    puts(szLastError);
    return -2;
  }

  // get file size
  //int fseek(FILE *stream, long offset, int whence);
  int iReturnCode = fseek(pFile, 0, SEEK_END);
  int nFileSize = ftell(pFile);

  // allocate reading buffer
  nCsvDataBufferSize = nFileSize + 1;
  pCsvDataBuffer = (char*)malloc(nCsvDataBufferSize);

  if (!pCsvDataBuffer) {
    sprintf(szLastError, "Not enough memory for reading input file '%s' (%d bytes)", szFileName, nFileSize);
    puts(szLastError);
    return -1;  // not enough free memory
  }

  // clear read buffer
  memset(pCsvDataBuffer, 0, nCsvDataBufferSize);

  // go to start of file
  iReturnCode = fseek(pFile, 0, SEEK_SET);

  // read content of file
  memset(pCsvDataBuffer, 0, nCsvDataBufferSize);
  int nBytesRead = fread(pCsvDataBuffer, nCsvDataBufferSize, 1, pFile);
  //int nContentLength = strlen(pCsvDataBuffer);
  //pchar pTest = pCsvDataBuffer + nCsvDataBufferSize - 8;

  // close file
  fclose(pFile);

  // initialize reading position
  char *pReadPos = pCsvDataBuffer;

  // get header line with column names
  char *pLine = GetNextLine(&pReadPos);

  // save header line
  memset(szCsvHeader, 0, MAX_HEADER_SIZE);
  mystrncpy(szCsvHeader, pLine, MAX_HEADER_SIZE);

  // detect column delimiter
  cColumnDelimiter = GetColumnDelimiter(pLine);

  // set list of characters to be ignored when parsing the csv line (outside of quoted values)
  strcpy(szIgnoreChars, (cColumnDelimiter == '\t') ? " " : " \t");

  // reset field indices
  pFieldMapping = aFieldMapping;
	for (int i = 0; i < nFieldMappings; i++, pFieldMapping++) 
    pFieldMapping->nCsvIndex = -1;

  // parse header line
  nCsvDataColumns = GetFields(pLine, cColumnDelimiter, aField, MAX_CSV_COLUMNS);

  // search for fields referenced by the mapping definition
  pFieldMapping = aFieldMapping;
  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++) {
    if (strchr("CIMU"/*CHANGE,IF,MAP,UNIQUE*/, pFieldMapping->csv.cOperation)) {
      // search for column in csv header
      nColumnIndex = nCsvDataColumns - 1;
      while (nColumnIndex >= 0 && stricmp(aField[nColumnIndex],pFieldMapping->csv.szContent) != 0)
        nColumnIndex--;
      pFieldMapping->nCsvIndex = nColumnIndex;

      if (nColumnIndex < 0 && pFieldMapping->csv.bMandatory) {
        sprintf(szErrorMessage, "Cannot find mandatory column in input file header");
        LogCsvError(1, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, szEmptyString, szErrorMessage);
      }
    }
    pFieldMapping++;
  }

  // count number of lines
  nCsvDataLines = 0;
  char *pc = pReadPos;
  while (*pc)
    if (*pc++ == '\n')
      nCsvDataLines++;

  // allocate memory for field pointer array
  nCsvDataFieldsBufferSize = nCsvDataLines * nCsvDataColumns * sizeof(pchar);
  aCsvDataFields = (pchar*)malloc(nCsvDataFieldsBufferSize);

  if (!aCsvDataFields)
    return -1;  // not enough free memory

  // initialize flags whether csv values has been quoted or not
  for (i = 0; i < nCsvDataColumns; i++)
    abColumnQuoted[i] = false;

  // clear field pointer array
  memset(aCsvDataFields, 0, nCsvDataFieldsBufferSize);

  pchar *pCsvDataFields = aCsvDataFields;

  while (pLine = GetNextLine(&pReadPos)) {
    //if (nRealCsvDataLines >= 761)
    //  i = 0;  // for debugging purposes only!

    // parse current csv line
    nColumns = GetFields(pLine, cColumnDelimiter, aField, nCsvDataColumns, (nRealCsvDataLines == 0) ? abColumnQuoted : NULL);

    // count non-empty columns
    nNonEmptyColumns = 0;
    for (i = 0; i < nColumns; i++)
      if (*aField[i])
        nNonEmptyColumns++;

    if (nNonEmptyColumns >= 3 && nRealCsvDataLines < nCsvDataLines) {
      // copy pointer of fields content to field pointer array
      memcpy(pCsvDataFields, aField, nColumns * sizeof(pchar));
      // initialize non existing fields at the end of the line with empty strings
      for (i = nColumns; i < nCsvDataColumns; i++)
        pCsvDataFields[i] = (pchar)szEmptyString;
      // next line
      pCsvDataFields += nCsvDataColumns;
      nRealCsvDataLines++;
    }
  }

  return nFieldMappings;
} // ReadCsvData

//--------------------------------------------------------------------------------------------------------

cpchar GetCsvFieldValue(int nCsvDataLine, int nCsvColumn)
{
  cpchar pResult = NULL;

  if (nCsvDataLine >= 0 && nCsvDataLine < nRealCsvDataLines && nCsvColumn >= 0 && nCsvColumn < nCsvDataColumns)
    pResult = aCsvDataFields[nCsvDataLine * nCsvDataColumns + nCsvColumn];

  return pResult;
}

//--------------------------------------------------------------------------------------------------------

bool IsNewCsvFieldValue(int nCsvDataLines, int nCsvColumn, cpchar szCsvValue)
{
  bool bResult = false;

  if (nCsvDataLines >= 0 && nCsvDataLines < nRealCsvDataLines && nCsvColumn >= 0 && nCsvColumn < nCsvDataColumns) {
    bResult = true;
    cpchar *ppCsvValue = (cpchar*)(aCsvDataFields + nCsvColumn);
    for (int i = 0; i < nCsvDataLines; i++) {
      if (strcmp(szCsvValue, *ppCsvValue) == 0)
        return false;
      ppCsvValue += nCsvDataColumns;
    }
  }

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

int GetNumberOfDigits(cpchar szValue)
{
  int nResult = 0;

  for (cpchar pPos = szValue; *pPos >= '0' && *pPos <= '9'; pPos++)
    nResult++;

  return nResult;
}

//--------------------------------------------------------------------------------------------------------

int GetCsvDecimalPoint()
{
  int i, nColumnIndex, nMapIndex, nLineIndex, nDigitsBehindComma, nDigitsBehindPoint;
  int nCommaUsage = 0;
  int nPointUsage = 0;
  int nReturnCode = 0;
  pchar *ppCsvDataField;
  cpchar pCsvFieldValue = NULL;
  cpchar pCommaPos = NULL;
  cpchar pPointPos = NULL;
  CPFieldMapping pFieldMapping = aFieldMapping;

  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
    if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->csv.cOperation == 'M'/*MAP*/ && pFieldMapping->nCsvIndex >= 0)
    {
      nColumnIndex = pFieldMapping->nCsvIndex;
      ppCsvDataField = aCsvDataFields + nColumnIndex;

      for (i = 0; i < nRealCsvDataLines; i++) {
        pCsvFieldValue = *ppCsvDataField;
        //pCsvFieldValue = GetCsvFieldValue(i, nColumnIndex);

        pCommaPos = strchr(pCsvFieldValue, ',');
        if (pCommaPos)
          nDigitsBehindComma = GetNumberOfDigits(pCommaPos+1);
        else
          nDigitsBehindComma = -1;

        pPointPos = strchr(pCsvFieldValue, '.');
        if (pPointPos)
          nDigitsBehindPoint = GetNumberOfDigits(pPointPos+1);
        else
          nDigitsBehindPoint = -1;

        if (pCommaPos) {
          if (pPointPos) {
            if (pCommaPos > pPointPos)
              nCommaUsage++;  // comma is behind point
            else
              nPointUsage++;  // point is behind comma
          }
          else
            if (nDigitsBehindComma != 3)
              nCommaUsage++;  // high probability of comma used as decimal point
        }
        else {
          if (pPointPos && nDigitsBehindPoint != 3)
            nPointUsage++;  // high probability of point used as decimal point
        }

        // increment data field pointer to next csv line
        ppCsvDataField += nCsvDataColumns;
      }
    }
  }

  cDecimalPoint = (nCommaUsage > nPointUsage) ? ',' : '.';
  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int GetColumnIndex(cpchar szColumnName)
{
  int nMapIndex;
  CPFieldMapping pFieldMapping;

  // loop over all mapping fields
  for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++)
    if (pFieldMapping->csv.cOperation == 'M' && stricmp(szColumnName, pFieldMapping->csv.szContent) == 0)
      return pFieldMapping->nCsvIndex;

  return -1;
}

//--------------------------------------------------------------------------------------------------------

bool CheckCsvCondition(int nCsvDataLine, CPFieldMapping pFieldMapping)
{
  char szCondition[MAX_CONDITION_SIZE];
  pchar pPos;
  cpchar pFieldValue,  pLeftFieldValue, pRightFieldValue;
  int i, len, nLeftColumnIndex, nRightColumnIndex;
  bool bMatch = false;

  if (stricmp(pFieldMapping->csv.szCondition, "contentisvalid()") == 0) {
    // get content of csv field
    pFieldValue = GetCsvFieldValue(nCsvDataLine, pFieldMapping->nCsvIndex);

    // check content of csv field
    if (pFieldMapping->csv.cType == 'N' /*NUMBER*/)
      bMatch = IsValidNumber(pFieldValue, cDecimalPoint);

    if (pFieldMapping->csv.cType == 'T' /*TEXT*/)
      bMatch = IsValidText(pFieldValue, pFieldMapping);

    return bMatch;
  }

  if (strlen(pFieldMapping->csv.szCondition) > MAX_CONDITION_LEN)
    return false;

  strcpy(szCondition, pFieldMapping->csv.szCondition);

  // condition like "CCY != FUND_CCY" ?
  pPos = (pchar)strstr(szCondition, "!=");
  if (pPos > szCondition) {
    *pPos++ = '\0';
    *pPos++ = '\0';

    // space in front of operator ?
    len = strlen(szCondition);
    if (szCondition[len-1] == ' ')
      szCondition[len-1] = '\0';  // remove space

    // space behind operator ?
    if (*pPos == ' ')
      pPos++;  // skip space

    nLeftColumnIndex = GetColumnIndex(szCondition);
    nRightColumnIndex = GetColumnIndex(pPos);

    pLeftFieldValue = GetCsvFieldValue(nCsvDataLine, nLeftColumnIndex);
    pRightFieldValue = GetCsvFieldValue(nCsvDataLine, nRightColumnIndex);

    if (pLeftFieldValue && pRightFieldValue)
      bMatch = (strcmp(pLeftFieldValue, pRightFieldValue) != 0);
  }

  return bMatch;
}

//--------------------------------------------------------------------------------------------------------

void ParseCondition(cpchar szCondition, pchar szConditionBuffer, pchar *ppLeftPart, pchar *ppOperator, pchar *ppRightPart)
{
	pchar pPos;
	int len;

	*ppLeftPart = NULL;
	*ppOperator = NULL;
	*ppRightPart = NULL;

	if (strlen(szCondition) > MAX_CONDITION_LEN)
		return;

	strcpy(szConditionBuffer, szCondition);

	len = 4;
	pPos = (pchar)strstr(szConditionBuffer, " != ");
	if (!pPos) {
	  len = 3;
	  pPos = (pchar)strstr(szConditionBuffer, " = ");
	}

	if (pPos > szConditionBuffer) {
	  *pPos++ = '\0';
		*ppLeftPart = szConditionBuffer;

		pPos[len-2] = '\0';
		*ppOperator = pPos;

		*ppRightPart = pPos + len - 1;
	}
}

//--------------------------------------------------------------------------------------------------------

/*
int AddXmlField(xmlDocPtr pDoc, CPFieldMapping pFieldMapping)
{
  if (pFieldMapping->csv.cOperation == 'F'/*FIX* / && pFieldMapping->xml.cOperation == 'M'/*MAP* /)
    SetNodeValue(pDoc, NULL, pFieldMapping->xml.szContent, pFieldMapping->csv.szContent, pFieldMapping);

  return 0;
}
*/

//--------------------------------------------------------------------------------------------------------

int AddXmlNodeField(xmlDocPtr pDoc, xmlNodePtr pParentNode, FieldMapping *pFieldMapping, int nXPathOffset, int nCsvDataLine)
{
  cpchar pCsvFieldValue = NULL;

  if (pFieldMapping->xml.cOperation == 'M'/*MAP*/) {
    if (pFieldMapping->csv.cOperation == 'F'/*FIX*/)
      SetNodeValue(pDoc, pParentNode, pFieldMapping->xml.szContent + nXPathOffset, pFieldMapping->csv.szContent, pFieldMapping);

    if (pFieldMapping->csv.cOperation == 'M'/*MAP*/) {
      pCsvFieldValue = GetCsvFieldValue(nCsvDataLine, pFieldMapping->nCsvIndex);
      SetNodeValue(pDoc, pParentNode, pFieldMapping->xml.szContent + nXPathOffset, pCsvFieldValue, pFieldMapping);
    }
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------------

/*
strftime: https://en.cppreference.com/w/c/chrono/strftime
time_t clk = time(NULL);
struct tm* tm_info;
time(&timer);
tm_info = localtime(&timer);
strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
*/

#define MAX_COUNTERS  20
#define MAX_COUNTER_LENGTH  50

typedef struct {
  char szCounterName[MAX_COUNTER_NAME_SIZE];
  char szPeriod[8];  // EVER,YEAR,MONTH,DAY,HOUR,MINUTE,SECOND
  int nPeriodLength;
  char szCurrentPeriod[15];  // YYYYMMDDHHMISS
  char szType[10];  // NUM,HEX,APLHA,ALPHANUM,NUMALPHA
  char szValidChars[37];  // 0-9,A-Z
  int nLength;
  char szStartValue[MAX_COUNTER_LENGTH+1];
  char szCurrentValue[MAX_COUNTER_LENGTH+1];
} CounterDefinition;

CounterDefinition aCounter[MAX_COUNTERS];
int nCounters = 0;

/*
EVER,,NUM,10,1,1
YEAR,2019,NUM,10,0,1
YEAR,2019,HEX,10,0,A
YEAR,2019,ALPHA,10,A,X
YEAR,2019,ALPHANUM,10,0,2C
MONTH,201901,NUM,9,1,1
DAY,20190103,NUM,8,0,1
HOUR,2019010400,NUM,7,0,1
MINUTE,201901041200,NUM,6,0,1
SECOND,20190104125900,NUM,5,0,1
*/

int GetNextCounterValue(char *pszResult, cpchar pszCounterName)
{
  int i, nReturnCode, nValues, nValueLen, nMissingChars, nValidChars, nPos, nCharIndex, nCounterIndex = -1;
  CounterDefinition *pCounterDef = NULL;
  char szCounterFileName[MAX_FILE_NAME_SIZE];
  char szLine[MAX_LINE_SIZE];
  char szNow[15];
  char *pChar;
  pchar aszValue[6];

  if (strlen(pszCounterName) > MAX_COUNTER_NAME_LEN) {
    sprintf(szLastError, "Name of counter is too long (maximum is %d)", MAX_COUNTER_NAME_LEN);
    return 1;
  }

  for (i = 0; i < nCounters && nCounterIndex < 0; i++)
    if (stricmp(pszCounterName, aCounter[i].szCounterName) == 0) {
      nCounterIndex = i;
      pCounterDef = aCounter + i;
    }

  if (nCounterIndex < 0) {
    if (nCounters == MAX_COUNTERS) {
      sprintf(szLastError, "Too many counters used (maximum is %d)", MAX_COUNTERS);
      return 2;
    }

    // read counter definition file
    sprintf(szCounterFileName, "%s%s%s", szCounterPath, pszCounterName, ".cnt");
    nReturnCode = MyLoadFile(szCounterFileName, szLine, MAX_LINE_SIZE);
    if (nReturnCode < 0)
      return nReturnCode;

    // remove CR/LF at the end of the line
    pChar = strchr(szLine, '\r');
    if (pChar != NULL)
      *pChar = '\0';
    pChar = strchr(szLine, '\n');
    if (pChar != NULL)
      *pChar = '\0';

    // Parse content of counter definition
    // Sample: "DAY,20190103,NUM,6,1" or "DAY;20190103;NUM;6;1;1"
    nValues = SplitString(szLine, ',', aszValue, 6, " ");
    if (nValues < 5)
      nValues = SplitString(szLine, ';', aszValue, 6, " ");

    if (nValues < 5) {
      sprintf(szLastError, "Missing value(s) in counter definition '%s' (must contain at least five values)", szCounterFileName);
      return 2;
    }

    pCounterDef = aCounter + nCounters;

    // counter name
    strcpy(pCounterDef->szCounterName, pszCounterName);

    // counter period
    if (!CheckRegex(aszValue[0], ",EVER,YEAR,MONTH,DAY,HOUR,MINUTE,SECOND,")) {
      sprintf(szLastError, "Invalid period in counter definition '%s' ('%s' not in 'EVER,YEAR,MONTH,DAY,HOUR,MINUTE,SECOND')", szCounterFileName, aszValue[0]);
      return 3;
    }
    strcpy(pCounterDef->szPeriod, aszValue[0]);

    pCounterDef->nPeriodLength = 0;
    if (strcmp(pCounterDef->szPeriod, "YEAR") == 0) pCounterDef->nPeriodLength = 4;
    if (strcmp(pCounterDef->szPeriod, "MONTH") == 0) pCounterDef->nPeriodLength = 6;
    if (strcmp(pCounterDef->szPeriod, "DAY") == 0) pCounterDef->nPeriodLength = 8;
    if (strcmp(pCounterDef->szPeriod, "HOUR") == 0) pCounterDef->nPeriodLength = 10;
    if (strcmp(pCounterDef->szPeriod, "MINUTE") == 0) pCounterDef->nPeriodLength = 12;
    if (strcmp(pCounterDef->szPeriod, "SECOND") == 0) pCounterDef->nPeriodLength = 14;

    // current period
    if (!ValidChars(aszValue[1], "0123456789") || strlen(aszValue[1]) > 14) {
      sprintf(szLastError, "Invalid current period '%s' in counter definition '%s'", aszValue[1], szCounterFileName);
      return 3;
    }
    strcpy(pCounterDef->szCurrentPeriod, aszValue[1]);

    // counter type
    if (!CheckRegex(aszValue[2], ",NUM,HEX,APLHA,ALPHANUM,NUMALPHA,")) {
      sprintf(szLastError, "Invalid type in counter definition '%s' ('%s' not in 'NUM,HEX,APLHA,ALPHANUM,NUMALPHA')", szCounterFileName, aszValue[2]);
      return 4;
    }
    strcpy(pCounterDef->szType, aszValue[2]);

    if (strcmp(aszValue[2], "NUM") == 0)
      strcpy(pCounterDef->szValidChars, "0123456789");
    if (strcmp(aszValue[2], "HEX") == 0)
      strcpy(pCounterDef->szValidChars, "0123456789ABCDEF");
    if (strcmp(aszValue[2], "APLHA") == 0)
      strcpy(pCounterDef->szValidChars, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (strcmp(aszValue[2], "APLHANUM") == 0)
      strcpy(pCounterDef->szValidChars, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    if (strcmp(aszValue[2], "NUMALPHA") == 0)
      strcpy(pCounterDef->szValidChars, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    // counter length
    if (!ValidChars(aszValue[3], "0123456789") || strlen(aszValue[3]) > 2 || atoi(aszValue[3]) > MAX_COUNTER_LENGTH) {
      sprintf(szLastError, "Invalid length '%s' in counter definition '%s'", aszValue[3], szCounterFileName);
      return 5;
    }
    pCounterDef->nLength = atoi(aszValue[3]);

    // check start value
    if (!ValidChars(aszValue[4], pCounterDef->szValidChars) || strlen(aszValue[4]) > pCounterDef->nLength) {
      sprintf(szLastError, "Invalid start value '%s' in counter definition '%s'", aszValue[4], szCounterFileName);
      return 6;
    }
    // load start value
    nValueLen = strlen(aszValue[4]);
    nMissingChars = pCounterDef->nLength - nValueLen;
    for (i = 0; i < nMissingChars; i++)
      pCounterDef->szStartValue[i] = pCounterDef->szValidChars[0];
    strcpy(pCounterDef->szStartValue + nMissingChars, aszValue[4]);

    // current value
    strcpy(pCounterDef->szCurrentValue, pCounterDef->szStartValue);
    if (nValues > 5) {
      if (!ValidChars(aszValue[5], pCounterDef->szValidChars) || strlen(aszValue[5]) > pCounterDef->nLength) {
        sprintf(szLastError, "Invalid current value '%s' in counter definition '%s'", aszValue[5], szCounterFileName);
        return 7;
      }
      nValueLen = strlen(aszValue[4]);
      nMissingChars = pCounterDef->nLength - nValueLen;
      strcpy(pCounterDef->szCurrentValue + nMissingChars, aszValue[5]);
    }

    nCounters++;
  }

  if (pCounterDef->nPeriodLength > 0) {
    // get current period
	  time_t now = time(NULL);
	  struct tm* tm_info;
	  tm_info = localtime(&now);
    strftime(szNow, MAX_VALUE_SIZE, "%Y%m%d%H%M%S", tm_info);  // e.g. "2018-12-08T12:30:00"
    szNow[pCounterDef->nPeriodLength] = '\0';

    if (strcmp(pCounterDef->szCurrentPeriod, szNow) != 0) {
      // new period --> reset current value
      strcpy(pCounterDef->szCurrentPeriod, szNow);
      strcpy(pCounterDef->szCurrentValue, pCounterDef->szStartValue);
    }
  }

  // return current counter value
  strcpy(pszResult, pCounterDef->szCurrentValue);

  // increment counter value
  //char cLastValidChar = pCounterDef->szValidChars[strlen(pCounterDef->szValidChars)-1];
  nValidChars = strlen(pCounterDef->szValidChars);
  nPos = pCounterDef->nLength - 1;

  while (nPos >= 0) {
    pChar = strchr(pCounterDef->szValidChars, pCounterDef->szCurrentValue[nPos]);
    nCharIndex = pChar ? (pChar - pCounterDef->szValidChars) : nValidChars;
    if (nCharIndex >= nValidChars - 1) {
      pCounterDef->szCurrentValue[nPos] = pCounterDef->szValidChars[0];
      nPos--;
    }
    else {
      pCounterDef->szCurrentValue[nPos] = pCounterDef->szValidChars[nCharIndex + 1];
      nPos = -2;  // increment successful
    }
  }

  if (nPos == -1)
    ; // counter overflow

  return 0;
}

//--------------------------------------------------------------------------------------------------------

int SaveCounterValues()
{
  int i, nValidChars, nPos, nCounterIndex, nReturnCode = 0;
  CounterDefinition *pCounterDef = aCounter;
  char szCounterFileName[MAX_FILE_NAME_SIZE];
  char szLine[MAX_LINE_SIZE];

  for (nCounterIndex = 0; nCounterIndex < nCounters; nCounterIndex++, pCounterDef++) {
    // Content of counter definition
    // Sample: "DAY,20190103,NUM,6,1,1" or "DAY;20190103;NUM;6;1;1"
    sprintf(szLine, "%s,%s,%s,%d,%s,%s\n", pCounterDef->szPeriod, pCounterDef->szCurrentPeriod, pCounterDef->szType, pCounterDef->nLength, pCounterDef->szStartValue, pCounterDef->szCurrentValue);

    sprintf(szCounterFileName, "%s%s%s", szCounterPath, pCounterDef->szCounterName, ".cnt");
    nReturnCode = MyWriteFile(szCounterFileName, szLine);
  }

  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int GetVarValue(CPFieldMapping pFieldMapping, char *pResult, int nMaxSize)
{
	char szValue[MAX_VALUE_SIZE] = "";
	char szVarDef[MAX_VALUE_SIZE];
	char szAppend[MAX_VALUE_SIZE];
  char szCounterName[MAX_COUNTER_NAME_SIZE];
	char szFormat[MAX_TIMESTAMP_FORMAT_SIZE];
	char szTimeFormat[MAX_TIMESTAMP_FORMAT_SIZE];
	char *pEnd, pNext, *pPos = szVarDef;
	int nLen;

	time_t now = time(NULL);
	struct tm* tm_info;
	tm_info = localtime(&now);

	strcpy(szVarDef, pFieldMapping->csv.szContent);
	// VAR; "'SAMPLE.' NOW(YYYY-MM-DD) '.' COUNTER(day-index-1)"; M; TEXT; ; 1; 128; ; ; MAP; "ControlData/UniqueDocumentID"; ; TEXT

	while (*pPos) {
		if (*pPos == '\'') {
		  pEnd = ++pPos;
			while (*pEnd && *pEnd != '\'')
			  pEnd++;
			if (*pEnd == '\'')
			  *pEnd++ = '\0';
			nLen = strlen(szValue);
			mystrncpy(szValue + nLen, pPos, MAX_VALUE_SIZE - nLen);
			pPos = pEnd;
		}
		else
			if (strncmp(pPos, "NOW", 3) == 0) {
			  pPos += 3;
				*szFormat = '\0';
				if (*pPos == '(') {
					pEnd = ++pPos;
					while (*pEnd && *pEnd != ')')
						pEnd++;
					if (*pEnd == ')')
						*pEnd++ = '\0';
					mystrncpy(szFormat, pPos, MAX_TIMESTAMP_FORMAT_SIZE);
				}
				else
				  pEnd = pPos;

				if (strcmp(szFormat, "YYYY-MM-DD") == 0)
					strcpy(szTimeFormat, "%Y-%m-%d");
				else
					strcpy(szTimeFormat, "%Y-%m-%dT%H:%M:%S");
				strftime(szAppend, MAX_VALUE_SIZE, szTimeFormat, tm_info);  // e.g. "2018-12-08T12:30:00"

				nLen = strlen(szValue);
				mystrncpy(szValue + nLen, szAppend, MAX_VALUE_SIZE - nLen);
				pPos = pEnd;
			}
      else
			  if (strncmp(pPos, "COUNTER(", 8) == 0) {
			    pPos += 8;
					pEnd = pPos;
					while (*pEnd && *pEnd != ')')
						pEnd++;
					if (*pEnd == ')')
						*pEnd++ = '\0';
					mystrncpy(szCounterName, pPos, MAX_COUNTER_NAME_SIZE);

				  GetNextCounterValue(szAppend, szCounterName);

				  nLen = strlen(szValue);
				  mystrncpy(szValue + nLen, szAppend, MAX_VALUE_SIZE - nLen);
				  pPos = pEnd;
			  }
			  else {
				  pPos++;
			  }

		// skip spaces
		while (*pPos == ' ')
		  pPos++;
	}

	mystrncpy(pResult, szValue, nMaxSize);

	/*
  if (stricmp(pFieldMapping->csv.szContent, "NOW") == 0) {
    time_t now = time(NULL);
    struct tm* tm_info;
    tm_info = localtime(&now);
    strftime(pResult, nMaxSize, "%Y-%m-%dT%H:%M:%S", tm_info);  // e.g. "2018-12-08T12:30:00"
  }*/
  return 0;
}

//--------------------------------------------------------------------------------------------------------

int AddXmlField(xmlDocPtr pDoc, CPFieldMapping pFieldMapping, cpchar XPath, int nCsvDataLine, cpchar pszConditionAttributeName = NULL, cpchar pszConditionAttributeValue = NULL)
{
  char szValue[MAX_VALUE_SIZE] = "";
  cpchar pCsvFieldValue = NULL;
  int nReturnCode;

  if (pFieldMapping->xml.cOperation == 'M'/*MAP*/)
  {
    if (pFieldMapping->csv.cOperation == 'F'/*FIX*/) {
      SetNodeValue(pDoc, NULL, XPath, pFieldMapping->csv.szContent, pFieldMapping);
    }

    if (pFieldMapping->csv.cOperation == 'M'/*MAP*/) {
      // get value of csv field
      pCsvFieldValue = GetCsvFieldValue(nCsvDataLine, pFieldMapping->nCsvIndex);

      // no csv field value available, but default value defined ?
      if ((!pCsvFieldValue || !*pCsvFieldValue) && *pFieldMapping->csv.szDefault)
        pCsvFieldValue = pFieldMapping->csv.szDefault;  // use default value

      if (pCsvFieldValue /* && (*pCsvFieldValue || pFieldMapping->xml.bMandatory)*/) {
        //strcpy(szValue, pCsvFieldValue);  // csv value is default for xml value
        //ValidateCsvValue(szValue, pFieldMapping, nCsvDataLine);

        // check csv value and transform to xml format
        nReturnCode = MapCsvToXmlValue(pFieldMapping, pCsvFieldValue, szValue, MAX_VALUE_SIZE);
        SetNodeValue(pDoc, NULL, XPath, szValue, pFieldMapping, pszConditionAttributeName, pszConditionAttributeValue);
      }
    }

    if (pFieldMapping->csv.cOperation == 'V'/*VAR*/) {
      GetVarValue(pFieldMapping, szValue, MAX_VALUE_SIZE);
      SetNodeValue(pDoc, NULL, XPath, szValue, pFieldMapping);
    }

    if (strcmp(XPath, "ControlData/UniqueDocumentID") == 0)
      mystrncpy(szUniqueDocumentID, szValue, MAX_UNIQUE_DOCUMENT_ID_SIZE);
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------------

CPFieldMapping GetFieldMapping(cpchar szXPath)
{
  CPFieldMapping pFieldMapping = aFieldMapping;

	for (int i = 0; i < nFieldMappings; i++, pFieldMapping++) 
		if (strcmp(pFieldMapping->xml.szContent, szXPath) == 0 && pFieldMapping->xml.cOperation == 'M' && pFieldMapping->csv.cOperation == 'M')
		  return pFieldMapping;

	return NULL;
}

//--------------------------------------------------------------------------------------------------------

void GetXPathWithLoopIndices(cpchar pszSourceXPath, char *pszResultXPath)
{
  int nLoopIndex, nLoopXPathLen;
	char szRemainingXPath[MAX_XPATH_SIZE];
  CPFieldMapping pLoopFieldMapping = NULL;

  // load xpath of current field mapping
  strcpy(pszResultXPath, pszSourceXPath);

  // insert loop indizes into xpath
  for (nLoopIndex = nLoops - 1; nLoopIndex >= 0; nLoopIndex--) {
    pLoopFieldMapping = apLoopFieldMapping[nLoopIndex];
    // is current loop used in current xpath ?
    nLoopXPathLen = strlen(pLoopFieldMapping->xml.szContent);
    if (strncmp(pLoopFieldMapping->xml.szContent, pszResultXPath, nLoopXPathLen) == 0) {
      // insert loop index to current loop node in xpath
      strcpy(szRemainingXPath, pszResultXPath + nLoopXPathLen);
      sprintf(pszResultXPath + nLoopXPathLen, "[%d]%s", anLoopIndex[nLoopIndex] + 1, szRemainingXPath);
    }
  }
}

//--------------------------------------------------------------------------------------------------------

int GenerateXmlDocument()
{
	xmlNodePtr pRootNode = NULL;
  xmlNodePtr pLoopNode = NULL;
  PFieldMapping pFieldMapping = NULL;
  CPFieldMapping pLoopFieldMapping = NULL;
  CPFieldMapping pPrevLoopFieldMapping = NULL;
	char xpath[MAX_XPATH_SIZE];
  cpchar pCsvFieldValue = NULL;
  cpchar szLastLoopXPath = NULL;
  cpchar apLastValues[MAX_LOOPS];
  cpchar szIgnoreXPath = NULL;
	char szConditionBuffer[MAX_CONDITION_SIZE];
	char szConditionAttributeName[MAX_CONDITION_SIZE];
	char szConditionAttributeValue[MAX_CONDITION_SIZE];
	pchar pLeftPart = NULL;
	pchar pOperator = NULL;
	pchar pRightPart = NULL;
	CPFieldMapping pLookupFieldMapping = NULL;
  bool abFirstLoopRecord[MAX_LOOPS];
  int i, j, nReturnCode, nColumnIndex, nMapIndex, nLoopIndex, nLoopMapIndex, nLoopEndMapIndex, nDataLine, nLoopFields, nXPathLen, nXPathOffset;
  int nFirstLoopMapIndex = -1;
  int nResult = 0;
  bool bAddFieldValue, bMap, bMatch, bNewValue;

	// Creates a new document, a node and set it as a root node
  pXmlDoc = xmlNewDoc(BAD_CAST "1.0");
  pRootNode = xmlNewNode(NULL, BAD_CAST "FundsXML4");
	xmlDocSetRootElement(pXmlDoc, pRootNode);
  nLoops = 0;

  // set first loop map index to end of field map if no loops existing in mapping definition
  if (nFirstLoopMapIndex < 0)
    nFirstLoopMapIndex = nFieldMappings;

  // main loop over all csv lines/records
  for (nDataLine = 0; nDataLine < nRealCsvDataLines; nDataLine++)
  {
    nCurrentCsvLine = nDataLine + 1;  // for csv error logging

    // initialize column error flags
    for (i = 0; i < nCsvDataColumns; i++)
      abColumnError[i] = false;

    if (nDataLine == 0) {
      // initialize loop indices and last csv values
      nFirstLoopMapIndex = -1;
      pFieldMapping = aFieldMapping;
      for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
        if (pFieldMapping->xml.cOperation == 'L'/*LOOP*/ && nLoops < MAX_LOOPS)
        {
          apLastValues[nLoops] = GetCsvFieldValue(0, pFieldMapping->nCsvIndex);
          apLoopFieldMapping[nLoops] = pFieldMapping;
          anLoopIndex[nLoops] = 0;
          abFirstLoopRecord[nLoops] = true;
          if (nFirstLoopMapIndex < 0)
            nFirstLoopMapIndex = nMapIndex;
          nLoops++;
        }
      }
    }
    else {
      // update loop indices
      for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++) {
        pLoopFieldMapping = apLoopFieldMapping[nLoopIndex];
        if (pLoopFieldMapping->nCsvIndex >= 0) {
          // csv field name found in csv header line --> get content of field
          pCsvFieldValue = GetCsvFieldValue(nDataLine, pLoopFieldMapping->nCsvIndex);
          if (pLoopFieldMapping->csv.cOperation == 'U' /*UNIQUE*/)
            bNewValue = IsNewCsvFieldValue(nDataLine, pLoopFieldMapping->nCsvIndex, pCsvFieldValue);
          else  // cOperation == 'C' /*CHANGE*/
            bNewValue = (strcmp(pCsvFieldValue, apLastValues[nLoopIndex]) != 0);

          // has value in corresponding csv column changed?
          if (bNewValue) {
            // value in csv column is new --> increment current loop index
            anLoopIndex[nLoopIndex]++;
            apLastValues[nLoopIndex] = pCsvFieldValue;
            abFirstLoopRecord[nLoopIndex] = true;

            // initialize loop indices and last csv values of nested loops
            for (i = nLoopIndex + 1; i < nLoops; i++) {
              pFieldMapping = apLoopFieldMapping[i];
              // is this loop a nested loop?
              if (strncmp(pFieldMapping->xml.szContent, pLoopFieldMapping->xml.szContent, strlen(pLoopFieldMapping->xml.szContent)) != 0)
                break;
              anLoopIndex[i] = 0;
              apLastValues[i] = GetCsvFieldValue(nDataLine, pFieldMapping->nCsvIndex);
              abFirstLoopRecord[i] = true;
            }

            // continue for-loop with current index in i
            nLoopIndex = i - 1;
          }
          else
            abFirstLoopRecord[nLoopIndex] = false;
        }
      }
    }

    if (bTrace) {
      // current loop indices
      printf("Loop indices for csv line %d:", nDataLine);
      for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++) {
        pFieldMapping = apLoopFieldMapping[nLoopIndex];
        printf("  %s: %d/%d", pFieldMapping->csv.szContent, anLoopIndex[nLoopIndex], abFirstLoopRecord[i]);
      }
      printf("\n");
    }

    // loop over all mapping fields
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++)
    {
      if (nDataLine == 0 || nMapIndex >= nFirstLoopMapIndex)
      {
        /*
        if (nMapIndex >= 58)  // for debugging purposes only!
          i = 0;  // Start of AssetMasterData block
        */
        // within conditional block to be skipped ?
        bMap = true;
        if (szIgnoreXPath) {
          if (strncmp(pFieldMapping->xml.szContent, szIgnoreXPath, strlen(szIgnoreXPath)) == 0)
            bMap = false;  // yes, still within conditional block to be skipped
          else
            szIgnoreXPath = NULL;  // no, end of conditional block reached
        }
        if (bMap) {
          // start of conditional block ?
          if (pFieldMapping->xml.cOperation == 'I'/*IF*/) {
            bMatch = false;
            pCsvFieldValue = GetCsvFieldValue(nDataLine, pFieldMapping->nCsvIndex);
            if (pCsvFieldValue) {
              if (pFieldMapping->csv.szFormat[0] == ',')  // originally '('
                bMatch = CheckRegex(pCsvFieldValue, pFieldMapping->csv.szFormat);  // e.g. AssetType matching "(EQ)" (containing "EQ") ?
              else
                bMatch = (*pCsvFieldValue != '\0');  // field non-empty ?
            }
            if (bMatch && *pFieldMapping->csv.szCondition)
              bMatch = CheckCsvCondition(nDataLine, pFieldMapping);
            if (!bMatch)
              szIgnoreXPath = pFieldMapping->xml.szContent;
          }
          else
            if (pFieldMapping->xml.cOperation == 'L'/*LOOP*/) {
              ; // create new loop node if attribute(s) has to be added
            }
            else {
              bAddFieldValue = true;
              // insert loop indizes into xpath
              xpath[0] = '\0';
              szLastLoopXPath = szEmptyString;
              // check defined loops for usage in current xpath
              for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++) {
                pLoopFieldMapping = apLoopFieldMapping[nLoopIndex];
                if (strncmp(pLoopFieldMapping->xml.szContent, pFieldMapping->xml.szContent, strlen(pLoopFieldMapping->xml.szContent)) == 0) {
                  // add current loop index to current loop node in xpath
                  sprintf(xpath + strlen(xpath), "%s[%d]", pLoopFieldMapping->xml.szContent + strlen(szLastLoopXPath), anLoopIndex[nLoopIndex] + 1);
                  szLastLoopXPath = pLoopFieldMapping->xml.szContent;
                  bAddFieldValue = abFirstLoopRecord[nLoopIndex];  // new data to be added to xml document ?
                }
              }

              if (bAddFieldValue && pFieldMapping->csv.cOperation == 'M' && pFieldMapping->nCsvIndex >= 0 && *pFieldMapping->csv.szCondition)
                bAddFieldValue = CheckCsvCondition(nDataLine, pFieldMapping);

              if (bAddFieldValue) {
                // add part of current xpath since last loop node
                if (strlen(pFieldMapping->xml.szContent) > strlen(szLastLoopXPath))
                  strcat(xpath, pFieldMapping->xml.szContent + strlen(szLastLoopXPath));

				        // xml condition like "@ccy = Funds/Fund/Currency" ?
				        *szConditionAttributeName = '\0';
				        *szConditionAttributeValue = '\0';
				        if (*pFieldMapping->xml.szCondition == '@') {
					        ParseCondition(pFieldMapping->xml.szCondition, szConditionBuffer, &pLeftPart, &pOperator, &pRightPart);
					        pLookupFieldMapping = GetFieldMapping(pRightPart);  // pRightPart should contain the xpath of the related xml field
					        if (pLeftPart && pOperator && pRightPart && pLookupFieldMapping && pLookupFieldMapping->nCsvIndex >= 0) {
						        strcpy(szConditionAttributeName, pLeftPart+1);  // name of attribute (without the '@')
                    pCsvFieldValue = GetCsvFieldValue(nDataLine, pLookupFieldMapping->nCsvIndex);
                    if (strlen(pCsvFieldValue) <= MAX_CONDITION_LEN)
                      strcpy(szConditionAttributeValue, pCsvFieldValue);
					        }
				        }

                if (bTrace)
                  printf("Line %d / Field %d: CSV %c '%s' - XML %c '%s' Attr:%s Cond:%s\r\n", nDataLine, nMapIndex, pFieldMapping->csv.cOperation, pFieldMapping->csv.szContent, pFieldMapping->xml.cOperation, xpath, pFieldMapping->xml.szAttribute, pFieldMapping->xml.szCondition);

                // write value of mapped csv column to current xml document
                AddXmlField(pXmlDoc, pFieldMapping, xpath, nDataLine, szConditionAttributeName, szConditionAttributeValue);
                //int AddXmlField(xmlDocPtr pDoc, CPFieldMapping pFieldMapping, cpchar XPath, int nCsvDataLine)
              }
            }
        }
      }
    }
  }

  if (bTrace)
    puts("");

  return nResult;
} // GenerateXmlDocument

//--------------------------------------------------------------------------------------------------------

/*
<?xml version="1.0" encoding="UTF-8"?>
<FundsXML4>
  <ControlData>
	  <UniqueDocumentID>SAMPLE.2018-09-22.12345</UniqueDocumentID>
	  <DocumentGenerated>2018-09-22T16:55:24</DocumentGenerated>
	  <ContentDate>2018-09-22</ContentDate>
	  <DataSupplier>
	    <SystemCountry>EU</SystemCountry>
	    <Short>SDS</Short>
	    <Name>Sample Data Supplier</Name>
	    <Type>Generic</Type>
	  </DataSupplier>
	  <Language>EN</Language>
  </ControlData>
  <Funds>
	  <Fund>
	    <Identifiers>
		    <LEI>2138000SAMPLE00LEI00</LEI>
	    </Identifiers>
	    <Names>
		    <OfficialName>FundsXML Sample Fund</OfficialName>
	    </Names>
	    <Currency>EUR</Currency>
	    <SingleFundFlag>true</SingleFundFlag>
*/

//--------------------------------------------------------------------------------------------------------

int ConvertCsvToXml(cpchar szCsvFileName, cpchar szXmlFileName)
{
	int i, j, nReturnCode;

  nErrors = 0;
  *szLastError = '\0';

  printf("CSV input file: %s\n", szCsvFileName);
  printf("XML output file: %s\n", szXmlFileName);
  printf("Error file: %s\n\n", szErrorFileName);

  nReturnCode = ReadCsvData(szCsvFileName);
  printf("CSV Data Columns: %d\nCSV Real Data Lines: %d\n\n", nCsvDataColumns, nRealCsvDataLines);

  nReturnCode = GetCsvDecimalPoint();

  if (bTrace) {
    pchar *pField = aCsvDataFields;
    for (i = 0; i < nCsvDataLines; i++) {
      for (j = 0; j < nCsvDataColumns; j++) {
        if (*pField)
          printf("%s; ", *pField);
        pField++;
      }
      puts("");
    }
    puts("");
  }

  // Sample code: http://xmlsoft.org/examples/index.html
  nReturnCode = GenerateXmlDocument();

  // Dumping document to stdio or file
  xmlSaveFormatFileEnc(szXmlFileName, pXmlDoc, "UTF-8", 1);
  printf("Result written to file '%s'\n\n", szXmlFileName);

  // save counter values (if used)
  nReturnCode = SaveCounterValues();

  // close error file (if open)
  if (pErrorFile) {
    fclose(pErrorFile);
    pErrorFile = NULL;
  }

  printf("Number of errors detected: %d\n\n", nErrors);

  // free the xml document
  xmlFreeDoc(pXmlDoc);

  // free the global variables that may have been allocated by the parser.
  xmlCleanupParser();

  // free csv buffers used for reading the csv data
  FreeCsvFileBuffers();

	return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

void CheckUniqueFieldMappings(cpchar pszFieldName, int nLoopIndex)
{
  int nMapIndex;
  FieldMapping *pFieldMapping = aFieldMapping;

  // set referenced unique loop index in field mappings
  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++)
    if (pFieldMapping->csv.cOperation == 'M'/*MAP*/ && pFieldMapping->xml.cOperation == 'M'/*MAP*/ && strcmp(pFieldMapping->csv.szContent, pszFieldName) == 0)
      pFieldMapping->nRefUniqueLoopIndex = nLoopIndex;
}

//--------------------------------------------------------------------------------------------------------

int GetUniqueLoopNodeIndex(CPFieldMapping pFieldMapping, CPFieldMapping pUniqueFieldMapping, cpchar pszFieldValue, int nLoopSize)
{
	// search node index with current csv value
  int i, nReturnCode, nIndex = -1;
  cpchar pXmlFieldValue = NULL;
  char xpath[MAX_XPATH_SIZE];
  xmlNodePtr pRootNode = xmlDocGetRootElement(pXmlDoc);

  // Sample mapping definition:
	// UNIQUE; "UNIQUE_ID"; M; TEXT; ; 1; 256; ; ; LOOP; "AssetMasterData/Asset"
	// MAP; "UNIQUE_ID"; M; TEXT; ; 1; 256; ; ; MAP; "AssetMasterData/Asset/UniqueID"; ; TEXT

	for (i = 0; i < nLoopSize && nIndex < 0; i++) {
		sprintf(xpath, "%s[%d]%s", pFieldMapping->xml.szContent, i+1, pUniqueFieldMapping->xml.szContent + strlen(pFieldMapping->xml.szContent));
		pXmlFieldValue = NULL;
		nReturnCode = GetNodeTextValue(pRootNode, xpath, &pXmlFieldValue);
		if (pXmlFieldValue != NULL && strcmp(pXmlFieldValue, pszFieldValue) == 0)
			nIndex = i;
	}

  return nIndex;
}

//--------------------------------------------------------------------------------------------------------

int WriteCsvFile(cpchar szFileName)
{
  int i, j, nIndex, nColumnIndex, nMapIndex, nLoopIndex, nLoopMapIndex, nLoopEndMapIndex, nLoopFields, nLoopXPathLen, nIncrementedLoopIndex, nXPathLen, nXPathOffset;
  int nReturnCode = 0;
  int nDataLine = 0;
  //int nFirstUniqueLoopIndex = -1;
  //int nLastUniqueLoopIndex = -1;
  char *pColumnName = NULL;
  char *pFieldValueBufferPos = NULL;
  char szTempFieldValue[MAX_VALUE_SIZE];
  cpchar aField[MAX_CSV_COLUMNS];
  FILE *pFile = NULL;
  errno_t error_code;
  char xpath[MAX_XPATH_SIZE];
  char xpath2[MAX_XPATH_SIZE];
	char szConditionBuffer[MAX_CONDITION_SIZE];
	char szConditionAttributeName[MAX_CONDITION_SIZE];
	char szConditionAttributeValue[MAX_CONDITION_SIZE];
	cpchar pCsvFieldName = NULL;
	cpchar pCsvFieldValue = NULL;
  cpchar pXmlFieldValue = NULL;
  //cpchar szLastLoopXPath = NULL;
  //cpchar apLastValues[MAX_LOOPS];
  cpchar szIgnoreXPath = NULL;
  cpchar szLastLoopXPath = NULL;
  cpchar szIncrementedXPath = NULL;
	pchar pLeftPart = NULL;
	pchar pOperator = NULL;
	pchar pRightPart = NULL;
  FieldMapping *pFieldMapping = NULL;
	CPFieldMapping pLookupFieldMapping = NULL;
	CPFieldMapping pLoopFieldMapping = NULL;
	PFieldMapping pSourceFieldMapping = NULL;
  CPFieldMapping pUniqueFieldMapping = NULL;
  //int nFirstLoopMapIndex = -1;
  bool bMap, bMatch, bLoopData;

  // open file in write mode
  error_code = fopen_s(&pFile, szFileName, "w");
  if (!pFile) {
    printf("Cannot open file '%s' (error code %d)\n", szFileName, error_code);
    return -2;
  }

  // write template header to file
  fprintf(pFile, "%s\n", szCsvHeader);

  // get root node of xml document
  xmlNodePtr pRootNode = xmlDocGetRootElement(pXmlDoc);

  // initialize loop indices and last csv values
  nLoops = 0;
  pFieldMapping = aFieldMapping;
  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
    // initialize loop indices
    pFieldMapping->nLoopIndex = -1;
    pFieldMapping->nRefUniqueLoopIndex = -1;

    if (pFieldMapping->xml.cOperation == 'L'/*LOOP*/ && nLoops < MAX_LOOPS)
    {
      pFieldMapping->nLoopIndex = nLoops;
      apLoopFieldMapping[nLoops] = pFieldMapping;
      anLoopIndex[nLoops] = 0;
      if (pFieldMapping->csv.cOperation == 'U'/*UNIQUE*/)
        CheckUniqueFieldMappings(pFieldMapping->csv.szContent, nLoops);
      anLoopSize[nLoops] = GetNodeCount(pRootNode, pFieldMapping->xml.szContent);
      nLoops++;
      if (bTrace)
        printf("Loop %d: MapIndex %d, XPath %s, NodeCount %d\n", nLoops, nMapIndex, pFieldMapping->xml.szContent, anLoopSize[nLoops - 1]);
    }
  }

  if (bTrace)
    puts("");

  bLoopData = true;

  while (bLoopData)
  {
    // next csv line
    nDataLine++;
    nCurrentCsvLine = nDataLine;  // for csv error logging

    //char szFieldValueBuffer[MAX_LINE_SIZE];
    pFieldValueBufferPos = szFieldValueBuffer;
    szIgnoreXPath = NULL;

    // initialize list of csv field contents with empty strings
    for (i = 0; i < nCsvDataColumns; i++)
      aField[i] = szEmptyString;

    // initialize unique loop node indeces
    for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++)
      if (apLoopFieldMapping[nLoopIndex]->csv.cOperation == 'U'/*UNIQUE*/)
        anLoopIndex[nLoopIndex] = -1;

    // get list of csv field contents
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
      pFieldMapping->csv.szValue = NULL;
      pFieldMapping->xml.szValue = NULL;
    }

    // get list of csv field contents
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
      // within conditional block to be skipped ?
      bMap = true;
      if (szIgnoreXPath) {
        if (strncmp(pFieldMapping->xml.szContent, szIgnoreXPath, strlen(szIgnoreXPath)) == 0)
          bMap = false;  // yes, still within conditional block to be skipped
        else
          szIgnoreXPath = NULL;  // no, end of conditional block reached
      }
      if (bMap) {
        // start of conditional block ?
        if (pFieldMapping->xml.cOperation == 'I'/*IF*/ && pFieldMapping->nSourceMapIndex >= 0) {
          bMatch = false;
          pSourceFieldMapping = aFieldMapping + pFieldMapping->nSourceMapIndex;
          // load field content from: nSourceMapIndex
          if (pSourceFieldMapping->xml.szValue == NULL) {
            GetXPathWithLoopIndices(pSourceFieldMapping->xml.szContent, xpath);
            pXmlFieldValue = NULL;
            nReturnCode = GetNodeTextValue(pRootNode, xpath, &pSourceFieldMapping->xml.szValue);
          }
          // assuming no mapping of xml content
          pCsvFieldValue = pSourceFieldMapping->xml.szValue;
          if (pCsvFieldValue) {
            if (pFieldMapping->csv.szFormat[0] == ',')  // originally '('
              bMatch = CheckRegex(pCsvFieldValue, pFieldMapping->csv.szFormat);  // e.g. AssetType matching "(EQ)" (containing "EQ") ?
            else
              bMatch = (*pCsvFieldValue != '\0');  // field non-empty ?
          }
          if (bMatch && *pFieldMapping->csv.szCondition)
            bMatch = CheckCsvCondition(nDataLine, pFieldMapping);
          if (!bMatch)
            szIgnoreXPath = pFieldMapping->xml.szContent;
        }

        if (pFieldMapping->nCsvIndex >= 0 && strchr("FMV"/*Fix,Map,Var*/, pFieldMapping->xml.cOperation))
        {
          GetXPathWithLoopIndices(pFieldMapping->xml.szContent, xpath);

				  // xml condition like "@ccy = Funds/Fund/Currency" ?
				  *szConditionAttributeName = '\0';
				  *szConditionAttributeValue = '\0';
				  if (*pFieldMapping->xml.szCondition == '@') {
					  ParseCondition(pFieldMapping->xml.szCondition, szConditionBuffer, &pLeftPart, &pOperator, &pRightPart);
					  pLookupFieldMapping = GetFieldMapping(pRightPart);  // pRightPart should contain the xpath of the related xml field
					  if (pLeftPart && pOperator && pRightPart && pLookupFieldMapping && pLookupFieldMapping->nCsvIndex >= 0) {
						  strcpy(szConditionAttributeName, pLeftPart+1);  // name of attribute (without the '@')
						  strcpy(szConditionAttributeValue, aField[pLookupFieldMapping->nCsvIndex]);
					  }
				  }

          // get content of xml field
          pXmlFieldValue = NULL;

          //if (strcmp(pFieldMapping->csv.szContent, "CCY") == 0)
          //  i = 0;  // for debugging purposes only!

          nReturnCode = GetNodeTextValue(pRootNode, xpath, &pXmlFieldValue, szConditionAttributeName, szConditionAttributeValue);
          if (pXmlFieldValue) {
            *pFieldValueBufferPos = '\0';
            // map content of xml field to csv format
            nReturnCode = MapXmlToCsvValue(pFieldMapping, pXmlFieldValue, xpath, pFieldValueBufferPos, MAX_LINE_LEN - (pFieldValueBufferPos - szFieldValueBuffer + 2));

            if (pFieldMapping->nCsvIndex >= 0 && abColumnQuoted[pFieldMapping->nCsvIndex] && strlen(pFieldValueBufferPos) < MAX_VALUE_SIZE) {
              // add quotes at the beginning and end of csv value
              strcpy(szTempFieldValue, pFieldValueBufferPos);
              strcpy(pFieldValueBufferPos, "\"");
              strcat(pFieldValueBufferPos, szTempFieldValue);
              strcat(pFieldValueBufferPos, "\"");
            }

            // add content of csv field to csv buffer
            aField[pFieldMapping->nCsvIndex] = pFieldValueBufferPos;
            pFieldValueBufferPos += strlen(pFieldValueBufferPos) + 1;

            // is content of this field used for unique loop ?
            if (pFieldMapping->nRefUniqueLoopIndex >= 0 && anLoopIndex[pFieldMapping->nRefUniqueLoopIndex] < 0) {
              pLoopFieldMapping = apLoopFieldMapping[pFieldMapping->nRefUniqueLoopIndex];
				      pUniqueFieldMapping = pLoopFieldMapping + 1;  // mapping for content of unique field
              pCsvFieldValue = aField[pFieldMapping->nCsvIndex];
              nIndex = GetUniqueLoopNodeIndex(pLoopFieldMapping, pUniqueFieldMapping, pCsvFieldValue, anLoopSize[pLoopFieldMapping->nLoopIndex]);
				      if (nIndex >= 0)
					      anLoopIndex[pLoopFieldMapping->nLoopIndex] = nIndex;
			      }

            if (bTrace)
              printf("Line %d: MapIndex %d, Column %d, Field %s, Value '%s', XPath %s\n", nDataLine, nMapIndex, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXmlFieldValue, xpath);
          }
          else
            if (pFieldMapping->xml.bMandatory)
              LogXmlError(nDataLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, xpath, szEmptyString, "Mandatory field/node missing");
        }

			  if (pFieldMapping->nCsvIndex >= 0 && pFieldMapping->csv.cOperation == 'U'/*UNIQUE*/ && anLoopIndex[pFieldMapping->nLoopIndex] < 0) {
				  pUniqueFieldMapping = pFieldMapping + 1;  // mapping for content of unique field
          pCsvFieldValue = aField[pFieldMapping->nCsvIndex];
          nIndex = GetUniqueLoopNodeIndex(pFieldMapping, pUniqueFieldMapping, pCsvFieldValue, anLoopSize[pFieldMapping->nLoopIndex]);
				  if (nIndex >= 0)
					  anLoopIndex[pFieldMapping->nLoopIndex] = nIndex;
			  }
      }
		}

    // fill line buffer with field contents
    strcpy(szLineBuffer, aField[0]);
    for (i = 1; i < nCsvDataColumns; i++)
      if (strlen(szLineBuffer) + strlen(aField[i]) < MAX_LINE_LEN)
        sprintf(szLineBuffer + strlen(szLineBuffer), "%c%s", cColumnDelimiter, aField[i]);
    strcat(szLineBuffer, "\n");

    // write template header to file
    fputs(szLineBuffer, pFile);
    if (bTrace)
      printf("%d: %s", nDataLine, szLineBuffer);

    // increment loop indices
    nIncrementedLoopIndex = -1;
    for (nLoopIndex = nLoops - 1; nLoopIndex >= 0 && nIncrementedLoopIndex < 0; nLoopIndex--) {
      if (anLoopSize[nLoopIndex] > 0 && apLoopFieldMapping[nLoopIndex]->csv.cOperation == 'C'/*CHANGE*/)
        if (anLoopIndex[nLoopIndex] < anLoopSize[nLoopIndex] - 1) {
          // increment loop index
          anLoopIndex[nLoopIndex]++;
          nIncrementedLoopIndex = nLoopIndex;
          nColumnIndex = apLoopFieldMapping[nLoopIndex--]->nCsvIndex;
          // are there loops existing triggered by the same csv column ?
          while (nColumnIndex >= 0 && nLoopIndex >= 0) {
            if (apLoopFieldMapping[nLoopIndex]->nCsvIndex == nColumnIndex && apLoopFieldMapping[nLoopIndex]->csv.cOperation == 'C'/*CHANGE*/) {
              // increment loop(s) with same connected csv column (they must be aligned)
              anLoopIndex[nLoopIndex]++;
              nIncrementedLoopIndex = nLoopIndex;
            }
            nLoopIndex--;
          }
        }
        /*else {
          anLoopIndex[nLoopIndex] = -1;
          anLoopSize[nLoopIndex] = -1;
        }*/
    }

    if (nIncrementedLoopIndex < 0)
      bLoopData = false;  // no more data found to add to result csv file
    else {
      // insert node indices into xpath till current loop index
      xpath[0] = '\0';
      szLastLoopXPath = szEmptyString;
      szIncrementedXPath = apLoopFieldMapping[nIncrementedLoopIndex]->xml.szContent;
      for (i = 0; i <= nIncrementedLoopIndex; i++) {
        pFieldMapping = apLoopFieldMapping[i];
        if (anLoopSize[i] > 0 && strncmp(szIncrementedXPath, pFieldMapping->xml.szContent, strlen(pFieldMapping->xml.szContent)) == 0) {
          sprintf(xpath + strlen(xpath), "%s[%d]", pFieldMapping->xml.szContent + strlen(szLastLoopXPath), anLoopIndex[i] + 1);
          szLastLoopXPath = pFieldMapping->xml.szContent;
        }
      }
      // get new node counts behind current loop node
      for (nLoopIndex = nIncrementedLoopIndex + 1; nLoopIndex < nLoops; nLoopIndex++) {
        pFieldMapping = apLoopFieldMapping[nLoopIndex];
        if (strncmp(pFieldMapping->xml.szContent, szLastLoopXPath, strlen(szLastLoopXPath)) == 0) {
          sprintf(xpath2, "%s%s", xpath, pFieldMapping->xml.szContent + strlen(szLastLoopXPath));
          anLoopIndex[nLoopIndex] = 0;
          anLoopSize[nLoopIndex] = GetNodeCount(pRootNode, xpath2);
        }
      }
      // are there loops existing triggered by the same csv column ?
      nLoopIndex = nIncrementedLoopIndex + 1;
      while (nColumnIndex >= 0 && nLoopIndex < nLoops) {
        if (apLoopFieldMapping[nLoopIndex]->nCsvIndex == nColumnIndex)
				  if (apLoopFieldMapping[nLoopIndex]->csv.cOperation == 'C'/*CHANGE*/) {
						//anLoopIndex[nLoopIndex]++;  // increment loop(s) with same connected csv column (they must be aligned)
						anLoopIndex[nLoopIndex] = anLoopIndex[nIncrementedLoopIndex];  // align loop index of loops connected to same csv column
					}
				nLoopIndex++;
      }
    }
  }

  // close file
  fclose(pFile);

  nRealCsvDataLines = nDataLine;
  return nReturnCode;
} // WriteCsvFile

//--------------------------------------------------------------------------------------------------------

int ConvertXmlToCsv(cpchar szXmlFileName, cpchar szCsvTemplateFileName, cpchar szCsvResultFileName)
{
	int i, j, nReturnCode;
  cpchar pszValue = NULL;
  char szValue[MAX_VALUE_SIZE];

  nErrors = 0;
  *szLastError = '\0';
  *szUniqueDocumentID = '\0';

  printf("XML input file: %s\n", szXmlFileName);
  printf("CSV template file: %s\n", szCsvTemplateFileName);
  printf("CSV output file: %s\n", szCsvResultFileName);
  printf("Error file: %s\n\n", szErrorFileName);

  // read source xml file
  //xmlDocPtr	xmlReadMemory (const char * buffer, int size, const char * URL, const char * encoding, int options)
  //xmlDocPtr	xmlReadFile (const char * filename, const char * encoding, int options)
  pXmlDoc = xmlReadFile(szXmlFileName, NULL, 0);

  xmlNodePtr pRootNode = xmlDocGetRootElement(pXmlDoc);
  nReturnCode = GetNodeTextValue(pRootNode, "ControlData/UniqueDocumentID", &pszValue);
  if (pszValue) {
    printf("Content of ControlData/UniqueDocumentID: %s\n\n", pszValue);
    mystrncpy(szUniqueDocumentID, pszValue, MAX_UNIQUE_DOCUMENT_ID_SIZE);
  }
  else
    puts("Node ControlData/UniqueDocumentID not found.\n");

  // read csv template file
  nReturnCode = ReadCsvData(szCsvTemplateFileName);
  printf("CSV Data Columns: %d\nCSV Template Lines: %d\n\n", nCsvDataColumns, nRealCsvDataLines);

  nReturnCode = GetCsvDecimalPoint();

  if (bTrace) {
    pchar *pField = aCsvDataFields;
    for (i = 0; i < nCsvDataLines; i++) {
      for (j = 0; j < nCsvDataColumns; j++) {
        if (*pField)
          printf("%s; ", *pField);
        pField++;
      }
      puts("");
    }
    puts("");
  }

  // write csv result file
  nReturnCode = WriteCsvFile(szCsvResultFileName);

  // save counter values (if used)
  nReturnCode = SaveCounterValues();

  // close error file (if open)
  if (pErrorFile) {
    fclose(pErrorFile);
    pErrorFile = NULL;
  }

  printf("CSV Data Columns: %d\nCSV Data Lines: %d\n\n", nCsvDataColumns, nRealCsvDataLines);
  printf("Number of errors detected: %d\n\n", nErrors);

  /*free the document */
  xmlFreeDoc(pXmlDoc);

  // Free the global variables that may have been allocated by the parser.
  xmlCleanupParser();
	return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int MyMoveFile(cpchar szSourceFileName, cpchar szDestinationFileName)
{
  int nReturnCode;

	nReturnCode = rename(szSourceFileName, szDestinationFileName);

	/*
	if (nReturnCode == EXDEV) {
		// maybe we have to copy the file and remove the original file then
	}
	*/

	return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	int i, j, nReturnCode, nFiles = 0;
  char szConversion[8] = "csv2xml";
  char szInput[MAX_FILE_NAME_SIZE] = "";
  char szInputFileName[MAX_FILE_NAME_SIZE] = "";
  char szInputPath[MAX_FILE_NAME_SIZE] = "";
  char szLogFileName[MAX_FILE_NAME_SIZE] = "log.csv";
  char szMappingFileName[MAX_FILE_NAME_SIZE] = "mapping.csv";
  char szTemplateFileName[MAX_FILE_NAME_SIZE] = "template.csv";
  char szOutput[MAX_FILE_NAME_SIZE] = "";
  char szOutputFileName[MAX_FILE_NAME_SIZE] = "";
  char szOutputPath[MAX_FILE_NAME_SIZE] = "";
  char szError[MAX_FILE_NAME_SIZE] = "errors.csv";
  char szProcessed[MAX_FILE_NAME_SIZE] = "";
  char szProcessedFileName[MAX_FILE_NAME_SIZE] = "";
  //char szErrorFileName[MAX_FILE_NAME_SIZE] = "";
  cpchar pcParameter = NULL;
  cpchar pcContent = NULL;
  pchar pStarPos = NULL;
  char ch;

  puts("");
  puts("FundsXML-CSV-Converter Version 0.8 from 21.01.2019");
  puts("Open source command line tool for the FundsXML community");
  puts("Source code is licenced under the MIT licence on GitHub: https://github.com/peterraf/csv-xml-converter");
  puts("Copyright (c) 2019 by Peter Raffelsberger (peter@raffelsberger.net)");
  puts("");

  // Parameters:
  //
  // convert -conversion csv2xml -input input.csv -mapping mapping.csv -output result.xml -errors errors.csv
  // convert -c c2x -i input.csv -m mapping.csv -o result.xml -e errors.csv
  //
  // convert -conversion xml2csv -input input.xml -mapping mapping.csv -template template.csv -output result.csv -errors errors.csv
  // convert -c x2c -i input.xml -m mapping.csv -t template.csv -o result.csv -e errors.csv
  //
  // convert -c x2c -i nav-input.xml -m nav-mapping.csv -t nav-template.csv -o nav-output.csv -e nav-errors.csv
  //
  // EMT mappings:
  // convert -c c2x -i emt-input.csv -m emt-mapping.csv -o emt-output.xml -e emt-errors.csv
  // convert -c x2c -i emt-input.xml -m emt-mapping.csv -t emt-template.csv -o emt-output.csv -e emt-errors.csv
  //
  // BATCH CONVERSIONS:
  //
  // convert -conversion csv2xml -input c:\csv-xml-converter\inputfiles\*.csv -mapping mapping.csv -output c:\csv-xml-converter\outputfiles -errors error -log log.csv -processed processed -counter counter
  // convert -c c2x -i input\*.csv -m holdings-mapping.csv -o output\*.xml -e error -l log.csv -p processed -r counter
  //
  // convert -conversion xml2csv -iinput input\*.xml -mapping holdings-mapping.csv -template holdings-template.csv -ooutput output\*.csv -error error -log log.csv -processed processed -counter counter
  // convert -c x2c -i input\*.xml -m holdings-mapping.csv -t holdings-template.csv -o output\*.csv -e error -l log.csv -p processed -r counter
  //

  // parse command for parameters
  for (i = 1; i < argc; i++)
  {
    pcParameter = argv[i];
    if (*pcParameter++ == '-' && i < argc - 1)
    {
      pcContent = argv[++i];
      if (strlen(pcContent) > MAX_FILE_NAME_LEN) {
        printf("Content of command line parameter '%s' is too long (maximum length is %d characters)\n", pcParameter, MAX_FILE_NAME_LEN);
        goto ProcEnd;
      }
      else {
        // conversion direction
        if (stricmp(pcParameter, "CONVERSION") == 0 || stricmp(pcParameter, "C") == 0) {
          if (stricmp(pcContent, "xml2csv") == 0 || stricmp(pcContent, "x2c") == 0)
            strcpy(szConversion, "xml2csv");
        }

        // input file name or directory
        if (stricmp(pcParameter, "INPUT") == 0 || stricmp(pcParameter, "I") == 0)
          strcpy(szInput, pcContent);

        // log file name
        if (stricmp(pcParameter, "LOG") == 0 || stricmp(pcParameter, "L") == 0)
          strcpy(szLogFileName, pcContent);

        // mapping file name
        if (stricmp(pcParameter, "MAPPING") == 0 || stricmp(pcParameter, "M") == 0)
          strcpy(szMappingFileName, pcContent);

        // template file name
        if (stricmp(pcParameter, "TEMPLATE") == 0 || stricmp(pcParameter, "T") == 0)
          strcpy(szTemplateFileName, pcContent);

        // output file name or directory
        if (stricmp(pcParameter, "OUTPUT") == 0 || stricmp(pcParameter, "O") == 0)
          strcpy(szOutput, pcContent);

        // log file name or directory
        if (stricmp(pcParameter, "ERRORS") == 0 || stricmp(pcParameter, "E") == 0)
          strcpy(szError, pcContent);

        // directory for processed files
        if ((stricmp(pcParameter, "PROCESSED") == 0 || stricmp(pcParameter, "P") == 0) && strlen(pcContent) < MAX_PATH_LEN)
          strcpy(szProcessed, pcContent);

        // directory for processed files
        if ((stricmp(pcParameter, "COUNTER") == 0 || stricmp(pcParameter, "R") == 0) && strlen(pcContent) < MAX_PATH_LEN)
          sprintf(szCounterPath, "%s%c", pcContent, cPathSeparator);
      }
    }
  }

  // check existance of log file (and write header if not existing)
  nReturnCode = CheckLogFileHeader(szLogFileName);
  if (nReturnCode != 0)
    goto ProcEnd;

  // log start of processing
  AddLog(szLogFileName, "START", szConversion, "", 0, 0, szInput, szMappingFileName, szTemplateFileName, szOutput, szProcessed, "", "");

  // read mapping file
  printf("Mapping definition: %s\n", szMappingFileName);
  nReturnCode = ReadFieldMappings(szMappingFileName);
  printf("Field mappings loaded: %d\n\n", nFieldMappings);

  if (nReturnCode <= 0) {
    AddLog(szLogFileName, "ERROR", szConversion, szLastError, nErrors, nCsvDataLines, szInput, szMappingFileName, szTemplateFileName, szOutput, szProcessed, szUniqueDocumentID, szError);
    goto ProcEnd;
  }

  // single or multiple file conversions ?
  pStarPos = strchr(szInput, '*');

  if (pStarPos) {
    // multiple file conversion
    ExtractPath(szInputPath, szInput);
    ExtractPath(szOutputPath, szOutput);

    // Source of sample code (for different operating systems): https://faq.cprogramming.com/cgi-bin/smartfaq.cgi?answer=1046380353&id=1044780608
    HANDLE hFileSearch;
    //WIN32_FIND_DATA info;
    WIN32_FIND_DATAA fileSearchInfo;

    printf("\nSearching for input files: %s\n", szInput);
    // WINBASEAPI __out HANDLE WINAPI FindFirstFileA (__in LPCSTR lpFileName, __out LPWIN32_FIND_DATAA lpFindFileData);

    hFileSearch = FindFirstFileA(szInput, &fileSearchInfo);
    if (hFileSearch != INVALID_HANDLE_VALUE) {
      do {
        if ((fileSearchInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
          nFiles++;
          printf("Input file %d: %s\n", nFiles, fileSearchInfo.cFileName);

          sprintf(szInputFileName, "%s%s", szInputPath, fileSearchInfo.cFileName);
          sprintf(szOutputFileName, "%s%s", szOutputPath, fileSearchInfo.cFileName);
					sprintf(szProcessedFileName, "%s%c%s", szProcessed, cPathSeparator, fileSearchInfo.cFileName);
					sprintf(szErrorFileName, "%s%c%s", szError, cPathSeparator, fileSearchInfo.cFileName);

          if (stricmp(szConversion, "csv2xml") == 0) {
            // convert from csv to xml format
            convDir = CSV2XML;
            ReplaceExtension(szOutputFileName, "xml");
            FindUniqueFileName(szOutputFileName);
						FindUniqueFileName(szProcessedFileName);
						ReplaceExtension(szErrorFileName, "csv");  // input file might have different extension like .txt
						FindUniqueFileName(szErrorFileName);

            nReturnCode = ConvertCsvToXml(szInputFileName, szOutputFileName);
						MyMoveFile(szInputFileName, szProcessedFileName);

            AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, nRealCsvDataLines, szInputFileName, szMappingFileName, "", szOutputFileName, szProcessedFileName, szUniqueDocumentID, szErrorFileName);
          }

          if (stricmp(szConversion, "xml2csv") == 0) {
            // convert from csv to xml format
            convDir = XML2CSV;
            ReplaceExtension(szOutputFileName, "csv");
            FindUniqueFileName(szOutputFileName);
						FindUniqueFileName(szProcessedFileName);
						ReplaceExtension(szErrorFileName, "csv");
            FindUniqueFileName(szErrorFileName);

            nReturnCode = ConvertXmlToCsv(szInputFileName, szTemplateFileName, szOutputFileName);
						MyMoveFile(szInputFileName, szProcessedFileName);

            AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, nRealCsvDataLines, szInputFileName, szMappingFileName, szTemplateFileName, szOutputFileName, szProcessedFileName, szUniqueDocumentID, szErrorFileName);
          }

        }
      } while (FindNextFileA(hFileSearch, &fileSearchInfo));

      if (GetLastError() != ERROR_NO_MORE_FILES)
        printf("FindFile error: %u\n", GetLastError());

      FindClose(hFileSearch);
    }

		// log end of processing
		//AddLog(szLogFileName, "END", szConversion, "", 0, 0, szInput, szMappingFileName, szTemplateFileName, szOutput, szProcessed, "", "");

		printf("Input files processed: %d\n", nFiles);

    /*
    // Linux Version:
    //
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
      while ((dir = readdir(d)) != NULL) {
        printf("%s\n", dir->d_name);
      }
      closedir(d);
    }
    */


  }
  else {
    // single file conversion

    // conversion from csv to xml format ?
    if (stricmp(szConversion, "csv2xml") == 0) {
      convDir = CSV2XML;

      // set default input file name
      if (!*szInput)
        strcpy(szInput, "input.csv");

      // set default output file name
      if (!*szOutput)
        strcpy(szOutput, "output.xml");

      // convert from csv to xml format
      strcpy(szErrorFileName, szError);
      nReturnCode = ConvertCsvToXml(szInput, szOutput);
    }

    // conversion from xml to csv format ?
    if (stricmp(szConversion, "xml2csv") == 0) {
      convDir = XML2CSV;

      // set default input file name
      if (!*szInput)
        strcpy(szInput, "input.xml");

      // set default output file name
      if (!*szOutput)
        strcpy(szOutput, "output.csv");

      // convert from csv to xml format
      strcpy(szErrorFileName, szError);
      nReturnCode = ConvertXmlToCsv(szInput, szTemplateFileName, szOutput);
    }
  }

ProcEnd:
  /*
  // used for easier testing
  puts("\nPress <Enter> to continue/close window\n");
  getchar();
  */
	return 0;
}

#ifdef __cplusplus
}
#endif
