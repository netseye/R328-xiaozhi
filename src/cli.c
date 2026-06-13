#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void xz_cli_init(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

static xz_cli_command_t parse_line(const char *line) {
    xz_cli_command_t cmd = {XZ_CMD_NONE, ""};

    char buf[1024];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    nl = strchr(buf, '\r');
    if (nl) *nl = '\0';

    if (buf[0] == '\0') return cmd;

    if (strcmp(buf, "chat") == 0) {
        cmd.cmd = XZ_CMD_CHAT;
    } else if (strncmp(buf, "text ", 5) == 0) {
        cmd.cmd = XZ_CMD_TEXT;
        strncpy(cmd.arg, buf + 5, sizeof(cmd.arg) - 1);
    } else if (strcmp(buf, "stop") == 0) {
        cmd.cmd = XZ_CMD_STOP;
    } else if (strcmp(buf, "status") == 0) {
        cmd.cmd = XZ_CMD_STATUS;
    } else if (strncmp(buf, "volume ", 7) == 0) {
        cmd.cmd = XZ_CMD_VOLUME;
        strncpy(cmd.arg, buf + 7, sizeof(cmd.arg) - 1);
    } else if (strcmp(buf, "ota") == 0) {
        cmd.cmd = XZ_CMD_OTA;
    } else if (strcmp(buf, "quit") == 0 || strcmp(buf, "q") == 0) {
        cmd.cmd = XZ_CMD_QUIT;
    } else if (strcmp(buf, "help") == 0) {
        xz_cli_print_help();
    } else {
        cmd.cmd = XZ_CMD_UNKNOWN;
    }

    return cmd;
}

xz_cli_command_t xz_cli_read(void) {
    char line[1024];
    ssize_t n = read(STDIN_FILENO, line, sizeof(line) - 1);
    if (n <= 0) return (xz_cli_command_t){XZ_CMD_NONE, ""};
    line[n] = '\0';
    return parse_line(line);
}

void xz_cli_print_help(void) {
    printf("Commands:\n");
    printf("  chat            Start voice conversation\n");
    printf("  text <message>  Send text message\n");
    printf("  stop            Abort current conversation\n");
    printf("  status          Show current state\n");
    printf("  volume <0-31>   Set playback volume\n");
    printf("  ota             Register device via OTA\n");
    printf("  quit            Exit program\n");
}
