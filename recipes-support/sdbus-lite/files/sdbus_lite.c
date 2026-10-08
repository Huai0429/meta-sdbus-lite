#include "sdbus_lite.h"

/**
 * sdbl_init - Initialize a sdbl structure.
 * @pszbus_name:  DBus bus name
 * @pszpath:      DBus object path
 * @pszinterface: DBus interface name
 * @pdbus:            sdbl structure to initialize
 * @server_mode:       true for server mode, false for client mode
 *
 * Return: 0 on success, -1 on failure.
 */
int sdbl_init(sdbl *pdbus, char *pszbus_name, char *pszpath, char *pszinterface, bool server_mode)
{
    int ret;
    if (!pszbus_name || !pszpath || !pszinterface || !pdbus) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        printf("[Q_Q] Bus name: %s, Path: %s, Interface: %s\n",
               pszbus_name ? pszbus_name : "NULL",
               pszpath ? pszpath : "NULL",
               pszinterface ? pszinterface : "NULL");
        return -1;
    }
    if (pdbus->pconn == NULL) {
        printf("[Q_Q] DBus connection is not opened before calling %s\n", __func__);
        return -1;
    }

    pthread_mutex_init(&pdbus->mutex, NULL);
    pdbus->pszbus_name  = pszbus_name;
    pdbus->pszpath      = pszpath;
    pdbus->pszinterface = pszinterface;
    pdbus->server_mode = server_mode;
    if (pdbus->server_mode) {
        ret = sd_bus_request_name(pdbus->pconn, pdbus->pszbus_name, 0);
        if (ret < 0) {
            printf("[Q_Q] Failed to obtain bus name %s: %s\n", pdbus->pszbus_name, strerror(-ret));
            return -1;
        }
    }

    return 0;
}

/**
 * sdbl_deinit - Deinitialize a sdbl structure and release resources.
 * @pdbus: sdbl structure to deinitialize
 *
 * Return: 0 on success, -1 on failure.
 */
int sdbl_deinit(sdbl *pdbus)
{
    if (!pdbus) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    pthread_mutex_destroy(&pdbus->mutex);
    if (pdbus->pconn) {
        sd_bus_unref(pdbus->pconn);
        pdbus->pconn = NULL;
    }
    return 0;
}

/**
 * sdbl_copy_sdbl_msg_t - Copy the contents of a sdbl_msg_t.
 * @pdest: destination sdbl_msg_t
 * @psrc:  source sdbl_msg_t
 */
void sdbl_copy_sdbl_msg_t(sdbl_msg_t *pdest, const sdbl_msg_t *psrc)
{
    memcpy(pdest->pszmsg_type, psrc->pszmsg_type, sizeof(pdest->pszmsg_type));
    pdest->msg_type_size = psrc->msg_type_size;
    pdest->format        = psrc->format;
    if (psrc->format == MSG_FMT_STRUCT) {
        for (int i = 0; i < psrc->msg_type_size; i++) {
            pdest->fields[i] = psrc->fields[i];
        }
    }
    else {
        for (int i = 0; i < psrc->msg_type_size / 2; i++) {
            pdest->key[i]   = psrc->key[i];
            pdest->value[i] = psrc->value[i];
        }
    }
    free(pdest->pszinterface);
    free(pdest->pszmember);
    snprintf(pdest->pszpath, sizeof(pdest->pszpath), "%s", psrc->pszpath);
    pdest->pszinterface = psrc->pszinterface ? strdup(psrc->pszinterface) : NULL;
    pdest->pszmember    = psrc->pszmember ? strdup(psrc->pszmember) : NULL;
}

/**
 * sdbl_signal_add_match - Add match rules for signals on a DBus connection.
 * @pconn:       DBus connection
 * @pszinterface: interface name string
 * @pszmember:    member (signal) name string
 * @phandler:   callback handler for the signal
 * @puser_data:   user data passed to the callback handler
 *
 * Return: 0 on success, -1 on failure.
 */
int  sdbl_signal_add_match(sd_bus *pconn, const char *pszinterface, const char *pszmember, sdbl_callback phandler, void *puser_data)
{
    char szmatch_rule[256];
    int ret;

    if (!pconn || !pszinterface || !pszmember || !phandler) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }


    snprintf(szmatch_rule, sizeof(szmatch_rule), "type='signal',interface='%s',member='%s'", pszinterface, pszmember);
    SDBL_DBG("Adding match rule: %s\n", szmatch_rule);
    ret = sd_bus_match_signal(pconn, NULL,
                              NULL,         /* sender: any */
                              NULL,         /* path:   any */
                              pszinterface,
                              pszmember,
                              phandler,
                              puser_data);
    if (ret < 0) {
        printf("[Q_Q] Failed to add match rule: %s\n",
               strerror(-ret));
        return -1;
    }
    return 0;
}

/**
 * sdbl_basic_handle - Handle basic type read/write for DBus messages.
 * @pmsg:    DBus message
 * @type:   basic type character ('u', 'i', 'd', 'y', 's')
 * @action: IO_READ or IO_APPEND
 * @pval:    basic_type value to read into or append from
 *
 * Return: 0 on success, negative errno on failure.
 */
static int sdbl_basic_handle(sd_bus_message *pmsg, char type,  sdbl_io_action_e action, basic_type *pval)
{
    const char *szaction;
    const void *pappend_ptr = NULL;
    void *pread_ptr = NULL;
    int ret;

    if (!pmsg || !pval) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    if (type == '\0') {
        printf("[Q_Q] Invalid basic type for %s\n", __func__);
        return -1;
    }

    switch (type) {
        case 'u':
            pread_ptr   = &pval->u_val;
            pappend_ptr = &pval->u_val;
            break;
        case 'i':
            pread_ptr   = &pval->i_val;
            pappend_ptr = &pval->i_val;
            break;
        case 'd':
            pread_ptr   = &pval->d_val;
            pappend_ptr = &pval->d_val;
            break;
        case 'y':
            pread_ptr   = &pval->y_val;
            pappend_ptr = &pval->y_val;
            break;
        case 's':
            pread_ptr   = &pval->s_val;  /* read: address of char * */
            pappend_ptr = pval->s_val;   /* append: char * itself */
            break;
        default:
            printf("[Q_Q] Unsupported value type '%c'\n", type);
            return -EINVAL;
    }

    if (action == IO_READ) {
        ret = sd_bus_message_read_basic(pmsg, type, pread_ptr);
    }
    else {
        if (type == 's' && !pappend_ptr) {
            printf("[Q_Q] Cannot append NULL string!\n");
            return -EINVAL;
        }
        ret = sd_bus_message_append_basic(pmsg, type, pappend_ptr);
    }

    if (ret < 0) {
        printf("[Q_Q] Failed to %s basic type '%c': %s\n",
               (action == IO_READ) ? "read" : "append",
               type, strerror(-ret));
        return ret;
    }
    pval->type = type;
    if (SDBL_DEBUG) {
        szaction = (action == IO_READ) ? "Read" : "Appended";
        switch (type) {
            case 'u':
                printf("%s uint32_t: %u\n", szaction, pval->u_val);
                break;
            case 'i':
                printf("%s int32_t: %d\n", szaction, pval->i_val);
                break;
            case 'd':
                printf("%s double: %f\n", szaction, pval->d_val);
                break;
            case 'y':
                printf("%s byte: 0x%02X\n", szaction, pval->y_val);
                break;
            case 's':
                printf("%s string: '%s'\n", szaction, pval->s_val ? pval->s_val : "NULL");
                break;
        }
    }
    return 0;
}

/**
 * sdbl_read_variant - Read a variant type from a DBus message.
 * @pmsg: DBus message
 * @pval: basic_type structure to store the result
 *
 * Return: 0 on success, negative errno on failure.
 */
static int sdbl_read_variant(sd_bus_message *pmsg, basic_type *pval)
{
    const char *pinner_contents;
    char inner_type;
    int ret;

    ret = sd_bus_message_peek_type(pmsg, &inner_type, &pinner_contents);
    if (ret < 0) {
        printf("[Q_Q] Failed to peek variant type: %s\n", strerror(-ret));
        return ret;
    }
    ret = sd_bus_message_enter_container(pmsg, SD_BUS_TYPE_VARIANT, pinner_contents);
    if (ret < 0) {
        printf("[Q_Q] Failed to enter variant container: %s\n", strerror(-ret));
        return ret;
    }
    pval->type = pinner_contents[0];
    ret = sdbl_basic_handle(pmsg, pval->type, IO_READ, pval);
    sd_bus_message_exit_container(pmsg);
    return ret;
}

/**
 * sdbl_extract_dict_types - Extract key/value types from a dict signature.
 * @pmsg:       DBus message
 * @pmsg_type:  buffer to store the extracted type signature (e.g. "su")
 * @type_size: size of the pmsg_type buffer
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_extract_dict_types(sd_bus_message *pmsg, char *pmsg_type, size_t type_size)
{
    const char *szsignature;
    const char *pszopen;
    const char *pszclose;
    size_t inner_len;

    if (!pmsg_type || type_size < 3) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    szsignature = sd_bus_message_get_signature(pmsg, 1);
    if (!szsignature) {
        printf("[Q_Q] Failed to get message signature in %s\n", __func__);
        return -1;
    }
    pszopen  = strchr(szsignature, '{');
    pszclose = strchr(szsignature, '}');
    if (!pszopen || !pszclose || pszclose <= pszopen + 1) {
        printf("[Q_Q] No dict entry found in signature: %s\n", szsignature);
        return -1;
    }
    inner_len = (size_t)(pszclose - pszopen - 1);
    if (inner_len >= type_size) {
        printf("[Q_Q] Dict type length %zu exceeds buffer size %zu\n", inner_len, type_size);
        return -1;
    }
    memcpy(pmsg_type, pszopen + 1, inner_len);
    pmsg_type[inner_len] = '\0';
    return 0;
}

int sdbl_parse_basic(sd_bus_message *pmsg, basic_type *pval, char msg_type)
{
    int ret;
    if (msg_type == 'v') {
        ret = sdbl_read_variant(pmsg, pval);
    }
    else {
        ret = sdbl_basic_handle(pmsg, msg_type, IO_READ, pval);
    }
    if (ret < 0) {
        printf("[Q_Q] Failed to parse basic type at %s %d: %s\n", __func__, __LINE__, strerror(-ret));
        sd_bus_message_exit_container(pmsg);
        return -1;
    }
    return 0;
}

static int sdbl_parse_struct_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);
static int sdbl_parse_plain_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg);

/**
 * sdbl_parse_msg - Parse a DBus message into a sdbl_msg_t.
 *
 * Dispatches to the dict (a{XX}) or struct (XX...) parser based on
 * psdbl_msg->format.  For MSG_FMT_DICT, parses [ { key: value } ].
 * For MSG_FMT_STRUCT, parses ( field0, field1, ... ).
 *
 * @pmsg:       DBus message to parse
 * @psdbl_msg: sdbl_msg_t to store the extracted data
 *
 * Return: 0 on success, -1 on failure.
 */
int sdbl_parse_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    if (!pmsg || !psdbl_msg) {
        printf("[Q_Q] Cannot parse a %s message %s %d\n", pmsg ? "valid" : "NULL", __func__, __LINE__);
        return -1;
    }
    if (psdbl_msg->format == MSG_FMT_STRUCT) {
        return sdbl_parse_struct_msg(pmsg, psdbl_msg);
    }
    if (psdbl_msg->format == MSG_FMT_PLAIN) {
        return sdbl_parse_plain_msg(pmsg, psdbl_msg);
    }
    char szdict_type[MAX_MSG_TYPE_LENGTH + 3]; /* '{' + type_sig + '}' + '\0' */
    int ret;

    ret = sdbl_extract_dict_types(pmsg, psdbl_msg->pszmsg_type, sizeof(psdbl_msg->pszmsg_type));
    if (ret < 0) {
        printf("[Q_Q] Failed to extract dict types from signature '%s'\n", sd_bus_message_get_signature(pmsg, 1));
        return -1;
    }
    SDBL_DBG("Extracted dict types: %s\n", psdbl_msg->pszmsg_type);
    if (psdbl_msg->pszmsg_type[0] == '\0') {
        printf("[Q_Q] Message type signature is empty in %s %d\n", __func__, __LINE__);
        printf("[Q_Q] You need to specify the type you want to parse\n");
        return -1;
    }
    snprintf(szdict_type, sizeof(szdict_type), "{%s}", psdbl_msg->pszmsg_type);
    ret = sd_bus_message_enter_container(pmsg, SD_BUS_TYPE_ARRAY, szdict_type);
    if (ret < 0) {
        printf("[Q_Q] Failed to enter array container at %s %d: %s\n", __func__, __LINE__, strerror(-ret));
        return -1;
    }

    while ((ret = sd_bus_message_enter_container(pmsg, SD_BUS_TYPE_DICT_ENTRY, psdbl_msg->pszmsg_type)) > 0) {
        SDBL_DBG("Key: ");
        ret = sdbl_parse_basic(pmsg, &psdbl_msg->key[0], psdbl_msg->pszmsg_type[0]);
        if (ret < 0) {
            printf("[Q_Q]Failed to read key at %s %d: %s\n", __func__, __LINE__, strerror(-ret));
            sd_bus_message_exit_container(pmsg);
            break;
        }
        SDBL_DBG("Value: ");
        ret = sdbl_parse_basic(pmsg, &psdbl_msg->value[0], psdbl_msg->pszmsg_type[1]);
        if (ret < 0) {
            printf("[Q_Q]Failed to read value at %s %d: %s\n", __func__, __LINE__, strerror(-ret));
            sd_bus_message_exit_container(pmsg);
            break;
        }
        sd_bus_message_exit_container(pmsg);
    }
    sd_bus_message_exit_container(pmsg);
    return 0;
}

/**
 * sdbl_create_dict_msg - Append an array-of-dict body to a DBus message.
 *
 * Builds a message of the form: [ { key: value } ]
 *
 * @pmsg:       DBus message to append to
 * @psdbl_msg: sdbl_msg_t holding the key-value pairs
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_create_dict_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    char szdict_type[MAX_MSG_TYPE_LENGTH + 3]; /* '{' + type_sig + '}' + '\0' */
    int ret;

    /* All dict entries in a D-Bus array must share the same type signature.
     * Use only the first entry's two type characters (key + value) for the
     * array container signature, and build a per-entry type string in the
     * loop below. */
    snprintf(szdict_type, sizeof(szdict_type), "{%c%c}", psdbl_msg->pszmsg_type[0], psdbl_msg->pszmsg_type[1]);
    ret = sd_bus_message_open_container(pmsg, SD_BUS_TYPE_ARRAY, szdict_type);
    if (ret < 0) {
        printf("Failed to open array container: %s\n", strerror(-ret));
        return -1;
    }

    int dict_num = psdbl_msg->msg_type_size / 2;
    SDBL_DBG("Creating message with %d dict entries\n", dict_num);
    for (int i = 0; i < dict_num; i++) {
        char entry_type[3] = {
            psdbl_msg->pszmsg_type[i * 2],
            psdbl_msg->pszmsg_type[i * 2 + 1],
            '\0'
        };
        SDBL_DBG("Appending dict entry %d: Key type '%c', Value type '%c'\n", i, entry_type[0], entry_type[1]);
        ret = sd_bus_message_open_container(pmsg, SD_BUS_TYPE_DICT_ENTRY, entry_type);
        if (ret < 0) {
            printf("Failed to open dict entry container: %s\n",
                   strerror(-ret));
            return -1;
        }

        ret = sdbl_basic_handle(pmsg, psdbl_msg->pszmsg_type[i * 2], IO_APPEND, &psdbl_msg->key[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to append key at %s %d: %s\n", __func__, __LINE__, strerror(-ret));
            return -1;
        }

        ret = sdbl_basic_handle(pmsg, psdbl_msg->pszmsg_type[i * 2 + 1], IO_APPEND, &psdbl_msg->value[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to append value at %s %d: %s\n",  __func__, __LINE__, strerror(-ret));
            return -1;
        }

        ret = sd_bus_message_close_container(pmsg);
        if (ret < 0) {
            printf("Failed to close dict entry container: %s\n", strerror(-ret));
            return -1;
        }
    }
    ret = sd_bus_message_close_container(pmsg);
    if (ret < 0) {
        printf("Failed to close array container: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

/**
 * sdbl_parse_struct_msg - Parse a DBus struct message into a sdbl_msg_t.
 *
 * Parses messages of the form: ( field0, field1, ... )
 * The type signature and field count are auto-detected from the message.
 *
 * @pmsg:       DBus message to parse
 * @psdbl_msg: sdbl_msg_t to store the extracted fields in fields[]
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_parse_struct_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    const char *pcontents;
    char type;
    int  nfields;
    int  ret;
    int  i;

    ret = sd_bus_message_peek_type(pmsg, &type, &pcontents);
    if (ret < 0) {
        printf("[Q_Q] Failed to peek struct type: %s\n", strerror(-ret));
        return -1;
    }
    if (type != SD_BUS_TYPE_STRUCT) {
        printf("[Q_Q] Expected struct type '(' but got '%c'\n", type);
        return -1;
    }
    if (!pcontents || strlen(pcontents) >= sizeof(psdbl_msg->pszmsg_type)) {
        printf("[Q_Q] Struct signature is NULL or too long\n");
        return -1;
    }
    snprintf(psdbl_msg->pszmsg_type, sizeof(psdbl_msg->pszmsg_type), "%s", pcontents);
    nfields = (int)strlen(pcontents);
    psdbl_msg->msg_type_size = nfields;

    ret = sd_bus_message_enter_container(pmsg, SD_BUS_TYPE_STRUCT, pcontents);
    if (ret < 0) {
        printf("[Q_Q] Failed to enter struct container: %s\n", strerror(-ret));
        return -1;
    }
    for (i = 0; i < nfields; i++) {
        ret = sdbl_parse_basic(pmsg, &psdbl_msg->fields[i], psdbl_msg->pszmsg_type[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to read struct field %d: %s\n", i, strerror(-ret));
            sd_bus_message_exit_container(pmsg);
            return -1;
        }
    }
    sd_bus_message_exit_container(pmsg);
    return 0;
}

/**
 * sdbl_create_plain_msg - Append bare top-level arguments to a DBus message.
 *
 * Builds a message of the form: field0 field1 ...  (no container)
 * The caller must set psdbl_msg->pszmsg_type (e.g. "ss"), msg_type_size,
 * and populate fields[] before calling this function.
 *
 * @pmsg:       DBus message to append to
 * @psdbl_msg: sdbl_msg_t holding the fields in fields[]
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_create_plain_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    int ret;
    int i;

    for (i = 0; i < psdbl_msg->msg_type_size; i++) {
        ret = sdbl_basic_handle(pmsg, psdbl_msg->pszmsg_type[i], IO_APPEND, &psdbl_msg->fields[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to append plain field %d: %s\n", i, strerror(-ret));
            return -1;
        }
    }
    return 0;
}

/**
 * sdbl_parse_plain_msg - Parse bare top-level arguments from a DBus message.
 *
 * Reads fields directly from the message without entering any container.
 * The type signature and field count are taken from psdbl_msg->pszmsg_type
 * and psdbl_msg->msg_type_size.
 *
 * @pmsg:       DBus message to parse
 * @psdbl_msg: sdbl_msg_t to store the extracted fields in fields[]
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_parse_plain_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    int ret;
    int i;

    for (i = 0; i < psdbl_msg->msg_type_size; i++) {
        ret = sdbl_parse_basic(pmsg, &psdbl_msg->fields[i], psdbl_msg->pszmsg_type[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to read plain field %d: %s\n", i, strerror(-ret));
            return -1;
        }
    }
    return 0;
}

/**
 * sdbl_create_struct_msg - Append a struct body to a DBus message.
 *
 * Builds a message of the form: ( field0, field1, ... )
 * The caller must set psdbl_msg->pszmsg_type (e.g. "ss"), msg_type_size,
 * and populate fields[] before calling this function.
 *
 * @pmsg:       DBus message to append to
 * @psdbl_msg: sdbl_msg_t holding the struct fields in fields[]
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_create_struct_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    int ret;
    int i;

    ret = sd_bus_message_open_container(pmsg, SD_BUS_TYPE_STRUCT, psdbl_msg->pszmsg_type);
    if (ret < 0) {
        printf("[Q_Q] Failed to open struct container: %s\n", strerror(-ret));
        return -1;
    }
    for (i = 0; i < psdbl_msg->msg_type_size; i++) {
        ret = sdbl_basic_handle(pmsg, psdbl_msg->pszmsg_type[i], IO_APPEND, &psdbl_msg->fields[i]);
        if (ret < 0) {
            printf("[Q_Q] Failed to append struct field %d: %s\n", i, strerror(-ret));
            sd_bus_message_close_container(pmsg);
            return -1;
        }
    }
    ret = sd_bus_message_close_container(pmsg);
    if (ret < 0) {
        printf("[Q_Q] Failed to close struct container: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

/**
 * sdbl_create_msg - Dispatch to the appropriate message builder.
 *
 * Selects dict (a{XX}) or struct (XX...) creation based on psdbl_msg->format.
 *
 * @pmsg:       DBus message to append to
 * @psdbl_msg: sdbl_msg_t describing the message body
 *
 * Return: 0 on success, -1 on failure.
 */
static int sdbl_create_msg(sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    if (psdbl_msg->format == MSG_FMT_STRUCT) {
        SDBL_DBG("Creating struct message with signature: %s\n", psdbl_msg->pszmsg_type);
        return sdbl_create_struct_msg(pmsg, psdbl_msg);
    }
    if (psdbl_msg->format == MSG_FMT_PLAIN) {
        SDBL_DBG("Creating plain message with signature: %s\n", psdbl_msg->pszmsg_type);
        return sdbl_create_plain_msg(pmsg, psdbl_msg);
    }
    SDBL_DBG("Creating dict message with signature: %s\n", psdbl_msg->pszmsg_type);
    return sdbl_create_dict_msg(pmsg, psdbl_msg);
}

/**
 * sdbl_async_send - Send a DBus method call message asynchronously.
 * @pdbus:     sdbl structure
 * @psdbl_msg: sdbl_msg_t with the message body key-value pairs
 * @phandler:   callback to process the reply
 * @puser_data:  user data passed to the callback
 *
 * Return: 0 on success, negative errno on failure.
 */
int  sdbl_async_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message_handler_t phandler, void *puser_data)
{
    sd_bus_message *pmsg = NULL;
    int ret = 0;

    if (!pdbus || !psdbl_msg) {
        printf("[Q_Q] Invalid parameters for creating DBus message at %s %d\n", __func__, __LINE__);
        return -1;
    }
    if (!pdbus->pconn) {
        printf("[Q_Q] DBus connection is not initialized at %s %d\n", __func__, __LINE__);
        return -1;
    }

    SDBL_DBG("Bus: %s, Path: %s, Interface: %s, Member: %s\n", pdbus->pszbus_name, psdbl_msg->pszpath,  psdbl_msg->pszinterface, psdbl_msg->pszmember);

    pthread_mutex_lock(&pdbus->mutex);
    ret = sd_bus_message_new_method_call(pdbus->pconn, &pmsg,
                                         pdbus->pszbus_name,
                                         psdbl_msg->pszpath,
                                         psdbl_msg->pszinterface,
                                         psdbl_msg->pszmember);
    if (ret < 0) {
        printf("[Q_Q] Failed to create method call message: %s\n", strerror(-ret));
        pthread_mutex_unlock(&pdbus->mutex);
        return ret;
    }
    if (!pmsg) {
        printf("[Q_Q] Message allocation returned NULL\n");
        pthread_mutex_unlock(&pdbus->mutex);
        return ret;
    }
    pthread_mutex_unlock(&pdbus->mutex);

    ret = sdbl_create_msg(pmsg, psdbl_msg);
    if (ret < 0) {
        printf("[Q_Q] Failed to create DBus message body at %s %d\n", __func__, __LINE__);
        goto cleanup;
    }
    pthread_mutex_lock(&pdbus->mutex);
    ret = sd_bus_call_async(pdbus->pconn, NULL, pmsg, phandler, puser_data, 0);
    if (ret < 0) {
        printf("[Q_Q] Failed to send async call: %s\n", strerror(-ret));
        pthread_mutex_unlock(&pdbus->mutex);
        goto cleanup;
    }
    sd_bus_flush(pdbus->pconn);
    pthread_mutex_unlock(&pdbus->mutex);
cleanup:
    if (pmsg) {
        sd_bus_message_unref(pmsg);
    }
    return ret;
}

/**
 * sdbl_sync_send - Send a DBus method call and block until a reply arrives.
 * @pdbus:      sdbl structure
 * @psdbl_msg: sdbl_msg_t with the message body
 * @preply_out: if non-NULL, receives the reply message (caller must unref)
 *
 * Unlike sdbl_async_send, this function waits for the peer to reply before
 * returning, so the caller knows the call was processed successfully.
 *
 * Return: 0 on success, negative errno on failure.
 */
int sdbl_sync_send(sdbl *pdbus, sdbl_msg_t *psdbl_msg, sd_bus_message **preply_out)
{
    sd_bus_message *pmsg   = NULL;
    sd_bus_message *preply = NULL;
    sd_bus_error    error  = SD_BUS_ERROR_NULL;
    int ret = 0;

    if (!pdbus || !psdbl_msg) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    if (!pdbus->pconn) {
        printf("[Q_Q] DBus connection is not initialized at %s %d\n", __func__, __LINE__);
        return -1;
    }

    SDBL_DBG("Bus: %s, Path: %s, Interface: %s, Member: %s\n",
              pdbus->pszbus_name, psdbl_msg->pszpath,
              psdbl_msg->pszinterface, psdbl_msg->pszmember);

    pthread_mutex_lock(&pdbus->mutex);
    ret = sd_bus_message_new_method_call(pdbus->pconn, &pmsg,
                                         pdbus->pszbus_name,
                                         psdbl_msg->pszpath,
                                         psdbl_msg->pszinterface,
                                         psdbl_msg->pszmember);
    if (ret < 0) {
        printf("[Q_Q] Failed to create method call message: %s\n", strerror(-ret));
        pthread_mutex_unlock(&pdbus->mutex);
        return ret;
    }
    pthread_mutex_unlock(&pdbus->mutex);

    ret = sdbl_create_msg(pmsg, psdbl_msg);
    if (ret < 0) {
        printf("[Q_Q] Failed to build message body at %s %d\n", __func__, __LINE__);
        goto cleanup;
    }

    pthread_mutex_lock(&pdbus->mutex);
    ret = sd_bus_call(pdbus->pconn, pmsg, 0, &error, &preply);
    pthread_mutex_unlock(&pdbus->mutex);

    if (ret < 0) {
        printf("[Q_Q] Sync call failed for %s: %s\n",
               psdbl_msg->pszmember,
               error.message ? error.message : strerror(-ret));
        goto cleanup;
    }

    if (preply_out) {
        *preply_out = preply;
        preply = NULL; /* ownership transferred */
    }

cleanup:
    sd_bus_error_free(&error);
    if (preply) {
        sd_bus_message_unref(preply);
    }
    if (pmsg) {
        sd_bus_message_unref(pmsg);
    }
    return ret;
}

/**
 * sdbl_inhibit_lock - Acquire a systemd inhibit lock and listen for shutdown.
 * @psystem_conn:  system bus connection
 * @phandler:      callback for PrepareForShutdown signal
 * @puser_data:     user data passed to the callback
 * @pinhibit_data: inhibit parameters (what, who, why, mode)
 *
 * Return: 0 on success, -1 on failure.
 */
int  sdbl_inhibit_lock(sd_bus *psystem_conn, sdbl_callback phandler, void *puser_data, sdbl_inhibit_data_t *pinhibit_data)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *preply = NULL;
    int ret;

    if (!psystem_conn) {
        printf("[Q_Q] System bus connection is NULL\n");
        return -1;
    }
    ret = sd_bus_call_method(psystem_conn,
                             "org.freedesktop.login1",
                             "/org/freedesktop/login1",
                             "org.freedesktop.login1.Manager",
                             "Inhibit",
                             &error,
                             &preply,
                             "ssss",
                             pinhibit_data->pszwhat,
                             pinhibit_data->pszwho,
                             pinhibit_data->pszwhy,
                             pinhibit_data->pszmode);
    if (ret < 0) {
        printf("[Q_Q] Failed to issue Inhibit method call: %s\n", error.message);
        goto cleanup;
    }

    ret = sd_bus_message_read(preply, "h", &pinhibit_data->inhibit_fd);
    if (ret < 0) {
        printf("[Q_Q] Failed to parse reply: %s\n", strerror(-ret));
        goto cleanup;
    }

    printf("Got inhibit descriptor (FD: %d) — system shutdown is blocked\n", pinhibit_data->inhibit_fd);

    sd_bus_match_signal(psystem_conn,
                        NULL,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "PrepareForShutdown",
                        phandler,
                        puser_data);

cleanup:
    sd_bus_error_free(&error);
    if (preply) {
        sd_bus_message_unref(preply);
    }
    return (ret < 0) ? ret : 0;
}

/**
 * sdbl_inhibit_unlock - Release a systemd inhibit lock.
 * @pinhibit_data: pointer to the inhibit data structure
 *
 * Return: 0 on success.
 */
int sdbl_inhibit_unlock(sdbl_inhibit_data_t *pinhibit_data)
{
    if (pinhibit_data && pinhibit_data->inhibit_fd >= 0) {
        close(pinhibit_data->inhibit_fd);
        pinhibit_data->inhibit_fd = -1;
        printf("Inhibit unlocked — system may now shut down or suspend\n");
    }
    printf("Inhibit lock released successfully\n");
    return 0;
}

/**
 * sdbl_register_table - Register a vtable for server-side method dispatch.
 * @pdbus:        sdbl structure
 * @pvtable:   sd-bus vtable defining the methods
 * @puser_data: user data passed to method handlers
 *
 * Return: 0 on success, -1 on failure.
 */
int  sdbl_register_table(sdbl *pdbus, const sd_bus_vtable *pvtable, void *puser_data)
{
    int ret;
    if (!pdbus || !pvtable) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    ret = sd_bus_add_object_vtable(pdbus->pconn, NULL, pdbus->pszpath, pdbus->pszinterface, pvtable, puser_data);
    if (ret < 0) {
        printf("[Q_Q] Failed to register vtable: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

/**
 * sdbl_emit_signal - Emit a DBus signal with key-value data.
 * @pdbus:         sdbl structure
 * @psdbl_msg: sdbl_msg_t with the signal body key-value pairs
 *
 * Return: 0 on success, negative errno on failure.
 */
int  sdbl_emit_signal(sdbl *pdbus, sdbl_msg_t *psdbl_msg)
{
    sd_bus_message *pmsg = NULL;
    int ret;
    if (!pdbus || !psdbl_msg) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    ret = sd_bus_message_new_signal(pdbus->pconn, &pmsg, pdbus->pszpath, pdbus->pszinterface, psdbl_msg->pszmember);
    if (ret < 0) {
        printf("[Q_Q] Failed to create signal message: %s\n", strerror(-ret));
        return ret;
    }
    ret = sdbl_create_msg(pmsg, psdbl_msg);
    if (ret < 0) {
        printf("[Q_Q] Failed to create message container: %s\n", strerror(-ret));
        return -1;
    }

    ret = sd_bus_send(pdbus->pconn, pmsg, NULL);
    if (ret < 0) {
        printf("[Q_Q] DBus send failed in %s %d\n", __func__, __LINE__);
        return -1;
    }
    return 0;
}

/**
 * sdbl_method_reply - Reply to an incoming DBus method call.
 * @pdbus:         sdbl structure
 * @pmsg:       incoming DBus message to reply to
 * @psdbl_msg: sdbl_msg_t with the reply body key-value pairs
 *
 * Return: 0 on success, negative errno on failure.
 */
int  sdbl_method_reply(sdbl *pdbus, sd_bus_message *pmsg, sdbl_msg_t *psdbl_msg)
{
    sd_bus_message *preply = NULL;
    int ret;

    if (!pdbus || !pmsg || !psdbl_msg) {
        printf("[Q_Q] Invalid parameters for %s\n", __func__);
        return -1;
    }
    ret = sd_bus_message_new_method_return(pmsg, &preply);
    if (ret < 0) {
        printf("[Q_Q] Failed to create method return message: %s\n", strerror(-ret));
        return ret;
    }
    ret = sdbl_create_msg(preply, psdbl_msg);
    if (ret < 0) {
        printf("[Q_Q] Failed to create message container: %s\n", strerror(-ret));
        return -1;
    }

    ret = sd_bus_send(pdbus->pconn, preply, NULL);
    if (ret < 0) {
        printf("[Q_Q] DBus send failed in %s %d\n", __func__, __LINE__);
        return -1;
    }
    return 0;
}