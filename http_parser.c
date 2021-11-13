/**************************************************************
*
*                       http_parser.c
*
*     Final Project: High Performance HTTP Proxy
*     Author:  Keren Zhou (kzhou), Ruiyuan Gu (rgu03)
*     Date: 2021-11-12
*
*     Summary:
*     Implementation for http parser.
*
**************************************************************/

#include "http_parser.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Parse HTTP request/response and extract its head and body.
 *
 * @param buf Byte buffer should contain a whole HTTP request/response head and
 * may contain a whole/partial/empty entity body.
 * @param n Byte size of the buffer.
 * @param out_head Output pointer to a null-terminated string copy of head
 * without the empty line.
 * If the empty line between head and body is not found, *out_head will remain
 * its original value.
 * @param out_head_len Output; byte size of head.
 * @param out_body Output pointer to a null-terminated string copy of entity
 * body.
 * It may not be the whole entity body, or even be an empty string.
 * If empty line between head and body is not found, *out_body will remain its
 * original value.
 * @param out_body_len Output; byte size of body.
 */
void parse_body_head(const char* buf,
                     const int n,
                     char** out_head,
                     int* out_head_len,
                     char** out_body,
                     int* out_body_len)
{
    char* pos;
    unsigned size;

    if (buf == NULL) {
        return;
    }

    /* Find the empty line between head and body. */
    pos = strstr(buf, "\r\n\r\n");
    /* Invalid response; End of head is not found. */
    if (pos == NULL) {
        return;
    }

    /* Copy response head without the empty line. */
    pos += strlen("\r\n"); /* End of head. */
    size = pos - buf;
    *out_head = malloc(size + 1);
    if (*out_head == NULL) {
        PLOG_ERROR("malloc");
        return;
    }
    memcpy(*out_head, buf, size);
    (*out_head)[size] = '\0';
    *out_head_len = size;

    /* Skip the empty line between the head and body. */
    pos += strlen("\r\n"); /* End of empty line and start of body. */

    /* Copy response body. */
    size = n - size - strlen("\r\n");
    *out_body = malloc(size + 1);
    if (*out_body == NULL) {
        PLOG_ERROR("malloc");
        return;
    }
    memcpy(*out_body, pos, size);
    (*out_body)[size] = '\0';
    *out_body_len = size;
}

/**
 * Get substring before the first delimiter in the string.
 *
 * @param str String to get prefix from.
 * @param delim Delimiter string right after the prefix.
 * @param out_prefix Output pointer to a string copy of prefix.
 * If string starts with delimiter, *out_prefix will be an empty string.
 * If delimiter is not found, out_prefix will remain.
 * @return Pointer to the char right after the delimiter. NULL if delimiter is
 * not found.
 */
char* get_prefix(const char* str, const char* delim, char** out_prefix)
{
    int len;
    char* end;

    /* Empty string. */
    if (str == NULL) {
        return NULL;
    }

    end = strstr(str, delim);
    /* Prefix is not found. */
    if (end == NULL) {
        return NULL;
    }
    /* `end` points to the end of prefix and the beginning of the first
     * delimiter. */
    len = end - str;
    *out_prefix = malloc(len + 1);
    if (out_prefix == NULL) {
        PLOG_ERROR("malloc");
        return NULL;
    }
    strncpy(*out_prefix, str, len);
    (*out_prefix)[len] = '\0';
    end += strlen(delim); /* End of delimiter. */
    return end;
}

/**
 * @brief Parse the given HTTP request line and extract method, url and version 
 * fields.
 *
 * @param line String that starts with HTTP request line to parse. It may
 * contain other contents after the request line.
 * @param out_method Output pointer to a string copy of method field.
 * @param out_url Output pointer to a string copy of url field.
 * @param out_version Output pointer to a string copy of version field.
 * @return Length of request line including "\r\n"; -1 if the given request line
 * is invalid.
 */
int parse_request_line(const char* line,
                       char** out_method,
                       char** out_url,
                       char** out_version)
{
    char* st; /* Start of a field. */

    /* Extract method field. */
    st = get_prefix(line, " ", out_method);
    /* " " is not found. */
    if (st == NULL) {
        return -1;
    }
    
    /* Extract url field. */
    st = get_prefix(st, " ", out_url);
    /* " " is not found. */
    if (st == NULL) {
        return -1;
    }

    /* Extract version field. */
    st = get_prefix(st, "\r\n", out_version);
    /* "\r\n" is not found. */
    if (st == NULL) {
        return -1;
    }
    /* `st` points to the end of `line`. */

    return st - line;
}

/**
 * @brief Parse the given HTTP header line and extract field name and value.
 *
 * @param line String that starts with a HTTP header line to parse. It may
 * contain other content after the header line.
 * @param out_name Output pointer to a string copy of field name.
 * @param out_value Output pointer to a string copy of field value.
 * @return Length of header line including "\r\n"; -1 if the given request line
 * is invalid.
 */
int parse_header_line(const char* line, char** out_name, char** out_value)
{
    char* st; /* Start of a field. */

    /* Extract field name. */
    st = get_prefix(line, ": ", out_name);
    /* ": " is not found. */
    if (st == NULL) {
        return -1;
    }

    /* Extract field value. */
    st = get_prefix(st, "\r\n", out_value);
    /* "\r\n" is not found. */
    if (st == NULL) {
        return -1;
    }
    /* `st` points to the end of `line`. */

    return st - line;
}

/**
 * @brief Parse the given HTTP request and extract method, url, version and host
 * fields.
 *
 * @param request HTTP request to parse.
 * @param out_method Output pointer to a null-terminated string as a copy of
 * method field in the request.
 * @param out_url Output pointer to a null-terminated string as a copy of url
 * field in the request.
 * @param out_version Output pointer to a null-terminated string as a copy of
 * version field in the request.
 * @param host Output pointer to a null-terminated string as a copy of host
 * field in the requst.
 */
void parse_request_head(const char* request,
                        char** out_method,
                        char** out_url,
                        char** out_version,
                        char** out_host)
{
    const char* st = request; /* Start of the part to parse. */
    const char* end = request + strlen(request); /* End of request. */
    int len = 0; /* Byte size of the last parsed part. */
    char* name = NULL; /* Field name of a header line. */
    char* value = NULL; /* Field value of a header line. */

    /* Parse request line. */
    len = parse_request_line(st, out_method, out_url, out_version);

    /* Parse each header line. */
    st += len; /* End of request line. */
    while (st < end) {
        len = parse_header_line(st, &name, &value);
        if (name != NULL && strcmp(name, "Host") == 0) {
            *out_host = value;
            value = NULL;
        }
        free(name);
        name = NULL;
        free(value);
        value = NULL;
        st += len;
    }
}

/**
 * @brief Parse the given host field value of an HTTP request, and extract
 * hostname and port number.
 *
 * @param host Host field value in an HTTP request. It may not contain port
 * number.
 * @param out_hostname Output pointer to a string copy of hostname without port
 * number.
 * @param out_port Output pointer to an integer copy of port number.
 * If port number is not specified in host field, out_port remains its original
 * value.
 */
void parse_host_field(const char* host, char** out_hostname, int* out_port)
{
    char* st; /* Start of port number. */

    st = get_prefix(host, ":", out_hostname);
    /* No ":" is found. */
    if (st == NULL) {
        *out_hostname = strdup(host);
        /* out_port remains. */
        return;
    }
    /* Convert substring after the first ":" to port number in integer. */
    if (st < host + strlen(host)) {
        *out_port = atoi(st);
    }
    /* If the first ":" is the last char, out_port will remain.*/
}

/**
 * @brief Parse the given HTTP status line and extract version, status code and 
 * phrase fields.
 *
 * @param line String that starts with HTTP status line to parse. It may contain
 * other contents after the status line.
 * @param out_version Output pointer to a string copy of version field.
 * @param out_status_code Output pointer to an integer copy of status code
 * field.
 * @param out_phrase Pointer to a string copy of phrase field.
 * @return Length of status line including "\r\n"; -1 if the given request line
 * is invalid.
 */
int parse_status_line(const char* line,
                      char** out_version,
                      int* out_status_code,
                      char** out_phrase)
{
    char* st;
    char* status_code;
    
    /* Extract version field. */
    st = get_prefix(line, " ", out_version);
    /* " " is not found. */
    if (*out_version == NULL) {
        return -1;
    }

    /* Extract status code field. */
    st = get_prefix(st, " ", &status_code);
    /* " " is not found. */
    if (status_code == NULL) {
        return -1;
    }
    *out_status_code = atoi(status_code);
    free(status_code);
    status_code = NULL;

    /* Extract phrase field. */
    st = get_prefix(st, "\r\n", out_phrase);
    /* "\r\n" is not found. */
    if (*out_phrase == NULL) {
        return -1;
    }

    return st - line;
}


/**
 * @brief Parse the given HTTP response, extract version, status code, phrase, 
 * content length and cache_control fields.
 *
 * @param response String that starts with HTTP response to parse. It contains
 * the whole response header. But it may not contain the whole entity body.
 * @param out_version Output pointer to a string copy of version field.
 * @param out_status_code Output pointer to an integer copy of status code
 * field.
 * @param out_phrase Output pointer to a string copy of phrase field.
 * @param out_content_length Output pointer to a string copy of content length
 * field.
 * @param out_cache_control Output pointer to a string copy of cache control
 * field.
 */
void parse_response_head(const char* response,
                         char** out_version,
                         int* out_status_code,
                         char** out_phrase,
                         int* out_content_length,
                         char** out_cache_control)
{
    const char* st = response; /* Start of the part to parse. */
    const char* end = response + strlen(response); /* End of response. */
    int len = 0; /* Byte size of the last parsed part. */
    char* name = NULL; /* Field name of a header line. */
    char* value = NULL; /* Field value of a header line. */

    /* Parse status line. */
    len = parse_status_line(response, out_version, out_status_code, out_phrase);

    /* Parse each header line. */
    st += len; /* End of status line. */
    while (st < end) {
        len = parse_header_line(st, &name, &value);
        if (name != NULL) {
            if (strcmp(name, "Content-Length") == 0) {
                *out_content_length = atoi(value);
            }
            else if (strcmp(name, "Cache-Control") == 0) {
                *out_cache_control = value;
                value = NULL;
            }
        }
        free(name);
        name = NULL;
        free(value);
        value = NULL;
        st += len;
    }
}

/**
 * @brief Parse the given cache control field and extract the integer after 
 * "max-age=".
 *
 * @param cache_control String of cache control field.
 * @param out_max_age Ouput; Integer after "max-age=".
 * If "max-age=" is not found, *out_max_age will remain its original value.
 * @return 
 */
void parse_cache_control(const char* cache_control, int* out_max_age)
{
    char* pos;
    static const char* prefix = "max-age=";

    if (cache_control == NULL) {
        return;
    }

    pos = strstr(cache_control, prefix);
    /* "max-age=" is not found. */
    if (pos == NULL) {
        return;
    }
    pos += strlen(prefix); /* End of "max-age=". */
    /* Empty after "max-age=". */
    if (pos >= cache_control + strlen(cache_control)) {
        return;
    }
    *out_max_age = atoi(pos);
}