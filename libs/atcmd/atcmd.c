#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "atcmd.h"

#define LF 10
#define CR 13

static const char delimiter[] = "\r\n";

static int _putc(atcmd *at, unsigned char ch)
{
    return at->ops->putc_timeout(ch, at->timeout);
}

static int _getc(atcmd *at)
{
    return at->ops->getc_timeout(at->timeout);
}

static void debug_if(bool on, const char *format, ...)
{
    if (on) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

bool atcmd_init(atcmd *at, atcmd_ops *ops, char *buf, unsigned int buf_size)
{
    if (!ops || !buf || !buf_size) {
        return false;
    }
    memset(at, 0, sizeof(*at));
    at->ops = ops;
    at->buffer = buf;
    at->buffer_size = buf_size;
    at->dbg_on = false;
    return true;
}

bool atcmd_add_handler(atcmd *at, const char *prefix, void (*cb)(void))
{
    for (int i = 0; i < AT_PARSER_OOB_COUNT; i++) {
        struct handler *o = &at->handlers[i];
        if (o->prefix || (o->len > 0) || o->cb) {
            continue;
        }
        o->prefix = prefix;
        o->len = strlen(prefix);
        o->cb = cb;
        return true;
    }
    return false;
}

static bool atcmd_vsend(atcmd *at, const char *command, va_list args)
{
    if (vsnprintf(at->buffer, at->buffer_size, command, args) < 0) {
        return false;
    }

    for (int i = 0; at->buffer[i]; i++) {
        if (_putc(at, at->buffer[i]) < 0) {
            return false;
        }
    }

    for (int i = 0; delimiter[i]; i++) {
        if (_putc(at, delimiter[i]) < 0) {
            return false;
        }
    }
    debug_if(at->dbg_on, "AT> %s\n", at->buffer);
    return true;
}

bool atcmd_send(atcmd *at, const char *command, ...)
{
    va_list args;
    va_start(args, command);
    bool res = atcmd_vsend(at, command, args);
    va_end(args);
    return res;
}

static bool _vrecv(atcmd *at, const char *response, va_list args)
{
restart:
    at->aborted = false;
    // Iterate through each line in the expected response
    while (response[0]) {
        // Since response is const, we need to copy it into our buffer to
        // add the line's null terminator and clobber value-matches with asterisks.
        //
        // We just use the beginning of the buffer to avoid unnecessary allocations.
        int i = 0;
        int offset = 0;
        int whole_line_wanted = 0;

        while (response[i]) {
            if (response[i] == '%' && response[i + 1] != '%' && response[i + 1] != '*') {
                at->buffer[offset++] = '%';
                at->buffer[offset++] = '*';
                i++;
            } else {
                at->buffer[offset++] = response[i++];
                // Find linebreaks, taking care not to be fooled if they're in a %[^\n] conversion specification
                if (response[i - 1] == '\n' &&
                    !(i >= 3 && response[i - 3] == '[' && response[i - 2] == '^'))
                {
                    whole_line_wanted = 1;
                    break;
                }
            }
        }

        // Scanf has very poor support for catching errors
        // fortunately, we can abuse the %n specifier to determine
        // if the entire string was matched.
        at->buffer[offset++] = '%';
        at->buffer[offset++] = 'n';
        at->buffer[offset++] = 0;

        debug_if(at->dbg_on, "AT? %s\n", at->buffer);
        // To workaround scanf's lack of error reporting, we actually
        // make two passes. One checks the validity with the modified
        // format string that only stores the matched characters (%n).
        // The other reads in the actual matched values.
        //
        // We keep trying the match until we succeed or some other error
        // derails us.
        int j = 0;

        while (1) {
            // Receive next character
            int c = _getc(at);
            if (c < 0) {
                debug_if(at->dbg_on, "AT(Timeout)\n");
                return false;
            }
            // Simplify newlines (borrowed from retarget.cpp)
            if ((c == CR && at->in_prev != LF) ||
                (c == LF && at->in_prev != CR)) {
                at->in_prev = c;
                c = '\n';
            }
            else if ((c == CR && at->in_prev == LF) ||
                     (c == LF && at->in_prev == CR)) {
                at->in_prev = c;
                // onto next character
                continue;
            } else {
                at->in_prev = c;
            }
            at->buffer[offset + j++] = c;
            at->buffer[offset + j] = 0;

            // Check for oob data
            for (int i = 0; i < AT_PARSER_OOB_COUNT; i++) {

                struct handler *handler = &at->handlers[i];

                if ((unsigned)j == handler->len &&
                    memcmp(handler->prefix, at->buffer + offset, handler->len) == 0) {

                    debug_if(at->dbg_on, "AT! %s\n", handler->prefix);
                    handler->cb();

                    if (at->aborted) {
                        debug_if(at->dbg_on, "AT(Aborted)\n");
                        return false;
                    }
                    // oob may have corrupted non-reentrant buffer,
                    // so we need to set it up again
                    goto restart;
                }
            }

            // Check for match
            int count = -1;
            if (whole_line_wanted && c != '\n') {
                // Don't attempt scanning until we get delimiter if they included it in format
                // This allows recv("Foo: %s\n") to work, and not match with just the first character of a string
                // (scanf does not itself match whitespace in its format string, so \n is not significant to it)
            } else {
                sscanf(at->buffer + offset, at->buffer, &count);
            }

            // We only succeed if all characters in the response are matched
            if (count == j) {
                debug_if(at->dbg_on, "AT= %s\n", at->buffer + offset);
                // Reuse the front end of the buffer
                memcpy(at->buffer, response, i);
                at->buffer[i] = 0;

                // Store the found results
                vsscanf(at->buffer + offset, at->buffer, args);

                // Jump to next line and continue parsing
                response += i;
                break;
            }

            // Clear the buffer when we hit a newline or ran out of space
            // running out of space usually means we ran into binary data
            if (c == '\n' || (unsigned)(j + 1) >= (at->buffer_size - offset)) {
                debug_if(at->dbg_on, "AT< %s", at->buffer + offset);
                j = 0;
            }
        }
    }

    return true;
}

bool atcmd_recv(atcmd *at, const char *response, ...)
{
    va_list args;
    va_start(args, response);
    bool res = _vrecv(at, response, args);
    va_end(args);
    return res;
}

void atcmd_set_timeout(atcmd *at, int timeout)
{
    at->timeout = timeout;
}

void atcmd_abort(atcmd *at)
{
    at->aborted = true;
}

int atcmd_write(atcmd *at, const char *data, int size)
{
    int i = 0;
    for ( ; i < size; i++) {
        if (_putc(at, data[i]) < 0) {
            return -1;
        }
    }
    return i;
}

int atcmd_read(atcmd *at, char *data, int size)
{
    int i = 0;
    for ( ; i < size; i++) {
        int c = _getc(at);
        if (c < 0) {
            return -1;
        }
        data[i] = c;
    }
    return i;
}

bool atcmd_process_oob(atcmd *at)
{
    unsigned int i = 0;
    bool rc = false;

    while (1) {
        // Receive next character
        int c = _getc(at);
        if (c < 0) {
            rc = false;
            goto exit;
        }

        // Simplify newlines (borrowed from retarget.cpp)
        if ((c == CR && at->in_prev != LF) ||
            (c == LF && at->in_prev != CR)) {

            at->in_prev = c;
            c = '\n';
        }
        else if ((c == CR && at->in_prev == LF) ||
                 (c == LF && at->in_prev == CR)) {

            at->in_prev = c;
            // onto next character
            continue;
        } else {
            at->in_prev = c;
        }
        at->buffer[i++] = c;
        at->buffer[i] = 0;

        // Check for oob data
        for (int j = 0; j < AT_PARSER_OOB_COUNT; j++) {

            struct handler *handler = &at->handlers[j];

            if (i == handler->len &&
                memcmp(handler->prefix, at->buffer, handler->len) == 0) {

                debug_if(at->dbg_on, "AT! %s\r\n", handler->prefix);
                handler->cb();
                rc = true;
                goto exit;
            }
        }

        // Clear the buffer when we hit a newline or ran out of space
        // running out of space usually means we ran into binary data
        if (((i + 1) >= at->buffer_size) || (c == '\n')) {
            debug_if(at->dbg_on, "AT< %s", at->buffer);
            i = 0;
        }
    }

exit:
    return rc;
}
