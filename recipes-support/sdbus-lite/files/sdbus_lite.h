#ifndef SDBL_H
#define SDBL_H

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <systemd/sd-bus.h>

#define MAX_STRING_LENGTH	256
#define MAX_MSG_TYPE_LENGTH	16
#define SDBL_DEBUG		1
#define SDBL_DBG(fmt, ...)	\
	do { if (SDBL_DEBUG) printf(fmt, ##__VA_ARGS__); } while (0)

typedef int (*sdbl_callback)(sd_bus_message *pmsg, void *puser_data, sd_bus_error *pret_error);

typedef struct sdbl {
    const char *pszbus_name;
    const char *pszpath;
    const char *pszinterface;
    pthread_mutex_t mutex;
    sd_bus *pconn;
    bool server_mode; /* true if this sdbl instance is used for serving, false for client */
} sdbl;

typedef struct basic_type {
    char type; /* 'u' = uint32_t, 'i' = int32_t, 'd' = double,
		    * 's' = string,  'y' = uint8_t
		    */
    union {
        uint32_t u_val;
        int32_t  i_val;
        double   d_val;
        char    *s_val;
        uint8_t  y_val;
    };
} basic_type;

typedef enum msg_fmt {
    MSG_FMT_DICT,   /* a{XX} array-of-dict,      e.g. a{su}  */
    MSG_FMT_STRUCT, /* (XX...) struct,            e.g. (ss)   */
    MSG_FMT_PLAIN,  /* XX...  bare top-level args e.g. ss     */
} msg_fmt_e;

typedef struct sdbl_msg {
    char *pszmember;
    char *pszinterface;
    char  pszpath[MAX_STRING_LENGTH];
    char  pszmsg_type[MAX_MSG_TYPE_LENGTH]; /* type signature, e.g. "su", "ss" */
    int   msg_type_size;
    msg_fmt_e format;                      /* MSG_FMT_DICT or MSG_FMT_STRUCT */
    /* MSG_FMT_DICT: key/value pairs for a{XX} messages */
    basic_type key[MAX_MSG_TYPE_LENGTH / 2];
    basic_type value[MAX_MSG_TYPE_LENGTH / 2];
    /* MSG_FMT_STRUCT: flat field list for (XX...) messages */
    basic_type fields[MAX_MSG_TYPE_LENGTH];
} sdbl_msg_t;

typedef enum sdbl_io_action {
    IO_READ,
    IO_APPEND,
} sdbl_io_action_e;

typedef struct sdbl_inhibit_data {
    char *pszwhat;
    char *pszwho;
    char *pszwhy;
    char *pszmode;
    int inhibit_fd;
} sdbl_inhibit_data_t;

int sdbl_init(sdbl *pdbus, char *pszbus_name, char *pszpath, char *pszinterface, bool server_mode);
int sdbl_deinit(sdbl *pdbus);
void sdbl_copy_sdbl_msg_t(sdbl_msg_t *pdest, const sdbl_msg_t *psrc);

int sdbl_signal_add_match(sd_bus *pconn, const char *pszinterface, const char *pszmember, sdbl_callback phandler, void *puser_data);
int sdbl_parse_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);
int sdbl_async_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message_handler_t phandler, void *puser_data);
int sdbl_sync_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message **preply_out);
int sdbl_inhibit_lock(sd_bus *psystem_conn, sdbl_callback phandler, void *puser_data, sdbl_inhibit_data_t *pinhibit_data);
int sdbl_inhibit_unlock(sdbl_inhibit_data_t *pinhibit_data);

int sdbl_register_table(sdbl *pdbus, const sd_bus_vtable *pvtable, void *puser_data);
int sdbl_emit_signal(sdbl *pdbus, sdbl_msg_t *psdbl_msg);
int sdbl_method_reply(sdbl *pdbus, sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);

#endif /* SDBL_H */