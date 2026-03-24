#pragma once

#include <ntdef.h>

/* ── Shell constants ────────────────────────────────────────────────────── */
#define SHELL_INPUT_MAX     512
#define SHELL_HISTORY_MAX   16
#define SHELL_ENV_MAX       32
#define SHELL_ENV_NAME_MAX  32
#define SHELL_ENV_VAL_MAX   128

/* ── Environment variable ───────────────────────────────────────────────── */
typedef struct _SHELL_ENV_VAR {
    CHAR    Name[SHELL_ENV_NAME_MAX];
    CHAR    Value[SHELL_ENV_VAL_MAX];
    BOOL    Used;
} SHELL_ENV_VAR, *PSHELL_ENV_VAR;

/* ── Shell interface ────────────────────────────────────────────────────── */
VOID    Shell_Run(VOID);
VOID    Shell_PrintBanner(VOID);
