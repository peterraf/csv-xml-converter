// CSV-XML-Converter.cpp
/*
CSV-XML-Converter Version 1.05 from 13.06.2021

Command line tool for conversion of data files from CSV to XML and from XML to CSV via mapping definition

Source code is available under the MIT open source licence
on GitHub: https://github.com/peterraf/csv-xml-converter and http://www.xml-tools.net

Written by Peter Raffelsberger (peter@raffelsberger.net)

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
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>  // for Windows
#elif __linux__
#include <dirent.h>  // for Linux
#include <regex.h>
#endif
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

#define MAX_MAPPING_COLUMNS  32
#define MAX_FIELD_MAPPINGS  500
#define MAX_CSV_COLUMNS  500
#define MAX_LOOPS  100
#define MAX_LINKED_CSV_FILES  32

#define MAX_VALUE_SIZE  16384
#define MAX_VALUE_LEN  (MAX_VALUE_SIZE - 1)

#define MAX_NUMBER_VALUE_SIZE  64
#define MAX_NUMBER_VALUE_LEN  (MAX_NUMBER_VALUE_SIZE - 1)

#define MAX_CONDITION_SIZE  1024
#define MAX_CONDITION_LEN  (MAX_CONDITION_SIZE - 1)

#define MY_BUFFER_SIZE  65536
#define MAX_MY_BUFFERS  64

#define MAX_DIGITS  64

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

#define MAX_NODE_ATTRIBUTES  16

#define MAX_ATTR_NAME_SIZE  128
#define MAX_ATTR_NAME_LEN  (MAX_ATTR_NAME_SIZE - 1)

#define MAX_ATTR_VALUE_SIZE  256
#define MAX_ATTR_VALUE_LEN  (MAX_ATTR_VALUE_SIZE - 1)

// allow usage of original stdio functions like strcpy, memcpy, ...
//#pragma warning(suppress : 4996)

// defines and types for Linux

#ifdef __linux__

#define min(a,b) ((a)<(b))?(a):(b)
#define max(a,b) ((a)>(b))?(a):(b)

#define stricmp strcasecmp
#define strnicmp strncasecmp

typedef void* LPVOID;

#endif

// type definitions

typedef char *pchar;
typedef char const *cpchar;

typedef char FileName[MAX_FILE_NAME_SIZE];

typedef enum { CSV2XML, XML2CSV } ConversionDirection;

typedef struct {
  char szName[MAX_ATTR_NAME_SIZE];
  char szValue[MAX_ATTR_VALUE_SIZE];
  cpchar pszXPath;
} AttributeNameValue;

typedef struct {
  int nCount;
  int nMaxCount;
  AttributeNameValue *aAttrNameValue;
} AttributeNameValueList;

typedef struct SimpleCondition {
  cpchar szLeftPart;
  cpchar szOperator;
  cpchar szRightPart;
  pchar szCurrentValue;
} SimpleCondition;

typedef struct ComplexCondition;

typedef struct Condition {
  SimpleCondition *pSimpleCondition;
  ComplexCondition *pComplexCondition;
} Condition;

typedef enum { NO_OP, AND_OP, OR_OP } ConditionOperator;

typedef struct ComplexCondition {
  Condition LeftCondition;
  ConditionOperator op;
  Condition RightCondition;
} ComplexCondition;

typedef Condition *PCondition;
typedef Condition const *CPCondition;
typedef SimpleCondition *PSimpleCondition;
typedef ComplexCondition *PComplexCondition;

typedef struct {
  char cOperation;  // Fix, Var, Map, Line/Loop
  cpchar szContent;  // Constant, Variable, Column Name or XPath
  cpchar szContent2;  // Constant, Variable, Column Name or XPath
  cpchar szAttribute;
  cpchar szShortName;
  bool bMandatory;
  bool bAddEmptyNodes;
  char cType;  // Text, Integer, Number, Date, Boolean
  int nMinLen;
  int nMaxLen;
  cpchar szMinValue;
  cpchar szMaxValue;
  int nMinValue;
  int nMaxValue;
  double fMinValue;
  double fMaxValue;
  cpchar szFormat;
  cpchar szTransform;
  cpchar szDefault;
  pchar szCondition;
  Condition Condition;
  cpchar szMappingFormat;
  cpchar szValue;
} FieldDefinition;

typedef FieldDefinition const *CPFieldDefinition;

typedef struct {
  FieldDefinition csv;
  FieldDefinition xml;
  int nCsvFileIndex;
  int nCsvIndex;
  int nCsvFileIndex2;
  int nCsvIndex2;
  int nSourceMapIndex;
  int nLoopIndex;
  int nRefUniqueLoopIndex;
} FieldMapping;

typedef FieldMapping *PFieldMapping;
typedef FieldMapping const *CPFieldMapping;

typedef struct {
  FileName szFileName;
  int nDataBufferSize;
  char *pDataBuffer;
  int nColumns;
  int nDataLines;
  int nDataFieldsBufferSize;
  pchar *aDataFields;
  bool abColumnQuoted[MAX_CSV_COLUMNS];
  bool abColumnError[MAX_CSV_COLUMNS];
  int nRealDataLines;
  int nLinkedColumnIndex;
  int nLinkedMainColumnIndex;
  int nLinkedMainDataLine;
  cpchar pszLastKeyValue;
  cpchar pszCurrentKeyValue;
  int nMatchingFirstLine;
  int nMatchingLastLine;
  int nCurrentCsvLine;
} LinkedCsvFile;

// global constants

const char szEmptyString[] = "";
const char szCsvDefaultDateFormat[] = "DD.MM.YYYY";
const char szXmlDateFormat[] = "YYYY-MM-DD";
const char szXmlTimestampFormat[] = "YYYY-MM-DDThh:mm:ss";
const char szDefaultBooleanFormat[] = ",true,false,";  // internal version of regular expression "(true,false)"

const char szDigits[] = "0123456789";
const char szSmallLetters[] = "abcdefghijklmnopqrstuvwxyz";
const char szBigLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

const char szOperationFix[] = "FIX";
const char szOperationVar[] = "VAR";
const char szOperationMap[] = "MAP";
const char szOperationChange[] = "CHANGE";
const char szOperationLoop[] = "LOOP";
const char szOperationIf[] = "IF";
const char szOperationUnique[] = "UNIQUE";
const char szOperationAddFile[] = "ADDFILE";
const char szOperationUnknown[] = "?";

// global variables

ConversionDirection convDir;
int nFieldMappings = 0;
FieldMapping aFieldMapping[MAX_FIELD_MAPPINGS];
int nFieldMappingBufferSize = 0;
char *pFieldMappingsBuffer = NULL;

//int nCsvDataBufferSize = 0;
//char *pCsvDataBuffer = NULL;
int nMyBufferUsed = 0;
int nMyBuffers = 0;
pchar apMyBuffer[MAX_MY_BUFFERS];
char cColumnDelimiter = ';';  // default column delimiter is semicolon
char cDecimalPoint = '.';  // default decimal point is point
char szIgnoreChars[8] = " \t";  // default space-like characters to be ignored during parsing a csv line
char szTempConvertNumber[MAX_NUMBER_VALUE_SIZE];
int nLoops = 0;
int anLoopIndex[MAX_LOOPS];
int anLoopSize[MAX_LOOPS];
PFieldMapping apLoopFieldMapping[MAX_LOOPS];

int nLinkedCsvFiles = 0;
LinkedCsvFile aLinkedCsvFile[MAX_LINKED_CSV_FILES];
/*
int nCsvDataColumns = 0;
int nCsvDataLines = 0;
int nCsvDataFieldsBufferSize = 0;
pchar *aCsvDataFields = NULL;
int nRealCsvDataLines = 0;
int nCurrentCsvLine = 0;
*/

char szRootNodeName[MAX_NODE_NAME_SIZE] = "FundsXML4";  // default name root node name
AttributeNameValueList RootAttrNameValueList;
char szCsvHeader[MAX_HEADER_SIZE] = "";
char szCsvHeader2[MAX_HEADER_SIZE] = "";
char szLineBuffer[MAX_LINE_SIZE];
char szFieldValueBuffer[MAX_LINE_SIZE];
char szUniqueDocumentID[MAX_UNIQUE_DOCUMENT_ID_SIZE];
char szCounterPath[MAX_FILE_NAME_SIZE] = "";
char szLastError[MAX_ERROR_MESSAGE_SIZE];
char szLastFieldMappingError[MAX_ERROR_MESSAGE_SIZE];
char szErrorFileName[MAX_FILE_NAME_SIZE];
FILE *pErrorFile = NULL;
int nErrors = 0;
char szMappingErrorFileName[MAX_FILE_NAME_SIZE];
FILE *pMappingErrorFile = NULL;
int nMappingErrors = 0;
pchar pszLastErrorPos = NULL;
xmlDocPtr pXmlDoc = NULL;
bool bTrace = false; //true;
char cPathSeparator = '\\';  // change to '/' for linux

//--------------------------------------------------------------------------------------------------------

bool ConvertNumber(cpchar szValue, char cType, int *pnValue, double *pfValue);

//--------------------------------------------------------------------------------------------------------

LPVOID MyGetMemory(int size)
{
  pchar pResult;

  if (size <= 0)
    return NULL;

  if (nMyBuffers == 0 || nMyBufferUsed + size > MY_BUFFER_SIZE) {
    if (nMyBuffers >= MAX_MY_BUFFERS)
      return NULL;
    pResult = (pchar)malloc(MY_BUFFER_SIZE);
    if (pResult == NULL)
      return NULL;
    apMyBuffer[nMyBuffers++] = pResult;
    nMyBufferUsed = 0;
  }
  else
    pResult = apMyBuffer[nMyBuffers-1];

  pResult += nMyBufferUsed;
  nMyBufferUsed += size;

  return pResult;
}

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

void RemoveLastChar(char *pszString, char cLastChar)
{
  char *pLastChar = pszString + strlen(pszString) - 1;

  if (*pLastChar == cLastChar)
    *pLastChar = '\0';
}

//--------------------------------------------------------------------------------------------------------

int SplitString(char *pszString, char cSeparator, pchar *aszValue, int nMaxValues, cpchar pszIgnoreChars)
{
  int nValues = 0;
  char *pPos = pszString;
  char *pSep = strchr(pszString, cSeparator);

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

  //errno_t error_code = fopen_s(&pFile, szFileName, "r");
  pFile = fopen(szFileName, "r");

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

cpchar GetOperationLongName(char cOperation)
{
  cpchar pszResult = szOperationUnknown;

  if (cOperation == 'F') pszResult = szOperationFix;
  if (cOperation == 'V') pszResult = szOperationVar;
  if (cOperation == 'M') pszResult = szOperationMap;
  if (cOperation == 'C') pszResult = szOperationChange;
  if (cOperation == 'L') pszResult = szOperationLoop;
  if (cOperation == 'I') pszResult = szOperationIf;
  if (cOperation == 'U') pszResult = szOperationUnique;
  if (cOperation == 'A') pszResult = szOperationAddFile;

  return pszResult;
}

//--------------------------------------------------------------------------------------------------------

void AddLog(cpchar szFileName, cpchar szLogType, cpchar szConversion, cpchar szLastError, int nErrors, int nCsvDataLines, cpchar szInputFileName, cpchar szMappingFileName, cpchar szTemplateFileName, 
            cpchar szOutputFileName, cpchar szProcessedFileName, cpchar szUniqueDocumentID, cpchar szErrorFileName)
{
  FILE *pFile = NULL;
  //errno_t error_code;
  char szNow[20];

  if (*szFileName) {
    //error_code = fopen_s(&pFile, szFileName, "a");
    pFile = fopen(szFileName, "a");
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
      //printf("Cannot append to file %s (error %d)\n", szFileName, error_code);
      printf("Cannot append to file %s\n", szFileName);
  }
}

//--------------------------------------------------------------------------------------------------------

int CheckLogFileHeader(cpchar szFileName)
{
  FILE *pFile = NULL;
  //errno_t error_code;

  if (!FileExists(szFileName)) {
    //error_code = fopen_s(&pFile, szFileName, "w");
    pFile = fopen(szFileName, "w");
    if (pFile) {
      fputs("TIMESTAMP;LOG_TYPE;CONVERSION;ERROR;CONTENT_ERRORS;CSV_DATA_RECORDS;INPUT_FILE_NAME;MAPPING_FILE_NAME;TEMPLATE_FILE_NAME;OUTPUT_FILE_NAME;PROCESSED_FILE_NAME;UNIQUE_DOCUMENT_ID;ERROR_FILE_NAME\n", pFile);
      fclose(pFile);
      return 0;
    }
    //sprintf(szLastError, "Cannot create log file %s (error %d)", szFileName, error_code);
    sprintf(szLastError, "Cannot create log file %s", szFileName);
    puts(szLastError);
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------------

int LogMappingError(int nMapIndex, cpchar szOperation, cpchar szColumnName, cpchar szError)
{
  int nReturnCode = 0;
  //errno_t error_code;

  nMappingErrors++;

  printf("Error in mapping line %d: operation \"%s\", column \"%s\" %s\n", nMapIndex + 1, szOperation, szColumnName, szError);

  if (!pMappingErrorFile) {
    // open file in write mode
    //error_code = fopen_s(&pMappingErrorFile, szMappingErrorFileName, "w");
    pMappingErrorFile = fopen(szMappingErrorFileName, "w");
    if (!pMappingErrorFile) {
      //printf("Cannot open file '%s' (error code %d)\n", szMappingErrorFileName, error_code);
      printf("Cannot open file '%s'\n", szMappingErrorFileName);
      return -2;
    }

    // write header of error file
    nReturnCode = fputs("LINE;OPERATION;COLUMN_NAME;ERROR\n", pMappingErrorFile);
  }

  nReturnCode = fprintf(pMappingErrorFile, "%d;\"%s\";\"%s\";\"%s\"\n", nMapIndex + 1, szOperation, szColumnName, szError);

  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int LogXmlError(int nCsvFileIndex, int nLine, int nColumnIndex, cpchar szColumnName, cpchar szXPath, cpchar szValue, cpchar szError)
{
  int nReturnCode = 0;
  //errno_t error_code;
  LinkedCsvFile *pLinkedCsvFile;

  if (nCsvFileIndex < 0 || nCsvFileIndex >= nLinkedCsvFiles)
    return 0;  // invalid csv file index

  pLinkedCsvFile = aLinkedCsvFile + nCsvFileIndex;

  if (nColumnIndex < 0 || nColumnIndex >= pLinkedCsvFile->nColumns)
    return 0;  // invalid column index

  if (pLinkedCsvFile->abColumnError[nColumnIndex])
    return 0;  // report only first error for each column

  nErrors++;

  if (convDir == CSV2XML)
    pLinkedCsvFile->abColumnError[nColumnIndex] = true;

  if (!pErrorFile) {
    // open file in write mode
    //error_code = fopen_s(&pErrorFile, szErrorFileName, "w");
    pErrorFile = fopen(szErrorFileName, "w");
    if (!pErrorFile) {
      //printf("Cannot open file '%s' (error code %d)\n", szErrorFileName, error_code);
      printf("Cannot open file '%s'\n", szErrorFileName);
      return -2;
    }

    // write header of error file
    nReturnCode = fputs("FILE;LINE;COLUMN_NR;COLUMN_NAME;XPATH;VALUE;ERROR\n", pErrorFile);
  }

  if (*szXPath)
    nReturnCode = fprintf(pErrorFile, "%d;%d;%d;\"%s\";\"%s\";\"%s\";\"%s\"\n", nCsvFileIndex + 1, nLine, nColumnIndex + 1, szColumnName, szXPath, szValue, szError);
  else
    nReturnCode = fprintf(pErrorFile, "%d;%d;%d;\"%s\";;\"%s\";\"%s\"\n", nCsvFileIndex + 1, nLine, nColumnIndex + 1, szColumnName, szValue, szError);

  return nReturnCode;
}

//--------------------------------------------------------------------------------------------------------

int LogCsvError(int nCsvFileIndex, int nLine, int nColumnIndex, cpchar szColumnName, cpchar szValue, cpchar szError)
{
  return LogXmlError(nCsvFileIndex, nLine, nColumnIndex, szColumnName, szEmptyString, szValue, szError);
}

//--------------------------------------------------------------------------------------------------------

int MyLoadFile(cpchar pszFileName, char *pBuffer, int nMaxSize)
{
  int nBytesRead = 0;
  FILE *pFile = NULL;
  //errno_t error_code;

  //error_code = fopen_s(&pFile, pszFileName, "r");
  pFile = fopen(pszFileName, "r");
  if (!pFile) {
    //sprintf(szLastError, "Cannot open file '%s' (error code %d)", pszFileName, error_code);
    sprintf(szLastError, "Cannot open file '%s'", pszFileName);
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
  //errno_t error_code;

  // open file
  //error_code = fopen_s(&pFile, pszFileName, "w");
  pFile = fopen(pszFileName, "w");
  if (!pFile) {
    //sprintf(szLastError, "Cannot open file '%s' (error code %d)", pszFileName, error_code);
    sprintf(szLastError, "Cannot open file '%s'", pszFileName);
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

bool EvaluateCondition(xmlNodePtr pNode, CPCondition pCondition)
{
  bool bResult = false, bResult2;
	cpchar pszAttributeValue;
  int nCompareResult;

  if (pCondition->pComplexCondition) {
    bResult = EvaluateCondition(pNode, &pCondition->pComplexCondition->LeftCondition);
    bResult2 = EvaluateCondition(pNode, &pCondition->pComplexCondition->RightCondition);
    if (pCondition->pComplexCondition->op == AND_OP)
      bResult = bResult && bResult2;
    if (pCondition->pComplexCondition->op == OR_OP)
      bResult = bResult || bResult2;
  }
  
  if (pCondition->pSimpleCondition && *pCondition->pSimpleCondition->szLeftPart == '@') {
    pszAttributeValue = (cpchar)xmlGetProp(pNode, (const xmlChar *)pCondition->pSimpleCondition->szLeftPart + 1);
    if (!pszAttributeValue)
      pszAttributeValue = szEmptyString;

    nCompareResult = strcmp(pszAttributeValue, pCondition->pSimpleCondition->szCurrentValue);

    if (strcmp(pCondition->pSimpleCondition->szOperator, "=") == 0)
      bResult = (nCompareResult == 0);
    if (strcmp(pCondition->pSimpleCondition->szOperator, "!=") == 0)
      bResult = (nCompareResult != 0);
    if (strcmp(pCondition->pSimpleCondition->szOperator, "<") == 0)
      bResult = (nCompareResult < 0);
    if (strcmp(pCondition->pSimpleCondition->szOperator, "<=") == 0)
      bResult = (nCompareResult <= 0);
    if (strcmp(pCondition->pSimpleCondition->szOperator, ">") == 0)
      bResult = (nCompareResult > 0);
    if (strcmp(pCondition->pSimpleCondition->szOperator, ">=") == 0)
      bResult = (nCompareResult >= 0);
  }

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

xmlNodePtr GetNode(xmlNodePtr pParentNode, const char *pXPath, bool bCreateIfNotFound = false, AttributeNameValueList *pAttributeNameValueList = NULL, CPCondition pCondition = NULL)
{
  char szNodeName[MAX_NODE_NAME_SIZE];
  cpchar pChildXPath = NULL;
	cpchar pszAttributeValue = NULL;
  cpchar pSlash = NULL;
  char *pPos = NULL;
  xmlNodePtr pChildNode = NULL;
  xmlNodePtr pResult = NULL;
  CPCondition pCleanCondition = NULL;
  int nRequestedNodeIndex = 1;
  int nFoundNodeIndex = 0;
  int i;
  bool bMatch;

  // real condition ?
  if (pCondition && (pCondition->pComplexCondition || pCondition->pSimpleCondition))
    pCleanCondition = pCondition;

  // Extract node name from xpath (first position of slash)
  pSlash = strchr(pXPath, '/');
  if (pSlash == NULL)
    mystrncpy(szNodeName, pXPath, MAX_NODE_NAME_SIZE);
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

	if (((pAttributeNameValueList && pAttributeNameValueList->nCount > 0) || pCleanCondition) && pChildXPath == NULL) {
		// search for right node with node name and conditions for attribute values
		while (pChildNode != NULL) {
			if (strcmp((cpchar)pChildNode->name, szNodeName) == 0) {
				nFoundNodeIndex++;
        bMatch = true;

        if (pCleanCondition) {
          bMatch = EvaluateCondition(pChildNode, pCleanCondition);
        }
        else {
          for (i = 0; i < pAttributeNameValueList->nCount && bMatch; i++) {
				    // xmlChar *xmlGetProp (const xmlNode *node, const xmlChar *name)
				    pszAttributeValue = (cpchar)xmlGetProp(pChildNode, (const xmlChar *)pAttributeNameValueList->aAttrNameValue[i].szName);
            if (!pszAttributeValue)
              pszAttributeValue = szEmptyString;
            if (strcmp(pszAttributeValue, pAttributeNameValueList->aAttrNameValue[i].szValue) != 0)
              bMatch = false;
          }
        }

				if (bMatch)
					return pChildNode;  // node with matching attribute value found
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
						return GetNode(pChildNode, pChildXPath, bCreateIfNotFound, pAttributeNameValueList, pCleanCondition);
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

	if (pAttributeNameValueList && pAttributeNameValueList->nCount > 0 && pChildXPath == NULL) {
		// create <szNodeName> with requested attribute value
		pChildNode = xmlNewChild(pParentNode, NULL, (const xmlChar *)szNodeName, NULL);

    for (i = 0; i < pAttributeNameValueList->nCount; i++) {
		  // xmlAttrPtr	xmlSetProp(xmlNodePtr node, const xmlChar *name, const xmlChar *value)
		  xmlSetProp(pChildNode, (const xmlChar *)pAttributeNameValueList->aAttrNameValue[i].szName, (const xmlChar *)pAttributeNameValueList->aAttrNameValue[i].szValue);
    }
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
    pResult = GetNode(pChildNode, pChildXPath, bCreateIfNotFound, pAttributeNameValueList, pCleanCondition);
  else
    pResult = pChildNode;

  return pResult;
}
// end of function "GetNode"

//--------------------------------------------------------------------------------------------------------

xmlNodePtr GetCreateNode(xmlNodePtr pParentNode, const char *pXPath, AttributeNameValueList *pAttributeNameValueList = NULL, CPCondition pCondition = NULL)
{
  return GetNode(pParentNode, pXPath, true, pAttributeNameValueList, pCondition);
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
    mystrncpy(szNodeName, pXPath, MAX_NODE_NAME_SIZE);
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

int GetNodeTextValue(xmlNodePtr pParentNode, cpchar pXPath, cpchar *ppszValue, CPCondition pCondition = NULL)
{
  int nError = 0;

  xmlNodePtr pNode = GetNode(pParentNode, pXPath, false, NULL, pCondition);

  if (pNode)
    // xmlChar *xmlNodeGetContent	(const xmlNode * cur)
    *ppszValue = (cpchar)xmlNodeGetContent(pNode);
  else
    nError = 1;

  return nError;
}

//--------------------------------------------------------------------------------------------------------

xmlNodePtr SetNodeValue(xmlDocPtr pDoc, xmlNodePtr pParentNode, const char * pXPath, const char *pValue, FieldMapping const *pFieldMapping,
                        AttributeNameValueList *pAttributeNameValueList = NULL, CPCondition pCondition = NULL)
{
  xmlNodePtr pRealParentNode = NULL;
  xmlNodePtr pNode = NULL;
  /*
  documentation: http://xmlsoft.org/html/libxml-tree.html
	// xmlNewProp() creates attributes, which is "attached" to an node. It returns xmlAttrPtr, which isn't used here.
	node = xmlNewChild(root_node, NULL, BAD_CAST "node3", BAD_CAST "this node has attributes");
	xmlNewProp(node, BAD_CAST "attribute", BAD_CAST "yes");
  */

  if (*pValue || pFieldMapping->xml.bMandatory /*|| pFieldMapping->xml.bAddEmptyNodes*/)
  {
    if (pParentNode)
      pRealParentNode = pParentNode;
    else
      pParentNode = xmlDocGetRootElement(pDoc);

    xmlNodePtr pNode = GetCreateNode(pParentNode, pXPath, pAttributeNameValueList, pCondition);
    //xmlNodePtr pNode = GetCreateNode(pParentNode, pXPath, pszConditionAttributeName, pszConditionAttributeValue);

    if (strlen(pFieldMapping->xml.szAttribute) == 0) {
      if (*pValue)
        xmlNodeSetContent(pNode, xmlEncodeSpecialChars(pDoc, (const xmlChar *)pValue));
    }
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
  for (pPos = szLine; *pPos; pPos++) {
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
      *pDest++ = *pSource;
    pSource++;
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
  // Sample regular expressions:
  // Pattern with brackets: "[A-Z]{3}", "[0-9a-zA-Z]{18}[0-9]{2}"
  // Pattern with dots: "..[B-C]."
  // Enumeration: "(AIF,UCITS)"
  cpchar pStrPos = pszString;
  cpchar pRegexPos = pszRegex;
  //cpchar pElemEnd = NULL;
  char szValidChars[256] = "";
  char szSearch[MAX_VALUE_SIZE+2];
  bool bResult = true;  // default
  bool bMatch;

  if (IsEmptyString(pszRegex))
    return bResult;

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
  else
  if (strstr(pszRegex, ".[") != NULL || strstr(pszRegex, "].") != NULL) {
    // Samples for patterns with dots: "..[A]." or "..[B-C]."
    while (*pRegexPos && *pStrPos && bResult) {
      if (*pRegexPos == '[') {
        bMatch = false;
        pRegexPos++;  // skip '['
        while (*pRegexPos && *pRegexPos != ']' && !bMatch) {
          if (pRegexPos[1] == '-' && pRegexPos[2]) {
            bMatch = (*pStrPos >= *pRegexPos && *pStrPos <= pRegexPos[2]);
            pRegexPos += 3;
          }
          else {
            bMatch = (*pStrPos == *pRegexPos);
            pRegexPos++;
          }
        }
        while (*pRegexPos && *pRegexPos != ']')
          pRegexPos++;
        if (*pRegexPos != ']')
          bMatch = false; // error in regular expression (closing bracket missing)
        bResult = bMatch;
      }
      else
        bResult = (*pRegexPos == '.');
      pRegexPos++;
      pStrPos++;
    }
  }
  else
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

  if (pFieldMapping->csv.nMaxLen > 0 && (int)strlen(pszString) > pFieldMapping->csv.nMaxLen)
    return false;  // string is too long

  if (pFieldMapping->csv.nMinLen > 0 && (int)strlen(pszString) < pFieldMapping->csv.nMinLen)
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

  if ((int)strlen(pszSourceValue) > nMaxLen)
    return -1;  // destination value buffer is too small
 
  if (*pSourceValue == '+')
    pSourceValue++;  // remove plus sign at the start

  CopyIgnoreString(pszDestValue, pSourceValue, " ");

  if (*pDestValue == '-')
    pDestValue++;  // skip minus sign at the start

  if (strlen(pDestValue) > MAX_DIGITS) {
    sprintf(szLastFieldMappingError, "Number too long (maximum digits %d)", MAX_DIGITS);
    return 2;
  }
  if (!ValidChars(pDestValue, szDigits)) {
    strcpy(szLastFieldMappingError, "Invalid number (invalid characters found)");
    return 3;
  }

  return nErrorCode;
}

//--------------------------------------------------------------------------------------------------------

int CheckMinMaxValues(cpchar pszValue, CPFieldDefinition pFieldDefinition)
{
  int nErrorCode = 0;
  int nValue;
  double fValue;

  if (!IsEmptyString(pFieldDefinition->szMinValue) || !IsEmptyString(pFieldDefinition->szMaxValue)) {
    ConvertNumber(pszValue, pFieldDefinition->cType, &nValue, &fValue);

    /*
    if (pFieldDefinition->cType == 'I') {
      printf("CheckMinMaxValues '%s' %d", pszValue, nValue);
      if (!IsEmptyString(pFieldDefinition->szMinValue))
        printf(" Min=%s/%d", pFieldDefinition->szMinValue, pFieldDefinition->nMinValue);
      if (!IsEmptyString(pFieldDefinition->szMaxValue))
        printf(" Max=%s/%d", pFieldDefinition->szMaxValue, pFieldDefinition->nMaxValue);
      puts("\n");
    }
    if (pFieldDefinition->cType == 'N') {
      printf("CheckMinMaxValues '%s' %lf", pszValue, fValue);
      if (!IsEmptyString(pFieldDefinition->szMinValue))
        printf(" Min=%s/%lf", pFieldDefinition->szMinValue, pFieldDefinition->fMinValue);
      if (!IsEmptyString(pFieldDefinition->szMaxValue))
        printf(" Max=%s/%lf", pFieldDefinition->szMaxValue, pFieldDefinition->fMaxValue);
      puts("\n");
    }
    */

    if (pFieldDefinition->cType == 'I'/*INTEGER*/) {
      if (!IsEmptyString(pFieldDefinition->szMinValue) && nValue < pFieldDefinition->nMinValue) {
        sprintf(szLastFieldMappingError, "Integer value below limit %d", pFieldDefinition->nMinValue);
        return 4;
      }
      if (!IsEmptyString(pFieldDefinition->szMaxValue) && nValue > pFieldDefinition->nMaxValue) {
        sprintf(szLastFieldMappingError, "Integer value above limit %d", pFieldDefinition->nMaxValue);
        return 4;
      }
    }

    if (pFieldDefinition->cType == 'N'/*NUMBER*/) {
      if (!IsEmptyString(pFieldDefinition->szMinValue) && fValue < pFieldDefinition->fMinValue) {
        sprintf(szLastFieldMappingError, "Number value below limit %lf", pFieldDefinition->fMinValue);
        return 4;
      }
      if (!IsEmptyString(pFieldDefinition->szMaxValue) && fValue > pFieldDefinition->fMaxValue) {
        sprintf(szLastFieldMappingError, "Number value above limit %lf", pFieldDefinition->fMaxValue);
        return 4;
      }
    }
  }

  return nErrorCode;
}
// end of function "CheckMinMaxValues"

//--------------------------------------------------------------------------------------------------------

int MapNumberFormat(cpchar pszSourceValue, char cSourceDecimalPoint, cpchar pszSourceFormat, pchar pszDestValue, char cDestDecimalPoint, cpchar pszDestFormat, int nMaxLen)
{
  int nErrorCode = 0;
  char szIgnoreChars[3];
  cpchar pSourceValue = pszSourceValue;
  pchar pDestValue = pszDestValue;
  char *pPos = NULL;

  if ((int)strlen(pszSourceValue) > nMaxLen)
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
    if (strlen(pDestValue) > MAX_DIGITS) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d before decimal point)", MAX_DIGITS);
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
    if (strlen(pDestValue) > MAX_DIGITS) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d after decimal point)", MAX_DIGITS);
      return 2;
    }
    if (!ValidChars(pDestValue, szDigits)) {
      sprintf(szLastFieldMappingError, "Invalid number (invalid characters found, decimal point is '%c')", cSourceDecimalPoint);
      return 3;
    }
  }
  else {
    // check integer number
    if (strlen(pDestValue) > MAX_DIGITS) {
      sprintf(szLastFieldMappingError, "Number too long (maximum digits %d)", MAX_DIGITS);
      return 2;
    }
    if (!ValidChars(pDestValue, szDigits)) {
      sprintf(szLastFieldMappingError, "Invalid number (invalid characters found, decimal point is '%c')", cSourceDecimalPoint);
      return 3;
    }
  }

  return nErrorCode;
}
// end of function "MapNumberFormat"

//--------------------------------------------------------------------------------------------------------

int MapDateFormat(cpchar pszSourceValue, cpchar pszSourceFormat, pchar pszDestValue, cpchar pszDestFormat)
{
  int nErrorCode = 0;
  int nDay = 1;
  int nMonth = 1;
  int nYear = 2000;
  int nHour = 0;
  int nMinute = 0;
  int nSecond = 0;
  cpchar pPos = NULL;
  char szDay[3];
  char szMonth[3];
  char szYear[5];
  char szHour[3];
  char szMinute[3];
  char szSecond[3];

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

  pPos = strstr(pszSourceFormat, "hh");
  if (pPos) {
    memcpy(szHour, pszSourceValue + (pPos - pszSourceFormat), 2);
    szHour[2] = '\0';
    nHour = atoi(szHour);
    if (nHour < 0) { nHour = 0; nErrorCode = 2; }
    if (nHour > 23) { nHour = 0; nErrorCode = 2; }
  }

  pPos = strstr(pszSourceFormat, "mm");
  if (pPos) {
    memcpy(szMinute, pszSourceValue + (pPos - pszSourceFormat), 2);
    szMinute[2] = '\0';
    nMinute = atoi(szMinute);
    if (nMinute < 0) { nMinute = 0; nErrorCode = 2; }
    if (nMinute > 59) { nMinute = 0; nErrorCode = 2; }
  }

  pPos = strstr(pszSourceFormat, "ss");
  if (pPos) {
    memcpy(szSecond, pszSourceValue + (pPos - pszSourceFormat), 2);
    szSecond[2] = '\0';
    nSecond = atoi(szSecond);
    if (nSecond < 0) { nSecond = 0; nErrorCode = 2; }
    if (nSecond > 59) { nSecond = 0; nErrorCode = 2; }
  }

  // check delimiters
  for (int i = strlen(pszSourceFormat) - 1; i >= 0; i--)
    if (!strchr("DMYhms", pszSourceFormat[i]) && pszSourceFormat[i] != pszSourceValue[i])
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

  pPos = strstr(pszDestFormat, "hh");
  if (pPos) {
    sprintf(szHour, "%02d", nHour);
    memcpy(pszDestValue + (pPos - pszDestFormat), szHour, 2);
  }

  pPos = strstr(pszDestFormat, "mm");
  if (pPos) {
    sprintf(szMinute, "%02d", nHour);
    memcpy(pszDestValue + (pPos - pszDestFormat), szMinute, 2);
  }

  pPos = strstr(pszDestFormat, "ss");
  if (pPos) {
    sprintf(szSecond, "%02d", nHour);
    memcpy(pszDestValue + (pPos - pszDestFormat), szSecond, 2);
  }

  return nErrorCode;
}
// end of function "MapDateFormat"

//--------------------------------------------------------------------------------------------------------

int MapTextFormat(cpchar pszSourceValue, CPFieldDefinition pSourceFieldDef, pchar pszDestValue, CPFieldDefinition pDestFieldDef, int nMaxLen)
{
  char szShortFormat[MAX_FORMAT_ERROR_SIZE+4];
  int nLen = (int)strlen(pszSourceValue);
  int nErrorCode = 0;
  int nAddLen;

  *szLastFieldMappingError = '\0';
  *pszDestValue = '\0';

  //if (strcmp(pDestFieldDef->szContent, "CCY") == 0)
  //  nErrorCode = 0;  // for debugging purposes only!

  if (!*pszSourceValue && !pSourceFieldDef->bMandatory)
    return 0;  // empty value is allowed for optional fields

  if (nLen > nMaxLen)
    return -1;  // destination value buffer is too small

  if (nLen < pSourceFieldDef->nMinLen) {
    sprintf(szLastFieldMappingError, "Content too short (minimum length %d)", pSourceFieldDef->nMinLen);
    return 1;
  }

  if (pSourceFieldDef->nMaxLen >= 0 && nLen > pSourceFieldDef->nMaxLen) {
    sprintf(szLastFieldMappingError, "Content too long (maximum length %d)", pSourceFieldDef->nMaxLen);
    return 2;
  }

  if (!CheckRegex(pszSourceValue, pSourceFieldDef->szFormat)) {
    // source string does not match regular expression
    int nLen2 = strlen(pSourceFieldDef->szFormat);
    if (nLen2 > MAX_FORMAT_ERROR_LEN) {
      nLen2 = MAX_FORMAT_ERROR_LEN;
      mystrncpy(szShortFormat, pSourceFieldDef->szFormat, nLen2 + 1);
      strcpy(szShortFormat + nLen2, " ...");
    }
    else
      strcpy(szShortFormat, pSourceFieldDef->szFormat);

    if (*szShortFormat == ',') {
      *szShortFormat = '(';
      nLen2 = strlen(szShortFormat);
      if (nLen2 > 2 && szShortFormat[nLen2-1] == ',')
        szShortFormat[nLen2-1] = ')';
      sprintf(szLastFieldMappingError, "Value not found in list %s", szShortFormat);
    }
    else
      sprintf(szLastFieldMappingError, "Invalid characters found (does not match %s)", szShortFormat);

    return 3;
  }

  // source value is valid
  if (*pSourceFieldDef->szFormat == ',' && *pDestFieldDef->szFormat == ',' && strcmp(pSourceFieldDef->szFormat, pDestFieldDef->szFormat) != 0) {
    // source and destination format contain list of valid strings --> map content
    cpchar pszSourceFormat = pSourceFieldDef->szFormat + 1;
    cpchar pszDestFormat = pDestFieldDef->szFormat + 1;
    cpchar pszTemp = NULL;
    bool bFound = false;

    while (!bFound && *pszSourceFormat && *pszDestFormat) {
      if (strncmp(pszSourceFormat, pszSourceValue, nLen) == 0 /*&& strlen(pszSourceFormat) > nLen*/ && pszSourceFormat[nLen] == ',') {
        CopyStringTill(pszDestValue, pszDestFormat, ',');
        bFound = true;
      }
      else {
        // goto next string in list of source format
        pszTemp = strchr(pszSourceFormat, ',');
        pszSourceFormat = pszTemp ? pszTemp+1 : strchr(pszSourceFormat, '\0');
        // goto next string in list of destination format
        pszTemp = strchr(pszDestFormat, ',');
        pszDestFormat = pszTemp ? pszTemp+1 : strchr(pszDestFormat, '\0');
      }
    }
  }
  else {
    if (*pDestFieldDef->szMappingFormat) {
      nAddLen = strlen(pDestFieldDef->szMappingFormat) - 3;
      if (nLen + nAddLen > nMaxLen)
        return -1;  // destination value buffer is too small

      if (*pDestFieldDef->szMappingFormat == '\'') {
        // Sample: "'PRAEFIX-'*"
        strcpy(pszDestValue, pDestFieldDef->szMappingFormat + 1);
        strcpy(pszDestValue + nAddLen, pszSourceValue);
      }
      else {
        // Sample: "*'-POSTFIX'"
        strcpy(pszDestValue, pszSourceValue);
        mystrncpy(pszDestValue + nLen, pDestFieldDef->szMappingFormat + 2, nAddLen);
      }
    }
    else {
      // no enumeration, so copy source value simply to destination buffer
      strcpy(pszDestValue, pszSourceValue);
    }
  }

  return nErrorCode;
}
// end of function "MapTextFormat"

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
// end of function "MapBoolFormat"

//--------------------------------------------------------------------------------------------------------

bool ConvertNumber(cpchar szValue, char cType, int *pnValue, double *pfValue)
{
  int i, nErrorCode;
  int nValue;
  double fValue;
  bool bOK = FALSE;

  *szLastError = '\0';

  if (cType == 'I'/*INTEGER*/) {
    nErrorCode = MapIntFormat(szValue, NULL, szTempConvertNumber, NULL, 11);
    if (nErrorCode == 0) {
      i = sscanf(szValue, "%d", &nValue);
      if (i == 0)
        nErrorCode = 3;
    }
    if (nErrorCode > 0)
      strcpy(szLastError, "Invalid integer value");
    else {
      *pnValue = nValue;
      bOK = TRUE;
    }
  }

  if (cType == 'N'/*NUMBER*/) {
    nErrorCode = MapNumberFormat(szValue, cDecimalPoint, NULL, szTempConvertNumber, '.', NULL, MAX_NUMBER_VALUE_LEN);
    if (nErrorCode == 0) {
      i = sscanf(szValue, "%lf", &fValue);
      if (i == 0)
        nErrorCode = 3;
    }
    if (nErrorCode > 0)
      strcpy(szLastError, "Invalid number value");
    else {
      *pfValue = fValue;
      bOK = TRUE;
    }
  }

  return bOK;
}
// end of function "ConvertNumber"

//--------------------------------------------------------------------------------------------------------

int MapCsvToXmlValue(FieldMapping const *pFieldMapping, cpchar pCsvValue, pchar pXmlValue, int nMaxLen)
{
  int nErrorCode = 0;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  cpchar pszFormat = pFieldMapping->xml.szFormat;
  LinkedCsvFile *pLinkedCsvFile = aLinkedCsvFile + pFieldMapping->nCsvFileIndex;

  // mandatory field without content ?
  if (!*pCsvValue && pFieldMapping->csv.bMandatory) {
    LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, "Mandatory field empty");
    return 1;
  }

  // optional field without content ?
  if (!*pCsvValue && !pFieldMapping->csv.bMandatory)
    return 0;  // nothing to do

  if (pFieldMapping->csv.cType == 'B'/*BOOLEAN*/ && pFieldMapping->xml.cType == 'B'/*BOOLEAN*/) {
    // map boolean value
    nErrorCode = MapBoolFormat(pCsvValue, &pFieldMapping->csv, pXmlValue, &pFieldMapping->xml);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'D'/*DATE*/ && pFieldMapping->xml.cType == 'D'/*DATE*/) {
    // map date value
    if ((int)strlen(pszFormat) <= nMaxLen)
      nErrorCode = MapDateFormat(pCsvValue, pFieldMapping->csv.szFormat, pXmlValue, pszFormat);
    else
      nErrorCode = -1;

    if (nErrorCode > 0) {
      sprintf(szErrorMessage, "Invalid date (does not match '%s')", pFieldMapping->csv.szFormat);
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, szErrorMessage);
    }
  }

  if (pFieldMapping->csv.cType == 'I'/*INTEGER*/ && pFieldMapping->xml.cType == 'I'/*INTEGER*/) {
    nErrorCode = MapIntFormat(pCsvValue, pFieldMapping->csv.szFormat, pXmlValue, pszFormat, nMaxLen);
    if (nErrorCode == 0)
      nErrorCode = CheckMinMaxValues(pXmlValue, &pFieldMapping->csv);
    if (nErrorCode > 0) {
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, szLastFieldMappingError);
      *pXmlValue = '\0';
    }
  }

  if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->xml.cType == 'N'/*NUMBER*/) {
    nErrorCode = MapNumberFormat(pCsvValue, cDecimalPoint, pFieldMapping->csv.szFormat, pXmlValue, '.', pszFormat, nMaxLen);
    if (nErrorCode == 0)
      nErrorCode = CheckMinMaxValues(pXmlValue, &pFieldMapping->csv);
    if (nErrorCode > 0) {
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, szLastFieldMappingError);
      *pXmlValue = '\0';
    }
  }

  if (pFieldMapping->csv.cType == 'T'/*TEXT*/ && pFieldMapping->xml.cType == 'T'/*TEXT*/) {
    nErrorCode = MapTextFormat(pCsvValue, &pFieldMapping->csv, pXmlValue, &pFieldMapping->xml, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pFieldMapping->xml.szContent, pCsvValue, szLastFieldMappingError);
  }

  return nErrorCode;
}
// end of function "MapCsvToXmlValue"

//--------------------------------------------------------------------------------------------------------

int MapXmlToCsvValue(FieldMapping const *pFieldMapping, cpchar pXmlValue, cpchar pXPath, pchar pCsvValue, int nMaxLen)
{
  int nErrorCode = 0;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  cpchar pszFromFormat = pFieldMapping->xml.szFormat;
  cpchar pszToFormat = pFieldMapping->csv.szFormat;
  LinkedCsvFile *pLinkedCsvFile = aLinkedCsvFile + pFieldMapping->nCsvFileIndex;

  // mandatory field without content ?
  if (!*pXmlValue && pFieldMapping->xml.bMandatory) {
    LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, "Mandatory field missing or empty");
    return 1;
  }

  if (pFieldMapping->csv.cType == 'B'/*BOOLEAN*/ && pFieldMapping->xml.cType == 'B'/*BOOLEAN*/) {
    // map boolean value
    nErrorCode = MapBoolFormat(pXmlValue, &pFieldMapping->xml, pCsvValue, &pFieldMapping->csv);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'D'/*DATE*/ && pFieldMapping->xml.cType == 'D'/*DATE*/) {
    // map date value
    if (pszFromFormat == NULL || strlen(pszFromFormat) < 10)
      pszFromFormat = szXmlDateFormat;
    if (pszToFormat == NULL)
      pszToFormat = szCsvDefaultDateFormat;

    if ((int)strlen(pszToFormat) <= nMaxLen)
      nErrorCode = MapDateFormat(pXmlValue, pszFromFormat, pCsvValue, pszToFormat);
    else
      nErrorCode = -1;

    if (nErrorCode > 0) {
      sprintf(szErrorMessage, "Invalid date (does not match '%s')", pszFromFormat);
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szErrorMessage);
    }
  }

  if (pFieldMapping->csv.cType == 'I'/*INTEGER*/ && pFieldMapping->xml.cType == 'I'/*INTEGER*/) {
    // map integer value
    nErrorCode = CheckMinMaxValues(pXmlValue, &pFieldMapping->xml);
    if (nErrorCode == 0)
      nErrorCode = MapIntFormat(pXmlValue, pFieldMapping->xml.szFormat, pCsvValue, pFieldMapping->csv.szFormat, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->xml.cType == 'N'/*NUMBER*/) {
    // map xml number to csv number format
    nErrorCode = CheckMinMaxValues(pXmlValue, &pFieldMapping->xml);
    if (nErrorCode == 0)
      nErrorCode = MapNumberFormat(pXmlValue, '.', pFieldMapping->xml.szFormat, pCsvValue, cDecimalPoint, pszToFormat, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  if (pFieldMapping->csv.cType == 'T'/*TEXT*/ && pFieldMapping->xml.cType == 'T'/*TEXT*/) {
    // map text value
    nErrorCode = MapTextFormat(pXmlValue, &pFieldMapping->xml, pCsvValue, &pFieldMapping->csv, nMaxLen);
    if (nErrorCode > 0)
      LogXmlError(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXPath, pXmlValue, szLastFieldMappingError);
  }

  return nErrorCode;
}
// end of function "MapXmlToCsvValue"

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
  // parse line and returns pointers to field contents in aField array
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
// end of function "GetFields"

//--------------------------------------------------------------------------------------------------------

// OLD VERSION (still in use for csv conditions):
void OldParseCondition(cpchar pszCondition, pchar pszConditionBuffer, pchar *ppszLeftPart, pchar *ppszOperator, pchar *ppszRightPart)
{
  // Parses conditions like "CCY != FUND_CCY" or "48_* = '1'" (mandatory space before and after operator !)
  //   or like "(@fromCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency and @toCcy = Funds/Fund/Currency and @mulDiv = 'D') or (@fromCcy = Funds/Fund/Currency and @toCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency and @mulDiv = 'M')" ?
  // Supported operators: "=", "!=", "<", ">"
  //
  // Input Parameters:
  //   pszCondition .... string containing the condition (e.g. "CCY != FUND_CCY")
  //   pszConditionBuffer .... buffer for content of result strings (must have size MAX_CONDITION_SIZE!)
  // 
  // Result parameters:
  //   *ppszLeftPart .... pointer to the left part of the condition (e.g. "CCY")
  //   *ppszOperator .... pointer to the operator of the condition (e.g. "!=")
  //   *ppszRightPart ... pointer to the right part of the condition (e.g. "FUND_CCY")
  //
  char szOperator[5];
  char *pPos;

  // initialize result parameters
  *ppszLeftPart = NULL;
  *ppszOperator = NULL;
  *ppszRightPart = NULL;

	if (strlen(pszCondition) > MAX_CONDITION_LEN)
		return;

	strcpy(pszConditionBuffer, pszCondition);

  // search for operator !=
  strcpy(szOperator, " != ");
  pPos = strstr(pszConditionBuffer, szOperator);

  if (!pPos) {
    // search for operator =
    strcpy(szOperator, " = ");
    pPos = strstr(pszConditionBuffer, szOperator);
  }

  if (!pPos) {
    // search for operator <
    strcpy(szOperator, " < ");
    pPos = strstr(pszConditionBuffer, szOperator);
  }

  if (!pPos) {
    // search for operator >
    strcpy(szOperator, " > ");
    pPos = strstr(pszConditionBuffer, szOperator);
  }

  if (pPos) {
    // operator found
    *ppszLeftPart = pszConditionBuffer;
    *pPos++ = '\0';
    *ppszOperator = pPos;
    pPos += strlen(szOperator) - 2;
    *pPos++ = '\0';
    *ppszRightPart = pPos;
  }
}

//--------------------------------------------------------------------------------------------------------

int ParseSimpleCondition(pchar pszCondition, PSimpleCondition pSimpleCondition)
{
  // Parses conditions like "CCY != FUND_CCY" or "48_* = '1'" (mandatory space before and after operator !)
  // Supported operators: "=", "!=", "<", ">"
  //
  // Input Parameters:
  //   pszCondition .... string containing the condition (e.g. "CCY != FUND_CCY")
  // 
  // Result parameters:
  //   *ppszLeftPart .... pointer to the left part of the condition (e.g. "CCY")
  //   *ppszOperator .... pointer to the operator of the condition (e.g. "!=")
  //   *ppszRightPart ... pointer to the right part of the condition (e.g. "FUND_CCY")
  //
  char szOperator[5];
  char *pPos;
  int len;

  // initialize result parameters
  pSimpleCondition->szLeftPart = NULL;
  pSimpleCondition->szOperator = NULL;
  pSimpleCondition->szRightPart = NULL;

  // search for operator !=
  strcpy(szOperator, " != ");
  pPos = strstr(pszCondition, szOperator);

  if (!pPos) {
    // search for operator =
    strcpy(szOperator, " = ");
    pPos = strstr(pszCondition, szOperator);
  }

  if (!pPos) {
    // search for operator <
    strcpy(szOperator, " < ");
    pPos = strstr(pszCondition, szOperator);
  }

  if (!pPos) {
    // search for operator >
    strcpy(szOperator, " > ");
    pPos = strstr(pszCondition, szOperator);
  }

  if (!pPos) {
    sprintf(szLastError, "No compare operator found (' < ',' > ',' = ',' != ')");
    pszLastErrorPos = pszCondition;
    return 1;
  }

  // operator found
  pSimpleCondition->szLeftPart = pszCondition;
  *pPos++ = '\0';
  pSimpleCondition->szOperator = pPos;
  pPos += strlen(szOperator) - 2;
  *pPos++ = '\0';
  pSimpleCondition->szRightPart = pPos;

  len = strlen(pPos);
  if (*pPos == '\'' && len >= 2) {
    // fill szCurrentValue with unquoted right part of condition (string content)
    pSimpleCondition->szCurrentValue = (pchar)MyGetMemory(len-1);
    if (pSimpleCondition->szCurrentValue)
      mystrncpy(pSimpleCondition->szCurrentValue, pPos + 1, len-1);
  }
  else
    pSimpleCondition->szCurrentValue = NULL;

  return 0;
}
// end of function "ParseSimpleCondition"

//--------------------------------------------------------------------------------------------------------

PComplexCondition GetNewComplexCondition()
{
  PComplexCondition pComplexCondition = (PComplexCondition)MyGetMemory(sizeof(ComplexCondition));

  if (pComplexCondition) {
    pComplexCondition->LeftCondition.pComplexCondition = NULL;
    pComplexCondition->LeftCondition.pSimpleCondition = NULL;
    pComplexCondition->op = NO_OP;
    pComplexCondition->RightCondition.pComplexCondition = NULL;
    pComplexCondition->RightCondition.pSimpleCondition = NULL;
  }

  return pComplexCondition;
}

//--------------------------------------------------------------------------------------------------------

int ParseCondition(pchar pszCondition, PCondition pCondition, pchar *ppNext = NULL)
{
  // Parses conditions like "CCY != FUND_CCY" or "48_* = '1'" (mandatory space before and after operator !)
  //   or like "((@fromCcy = xpath1) and (@toCcy = xpath2) and (@mulDiv = 'D')) or ((@fromCcy = xpath3) and (@toCcy = xpath4) and (@mulDiv = 'M'))"
  //   or like "((@fromCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@toCcy = Funds/Fund/Currency) and (@mulDiv = 'D')) or ((@fromCcy = Funds/Fund/Currency) and (@toCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@mulDiv = 'M'))"
  // Supported operators: "=", "!=", "<", ">"
  //
  pchar pPos = pszCondition;
  pchar pOpeningBracket = NULL;
  pchar pClosingBracket = NULL;
  PComplexCondition pComplexCondition = NULL;
  PComplexCondition pComplexSubCondition = NULL;
  int nReturnCode, nLen;
  bool bEnclosingBrackets = false;

  pCondition->pComplexCondition = NULL;
  pCondition->pSimpleCondition = NULL;

  /*if (*pPos) {
    pOpeningBracket = strchr(pPos+1, '(');
    pClosingBracket = strchr(pPos+1, ')');
    if (pClosingBracket < pOpeningBracket)
      pOpeningBracket = NULL;  // opening bracket behind closing bracket
  }*/

  if (*pPos == '(' /*&& pOpeningBracket*/) {
    // complex condition with logical operator
    pPos++;
    pCondition->pComplexCondition = pComplexCondition = GetNewComplexCondition();

    nReturnCode = ParseCondition(pPos, &pComplexCondition->LeftCondition, &pPos);
    if (nReturnCode != 0)
      return nReturnCode;

    while (*pPos == ' ')
      pPos++;  // skip spaces

    if (strnicmp(pPos, "and", 3) == 0) {
      pComplexCondition->op = AND_OP;
      pPos += 3;
    }
    else
      if (strnicmp(pPos, "or", 2) == 0) {
        pComplexCondition->op = OR_OP;
        pPos += 2;
      }
      else {
        sprintf(szLastError, "Missing logical operator ('and'/'or')");
        pszLastErrorPos = pPos;
        return 1;
      }

    while (*pPos == ' ')
      pPos++;  // skip spaces

    if (*pPos == '(')
      pPos++;
    else {
      sprintf(szLastError, "Opening bracket '(' expected behind logical operator");
      pszLastErrorPos = pPos;
      return 3;
    }

    nReturnCode = ParseCondition(pPos, &pComplexCondition->RightCondition, &pPos);
    if (nReturnCode != 0)
      return nReturnCode;

    while (*pPos == ' ')
      pPos++;  // skip spaces

    // more logical operators ?

    while (strnicmp(pPos, "and", 3) == 0 || strnicmp(pPos, "or", 2) == 0) {
      // add complex condition
      pComplexSubCondition = GetNewComplexCondition();
      // move old right condition to left condition of new complex condition (adding one level to condition tree)
      pComplexSubCondition->LeftCondition = pComplexCondition->RightCondition;
      pComplexCondition->RightCondition.pComplexCondition = pComplexSubCondition;
      pComplexCondition->RightCondition.pSimpleCondition = NULL;

      if (strnicmp(pPos, "and", 3) == 0) {
        pComplexSubCondition->op = AND_OP;
        nLen = 3;
      }
      else {
        pComplexSubCondition->op = OR_OP;
        nLen = 2;
      }

      if (pComplexCondition->op != pComplexSubCondition->op) {
        sprintf(szLastError, "Logical operators (and/or) cannot be mixed on same level");
        pszLastErrorPos = pPos;
        return 2;
      }

      pPos += nLen;  // go to end of operator (and/or)

      while (*pPos == ' ')
        pPos++;  // skip spaces

      if (*pPos == '(')
        pPos++;
      else {
        sprintf(szLastError, "Opening bracket '(' expected behind logical operator");
        pszLastErrorPos = pPos;
        return 3;
      }

      nReturnCode = ParseCondition(pPos, &pComplexSubCondition->RightCondition, &pPos);
      if (nReturnCode != 0)
        return nReturnCode;

      pComplexCondition = pComplexSubCondition;

      while (*pPos == ' ')
        pPos++;  // skip spaces
    }

    if (*pPos == ')')
      pPos++;
    else {
      sprintf(szLastError, "Closing bracket ')' or logical operator ('and'/'or') expected");
      pszLastErrorPos = pPos;
      return 3;
    }
  }
  else {
    // simple condition
    /*if (*pPos == '(') {
      bEnclosingBrackets = true;
      pPos++;
    }*/

    pClosingBracket = strchr(pPos, ')');
    if (pClosingBracket)
      *pClosingBracket = '\0';

    pCondition->pSimpleCondition = (PSimpleCondition)MyGetMemory(sizeof(SimpleCondition));

    nReturnCode = ParseSimpleCondition(pPos, pCondition->pSimpleCondition);
    if (nReturnCode != 0)
      return nReturnCode;

    if (pClosingBracket)
      pPos = pClosingBracket + 1;
    else
      pPos = strchr(pPos, '\0');
  }

  if (ppNext)
    *ppNext = pPos;

  return 0;
}
// end of function "ParseCondition"

//--------------------------------------------------------------------------------------------------------

void SimpleAddAttributeToList(AttributeNameValueList *pAttrNameValueList, cpchar pszAttrName, cpchar pszAttrValue)
{
  if (pAttrNameValueList->nCount >= pAttrNameValueList->nMaxCount)
    return;  // error

  AttributeNameValue *pAttrNameValue = pAttrNameValueList->aAttrNameValue + pAttrNameValueList->nCount;
  mystrncpy(pAttrNameValue->szName, pszAttrName, MAX_ATTR_NAME_SIZE);
  mystrncpy(pAttrNameValue->szValue, pszAttrValue, MAX_ATTR_VALUE_SIZE);
  pAttrNameValue->pszXPath = NULL;
  pAttrNameValueList->nCount++;
}

//--------------------------------------------------------------------------------------------------------

void AddAttributeToList(AttributeNameValueList *pAttrNameValueList, PCondition pCondition)
{
  if (pCondition->pComplexCondition) {
    AddAttributeToList(pAttrNameValueList, &pCondition->pComplexCondition->LeftCondition);
    if (pCondition->pComplexCondition->op == AND_OP)
      AddAttributeToList(pAttrNameValueList, &pCondition->pComplexCondition->RightCondition);
  }
  
  if (pCondition->pSimpleCondition && *pCondition->pSimpleCondition->szLeftPart == '@' && strcmp(pCondition->pSimpleCondition->szOperator, "=") == 0) {
    if (pAttrNameValueList->nCount >= pAttrNameValueList->nMaxCount)
      return;  // error

    mystrncpy(pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].szName, pCondition->pSimpleCondition->szLeftPart + 1, MAX_ATTR_NAME_SIZE);

    if (*pCondition->pSimpleCondition->szRightPart == '\'') {
      mystrncpy(pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].szValue, pCondition->pSimpleCondition->szRightPart + 1, MAX_ATTR_VALUE_SIZE);
      RemoveLastChar(pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].szValue, '\'');
      pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].pszXPath = NULL;
    }
    else {
      strcpy(pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].szValue, szEmptyString);
      pAttrNameValueList->aAttrNameValue[pAttrNameValueList->nCount].pszXPath = pCondition->pSimpleCondition->szRightPart;
    }

    pAttrNameValueList->nCount++;
  }
}

//--------------------------------------------------------------------------------------------------------

void GetAttributeList(AttributeNameValueList *pAttrNameValueList, PCondition pCondition)
{
  // Parses conditions like "CCY != FUND_CCY" or "48_* = '1'" (mandatory space before and after operator !)
  //   or like "((@fromCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@toCcy = Funds/Fund/Currency) and (@mulDiv = 'D')) or ((@fromCcy = Funds/Fund/Currency) and (@toCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@mulDiv = 'M'))" ?

  pAttrNameValueList->nCount = 0;
  AddAttributeToList(pAttrNameValueList, pCondition);
}

//--------------------------------------------------------------------------------------------------------

typedef struct {
  pchar *aField;
  int nFields;
  int nMapIndex;
  FieldMapping *pFieldMapping;
  FieldDefinition *pFieldDefinition;
} FieldMappingContext;

//--------------------------------------------------------------------------------------------------------

pchar LoadTextMappingField(FieldMappingContext const *pContext, cpchar pszFieldName, int nFieldIndex, /*bool bMandatory,*/ cpchar pszValidContent)
{
  pchar pszResult = (pchar)szEmptyString;

  if (nFieldIndex >= 0 && nFieldIndex < pContext->nFields) {
    pszResult = pContext->aField[nFieldIndex];

    // check content
    if ((!IsEmptyString(pszValidContent) /*|| bMandatory*/) && !CheckRegex(pszResult, pszValidContent)) {
      sprintf(szLastError, "Invalid content '%s' (not in '%s')", pszResult, pszValidContent+1);
      LogMappingError(pContext->nMapIndex, GetOperationLongName(pContext->pFieldDefinition->cOperation), pszFieldName, szLastError);
      pszResult = (pchar)szEmptyString;
    }
    else {
      int nLen = strlen(pszResult);
      if ((strcmp(pszFieldName, "CSV_FORMAT") == 0 || strcmp(pszFieldName, "XML_FORMAT") == 0) && nLen >= 3 && *pszResult == '(' && pszResult[nLen-1] == ')') {
        // change first and last character to comma for easier check of content (comma+text+comma must be part of this string)
        *pszResult = ',';
        pszResult[nLen-1] = ',';
      }
    }
  }

  return pszResult;
}

//--------------------------------------------------------------------------------------------------------

bool LoadBoolMappingField(FieldMappingContext const *pContext, cpchar pszFieldName, int nFieldIndex, cpchar pszTrue, cpchar pszFalse, bool bDefault)
{
  bool bResult = bDefault;
  pchar pszContent = NULL;

  if (nFieldIndex >= 0 && nFieldIndex < pContext->nFields) {
    pszContent = pContext->aField[nFieldIndex];
    if (!IsEmptyString(pszContent)) {
      if (strnicmp(pszContent, pszTrue, strlen(pszTrue)) == 0)
        bResult = true;
      else
        if (strnicmp(pszContent, pszFalse, strlen(pszFalse)) == 0)
          bResult = false;
        else {
          sprintf(szLastError, "Invalid content '%s' (not in '%s,%s')", pszContent, pszTrue, pszFalse);
          LogMappingError(pContext->nMapIndex, GetOperationLongName(pContext->pFieldDefinition->cOperation), pszFieldName, szLastError);
        }
    }
  }

  return bResult;
}

//--------------------------------------------------------------------------------------------------------

char LoadCharMappingField(FieldMappingContext const *pContext, cpchar pszFieldName, int nFieldIndex, bool bMandatory, cpchar pszValidContent, char cDefault)
{
  char cResult = cDefault;
  pchar pszContent = NULL;
  bool bOK;

  if (nFieldIndex >= 0 && nFieldIndex < pContext->nFields) {
    // check content
    pszContent = pContext->aField[nFieldIndex];
    bOK = IsEmptyString(pszContent) ? !bMandatory : CheckRegex(pszContent, pszValidContent);
    if (bOK)
      cResult = toupper(*pszContent);
    else {
      sprintf(szLastError, "Invalid content '%s' (not in '%s')", pszContent, pszValidContent+1);
      LogMappingError(pContext->nMapIndex, GetOperationLongName(pContext->pFieldDefinition->cOperation), pszFieldName, szLastError);
    }
  }

  return cResult;
}

//--------------------------------------------------------------------------------------------------------

int LoadIntMappingField(FieldMappingContext const *pContext, cpchar pszFieldName, int nFieldIndex, int nDefault)
{
  int nResult = nDefault;
  pchar pszContent = NULL;

  if (nFieldIndex >= 0 && nFieldIndex < pContext->nFields) {
    pszContent = pContext->aField[nFieldIndex];
    // TODO: check content
    if (*pszContent >= '0' && *pszContent <= '9')
      nResult = atoi(pszContent);
  }

  return nResult;
}

//--------------------------------------------------------------------------------------------------------

void CheckForFileIndex(pchar *ppszFieldName, int *pnCsvFileIndex)
{
  // checks column names with file index as praefix in the format ":2:column-name"
  // removes file index from column name and stores file index in second parameter
  //
  pchar pPos = *ppszFieldName;
  if (*pPos == ':') {
    pPos++;
    if (*pPos >= '0' && *pPos <= '9') {
      if (pPos[1] >= '0' && pPos[1] <= '9')
        pPos++;
      if (pPos[1] == ':') {
        pPos[1] = '\0';
        *pnCsvFileIndex = atoi(*ppszFieldName + 1) - 1;
        *ppszFieldName = pPos + 2;
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------------

void LoadMinMaxValues(FieldMappingContext *pContext, cpchar szMinFieldName, cpchar szMaxFieldName)
{
  if (!IsEmptyString(pContext->pFieldDefinition->szMinValue))
    if (!ConvertNumber(pContext->pFieldDefinition->szMinValue, pContext->pFieldDefinition->cType, &pContext->pFieldDefinition->nMinValue, &pContext->pFieldDefinition->fMinValue)) {
      LogMappingError(pContext->nMapIndex, GetOperationLongName(pContext->pFieldMapping->csv.cOperation), szMinFieldName, szLastError);
      pContext->pFieldDefinition->szMinValue = szEmptyString;
    }

  if (!IsEmptyString(pContext->pFieldDefinition->szMaxValue))
    if (!ConvertNumber(pContext->pFieldDefinition->szMaxValue, pContext->pFieldDefinition->cType, &pContext->pFieldDefinition->nMaxValue, &pContext->pFieldDefinition->fMaxValue)) {
      LogMappingError(pContext->nMapIndex, GetOperationLongName(pContext->pFieldMapping->csv.cOperation), szMaxFieldName, szLastError);
      pContext->pFieldDefinition->szMaxValue = szEmptyString;
    }

  *szLastError = '\0';
}

//--------------------------------------------------------------------------------------------------------

bool IsValidMappingFormat(cpchar szMappingFormat)
{
  // check syntax of mapping format columns
  // Samples: "'PRAEFIX-'*", "*'-POSTFIX'"

  if (!*szMappingFormat)
    return true;

  int len = strlen(szMappingFormat);
  if (len < 4)
    return false;

  return (*szMappingFormat == '\'' && strncmp(szMappingFormat + len - 2, "'*", 2) == 0) || (strncmp(szMappingFormat, "*'", 2) == 0 && szMappingFormat[len-1] == '\'');
}

//--------------------------------------------------------------------------------------------------------

int ReadFieldMappings(const char *szFileName)
{
  // read mapping definition
  int i, nMapIndex, nMapIndex2, nReturnCode, nNonEmptyFields;
  char *pColumnName = NULL;
  const char *pszValue = NULL;
  FieldMapping *pFieldMapping, *pFieldMapping2;
  pchar aField[MAX_MAPPING_COLUMNS];
  FILE *pFile = NULL;
  //errno_t error_code;
  char szTemp[256];

  // open csv file for input in binary mode (cr/lf are not changed)
  //error_code = fopen_s(&pFile, szFileName, "rb");
  pFile = fopen(szFileName, "rb");
  if (!pFile) {
    //sprintf(szLastError, "Cannot open mapping file '%s' (error code %d)", szFileName, error_code);
    sprintf(szLastError, "Cannot open mapping file '%s'", szFileName);
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

  // initialize list of root node attributes
  RootAttrNameValueList.nCount = 0;
  RootAttrNameValueList.nMaxCount = MAX_NODE_ATTRIBUTES;
  RootAttrNameValueList.aAttrNameValue = (AttributeNameValue*)malloc(RootAttrNameValueList.nMaxCount * sizeof(AttributeNameValue));

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
  int nCsvContent2Idx = -1;
  int nCsvShortNameIdx = -1;
  int nCsvMoIdx = -1;
  int nCsvTypeIdx = -1;
  int nCsvMinLenIdx = -1;
  int nCsvMaxLenIdx = -1;
  int nCsvMinValueIdx = -1;
  int nCsvMaxValueIdx = -1;
  int nCsvFormatIdx = -1;
  int nCsvTransformIdx = -1;
  int nCsvDefaultIdx = -1;
  int nCsvConditionIdx = -1;
  int nCsvMappingFormatIdx = -1;
  int nXmlOpIdx = -1;
  int nXmlContentIdx = -1;
  int nXmlContent2Idx = -1;
  int nXmlAttributeIdx = -1;
  int nXmlShortNameIdx = -1;
  int nXmlMoIdx = -1;
  int nXmlTypeIdx = -1;
  int nXmlMinLenIdx = -1;
  int nXmlMaxLenIdx = -1;
  int nXmlMinValueIdx = -1;
  int nXmlMaxValueIdx = -1;
  int nXmlFormatIdx = -1;
  int nXmlTransformIdx = -1;
  int nXmlDefaultIdx = -1;
  int nXmlConditionIdx = -1;
  int nXmlMappingFormatIdx = -1;
  pchar szCsvOperation = NULL;
  pchar szXmlOperation = NULL;
  FieldMappingContext MappingContext;

  // find position of mapping columns
  for (i = 0; i < nColumns; i++) {
    pColumnName = aField[i];
    // search for csv columns
    if (strcmp(pColumnName, "CSV_OP") == 0) nCsvOpIdx = i;
    if (strcmp(pColumnName, "CSV_CONTENT") == 0) nCsvContentIdx = i;
    if (strcmp(pColumnName, "CSV_CONTENT2") == 0) nCsvContent2Idx = i;
    if (strcmp(pColumnName, "CSV_SHORT_NAME") == 0) nCsvShortNameIdx = i;
    if (strcmp(pColumnName, "CSV_MO") == 0) nCsvMoIdx = i;
    if (strcmp(pColumnName, "CSV_TYPE") == 0) nCsvTypeIdx = i;
    if (strcmp(pColumnName, "CSV_MIN_LEN") == 0) nCsvMinLenIdx = i;
    if (strcmp(pColumnName, "CSV_MAX_LEN") == 0) nCsvMaxLenIdx = i;
    if (strcmp(pColumnName, "CSV_MIN_VALUE") == 0) nCsvMinValueIdx = i;
    if (strcmp(pColumnName, "CSV_MAX_VALUE") == 0) nCsvMaxValueIdx = i;
    if (strcmp(pColumnName, "CSV_FORMAT") == 0) nCsvFormatIdx = i;
    if (strcmp(pColumnName, "CSV_TRANSFORM") == 0) nCsvTransformIdx = i;
    if (strcmp(pColumnName, "CSV_DEFAULT") == 0) nCsvDefaultIdx = i;
    if (strcmp(pColumnName, "CSV_CONDITION") == 0) nCsvConditionIdx = i;
    if (strcmp(pColumnName, "CSV_MAPPING_FORMAT") == 0) nCsvMappingFormatIdx = i;
    // search for xml columns
    if (strcmp(pColumnName, "XML_OP") == 0) nXmlOpIdx = i;
    if (strcmp(pColumnName, "XML_CONTENT") == 0) nXmlContentIdx = i;
    if (strcmp(pColumnName, "XML_CONTENT2") == 0) nXmlContent2Idx = i;
    if (strcmp(pColumnName, "XML_ATTRIBUTE") == 0) nXmlAttributeIdx = i;
    if (strcmp(pColumnName, "XML_SHORT_NAME") == 0) nXmlShortNameIdx = i;
    if (strcmp(pColumnName, "XML_MO") == 0) nXmlMoIdx = i;
    if (strcmp(pColumnName, "XML_TYPE") == 0) nXmlTypeIdx = i;
    if (strcmp(pColumnName, "XML_MIN_LEN") == 0) nXmlMinLenIdx = i;
    if (strcmp(pColumnName, "XML_MAX_LEN") == 0) nXmlMaxLenIdx = i;
    if (strcmp(pColumnName, "XML_MIN_VALUE") == 0) nXmlMinValueIdx = i;
    if (strcmp(pColumnName, "XML_MAX_VALUE") == 0) nXmlMaxValueIdx = i;
    if (strcmp(pColumnName, "XML_FORMAT") == 0) nXmlFormatIdx = i;
    if (strcmp(pColumnName, "XML_TRANSFORM") == 0) nXmlTransformIdx = i;
    if (strcmp(pColumnName, "XML_DEFAULT") == 0) nXmlDefaultIdx = i;
    if (strcmp(pColumnName, "XML_CONDITION") == 0) nXmlConditionIdx = i;
    if (strcmp(pColumnName, "XML_MAPPING_FORMAT") == 0) nXmlMappingFormatIdx = i;
  }

  if (nCsvOpIdx < 0 || nCsvContentIdx < 0 || nCsvTypeIdx < 0 || nXmlOpIdx < 0 || nXmlContentIdx < 0)
    return -2;  // missing mandatory columns in mapping file

  pFieldMapping = aFieldMapping;

  // load field mapping definitions into mapping array
  while (pLine = GetNextLine(&pReadPos)) {
    nColumns = GetFields(pLine, ';', aField, MAX_MAPPING_COLUMNS);

    nNonEmptyFields = 0;
    for (i = 0; i < nColumns; i++)
      if (strlen(aField[i]) > 0)
        nNonEmptyFields++;

    // line with real content ?
    if (*pLine != '#' /*Comment*/ && nNonEmptyFields >= 3 && nFieldMappings < MAX_FIELD_MAPPINGS) {
      nMapIndex = nFieldMappings;
      MappingContext.aField = aField;
      MappingContext.nFields = nColumns;
      MappingContext.nMapIndex = nMapIndex;
      MappingContext.pFieldMapping = pFieldMapping;

      //pFieldMapping->csv = EmptyFieldDefinition;
      MappingContext.pFieldDefinition = &pFieldMapping->csv;
      pFieldMapping->csv.cOperation = LoadCharMappingField(&MappingContext, "CSV_OP", nCsvOpIdx, true, ",FIX,VAR,MAP,NOP,CHANGE,UNIQUE,IF,ADDFILE,", 'M');
      pFieldMapping->csv.szContent = LoadTextMappingField(&MappingContext, "CSV_CONTENT", nCsvContentIdx, NULL);
      pFieldMapping->csv.szContent2 = LoadTextMappingField(&MappingContext, "CSV_CONTENT2", nCsvContent2Idx, NULL);
      pFieldMapping->csv.szAttribute = NULL; // not used at the moment
      pFieldMapping->csv.szShortName = LoadTextMappingField(&MappingContext, "CSV_SHORT_NAME", nCsvShortNameIdx, NULL);
      pFieldMapping->csv.bMandatory = LoadBoolMappingField(&MappingContext, "CSV_MO", nCsvMoIdx, "M", "O", false);
      pFieldMapping->csv.cType = LoadCharMappingField(&MappingContext, "CSV_TYPE", nCsvTypeIdx, (pFieldMapping->csv.cOperation != 'N'), ",BOOLEAN,DATE,DATETIME,INTEGER,NUMBER,TEXT,", 'T');
      pFieldMapping->csv.nMinLen = LoadIntMappingField(&MappingContext, "CSV_MIN_LEN", nCsvMinLenIdx, 0);
      pFieldMapping->csv.nMaxLen = LoadIntMappingField(&MappingContext, "CSV_MAX_LEN", nCsvMaxLenIdx, -1);
      pFieldMapping->csv.szMinValue = LoadTextMappingField(&MappingContext, "CSV_MIN_VALUE", nCsvMinValueIdx, NULL);
      pFieldMapping->csv.szMaxValue = LoadTextMappingField(&MappingContext, "CSV_MAX_VALUE", nCsvMaxValueIdx, NULL);
      pFieldMapping->csv.szFormat = LoadTextMappingField(&MappingContext, "CSV_FORMAT", nCsvFormatIdx, NULL);
      pFieldMapping->csv.szTransform = LoadTextMappingField(&MappingContext, "CSV_TRANSFORM", nCsvTransformIdx, NULL);
      pFieldMapping->csv.szDefault = LoadTextMappingField(&MappingContext, "CSV_DEFAULT", nCsvDefaultIdx, NULL);
      pFieldMapping->csv.szCondition = LoadTextMappingField(&MappingContext, "CSV_CONDITION", nCsvConditionIdx, NULL);
      pFieldMapping->csv.szMappingFormat = LoadTextMappingField(&MappingContext, "CSV_MAPPING_FORMAT", nCsvMappingFormatIdx, NULL);
      LoadMinMaxValues(&MappingContext, "CSV_MIN_VALUE", "CSV_MAX_VALUE");

      //pFieldMapping->xml = EmptyFieldDefinition;
      MappingContext.pFieldDefinition = &pFieldMapping->xml;
      pFieldMapping->xml.cOperation = LoadCharMappingField(&MappingContext, "XML_OP", nXmlOpIdx, true, ",FIX,VAR,MAP,LOOP,IF,ROOT,", 0);
      pFieldMapping->xml.szContent = LoadTextMappingField(&MappingContext, "XML_CONTENT", nXmlContentIdx, NULL);
      pFieldMapping->xml.szContent2 = LoadTextMappingField(&MappingContext, "XML_CONTENT2", nXmlContent2Idx, NULL);
      pFieldMapping->xml.szAttribute = LoadTextMappingField(&MappingContext, "XML_ATTRIBUTE", nXmlAttributeIdx, NULL);
      pFieldMapping->xml.szShortName = LoadTextMappingField(&MappingContext, "XML_SHORT_NAME", nXmlShortNameIdx, NULL);
      pFieldMapping->xml.bMandatory = LoadBoolMappingField(&MappingContext, "XML_MO", nXmlMoIdx, "M", "O", pFieldMapping->csv.bMandatory);
      pFieldMapping->xml.bAddEmptyNodes = false;
      pFieldMapping->xml.cType = LoadCharMappingField(&MappingContext, "XML_TYPE", nXmlTypeIdx, false, ",BOOLEAN,DATE,DATETIME,INTEGER,NUMBER,TEXT,", pFieldMapping->csv.cType);
      pFieldMapping->xml.nMinLen = LoadIntMappingField(&MappingContext, "XML_MIN_LEN", nXmlMinLenIdx, pFieldMapping->csv.nMinLen);
      pFieldMapping->xml.nMaxLen = LoadIntMappingField(&MappingContext, "XML_MAX_LEN", nXmlMaxLenIdx, pFieldMapping->csv.nMaxLen);
      pFieldMapping->xml.szMinValue = LoadTextMappingField(&MappingContext, "XML_MIN_VALUE", nXmlMinValueIdx, NULL);
      pFieldMapping->xml.szMaxValue = LoadTextMappingField(&MappingContext, "XML_MAX_VALUE", nXmlMaxValueIdx, NULL);
      pFieldMapping->xml.szFormat = LoadTextMappingField(&MappingContext, "XML_FORMAT", nXmlFormatIdx, NULL);
      pFieldMapping->xml.szTransform = LoadTextMappingField(&MappingContext, "XML_TRANSFORM", nXmlTransformIdx, NULL);
      pFieldMapping->xml.szDefault = LoadTextMappingField(&MappingContext, "XML_DEFAULT", nXmlDefaultIdx, NULL);
      pFieldMapping->xml.szCondition = LoadTextMappingField(&MappingContext, "XML_CONDITION", nXmlConditionIdx, NULL);
      pFieldMapping->xml.Condition.pComplexCondition = NULL;
      pFieldMapping->xml.Condition.pSimpleCondition = NULL;
      pFieldMapping->xml.szMappingFormat = LoadTextMappingField(&MappingContext, "XML_MAPPING_FORMAT", nXmlMappingFormatIdx, NULL);
      LoadMinMaxValues(&MappingContext, "XML_MIN_VALUE", "XML_MAX_VALUE");

      if (pFieldMapping->csv.cOperation == 'N'/*NOP*/ && pFieldMapping->xml.cOperation == 'R'/*ROOT*/) {
        if (IsEmptyString(pFieldMapping->xml.szContent)) {
          sprintf(szLastError, "Missing mandatory content");
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->xml.cOperation), "XML_CONTENT", szLastError);
        }
        else {
          if (IsEmptyString(pFieldMapping->xml.szAttribute))
            mystrncpy(szRootNodeName, pFieldMapping->xml.szContent, MAX_NODE_NAME_SIZE);
          else
            SimpleAddAttributeToList(&RootAttrNameValueList, pFieldMapping->xml.szAttribute, pFieldMapping->xml.szContent);
        }
      }
      else {
        // add new mapping
        pszValue = aField[nXmlTypeIdx];
        if (strcmp(pszValue, "DATE") == 0 && strlen(pFieldMapping->xml.szFormat) < 10)
          pFieldMapping->xml.szFormat = szXmlDateFormat;
        if (strcmp(pszValue, "DATETIME") == 0 && strlen(pFieldMapping->xml.szFormat) < 19)
          pFieldMapping->xml.szFormat = szXmlTimestampFormat;

        // csv file indices
        pFieldMapping->nCsvFileIndex = 0;
        pFieldMapping->nCsvFileIndex2 = 0;
        if (strchr("FV", pFieldMapping->csv.cOperation) == NULL) {
          CheckForFileIndex((pchar*)&pFieldMapping->csv.szContent, &(pFieldMapping->nCsvFileIndex));
          CheckForFileIndex((pchar*)&pFieldMapping->csv.szContent2, &(pFieldMapping->nCsvFileIndex2));
        }

        if (pFieldMapping->csv.cOperation != 'F' && IsEmptyString(pFieldMapping->csv.szContent)) {
          sprintf(szLastError, "Missing mandatory content");
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->csv.cOperation), "CSV_CONTENT", szLastError);
        }

        if (pFieldMapping->xml.cOperation != 'F' && IsEmptyString(pFieldMapping->xml.szContent)) {
          sprintf(szLastError, "Missing mandatory content");
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->xml.cOperation), "XML_CONTENT", szLastError);
        }

        // check combination of mapping operations (csv/xml)
        sprintf(szTemp, "%c%c", pFieldMapping->csv.cOperation, pFieldMapping->xml.cOperation);
        if (!CheckRegex(szTemp, ",AM,FM,MF,VM,MV,MM,CL,UL,II,NR,")) {
          sprintf(szLastError, "Invalid combination of csv and xml operation '%s'-'%s'", GetOperationLongName(pFieldMapping->csv.cOperation), GetOperationLongName(pFieldMapping->xml.cOperation));
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->xml.cOperation), "XML_CONTENT", szLastError);
        }

        // use csv format for xml field if type is equal and xml format not specified
        if (pFieldMapping->csv.cOperation == 'M' && pFieldMapping->xml.cOperation == 'M' && IsEmptyString(pFieldMapping->xml.szFormat) &&
            strchr("BT", pFieldMapping->xml.cType) && pFieldMapping->csv.cType == pFieldMapping->xml.cType)
          pFieldMapping->xml.szFormat = pFieldMapping->csv.szFormat;

        // check syntax of xml condition
        if (!IsEmptyString(pFieldMapping->xml.szCondition)) {
          nReturnCode = ParseCondition(pFieldMapping->xml.szCondition, &pFieldMapping->xml.Condition);
          if (nReturnCode != 0) {
            LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->xml.cOperation), "XML_CONDITION", szLastError);
          }
        }

        // check syntax of mapping format columns
        // Samples: "'PRAEFIX-'*", "*'-POSTFIX'"
        if (!IsValidMappingFormat(pFieldMapping->csv.szMappingFormat)) {
          sprintf(szLastError, "Invalid csv mapping format \"%s\" (valid samples: \"'PRAEFIX-'*\", \"*'-POSTFIX'\")", pFieldMapping->csv.szMappingFormat);
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->csv.cOperation), "CSV_MAPPING_FORMAT", szLastError);
          pFieldMapping->csv.szMappingFormat = szEmptyString;
        }
        if (!IsValidMappingFormat(pFieldMapping->xml.szMappingFormat)) {
          sprintf(szLastError, "Invalid xml mapping format \"%s\" (valid samples: \"'PRAEFIX-'*\", \"*'-POSTFIX'\")", pFieldMapping->xml.szMappingFormat);
          LogMappingError(nMapIndex, GetOperationLongName(pFieldMapping->xml.cOperation), "XML_MAPPING_FORMAT", szLastError);
          pFieldMapping->xml.szMappingFormat = szEmptyString;
        }

        pFieldMapping->nCsvIndex = -1;
        pFieldMapping->nCsvIndex2 = -1;
        pFieldMapping->nSourceMapIndex = -1;
        pFieldMapping++;
        nFieldMappings++;
      }
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
}
// end of function "ReadFieldMappings"

//--------------------------------------------------------------------------------------------------------

void FreeCsvFileBuffers()
{
  LinkedCsvFile *pLinkedCsvFile = aLinkedCsvFile;

  for (int i = 0; i < nLinkedCsvFiles; i++) {
    if (pLinkedCsvFile->pDataBuffer != NULL) {
      free(pLinkedCsvFile->pDataBuffer);
      pLinkedCsvFile->pDataBuffer = NULL;
    }
    if (pLinkedCsvFile->aDataFields != NULL) {
      free(pLinkedCsvFile->aDataFields);
      pLinkedCsvFile->aDataFields = NULL;
    }
    pLinkedCsvFile++;
  }
}

//--------------------------------------------------------------------------------------------------------

bool MatchingColumnName(cpchar pszColumnName, cpchar pszSearchColumnName)
{
  // Compares given csv column name with search pattern from mapping definition
  // If search pattern contains asterics '*' at the end, then rest of the column name is ignored
  // Sample: column name "3_Portfolio_name" is matching "3_*"
  //
  bool bMatch;

  if (!pszColumnName || !*pszColumnName || !pszSearchColumnName || !*pszSearchColumnName)
    return false;

  int nPos = strlen(pszSearchColumnName) - 1;

  if (nPos > 0 && pszSearchColumnName[nPos] == '*')
    bMatch = (strnicmp(pszColumnName, pszSearchColumnName, nPos) == 0);
  else
    bMatch = (stricmp(pszColumnName, pszSearchColumnName) == 0);

  return bMatch;
}

//--------------------------------------------------------------------------------------------------------

void ResetFieldIndices()
{
  // reset field indices
  FieldMapping *pFieldMapping = aFieldMapping;
	for (int i = 0; i < nFieldMappings; i++, pFieldMapping++)  {
    pFieldMapping->nCsvIndex = -1;
    pFieldMapping->nCsvIndex2 = -1;
  }
}

//--------------------------------------------------------------------------------------------------------

int ReadCsvData(int nCsvFileIndex)
{
  // read and parse content of csv file
  int i, nColumnIndex, nMapIndex, nColumns, nNonEmptyColumns, nFound;
  LinkedCsvFile *pCsvFile = aLinkedCsvFile + nCsvFileIndex;
  char *pColumnName = NULL;
  char szErrorMessage[MAX_ERROR_MESSAGE_SIZE];
  char szTempHeader[MAX_HEADER_SIZE];
  cpchar szIgnoreXPath = NULL;
  pchar aField[MAX_CSV_COLUMNS];
  FieldMapping *pFieldMapping = NULL;
  FILE *pFile = NULL;
  //errno_t error_code;
  bool bCheck, bFound;

  // initialize buffer pointers and number of columns and csv data lines
  pCsvFile->pDataBuffer = NULL;
  pCsvFile->aDataFields = NULL;
  pCsvFile->nColumns = 0;
  pCsvFile->nDataLines = 0;
  pCsvFile->nRealDataLines = 0;

  // open csv file for input in binary mode (cr/lf are not changed)
  //error_code = fopen_s(&pFile, pCsvFile->szFileName, "rb");
  pFile = fopen(pCsvFile->szFileName, "rb");
  if (!pFile) {
    //sprintf(szLastError, "Cannot open input file '%s' (error code %d)", pCsvFile->szFileName, error_code);
    sprintf(szLastError, "Cannot open input file '%s'", pCsvFile->szFileName);
    puts(szLastError);
    return -2;
  }

  // get file size
  //int fseek(FILE *stream, long offset, int whence);
  int iReturnCode = fseek(pFile, 0, SEEK_END);
  int nFileSize = ftell(pFile);

  // allocate reading buffer
  pCsvFile->nDataBufferSize = nFileSize + 1;
  pCsvFile->pDataBuffer = (char*)malloc(pCsvFile->nDataBufferSize);

  if (!pCsvFile->pDataBuffer) {
    sprintf(szLastError, "Not enough memory for reading input file '%s' (%d bytes)", pCsvFile->szFileName, nFileSize);
    puts(szLastError);
    return -1;  // not enough free memory
  }

  // clear read buffer
  memset(pCsvFile->pDataBuffer, 0, pCsvFile->nDataBufferSize);

  // go to start of file
  iReturnCode = fseek(pFile, 0, SEEK_SET);

  // read content of file
  int nBytesRead = fread(pCsvFile->pDataBuffer, pCsvFile->nDataBufferSize, 1, pFile);
  //int nContentLength = strlen(pCsvDataBuffer);
  //pchar pTest = pCsvDataBuffer + nCsvDataBufferSize - 8;

  // close file
  fclose(pFile);

  // initialize reading position
  char *pReadPos = pCsvFile->pDataBuffer;

  // get header line with column names
  char *pLine = GetNextLine(&pReadPos);
  if (!pLine || *pLine <= '\n') {
    sprintf(szLastError, "Missing or empty header line in input file '%s'", pCsvFile->szFileName);
    puts(szLastError);
    return -2;
  }

  // save header line
  mystrncpy(szCsvHeader, pLine, MAX_HEADER_SIZE);
  *szCsvHeader2 = '\0';

  // detect column delimiter
  cColumnDelimiter = GetColumnDelimiter(pLine);

  // set list of characters to be ignored when parsing the csv line (outside of quoted values)
  strcpy(szIgnoreChars, (cColumnDelimiter == '\t') ? " " : " \t");

  // parse header line
  pCsvFile->nColumns = GetFields(pLine, cColumnDelimiter, aField, MAX_CSV_COLUMNS);

  // search for fields referenced by the mapping definition
  pFieldMapping = aFieldMapping;
  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
    //pFieldMapping->nCsvIndex = -1;

    //if (strcmp(pFieldMapping->csv.szContent2,":2:Fund Name") == 0)
    //  i = 0;  // for debugging purposes only!

    if (pFieldMapping->csv.cOperation == 'A'/*ADDFILE*/) {
      if (pFieldMapping->nCsvFileIndex == nCsvFileIndex) {
        for (nColumnIndex = 0; nColumnIndex < pCsvFile->nColumns; nColumnIndex++)
          if (MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent)) {
            pFieldMapping->nCsvIndex = nColumnIndex;
            if (pFieldMapping->nCsvFileIndex2 >= 0 && pFieldMapping->nCsvFileIndex2 < nLinkedCsvFiles)
              aLinkedCsvFile[pFieldMapping->nCsvFileIndex2].nLinkedMainColumnIndex = nColumnIndex;
            break;
          }
        if (pFieldMapping->nCsvIndex < 0) {
          sprintf(szErrorMessage, "Cannot find mandatory column in input file header");
          LogCsvError(nCsvFileIndex, 0, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, szEmptyString, szErrorMessage);
        }
      }
      if (pFieldMapping->nCsvFileIndex2 == nCsvFileIndex) {
        for (nColumnIndex = 0; nColumnIndex < pCsvFile->nColumns; nColumnIndex++)
          if (MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent2)) {
            pFieldMapping->nCsvIndex2 = nColumnIndex;
            pCsvFile->nLinkedColumnIndex = nColumnIndex;
            break;
          }
        if (pFieldMapping->nCsvIndex2 < 0) {
          sprintf(szErrorMessage, "Cannot find mandatory column in input file header");
          LogCsvError(nCsvFileIndex, 0, pFieldMapping->nCsvIndex2, pFieldMapping->csv.szContent2, szEmptyString, szErrorMessage);
        }
      }
    }

    if (strchr("CIMU"/*CHANGE,IF,MAP,UNIQUE*/, pFieldMapping->csv.cOperation)) {
      //if (strcmp(pFieldMapping->csv.szContent2, "67_*") == 0)
      //  i = 0;  // ColumnIndex = 68; for debugging purposes only!

      if (pFieldMapping->nCsvFileIndex == nCsvFileIndex) {
        // search for column in csv header line(s)
        for (nColumnIndex = 0; nColumnIndex < pCsvFile->nColumns; nColumnIndex++)
          if (MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent) || MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent2)) {
            pFieldMapping->nCsvIndex = nColumnIndex;
            break;
          }
      }

      // within conditional block ?
      bCheck = true;
      if (szIgnoreXPath) {
        if (strncmp(pFieldMapping->xml.szContent, szIgnoreXPath, strlen(szIgnoreXPath)) == 0)
          bCheck = false;  // yes, still within conditional block to be skipped
        else
          szIgnoreXPath = NULL;  // no, end of conditional block reached
      }

      if (pFieldMapping->nCsvFileIndex == nCsvFileIndex) {
        // mandatory column missing in header line ?
        if (bCheck && pFieldMapping->nCsvIndex < 0 && pFieldMapping->csv.bMandatory) {
          sprintf(szErrorMessage, "Cannot find mandatory column in input file header");
          LogCsvError(pFieldMapping->nCsvFileIndex, 0, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, szEmptyString, szErrorMessage);
        }
      }

      // start of new IF block ?
      if (!szIgnoreXPath && pFieldMapping->csv.cOperation == 'I')
        szIgnoreXPath = pFieldMapping->xml.szContent;
    }
  }

  // count number of lines
  pCsvFile->nDataLines = 1;
  char *pc = pReadPos;
  while (*pc)
    if (*pc++ == '\n')
      pCsvFile->nDataLines++;

  // allocate memory for field pointer array
  pCsvFile->nDataFieldsBufferSize = pCsvFile->nDataLines * pCsvFile->nColumns * sizeof(pchar);
  pCsvFile->aDataFields = (pchar*)malloc(pCsvFile->nDataFieldsBufferSize);

  if (!pCsvFile->aDataFields) {
    sprintf(szLastError, "Not enough memory for csv content field buffer input file '%s' (%d bytes for %d lines and %d columns)", pCsvFile->szFileName, pCsvFile->nDataFieldsBufferSize, pCsvFile->nDataLines, pCsvFile->nColumns);
    puts(szLastError);
    return -1;  // not enough free memory
  }

  // initialize flags whether csv values has been quoted or not
  for (i = 0; i < pCsvFile->nColumns; i++)
    pCsvFile->abColumnQuoted[i] = false;

  // clear field pointer array
  memset(pCsvFile->aDataFields, 0, pCsvFile->nDataFieldsBufferSize);

  pchar *pCsvDataFields = pCsvFile->aDataFields;

  while (pLine = GetNextLine(&pReadPos)) {
    //if (nRealCsvDataLines >= 761)
    //  i = 0;  // for debugging purposes only!

    if (pCsvFile->nRealDataLines == 0 && !*szCsvHeader2)
      mystrncpy(szTempHeader, pLine, MAX_HEADER_SIZE);

    // parse current csv line
    nColumns = GetFields(pLine, cColumnDelimiter, aField, pCsvFile->nColumns, (pCsvFile->nRealDataLines == 0) ? pCsvFile->abColumnQuoted : NULL);

    // count non-empty columns
    nNonEmptyColumns = 0;
    for (i = 0; i < nColumns; i++)
      if (*aField[i])
        nNonEmptyColumns++;

    if (nNonEmptyColumns >= 3 && pCsvFile->nRealDataLines == 0 && !*szCsvHeader2) {
      // check existance of second header line
      nFound = 0;

      for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
        bFound = false;

        if (strchr("CIMU"/*CHANGE,IF,MAP,UNIQUE*/, pFieldMapping->csv.cOperation)) {
          // search for column name in potential csv header
          for (nColumnIndex = 0; nColumnIndex < nColumns && !bFound; nColumnIndex++)
            if (MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent) || MatchingColumnName(aField[nColumnIndex], pFieldMapping->csv.szContent2)) {
              bFound = true;
              nFound++;
            }
        }
      }

      if (nFound >= pCsvFile->nColumns / 2) {
        // save second header line, if minimum half of the columns is matching column names
        mystrncpy(szCsvHeader2, szTempHeader, MAX_HEADER_SIZE);
        nNonEmptyColumns = 0;
      }
    }

    if (nNonEmptyColumns >= 3 && pCsvFile->nRealDataLines < pCsvFile->nDataLines) {
      // copy pointer of fields content to field pointer array
      memcpy(pCsvDataFields, aField, nColumns * sizeof(pchar));
      // initialize non existing fields at the end of the line with empty strings
      for (i = nColumns; i < pCsvFile->nColumns; i++)
        pCsvDataFields[i] = (pchar)szEmptyString;
      // next line
      pCsvDataFields += pCsvFile->nColumns;
      pCsvFile->nRealDataLines++;
    }
  }

  return nFieldMappings;
}
// end of function "ReadCsvData"

//--------------------------------------------------------------------------------------------------------

cpchar GetCsvFieldValue(int nCsvFileIndex, int nCsvDataLine, int nCsvColumn)
{
  cpchar pResult = NULL;
  LinkedCsvFile *pLinkedCsvFile;

  if (nCsvFileIndex >= 0 && nCsvFileIndex < nLinkedCsvFiles) {
    pLinkedCsvFile = aLinkedCsvFile + nCsvFileIndex;
    if (nCsvDataLine >= 0 && nCsvDataLine < pLinkedCsvFile->nRealDataLines && nCsvColumn >= 0 && nCsvColumn < pLinkedCsvFile->nColumns)
      pResult = pLinkedCsvFile->aDataFields[nCsvDataLine * pLinkedCsvFile->nColumns + nCsvColumn];
  }

  return pResult;
}

//--------------------------------------------------------------------------------------------------------

cpchar GetLinkedCsvFieldValue(int nCsvFileIndex, int nCsvDataLine, int nCsvColumn)
{
  // get csv field value within current table window (starting at nMatchingFirstLine and ending at nMatchingLastLine)
  cpchar pResult = NULL;
  LinkedCsvFile *pLinkedCsvFile;

  if (nCsvFileIndex >= 0 && nCsvFileIndex < nLinkedCsvFiles) {
    pLinkedCsvFile = aLinkedCsvFile + nCsvFileIndex;
    if (pLinkedCsvFile->nMatchingFirstLine >= 0 && pLinkedCsvFile->nMatchingLastLine >= 0) {
      int nRealCsvDataLine = nCsvDataLine + pLinkedCsvFile->nMatchingFirstLine;
      if (nRealCsvDataLine >= pLinkedCsvFile->nMatchingFirstLine && nRealCsvDataLine <= pLinkedCsvFile->nMatchingLastLine && nCsvColumn >= 0 && nCsvColumn < pLinkedCsvFile->nColumns)
        pResult = pLinkedCsvFile->aDataFields[nRealCsvDataLine * pLinkedCsvFile->nColumns + nCsvColumn];
    }
  }

  return pResult;
}

//--------------------------------------------------------------------------------------------------------

bool IsNewCsvFieldValue(int nCsvFileIndex, int nCsvDataLines, int nCsvColumn, cpchar szCsvValue)
{
  bool bResult = false;
  LinkedCsvFile *pLinkedCsvFile;

  if (nCsvFileIndex >= 0 && nCsvFileIndex < nLinkedCsvFiles) {
    pLinkedCsvFile = aLinkedCsvFile + nCsvFileIndex;
    if (nCsvDataLines >= 0 && nCsvDataLines < pLinkedCsvFile->nRealDataLines && nCsvColumn >= 0 && nCsvColumn < pLinkedCsvFile->nColumns) {
      bResult = true;
      cpchar *ppCsvValue = (cpchar*)(pLinkedCsvFile->aDataFields + nCsvColumn);
      for (int i = 0; i < nCsvDataLines; i++) {
        if (strcmp(szCsvValue, *ppCsvValue) == 0)
          return false;
        ppCsvValue += pLinkedCsvFile->nColumns;
      }
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
  int i, nColumnIndex, nMapIndex, nDigitsBehindComma, nDigitsBehindPoint;
  int nCommaUsage = 0;
  int nPointUsage = 0;
  int nReturnCode = 0;
  pchar *ppCsvDataField;
  cpchar pCsvFieldValue = NULL;
  cpchar pCommaPos = NULL;
  cpchar pPointPos = NULL;
  CPFieldMapping pFieldMapping = aFieldMapping;
  LinkedCsvFile *pCsvFile = aLinkedCsvFile;

  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
    if (pFieldMapping->csv.cType == 'N'/*NUMBER*/ && pFieldMapping->csv.cOperation == 'M'/*MAP*/ && pFieldMapping->nCsvIndex >= 0)
    {
      nColumnIndex = pFieldMapping->nCsvIndex;
      ppCsvDataField = pCsvFile->aDataFields + nColumnIndex;

      for (i = 0; i < pCsvFile->nRealDataLines; i++) {
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
        ppCsvDataField += pCsvFile->nColumns;
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
    if (pFieldMapping->csv.cOperation == 'M' && (stricmp(szColumnName, pFieldMapping->csv.szContent) == 0 || stricmp(szColumnName, pFieldMapping->csv.szContent2) == 0))
      return pFieldMapping->nCsvIndex;

  return -1;
}

//--------------------------------------------------------------------------------------------------------

bool CheckCsvCondition(int nCsvDataLine, CPFieldMapping pFieldMapping)
{
  char szCondition[MAX_CONDITION_SIZE];
  pchar pszLeftPart, pszOperator, pszRightPart;
  cpchar pszFieldValue, pszLeftValue, pszRightValue;
  int len, nLeftColumnIndex, nRightColumnIndex;
  bool bMatch = false;

  if (stricmp(pFieldMapping->csv.szCondition, "contentisvalid()") == 0) {
    // get content of csv field
    pszFieldValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, nCsvDataLine, pFieldMapping->nCsvIndex);

    // check content of csv field
    if (pFieldMapping->csv.cType == 'N' /*NUMBER*/)
      bMatch = IsValidNumber(pszFieldValue, cDecimalPoint);

    if (pFieldMapping->csv.cType == 'T' /*TEXT*/)
      bMatch = IsValidText(pszFieldValue, pFieldMapping);

    return bMatch;
  }

  // condition likes "CCY != FUND_CCY" or "48_* = '1'" ?
  OldParseCondition(pFieldMapping->csv.szCondition, szCondition, &pszLeftPart, &pszOperator, &pszRightPart);

  if (pszLeftPart && pszOperator && pszRightPart) {
    // condition found
    nLeftColumnIndex = GetColumnIndex(pszLeftPart);
    pszLeftValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, nCsvDataLine, nLeftColumnIndex);

    if (*pszRightPart == '\'') {
      pszRightValue = pszRightPart + 1;
      len = strlen(pszRightPart);
      if (len > 0 && pszRightPart[len-1] == '\'')
        pszRightPart[len-1] = '\0';
    }
    else {
      nRightColumnIndex = GetColumnIndex(pszRightPart);
      pszRightValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, nCsvDataLine, nRightColumnIndex);
    }

    if (pszLeftValue && pszRightValue) {
      if (*pszOperator == '=')
        bMatch = (strcmp(pszLeftValue, pszRightValue) == 0);
      if (*pszOperator == '!')
        bMatch = (strcmp(pszLeftValue, pszRightValue) != 0);
      if (*pszOperator == '<')
        bMatch = (strcmp(pszLeftValue, pszRightValue) < 0);
      if (*pszOperator == '>')
        bMatch = (strcmp(pszLeftValue, pszRightValue) > 0);
    }
  }

  return bMatch;
}

//--------------------------------------------------------------------------------------------------------

int AddXmlNodeField(xmlDocPtr pDoc, xmlNodePtr pParentNode, FieldMapping *pFieldMapping, int nXPathOffset, int nCsvDataLine)
{
  cpchar pCsvFieldValue = NULL;

  if (pFieldMapping->xml.cOperation == 'M'/*MAP*/) {
    if (pFieldMapping->csv.cOperation == 'F'/*FIX*/)
      SetNodeValue(pDoc, pParentNode, pFieldMapping->xml.szContent + nXPathOffset, pFieldMapping->csv.szContent, pFieldMapping);

    if (pFieldMapping->csv.cOperation == 'M'/*MAP*/) {
      pCsvFieldValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, nCsvDataLine, pFieldMapping->nCsvIndex);
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
    if (!ValidChars(aszValue[4], pCounterDef->szValidChars) || (int)strlen(aszValue[4]) > pCounterDef->nLength) {
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
      if (!ValidChars(aszValue[5], pCounterDef->szValidChars) || (int)strlen(aszValue[5]) > pCounterDef->nLength) {
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

  /*
  if (nPos == -1)
    ; // counter overflow
  */

  return 0;
}

//--------------------------------------------------------------------------------------------------------

int SaveCounterValues()
{
  int nCounterIndex, nReturnCode = 0;
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
	char *pEnd, *pPos = szVarDef;
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

int AddXmlField(xmlDocPtr pDoc, CPFieldMapping pFieldMapping, cpchar XPath, int nCsvDataLine, AttributeNameValueList *pAttributeNameValueList = NULL)
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
      pCsvFieldValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, nCsvDataLine, pFieldMapping->nCsvIndex);
      if (!pCsvFieldValue)
        pCsvFieldValue = szEmptyString;

      // no csv field value available, but default value defined ?
      if (!*pCsvFieldValue && *pFieldMapping->csv.szDefault)
        pCsvFieldValue = pFieldMapping->csv.szDefault;  // use default value

      if (pCsvFieldValue /* && (*pCsvFieldValue || pFieldMapping->xml.bMandatory)*/) {
        // check csv value and transform to xml format
        nReturnCode = MapCsvToXmlValue(pFieldMapping, pCsvFieldValue, szValue, MAX_VALUE_SIZE);
        SetNodeValue(pDoc, NULL, XPath, szValue, pFieldMapping, pAttributeNameValueList);
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

void GetMatchingLines(LinkedCsvFile *pLinkedCsvFile)
{
  int nDataLine = 0;
  pchar *pDataField = pLinkedCsvFile->aDataFields + pLinkedCsvFile->nLinkedColumnIndex;

  pLinkedCsvFile->nMatchingFirstLine = -1;
  pLinkedCsvFile->nMatchingLastLine = -1;

  if (pLinkedCsvFile->pszCurrentKeyValue != NULL)
  {
    while (nDataLine < pLinkedCsvFile->nRealDataLines) {
      if (strcmp(pLinkedCsvFile->pszCurrentKeyValue, *pDataField) == 0) {
        pLinkedCsvFile->nMatchingFirstLine = nDataLine;
        break;
      }
      pDataField += pLinkedCsvFile->nColumns;
      nDataLine++;
    }

    if (pLinkedCsvFile->nMatchingFirstLine >= 0) {
      pDataField += pLinkedCsvFile->nColumns;
      nDataLine++;
      while (nDataLine < pLinkedCsvFile->nRealDataLines) {
        if (strcmp(pLinkedCsvFile->pszCurrentKeyValue, *pDataField) != 0)
          break;
        pDataField += pLinkedCsvFile->nColumns;
        nDataLine++;
      }
      pLinkedCsvFile->nMatchingLastLine = nDataLine - 1;
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
	pchar pLeftPart = NULL;
	pchar pOperator = NULL;
	pchar pRightPart = NULL;
	CPFieldMapping pLookupFieldMapping = NULL;
  bool abFirstLoopRecord[MAX_LOOPS];
  AttributeNameValue *pAttrNameValue;
  AttributeNameValueList AttrNameValueList;
  int i, j, nVirtualDataLine, nMapIndex, nLoopIndex, nActiveCsvFileIndex, nNextCsvFileIndex;
  int nFirstLoopMapIndex = -1;
  int nResult = 0;
  bool bAddFieldValue, bMap, bMatch, bNewValue;
  LinkedCsvFile *pFirstCsvFile = aLinkedCsvFile;
  LinkedCsvFile *pLinkedCsvFile;
  LinkedCsvFile *pLookupCsvFile;

  AttrNameValueList.nCount = 0;
  AttrNameValueList.nMaxCount = 16;
  AttrNameValueList.aAttrNameValue = (AttributeNameValue*)malloc(AttrNameValueList.nMaxCount * sizeof(AttributeNameValue));

	// Creates a new document, a node and set it as a root node
  pXmlDoc = xmlNewDoc(BAD_CAST "1.0");
  pRootNode = xmlNewNode(NULL, BAD_CAST szRootNodeName);
	xmlDocSetRootElement(pXmlDoc, pRootNode);
  nLoops = 0;

  // <FundsXML4 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="file:///C:/Eigene%20Dateien/FundsXML/FundsXML_4.1.7/FundsXML_4.1.7ai.xsd">

  pAttrNameValue = RootAttrNameValueList.aAttrNameValue;
  for (i = 0; i < RootAttrNameValueList.nCount; i++) {
    xmlSetProp(pRootNode, (const xmlChar *)pAttrNameValue->szName, (const xmlChar *)pAttrNameValue->szValue);
    pAttrNameValue++;
  }
  /*
  //xmlSetProp(pRootNode, (const xmlChar *)pFieldMapping->xml.szAttribute, xmlEncodeSpecialChars(pDoc, (const xmlChar *)pValue));
  xmlSetProp(pRootNode, (const xmlChar *)"xmlns:xsi", (const xmlChar *)"http://www.w3.org/2001/XMLSchema-instance");
  xmlSetProp(pRootNode, (const xmlChar *)"xsi:noNamespaceSchemaLocation", (const xmlChar *)"file:///C:/Eigene%20Dateien/FundsXML/FundsXML_4.1.7/FundsXML_4.1.7ai.xsd");
  */

  // set first loop map index to end of field map if no loops existing in mapping definition
  if (nFirstLoopMapIndex < 0)
    nFirstLoopMapIndex = nFieldMappings;

  nActiveCsvFileIndex = 0;
  nVirtualDataLine = 0;

  // initialize line indices of csv files
  pLinkedCsvFile = aLinkedCsvFile;
  pLinkedCsvFile->nLinkedMainDataLine = -1;
  pLinkedCsvFile->nMatchingFirstLine = 0;
  pLinkedCsvFile->nMatchingLastLine = pLinkedCsvFile->nRealDataLines - 1;
  pLinkedCsvFile->nCurrentCsvLine = 0;
  pLinkedCsvFile->pszLastKeyValue = NULL;
  pLinkedCsvFile->pszCurrentKeyValue = NULL;

  for (i = 1; i < nLinkedCsvFiles; i++) {
    pLinkedCsvFile++;
    pLinkedCsvFile->nLinkedMainDataLine = -1;
    pLinkedCsvFile->pszLastKeyValue = NULL;
    pLinkedCsvFile->pszCurrentKeyValue = NULL;
    pLinkedCsvFile->nMatchingFirstLine = -1;
    pLinkedCsvFile->nMatchingLastLine = -1;
    pLinkedCsvFile->nCurrentCsvLine = -1;
  }

  // main loop over all csv lines/records in all csv input files
  //for (nDataLine = 0; nDataLine < nRealCsvDataLines; nDataLine++)
  while (nActiveCsvFileIndex >= 0)
  {
    //nCurrentCsvLine = nDataLine + 1;  // for csv error logging

    // initialize column error flags
    pLinkedCsvFile = aLinkedCsvFile;
    for (i = 0; i < nLinkedCsvFiles; i++) {
      for (j = pLinkedCsvFile->nColumns - 1; j >= 0; j--)
        pLinkedCsvFile->abColumnError[j] = false;
      pLinkedCsvFile++;
    }

    // update matching window of linked csv files (if necessary)
    pLinkedCsvFile = aLinkedCsvFile;
    for (i = 1; i < nLinkedCsvFiles; i++) {
      pLinkedCsvFile++;
      if (pLinkedCsvFile->nLinkedMainDataLine < 0 && i > nActiveCsvFileIndex && pLinkedCsvFile->nLinkedMainColumnIndex >= 0) {
        // has content of linked csv column changed ?
        pCsvFieldValue = GetCsvFieldValue(0, pFirstCsvFile->nCurrentCsvLine, pLinkedCsvFile->nLinkedMainColumnIndex);
        if (pCsvFieldValue != NULL && (pLinkedCsvFile->pszLastKeyValue == NULL || strcmp(pCsvFieldValue, pLinkedCsvFile->pszLastKeyValue) != 0)) {
          pLinkedCsvFile->nLinkedMainDataLine = pFirstCsvFile->nCurrentCsvLine;
          pLinkedCsvFile->pszCurrentKeyValue = pCsvFieldValue;
          GetMatchingLines(pLinkedCsvFile);
          pLinkedCsvFile->nCurrentCsvLine = pLinkedCsvFile->nMatchingFirstLine;
        }
      }
    }

    if (nVirtualDataLine == 0) {
      // initialize loop indices and last csv values
      nFirstLoopMapIndex = -1;
      pFieldMapping = aFieldMapping;
      for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
        if (pFieldMapping->xml.cOperation == 'L'/*LOOP*/ && nLoops < MAX_LOOPS && pFieldMapping->nCsvFileIndex >= 0 && pFieldMapping->nCsvFileIndex < nLinkedCsvFiles)
        {
          pLinkedCsvFile = aLinkedCsvFile + pFieldMapping->nCsvFileIndex;
          apLastValues[nLoops] = GetLinkedCsvFieldValue(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex);
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
        if (pLoopFieldMapping->nCsvIndex >= 0 && pLoopFieldMapping->nCsvFileIndex == nActiveCsvFileIndex) {
          // csv field name found in csv header line --> get content of field
          pLinkedCsvFile = aLinkedCsvFile + pLoopFieldMapping->nCsvFileIndex;
          pCsvFieldValue = GetLinkedCsvFieldValue(pLoopFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pLoopFieldMapping->nCsvIndex);
          if (pLoopFieldMapping->csv.cOperation == 'U' /*UNIQUE*/)
            bNewValue = IsNewCsvFieldValue(pLoopFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pLoopFieldMapping->nCsvIndex, pCsvFieldValue);
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
              pLinkedCsvFile = aLinkedCsvFile + pFieldMapping->nCsvFileIndex;
              apLastValues[i] = GetLinkedCsvFieldValue(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex);
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
      printf("\nLoop indices for virtual csv line %d:", nVirtualDataLine);
      for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++) {
        pFieldMapping = apLoopFieldMapping[nLoopIndex];
        printf("  %s: %d/%d", pFieldMapping->csv.szContent, anLoopIndex[nLoopIndex], abFirstLoopRecord[i]);
      }
      printf("\n");
    }

    // loop over all mapping fields
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++)
    {
      pLinkedCsvFile = aLinkedCsvFile + pFieldMapping->nCsvFileIndex;
      if (nVirtualDataLine == 0 || nMapIndex >= nFirstLoopMapIndex)
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
            pCsvFieldValue = GetCsvFieldValue(pFieldMapping->nCsvFileIndex, pLinkedCsvFile->nCurrentCsvLine, pFieldMapping->nCsvIndex);
            if (pCsvFieldValue) {
              if (strchr(",.[", pFieldMapping->csv.szFormat[0]) != NULL)  // originally '('
                bMatch = CheckRegex(pCsvFieldValue, pFieldMapping->csv.szFormat);  // e.g. AssetType matching "(EQ)" [containing "EQ"] ?
              else
                bMatch = (*pCsvFieldValue != '\0');  // field non-empty ?
            }
            if (bMatch && *pFieldMapping->csv.szCondition)
              bMatch = CheckCsvCondition(pLinkedCsvFile->nCurrentCsvLine, pFieldMapping);  // e.g. "CCY != FUND_CCY" or "48_* = '1'"
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
                bAddFieldValue = CheckCsvCondition(pLinkedCsvFile->nCurrentCsvLine, pFieldMapping);

              if (bAddFieldValue) {
                // add part of current xpath since last loop node
                if (strlen(pFieldMapping->xml.szContent) > strlen(szLastLoopXPath))
                  strcat(xpath, pFieldMapping->xml.szContent + strlen(szLastLoopXPath));

				        // xml condition like "@ccy = Funds/Fund/Currency" ?
                // or like "((@fromCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@toCcy = Funds/Fund/Currency) and (@mulDiv = 'D')) or ((@fromCcy = Funds/Fund/Currency) and (@toCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@mulDiv = 'M'))" ?
                if (pFieldMapping->xml.Condition.pComplexCondition || pFieldMapping->xml.Condition.pSimpleCondition) {
                  GetAttributeList(&AttrNameValueList, &pFieldMapping->xml.Condition);
                  for (i = 0; i < AttrNameValueList.nCount; i++)
                    if (AttrNameValueList.aAttrNameValue[i].pszXPath) {
					            pLookupFieldMapping = GetFieldMapping(AttrNameValueList.aAttrNameValue[i].pszXPath);
					            if (pLookupFieldMapping && pLookupFieldMapping->nCsvIndex >= 0 && pLookupFieldMapping->nCsvFileIndex >= 0 && pLookupFieldMapping->nCsvFileIndex < nLinkedCsvFiles) {
                        pLookupCsvFile = aLinkedCsvFile + pLookupFieldMapping->nCsvFileIndex;
                        pCsvFieldValue = GetCsvFieldValue(pLookupFieldMapping->nCsvFileIndex, pLookupCsvFile->nCurrentCsvLine, pLookupFieldMapping->nCsvIndex);
                        mystrncpy(AttrNameValueList.aAttrNameValue[i].szValue, pCsvFieldValue, MAX_ATTR_VALUE_SIZE);
					            }
                    }
				        }
                else
                  AttrNameValueList.nCount = 0;

                if (bTrace)
                  printf("Line %d / Field %d: CSV %c '%s' - XML %c '%s' Attr:%s Cond:%s\r\n", pLinkedCsvFile->nCurrentCsvLine, nMapIndex, pFieldMapping->csv.cOperation, pFieldMapping->csv.szContent, pFieldMapping->xml.cOperation, xpath, pFieldMapping->xml.szAttribute, pFieldMapping->xml.szCondition);

                // write value of mapped csv column to current xml document
                AddXmlField(pXmlDoc, pFieldMapping, xpath, pLinkedCsvFile->nCurrentCsvLine, &AttrNameValueList);
              }
            }
        }
      }
    }

    // increment csv line index (or switch to next csv file index)

    pLinkedCsvFile = aLinkedCsvFile + nActiveCsvFileIndex;
    if (nActiveCsvFileIndex > 0 && pLinkedCsvFile->nCurrentCsvLine < pLinkedCsvFile->nMatchingLastLine) {
      // goto next line within current linked csv file
    	pLinkedCsvFile->nCurrentCsvLine++;
    }
    else {
    	if (nActiveCsvFileIndex > 0) {
        // reset current linked csv file
        pLinkedCsvFile->nLinkedMainDataLine = -1;
        pLinkedCsvFile->pszLastKeyValue = pLinkedCsvFile->pszCurrentKeyValue;
        pLinkedCsvFile->pszCurrentKeyValue = NULL;
        pLinkedCsvFile->nMatchingFirstLine = -1;
        pLinkedCsvFile->nMatchingLastLine = -1;
        pLinkedCsvFile->nCurrentCsvLine = -1;
      }

      // search for next csv file
      nNextCsvFileIndex = nActiveCsvFileIndex + 1;
      while (nNextCsvFileIndex < nLinkedCsvFiles && aLinkedCsvFile[nNextCsvFileIndex].nCurrentCsvLine < 0) {
        aLinkedCsvFile[nNextCsvFileIndex].nLinkedMainDataLine = -1;
        nNextCsvFileIndex++;
      }

      if (nNextCsvFileIndex < nLinkedCsvFiles)
        nActiveCsvFileIndex = nNextCsvFileIndex;  // goto next csv file
      else {
      	nActiveCsvFileIndex = 0;  // go back to first/main csv file

        pLinkedCsvFile = aLinkedCsvFile + nActiveCsvFileIndex;
        if (pLinkedCsvFile->nCurrentCsvLine < pLinkedCsvFile->nMatchingLastLine)
          pLinkedCsvFile->nCurrentCsvLine++;  // goto next line within this csv file
        else
      	  nActiveCsvFileIndex = -1;  // all data processed
      }
    }

    nVirtualDataLine++;
  }

  if (bTrace)
    puts("");

  return nResult;
}
// end of function "GenerateXmlDocument"

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

int ConvertCsvToXml(cpchar szXmlFileName)
{
  // Convert data from csv to xml format
  // Input file(s): aLinkedCsvFile[i].szFileName
  // Output file: szXmlFileName
  //
	int i, nReturnCode;
  LinkedCsvFile *pLinkedCsvFile;

  nErrors = 0;
  *szLastError = '\0';
  ResetFieldIndices();

  // read csv input files
  pLinkedCsvFile = aLinkedCsvFile;
  for (i = 0; i < nLinkedCsvFiles; i++) {
    nReturnCode = ReadCsvData(i);
    printf("CSV file %d (%s) :  %d columns, %d real data lines\n", i+1, pLinkedCsvFile->szFileName, pLinkedCsvFile->nColumns, pLinkedCsvFile->nRealDataLines);
    pLinkedCsvFile++;
  }

  printf("XML output file: %s\n", szXmlFileName);
  printf("Error file: %s\n\n", szErrorFileName);

  nReturnCode = GetCsvDecimalPoint();

  /*
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
  */

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
// end of function "ConvertCsvToXml"

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

void UpdateCurrentConditionValue(SimpleCondition *pSimpleCondition, cpchar *aFieldValue)
{
  if (*pSimpleCondition->szRightPart != '\'') {
		CPFieldMapping pLookupFieldMapping = GetFieldMapping(pSimpleCondition->szRightPart);
		if (pLookupFieldMapping && pLookupFieldMapping->nCsvIndex >= 0)
      pSimpleCondition->szCurrentValue = (pchar)aFieldValue[pLookupFieldMapping->nCsvIndex];
  }
}

//--------------------------------------------------------------------------------------------------------

void UpdateCurrentConditionValues(Condition *pCondition, cpchar *aFieldValue)
{
  if (pCondition->pComplexCondition) {
    UpdateCurrentConditionValues(&pCondition->pComplexCondition->LeftCondition, aFieldValue);
    UpdateCurrentConditionValues(&pCondition->pComplexCondition->RightCondition, aFieldValue);
  }
  else
    UpdateCurrentConditionValue(pCondition->pSimpleCondition, aFieldValue);
}

//--------------------------------------------------------------------------------------------------------

int WriteCsvFile(cpchar szFileName)
{
  int i, nIndex, nColumnIndex, nMapIndex, nLoopIndex, nIncrementedLoopIndex; //j, nLoopMapIndex, nLoopEndMapIndex, nLoopFields, nLoopXPathLen, nXPathLen, nXPathOffset;
  int nReturnCode = 0;
  int nDataLine = 0;
  char *pColumnName = NULL;
  char *pFieldValueBufferPos = NULL;
  char szTempFieldValue[MAX_VALUE_SIZE];
  cpchar aFieldValue[MAX_CSV_COLUMNS];
  FILE *pFile = NULL;
  //errno_t error_code;
  xmlNodePtr pNode;
  char xpath[MAX_XPATH_SIZE];
  char xpath2[MAX_XPATH_SIZE];
	cpchar pCsvFieldName = NULL;
	cpchar pCsvFieldValue = NULL;
  cpchar pXmlFieldValue = NULL;
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
  bool bMap, bMatch, bLoopData;
  LinkedCsvFile *pLinkedCsvFile = aLinkedCsvFile;

  // open file in write mode
  //error_code = fopen_s(&pFile, szFileName, "w");
  pFile = fopen(szFileName, "w");
  if (!pFile) {
    //printf("Cannot open file '%s' (error code %d)\n", szFileName, error_code);
    printf("Cannot open file '%s'\n", szFileName);
    return -2;
  }

  // write template header to file
  fprintf(pFile, "%s\n", szCsvHeader);

  // write second header line, if existing
  if (*szCsvHeader2)
    fprintf(pFile, "%s\n", szCsvHeader2);

  // get root node of xml document
  xmlNodePtr pRootNode = xmlDocGetRootElement(pXmlDoc);

  // initialize loop indices and last csv values
  nLoops = 0;
  pFieldMapping = aFieldMapping;
  for (nMapIndex = 0; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
    // initialize loop indices
    pFieldMapping->nLoopIndex = -1;
    pFieldMapping->nRefUniqueLoopIndex = -1;

    if (pFieldMapping->xml.cOperation == 'L'/*LOOP*/ && nLoops < MAX_LOOPS) {
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

  while (bLoopData) {
    // next csv line
    nDataLine++;
    pLinkedCsvFile->nCurrentCsvLine = nDataLine;  // for csv error logging

    //char szFieldValueBuffer[MAX_LINE_SIZE];
    pFieldValueBufferPos = szFieldValueBuffer;
    szIgnoreXPath = NULL;

    // initialize list of csv field contents with empty strings
    for (i = 0; i < pLinkedCsvFile->nColumns; i++)
      aFieldValue[i] = szEmptyString;

    // initialize unique loop node indeces
    for (nLoopIndex = 0; nLoopIndex < nLoops; nLoopIndex++)
      if (apLoopFieldMapping[nLoopIndex]->csv.cOperation == 'U'/*UNIQUE*/)
        anLoopIndex[nLoopIndex] = -1;

    // reset csv and xml values in mapping definition
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
      pFieldMapping->csv.szValue = NULL;
      pFieldMapping->xml.szValue = NULL;
    }

    // get list of csv field contents
    for (nMapIndex = 0, pFieldMapping = aFieldMapping; nMapIndex < nFieldMappings; nMapIndex++, pFieldMapping++) {
      //if (strcmp(pFieldMapping->csv.szContent2, "67_*") == 0)
      //  bMap = false;  // for debugging purposes only!

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
        if (pFieldMapping->xml.cOperation == 'I'/*IF*/ /*&& pFieldMapping->nSourceMapIndex >= 0*/) {
          bMatch = false;
          if (!IsEmptyString(pFieldMapping->xml.szContent)) {
            GetXPathWithLoopIndices(pFieldMapping->xml.szContent, xpath);
            pNode = GetNode(pRootNode, xpath);
            bMatch = (pNode != NULL);
          }
          /*
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
          */
          if (!bMatch)
            szIgnoreXPath = pFieldMapping->xml.szContent;
        }

        if (pFieldMapping->nCsvIndex >= 0 && strchr("FMV"/*Fix,Map,Var*/, pFieldMapping->xml.cOperation))
        {
          GetXPathWithLoopIndices(pFieldMapping->xml.szContent, xpath);

				  // xml condition like "@ccy = Funds/Fund/Currency" ?
          // or like "((@fromCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@toCcy = Funds/Fund/Currency) and (@mulDiv = 'D')) or ((@fromCcy = Funds/Fund/Currency) and (@toCcy = Funds/Fund/FundDynamicData/Portfolios/Portfolio/Positions/Position/Currency) and (@mulDiv = 'M'))" ?
          if (pFieldMapping->xml.Condition.pComplexCondition || pFieldMapping->xml.Condition.pSimpleCondition)
            UpdateCurrentConditionValues(&pFieldMapping->xml.Condition, aFieldValue);

          pXmlFieldValue = NULL;
          //if (strcmp(pFieldMapping->csv.szContent, "CCY") == 0)
          //  i = 0;  // for debugging purposes only!

          // get content of xml field
          nReturnCode = GetNodeTextValue(pRootNode, xpath, &pXmlFieldValue, &pFieldMapping->xml.Condition);
          if (pXmlFieldValue) {
            *pFieldValueBufferPos = '\0';
            // map content of xml field to csv format
            nReturnCode = MapXmlToCsvValue(pFieldMapping, pXmlFieldValue, xpath, pFieldValueBufferPos, MAX_LINE_LEN - (pFieldValueBufferPos - szFieldValueBuffer + 2));

            if (pFieldMapping->nCsvIndex >= 0 && pLinkedCsvFile->abColumnQuoted[pFieldMapping->nCsvIndex] && strlen(pFieldValueBufferPos) < MAX_VALUE_SIZE) {
              // add quotes at the beginning and end of csv value
              strcpy(szTempFieldValue, pFieldValueBufferPos);
              strcpy(pFieldValueBufferPos, "\"");
              strcat(pFieldValueBufferPos, szTempFieldValue);
              strcat(pFieldValueBufferPos, "\"");
            }

            // add content of csv field to csv buffer
            aFieldValue[pFieldMapping->nCsvIndex] = pFieldValueBufferPos;
            pFieldValueBufferPos += strlen(pFieldValueBufferPos) + 1;

            // is content of this field used for unique loop ?
            if (pFieldMapping->nRefUniqueLoopIndex >= 0 && anLoopIndex[pFieldMapping->nRefUniqueLoopIndex] < 0) {
              pLoopFieldMapping = apLoopFieldMapping[pFieldMapping->nRefUniqueLoopIndex];
				      pUniqueFieldMapping = pLoopFieldMapping + 1;  // mapping for content of unique field
              pCsvFieldValue = aFieldValue[pFieldMapping->nCsvIndex];
              nIndex = GetUniqueLoopNodeIndex(pLoopFieldMapping, pUniqueFieldMapping, pCsvFieldValue, anLoopSize[pLoopFieldMapping->nLoopIndex]);
				      if (nIndex >= 0)
					      anLoopIndex[pLoopFieldMapping->nLoopIndex] = nIndex;
			      }

            if (bTrace)
              printf("Line %d: MapIndex %d, Column %d, Field %s, Value '%s', XPath %s\n", nDataLine, nMapIndex, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, pXmlFieldValue, xpath);
          }
          else
            if (pFieldMapping->xml.bMandatory)
              LogXmlError(0, nDataLine, pFieldMapping->nCsvIndex, pFieldMapping->csv.szContent, xpath, szEmptyString, "Mandatory field/node missing");
        }

			  if (pFieldMapping->nCsvIndex >= 0 && pFieldMapping->csv.cOperation == 'U'/*UNIQUE*/ && anLoopIndex[pFieldMapping->nLoopIndex] < 0) {
				  pUniqueFieldMapping = pFieldMapping + 1;  // mapping for content of unique field
          pCsvFieldValue = aFieldValue[pFieldMapping->nCsvIndex];
          nIndex = GetUniqueLoopNodeIndex(pFieldMapping, pUniqueFieldMapping, pCsvFieldValue, anLoopSize[pFieldMapping->nLoopIndex]);
				  if (nIndex >= 0)
					  anLoopIndex[pFieldMapping->nLoopIndex] = nIndex;
			  }
      }
		}

    // fill line buffer with field contents
    strcpy(szLineBuffer, aFieldValue[0]);
    for (i = 1; i < pLinkedCsvFile->nColumns; i++)
      if (strlen(szLineBuffer) + strlen(aFieldValue[i]) < MAX_LINE_LEN)
        sprintf(szLineBuffer + strlen(szLineBuffer), "%c%s", cColumnDelimiter, aFieldValue[i]);
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

  pLinkedCsvFile->nRealDataLines = nDataLine;
  return nReturnCode;
}
// end of funtion "WriteCsvFile"

//--------------------------------------------------------------------------------------------------------

int ConvertXmlToCsv(cpchar szXmlFileName, cpchar szCsvTemplateFileName, cpchar szCsvResultFileName)
{
	int i, j, nReturnCode;
  cpchar pszValue = NULL;
  //char szValue[MAX_VALUE_SIZE];

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
  LinkedCsvFile *pCsvFile = aLinkedCsvFile;
  ResetFieldIndices();
  nReturnCode = ReadCsvData(0);  // szCsvTemplateFileName);
  printf("CSV Data Columns: %d\nCSV Template Lines: %d\n\n", pCsvFile->nColumns, pCsvFile->nRealDataLines);

  nReturnCode = GetCsvDecimalPoint();

  if (bTrace) {
    pchar *pField = pCsvFile->aDataFields;
    for (i = 0; i < pCsvFile->nDataLines; i++) {
      for (j = 0; j < pCsvFile->nColumns; j++) {
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

  printf("CSV Data Columns: %d\nCSV Data Lines: %d\n\n", pCsvFile->nColumns, pCsvFile->nRealDataLines);
  printf("Number of errors detected: %d\n\n", nErrors);

  xmlFreeDoc(pXmlDoc);  // free the xml document
  xmlCleanupParser();  // free the global variables that may have been allocated by the parser

  return nReturnCode;
}
// end of function "ConvertXmlToCsv"

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
	int i, nReturnCode, nFiles = 0;
  char szConversion[8] = "";  //"csv2xml";
  int nInputFileDirs = 0;
  FileName aInputFileDir[MAX_LINKED_CSV_FILES];
  char szInput[MAX_FILE_NAME_SIZE] = "";
  char szInputFileName[MAX_FILE_NAME_SIZE] = "";
  char szInputPath[MAX_FILE_NAME_SIZE] = "";
  char szLogFileName[MAX_FILE_NAME_SIZE] = "";  //"log.csv";
  char szMappingFileName[MAX_FILE_NAME_SIZE] = "";  //"mapping.csv";
  char szTemplateFileName[MAX_FILE_NAME_SIZE] = "";  //"template.csv";
  int nOutputFileDirs = 0;
  FileName aOutputFileDir[MAX_LINKED_CSV_FILES];
  char szOutput[MAX_FILE_NAME_SIZE] = "";
  char szOutputFileName[MAX_FILE_NAME_SIZE] = "";
  char szOutputPath[MAX_FILE_NAME_SIZE] = "";
  char szError[MAX_FILE_NAME_SIZE] = "";  //"errors.csv";
  char szProcessed[MAX_FILE_NAME_SIZE] = "";
  char szProcessedFileName[MAX_FILE_NAME_SIZE] = "";
  //char szErrorFileName[MAX_FILE_NAME_SIZE] = "";
  cpchar pcParameter = NULL;
  cpchar pcContent = NULL;
  pchar pStarPos = NULL;
  bool bMissingParameter = false;
  bool bParameterProcessed;
  bool bWaitAtEnd = false;
  //LinkedCsvFile *pLinkedCsvFile;

  puts("");
  puts("FundsXML-CSV-Converter Version 1.05 from 13.06.2021");
  puts("Open source command line tool for the FundsXML community");
  puts("Source code is available under the MIT open source licence");
  puts("on GitHub: https://github.com/peterraf/csv-xml-converter and http://www.xml-tools.net");
  puts("Written by Peter Raffelsberger (peter@raffelsberger.net)");
  puts("");

  // Parameters:
  //
  // SINGLE FILE CONVERSIONS:
  //
  // NAV mappings:
  // convert -conversion csv2xml -input input.csv -mapping mapping.csv -output result.xml -errors errors.csv
  // convert -c c2x -i input.csv -m mapping.csv -o result.xml -e errors.csv
  //
  // convert -conversion xml2csv -input input.xml -mapping mapping.csv -template template.csv -output result.csv -errors errors.csv
  // convert -c x2c -i nav-input.xml -m nav-mapping.csv -t nav-template.csv -o nav-output.csv -e nav-errors.csv
  //
  // HOLDINGS mappings:
  // convert -c c2x -i holdings.csv -m holdings-mapping.csv -o holdings.xml -e holdings-errors.csv
  // convert -c x2c -i holdings2.xml -m holdings-mapping.csv -t holdings-template.csv -o holdings2.csv -e holdings2-errors.csv
  //
  // ROBECO mappings:
  // convert -c c2x -i robeco-holdings.csv -m robeco-holdings-mapping.csv -o robeco-holdings.xml -e robeco-holdings-errors.csv
  //
  // COLLATERAL HOLDINGS mappings:
  // convert -c c2x -i collateral-input.csv -m collateral-mapping.csv -o collateral-output.xml -e collateral-errors.csv
  //
  // OPENFUNDS mappings:
  // convert -c c2x -i openfunds\openfunds-input.csv -m openfunds\openfunds-mapping.csv -o openfunds\openfunds-output.xml -e openfunds\openfunds-csv-errors.csv
  // convert -c x2c -i openfunds\openfunds-input.xml -m openfunds\openfunds-mapping.csv -t openfunds\openfunds-template.csv -o openfunds\openfunds-output.csv -e openfunds\openfunds-xml-errors.csv
  //
  // EMT mappings:
  // convert -c c2x -i emt-input.csv -m emt-mapping.csv -o emt-output.xml -e emt-errors.csv
  // convert -c x2c -i emt-input.xml -m emt-mapping.csv -t emt-template.csv -o emt-output.csv -e emt-errors.csv
  //
  // EMT-V3 mappings:
  // convert -conversion csv2xml -input emt3-input.csv -mapping emt3-mapping.csv -output emt3-output.xml -errors emt3-errors.csv
  // convert -c c2x -i emt3-input.csv -m emt3-mapping.csv -o emt3-output.xml -e emt3-errors.csv
  //
  // EMT mappings (openfunds version):
  // convert -c c2x -i openfunds-emt-input.csv -m openfunds-emt-mapping.csv -o openfunds-emt-output.xml -e openfunds-emt-csv-errors.csv
  // convert -c x2c -i openfunds-emt-input.xml -m openfunds-emt-mapping.csv -t openfunds-emt-template.csv -o openfunds-emt-output.csv -e openfunds-emt-xml-errors.csv
  //
  // TPT mappings:
  // convert -c c2x -i tpt-input.csv -m tpt-mapping.csv -o tpt-output.xml -e tpt-errors.csv
  // convert -c x2c -i tpt-input.xml -m tpt-mapping.csv -t tpt-template.csv -o tpt-output.csv -e tpt-errors2.csv
  //
  // TPT to HOLDINGS mappings:
  // convert -c c2x -i tpt-holdings-input.csv -m tpt-holdings-mapping.csv -o tpt-holdings-output.xml -e tpt-holdings-errors.csv
  // convert -c x2c -i tpt-holdings-input.xml -m tpt-holdings-mapping.csv -t tpt-holdings-template.csv -o tpt-holdings-output.csv -e tpt-holdings-errors2.csv
  //
  // TPT to HOLDINGS mappings (plus share class data):
  // convert -c c2x -i tpt-holdings-input.csv -i tpt-shareclasses.csv -m tpt-holdings-sc-mapping.csv -o tpt-holdings-sc-output.xml -e tpt-holdings-sc-errors.csv
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
    bParameterProcessed = false;

    if (stricmp(pcParameter, "-TRACE") == 0) {
      // trace details of processing
      bTrace = true;
      bParameterProcessed = true;
    }

    if ((stricmp(pcParameter, "-WAIT") == 0 || stricmp(pcParameter, "-W") == 0)) {
      // wait at end of processing
      bWaitAtEnd = true;
      bParameterProcessed = true;
    }

    if (!bParameterProcessed && *pcParameter++ == '-' && i < argc - 1)
    {
      pcContent = argv[++i];
      if (strlen(pcContent) > MAX_FILE_NAME_LEN) {
        printf("Content of command line parameter '%s' is too long (maximum length is %d characters)\n", pcParameter, MAX_FILE_NAME_LEN);
        goto ProcEnd;
      }
      else {
        // conversion direction
        if (stricmp(pcParameter, "CONVERSION") == 0 || stricmp(pcParameter, "C") == 0) {
          if (stricmp(pcContent, "csv2xml") == 0 || stricmp(pcContent, "c2x") == 0)
            strcpy(szConversion, "csv2xml");
          if (stricmp(pcContent, "xml2csv") == 0 || stricmp(pcContent, "x2c") == 0)
            strcpy(szConversion, "xml2csv");
        }

        // input file name or directory
        if ((stricmp(pcParameter, "INPUT") == 0 || stricmp(pcParameter, "I") == 0) && nInputFileDirs < MAX_LINKED_CSV_FILES)
          strcpy(aInputFileDir[nInputFileDirs++], pcContent);

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
        if ((stricmp(pcParameter, "OUTPUT") == 0 || stricmp(pcParameter, "O") == 0) && nOutputFileDirs < MAX_LINKED_CSV_FILES)
          strcpy(aOutputFileDir[nOutputFileDirs++], pcContent);

        // log file name or directory
        if (stricmp(pcParameter, "ERRORS") == 0 || stricmp(pcParameter, "E") == 0)
          strcpy(szError, pcContent);

        // directory for processed files
        if ((stricmp(pcParameter, "PROCESSED") == 0 || stricmp(pcParameter, "P") == 0) && strlen(pcContent) < MAX_PATH_LEN)
          strcpy(szProcessed, pcContent);

        // directory for counter files
        if ((stricmp(pcParameter, "COUNTER") == 0 || stricmp(pcParameter, "R") == 0) && strlen(pcContent) < MAX_PATH_LEN)
          sprintf(szCounterPath, "%s%c", pcContent, cPathSeparator);
      }
    }
  }

  if (!*szConversion) {
    puts("Missing or invalid parameter 'conversion' for conversion direction (valid options: 'csv2xml','c2x','xml2csv','x2c')");
    bMissingParameter = true;
  }

  if (nInputFileDirs == 0) {
    puts("Missing parameter 'input' for input file(s)");
    bMissingParameter = true;
  }

  if (!*szMappingFileName) {
    puts("Missing parameter 'mapping' for mapping definition");
    bMissingParameter = true;
  }

  if (nOutputFileDirs == 0) {
    puts("Missing parameter 'output' for output file(s)");
    bMissingParameter = true;
  }

  if (stricmp(szConversion, "xml2csv") == 0 && !*szTemplateFileName) {
    puts("Missing parameter 'template' for template csv file");
    bMissingParameter = true;
  }

  if (bMissingParameter)
    goto ProcEnd;

  strcpy(szInput, aInputFileDir[0]);  // first/main input file
  strcpy(szOutput, aOutputFileDir[0]);  // first/main output file

  if (*szLogFileName) {
    // check existance of log file (and write header if not existing)
    nReturnCode = CheckLogFileHeader(szLogFileName);
    if (nReturnCode != 0)
      goto ProcEnd;

    // log start of processing
    AddLog(szLogFileName, "START", szConversion, "", 0, 0, szInput, szMappingFileName, szTemplateFileName, szOutput, szProcessed, "", "");
  }

  // name of mapping error file
  strcpy(szMappingErrorFileName, szMappingFileName);
  ReplaceExtension(szMappingErrorFileName, "err");

  // read mapping file
  printf("Mapping definition: %s\n", szMappingFileName);
  nReturnCode = ReadFieldMappings(szMappingFileName);
  printf("Field mappings loaded: %d\n\n", nFieldMappings);

  if (nReturnCode <= 0) {
    AddLog(szLogFileName, "ERROR", szConversion, szLastError, nErrors, nFieldMappings, szInput, szMappingFileName, szTemplateFileName, szOutput, szProcessed, szUniqueDocumentID, szError);
    goto ProcEnd;
  }

  // single or multiple file conversions ?
  pStarPos = strchr(szInput, '*');

  if (pStarPos) {
    // multiple file conversion
    ExtractPath(szInputPath, szInput);
    ExtractPath(szOutputPath, szOutput);

//#ifdef _WIN32
    // Windows version:

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

            // convert csv file to xml format
            nReturnCode = ConvertCsvToXml(szOutputFileName);
						MyMoveFile(szInputFileName, szProcessedFileName);

            // add log entry to application log
            AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, aLinkedCsvFile[0].nRealDataLines, aLinkedCsvFile[0].szFileName, szMappingFileName, "", szOutputFileName, szProcessedFileName, szUniqueDocumentID, szErrorFileName);
          }

          if (stricmp(szConversion, "xml2csv") == 0) {
            // convert from csv to xml format
            convDir = XML2CSV;
            ReplaceExtension(szOutputFileName, "csv");
            FindUniqueFileName(szOutputFileName);
						FindUniqueFileName(szProcessedFileName);
						ReplaceExtension(szErrorFileName, "csv");
            FindUniqueFileName(szErrorFileName);

            // convert xml file to csv format
            nReturnCode = ConvertXmlToCsv(szInputFileName, szTemplateFileName, szOutputFileName);
						MyMoveFile(szInputFileName, szProcessedFileName);

            // add log entry to application log
            AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, aLinkedCsvFile[0].nRealDataLines, szInputFileName, szMappingFileName, szTemplateFileName, szOutputFileName, szProcessedFileName, szUniqueDocumentID, szErrorFileName);
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

    if (stricmp(szConversion, "csv2xml") == 0) {
      // conversion from csv to xml format
      convDir = CSV2XML;

      nLinkedCsvFiles = nInputFileDirs;
      for (i = 0; i < nInputFileDirs; i++)
        strcpy(aLinkedCsvFile[i].szFileName, aInputFileDir[i]);

      // convert data from csv to xml format
      strcpy(szErrorFileName, szError);
      nReturnCode = ConvertCsvToXml(szOutput);

      // add log entry to application log
      AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, aLinkedCsvFile[0].nRealDataLines, aLinkedCsvFile[0].szFileName, szMappingFileName, "", szOutput, "", szUniqueDocumentID, szErrorFileName);
    }

    if (stricmp(szConversion, "xml2csv") == 0) {
      // conversion from xml to csv format
      convDir = XML2CSV;

      // convert data from xml to csv format
      strcpy(szErrorFileName, szError);
      nReturnCode = ConvertXmlToCsv(szInput, szTemplateFileName, szOutput);

      // add log entry to application log
      AddLog(szLogFileName, "FILE", szConversion, szLastError, nErrors, aLinkedCsvFile[0].nRealDataLines, szInput, szMappingFileName, "", szOutput, "", szUniqueDocumentID, szErrorFileName);
    }
  }

ProcEnd:
  if (bWaitAtEnd) {
    puts("\nPress <Enter> to continue/close window\n");
    getchar();
  }

	return 0;
}

#ifdef __cplusplus
}
#endif
