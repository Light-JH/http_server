#pragma once

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct HttpCheckResult
{
    // 400 = invalid request
    // 404 = invalid route (not /exec/<cmd>)
    // 200 = valid execution
    int code;
    // The actual url to use for execution (only filled in for GET)
    char *url;
};

void checkHttpRequest(char *buffer, int len, struct HttpCheckResult *result)
{
    // zero out the result
    memset(result, 0, sizeof(struct HttpCheckResult));

    char *current = buffer;
    int line_count = 0;
    while (current - buffer < len)
    {
        char *line_end = strstr(current, "\r\n");
        if (line_end != NULL)
        {
            *line_end = '\0'; // add a null-terminator so we can print more easily
        }

        // Request line
        if (line_count == 0)
        {
            char *method = strtok(current, " ");
            char *url = strtok(NULL, " ");
            char *version = strtok(NULL, " ");
            char *next = strtok(NULL, "");
            // Wrong number of fields in request line
            if (method == NULL || url == NULL || version == NULL || next != NULL)
            {
                printf("wrong number of fields in request\n");
                result->code = 400;
                return;
            }

            // Check if method if valid
            if (!(
                    0 == strcmp(method, "GET") ||
                    0 == strcmp(method, "HEAD") ||
                    0 == strcmp(method, "POST") ||
                    0 == strcmp(method, "PUT") ||
                    0 == strcmp(method, "DELETE") ||
                    0 == strcmp(method, "CONNECT") ||
                    0 == strcmp(method, "OPTIONS") ||
                    0 == strcmp(method, "TRACE")))
            {
                printf("invalid method\n");
                result->code = 400;
                return;
            }

            // CHeck if version is correct
            if (!(0 == strcmp(version, "HTTP/1.1")))
            {
                printf("invalid version: %s\n", version);
                printf("%d vs %d\n", strlen(version), strlen("HTTP/1.1"));
                result->code = 400;
                return;
            }

            if (0 == strcmp(method, "GET") && url == strstr(url, "/exec/"))
            {
                result->code = 200;
                result->url = url;
            }
            else
            {
                // May need to over-ride. Just because valid request line doesn't mean a valid request.
                result->code = 404;
            }
        }
        else
        {
            // Process non-request lines?
        }

        current = line_end + 2;
        line_count += 1;
    }

    return;
}

void parseCommand(char *command)
{
    printf("parsing command...\n");

    const int len = strlen(command);

    // Count the number of "%20" (spaces) that we will replace
    char *current = command;
    while (1)
    {
        char *next = strstr(current, "%20");
        if (next)
        {
            // want to replace "%20" with " "
            *next = ' ';
            // strcpy(next, next);
            for (int idx = next - command + 1; idx < len; ++idx)
            {
                command[idx] = command[idx + 2];
            }
            next = next + 1;
        }
        if (!next)
        {
            break;
        }
        current = next;
    }

    // // New length of string
    // int old_string_length = strlen(command);
    // int new_string_length = old_string_length - 2 * num_spaces;
    // char *result = (char *)malloc((new_string_length + 1) * sizeof(char));

    // return result;
}