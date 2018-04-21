#ifndef _AT_PARSER_H_
#define _AT_PARSER_H_

#include <stdbool.h>

#define AT_PARSER_OOB_COUNT     10

typedef struct atcmd_ops {
    /*
     * return 0 on success, -1 on failed or timeout
     */
    int (*getc_timeout)(int timeout_ms);
    int (*putc_timeout)(unsigned char  ch, int timeout_ms);
} atcmd_ops;

typedef struct atcmd {
    char *buffer;
    unsigned int buffer_size;
    int timeout;
    bool dbg_on;
    bool aborted;
    int in_prev;

    struct handler {
        const char *prefix;
        unsigned int len;
        void (*cb)(void);
    } handlers[AT_PARSER_OOB_COUNT];

    atcmd_ops *ops;
} atcmd;

bool atcmd_init(atcmd *at, atcmd_ops *ops, char *buf, unsigned int buf_size);

bool atcmd_add_handler(atcmd *at, const char *prefix, void (*cb)(void));

bool atcmd_send(atcmd *at, const char *command, ...);

bool atcmd_recv(atcmd *at, const char *response, ...);

void atcmd_set_timeout(atcmd *at, int timeout);

void atcmd_abort(atcmd *at);

int atcmd_write(atcmd *at, const char *data, int size);

int atcmd_read(atcmd *at, char *data, int size);

bool atcmd_process_oob(atcmd *at);

#endif /* #ifndef _AT_PARSER_H_ */

