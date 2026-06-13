#ifndef XZ_CLI_H
#define XZ_CLI_H

typedef enum {
    XZ_CMD_NONE = 0,
    XZ_CMD_CHAT,
    XZ_CMD_TEXT,
    XZ_CMD_STOP,
    XZ_CMD_STATUS,
    XZ_CMD_VOLUME,
    XZ_CMD_OTA,
    XZ_CMD_QUIT,
    XZ_CMD_UNKNOWN
} xz_cmd_t;

typedef struct {
    xz_cmd_t cmd;
    char arg[1024];
} xz_cli_command_t;

void xz_cli_init(void);
xz_cli_command_t xz_cli_read(void);
void xz_cli_print_help(void);

#endif
