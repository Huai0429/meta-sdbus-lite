# meta-sdbus-lite

A lightweight C helper library for systemd's `sd-bus` API. It wraps common D-Bus operations behind a simpler message model so applications can send method calls, emit signals, parse replies, and register object vtables without manually composing every low-level `sd_bus_message` operation.

This project is intended for embedded Linux and Yocto/OpenEmbedded systems where a compact, reusable IPC layer is preferred over heavy application-specific D-Bus glue code.

## Features

- Connect and initialize a D-Bus client or server context
- Send synchronous or asynchronous method calls
- Emit signals and reply to method calls
- Register object vtables for server-side method dispatch
- Add signal match rules
- Parse and construct simple message payloads using a compact `sdbl_msg_t` model
- Work with common basic types: `uint32_t`, `int32_t`, `double`, `string`, and `uint8_t`
- Support simple dict, struct, and plain argument layouts

## Project structure

```text
meta-sdbus-lite/
├── README.md
├── conf/
│   └── layer.conf
├── recipes-support/
│   └── sdbus-lite/
│       ├── sdbus-lite_1.0.bb
│       └── files/
│           ├── Makefile
│           ├── sdbus_lite.c
│           ├── sdbus_lite.h
│           └── sdbus_lite.pc.in
└── ...
```

## Dependencies

- `libsystemd` / `sd-bus`
- pthreads
- GCC or a compatible C compiler
- Yocto/OpenEmbedded environment when used as an OE layer recipe

## Build with Yocto/OpenEmbedded

This layer provides a Yocto recipe for building the library as part of an OpenEmbedded image. The package is defined in `recipes-support/sdbus-lite/sdbus-lite_1.0.bb`.

Add the layer to your build environment:

```bash
# in conf/bblayers.conf
BBLAYERS += "/path/to/meta-sdbus-lite"
```
or 
``` bash
# in terminal
bitbake-layers add-layer ../layers/meta-sdbus-lite
```
Then build the library with:

```bash
bitbake sdbus-lite
```

This produces the library artifacts as part of the Yocto build system. The recipe installs:

- `libsdbus_lite.so`
- `libsdbus_lite.a`
- `sdbus_lite.h`
- `sdbus_lite.pc`

The included `Makefile` is also used during the recipe compile step, but the normal build path is via `bitbake`, not by calling `make` directly from the source tree.

## Using it in another Yocto recipe

To consume this library from another package, add it to `DEPENDS`:

```bitbake
DEPENDS += "sdbus-lite"
```

And include the header path and link flags as needed:

```make
CFLAGS += "-I${STAGING_DIR_TARGET}/usr/include/sdbus_lite"
LDFLAGS += "-lsdbus_lite"
```

## Typical usage from an application

### 1) Open a D-Bus connection

```c
#include <systemd/sd-bus.h>
#include "sdbus_lite.h"

sd_bus *conn = NULL;
sdbl bus = {0};
int ret;

ret = sd_bus_open_system(&conn);
if (ret < 0) {
    fprintf(stderr, "failed to open system bus: %s\n", strerror(-ret));
    return -1;
}

bus.pconn = conn;
ret = sdbl_init(&bus,
                "org.example.Service",
                "/org/example/Service",
                "org.example.Service",
                true);
if (ret < 0) {
    fprintf(stderr, "failed to initialize sdbus instance\n");
    return -1;
}
```

### 2) Create a plain message payload

```c
sdbl_msg_t msg = {0};

msg.format = MSG_FMT_PLAIN;
msg.msg_type_size = 2;
strncpy(msg.pszmsg_type, "ss", sizeof(msg.pszmsg_type));
msg.fields[0].type = 's';
msg.fields[0].s_val = "hello";
msg.fields[1].type = 's';
msg.fields[1].s_val = "world";
msg.pszmember = "SetText";
msg.pszinterface = "org.example.Service";
strncpy(msg.pszpath, "/org/example/Service", sizeof(msg.pszpath));
```

### 3) Send a synchronous call

```c
sd_bus_message *reply = NULL;
ret = sdbl_sync_send(&bus, &msg, &reply);
if (ret < 0) {
    fprintf(stderr, "method call failed: %s\n", strerror(-ret));
} else if (reply) {
    printf("reply received\n");
    sd_bus_message_unref(reply);
}
```

### 4) Emit a signal

```c
sdbl_msg_t sig = {0};

sig.format = MSG_FMT_PLAIN;
sig.msg_type_size = 1;
strncpy(sig.pszmsg_type, "s", sizeof(sig.pszmsg_type));
sig.fields[0].type = 's';
sig.fields[0].s_val = "status changed";
sig.pszmember = "StatusChanged";

ret = sdbl_emit_signal(&bus, &sig);
```

### 5) Add a match rule for incoming signals

```c
ret = sdbl_signal_add_match(conn,
                            "org.example.Service",
                            "StatusChanged",
                            your_signal_handler,
                            NULL);
```

## Core data structures

### `sdbl`

The main library context.

```c
typedef struct sdbl {
    const char *pszbus_name;
    const char *pszpath;
    const char *pszinterface;
    pthread_mutex_t mutex;
    sd_bus *pconn;
    bool server_mode;
} sdbl;
```

### `sdbl_msg_t`

A compact message description that describes the D-Bus payload format and values.

```c
typedef struct sdbl_msg {
    char *pszmember;
    char *pszinterface;
    char  pszpath[MAX_STRING_LENGTH];
    char  pszmsg_type[MAX_MSG_TYPE_LENGTH];
    int   msg_type_size;
    msg_fmt_e format;
    basic_type key[MAX_MSG_TYPE_LENGTH / 2];
    basic_type value[MAX_MSG_TYPE_LENGTH / 2];
    basic_type fields[MAX_MSG_TYPE_LENGTH];
} sdbl_msg_t;
```

### Supported message formats

- `MSG_FMT_PLAIN`: bare arguments, for example `ss`, `iu`, `s`
- `MSG_FMT_STRUCT`: a struct, for example `(ss)`
- `MSG_FMT_DICT`: a dictionary array, for example `a{su}`

## API summary

```c
int sdbl_init(sdbl *pdbus, char *pszbus_name, char *pszpath, char *pszinterface, bool server_mode);
int sdbl_deinit(sdbl *pdbus);
void sdbl_copy_sdbl_msg_t(sdbl_msg_t *pdest, const sdbl_msg_t *psrc);

int sdbl_signal_add_match(sd_bus *pconn, const char *pszinterface, const char *pszmember, sdbl_callback phandler, void *puser_data);
int sdbl_parse_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);
int sdbl_async_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message_handler_t phandler, void *puser_data);
int sdbl_sync_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message **preply_out);
int sdbl_register_table(sdbl *pdbus, const sd_bus_vtable *pvtable, void *puser_data);
int sdbl_emit_signal(sdbl *pdbus, sdbl_msg_t *psdbl_msg);
int sdbl_method_reply(sdbl *pdbus, sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);
```

## Notes

- This library intentionally keeps the interface small and focused on common D-Bus transactions.
- The message type strings are simple ASCII signatures such as `s`, `u`, `i`, `d`, `y`, and combinations like `ss`, `(ss)`, or `a{su}`.
- For a library consumer, the most important tasks are to fill in `pszmsg_type`, `msg_type_size`, `format`, and the relevant `fields`/`key`/`value` entries before calling the send or reply helpers.

## License

This project is distributed under the MIT license.
