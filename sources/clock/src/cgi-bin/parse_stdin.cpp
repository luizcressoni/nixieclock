/*! \file parse_stdin.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "parse_stdin.h"

char *buffer = nullptr;

//The forms on the page are a few hundred bytes. Anything far beyond that is not one of them.
#define MAX_CONTENT_LENGTH  (64 * 1024)
#define MAX_FIELD_LENGTH    255


/*! \brief Copies the value of a multipart field into a caller buffer
    \param fieldName Name of the form field.
    \param fieldValue Receives the value, always zero terminated; empty when the field is absent.
    \param fieldValueSize Size of fieldValue. Longer values are truncated, never overflowed.
    \note This runs as root behind an open hotspot: the size is the whole point of the function.
*/
void extractField(const char* fieldName, char* fieldValue, size_t fieldValueSize) {
    if(fieldValueSize == 0)
        return;
    fieldValue[0] = '\0';
    if(buffer == nullptr)
        return;

    char fieldString[256];
    snprintf(fieldString, sizeof(fieldString), "name=\"%s\"\r\n\r\n", fieldName);

    const char* fieldStart = strstr(buffer, fieldString);
    if (fieldStart == nullptr)
        return;
    fieldStart += strlen(fieldString);

    const char* fieldEnd = strstr(fieldStart, "\r\n--");
    if (fieldEnd == nullptr)
        return;

    size_t fieldValueLength = fieldEnd - fieldStart;
    if(fieldValueLength > fieldValueSize - 1)
        fieldValueLength = fieldValueSize - 1;
    memcpy(fieldValue, fieldStart, fieldValueLength);
    fieldValue[fieldValueLength] = '\0';
}


char *parse_stdin()
{
    const char* contentLengthStr = getenv("CONTENT_LENGTH");
    if (contentLengthStr == nullptr)
        return nullptr;

    const long contentLength = strtol(contentLengthStr, nullptr, 10);
    if (contentLength <= 0 || contentLength > MAX_CONTENT_LENGTH)
        return nullptr;

    buffer = new char[contentLength + 1];
    if (fread(buffer, 1, contentLength, stdin) != static_cast<size_t>(contentLength)) {
        free_buffer();
        return nullptr;
    }
    buffer[contentLength] = '\0';
    return buffer;
}

char *get_form_name()
{
    static char formname[MAX_FIELD_LENGTH + 1];
    extractField("form", formname, sizeof(formname));
    return formname;
}

char *get_field_value(const char *_fieldname)
{
    static char fieldvalue[MAX_FIELD_LENGTH + 1];
    extractField(_fieldname, fieldvalue, sizeof(fieldvalue));
    return fieldvalue;
}

void free_buffer()
{
    delete []buffer;
    buffer = nullptr;
}


//eof parse_stdin.cpp
