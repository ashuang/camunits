#ifndef DBG_H
#define DBG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define _NORMAL_    "\x1b[0m"
#define _BLACK_     "\x1b[30;47m"
#define _RED_       "\x1b[31;40m"
#define _GREEN_     "\x1b[32;40m"
#define _YELLOW_    "\x1b[33;40m"
#define _BLUE_      "\x1b[34;40m"
#define _MAGENTA_   "\x1b[35;40m"
#define _CYAN_      "\x1b[36;40m"
#define _WHITE_     "\x1b[37;40m"

#define _BRED_      "\x1b[1;31;40m"
#define _BGREEN_    "\x1b[1;32;40m"
#define _BYELLOW_   "\x1b[1;33;40m"
#define _BBLUE_     "\x1b[1;34;40m"
#define _BMAGENTA_  "\x1b[1;35;40m"
#define _BCYAN_     "\x1b[1;36;40m"
#define _BWHITE_    "\x1b[1;37;40m"

#define DBG_MODE(x) (1ULL << (x))

#define DBG_ALL     (~0ULL)
#define DBG_ERROR   DBG_MODE(0)
#define DBG_DEFAULT DBG_ERROR

// ================== add debugging modes here ======================

#define DBG_UNIT            DBG_MODE(1) /* foo */
#define DBG_CHAIN           DBG_MODE(2)
#define DBG_DRIVER          DBG_MODE(3)
#define DBG_MANAGER         DBG_MODE(4)
#define DBG_INPUT           DBG_MODE(5)
#define DBG_FILTER          DBG_MODE(6)
#define DBG_OUTPUT          DBG_MODE(7)
#define DBG_GUI             DBG_MODE(8)
#define DBG_CONTROL         DBG_MODE(9)
#define DBG_REF             DBG_MODE(10)
#define DBG_PLUGIN          DBG_MODE(11)
#define DBG_LOG             DBG_MODE(12)
#define DBG_13              DBG_MODE(13)
#define DBG_14              DBG_MODE(14)
#define DBG_15              DBG_MODE(15)
#define DBG_16              DBG_MODE(16)

/// There can be no white space in these strings

#define DBG_NAMETAB \
{ "all", DBG_ALL }, \
{ "error", DBG_ERROR }, \
{ "unit", DBG_UNIT }, \
{ "driver", DBG_DRIVER }, \
{ "chain", DBG_CHAIN }, \
{ "manager", DBG_MANAGER }, \
{ "input", DBG_INPUT }, \
{ "filter", DBG_FILTER }, \
{ "output", DBG_OUTPUT }, \
{ "gui",  DBG_GUI }, \
{ "control",  DBG_CONTROL }, \
{ "ref", DBG_REF }, \
{ "plugin", DBG_PLUGIN }, \
{ "log", DBG_LOG }, \
{ "13", DBG_13 }, \
{ "14", DBG_14 }, \
{ "15", DBG_15 }, \
{ "16", DBG_16 }, \
{ NULL,     0 } 

#define DBG_COLORTAB \
{ DBG_UNIT, _CYAN_ "Unit: "}, \
{ DBG_CHAIN, _RED_ "Chain: "}, \
{ DBG_DRIVER, _WHITE_ "Driver: "}, \
{ DBG_MANAGER, _YELLOW_ "Manager: "}, \
{ DBG_INPUT, _GREEN_ "Input: "}, \
{ DBG_FILTER, _BLUE_ "Filter: "}, \
{ DBG_OUTPUT, _MAGENTA_ "Output: "}, \
{ DBG_GUI, _BCYAN_ "GUI: "}, \
{ DBG_CONTROL, _BYELLOW_ "UnitControl: " }, \
{ DBG_REF, _BGREEN_ "RefCount: " }, \
{ DBG_PLUGIN, _BMAGENTA_ "Plugin:" }, \
{ DBG_LOG, _BYELLOW_ "Log:" }, \
{ DBG_13, _CYAN_ }, \
{ DBG_14, _BBLUE_ }, \
{ DBG_15, _BMAGENTA_ }, \
{ DBG_16, _BWHITE_ } \

#define DBG_ENV     "CAM_DBG"


// ===================  do not modify after this line ==================

static long long dbg_modes = 0;
static short dbg_initiated = 0;

typedef struct dbg_mode {
    const char *d_name;
    unsigned long long d_mode;
    const char *color;
} dbg_mode_t;

typedef struct dbg_mode_color {
    unsigned long long d_mode;
    const char *color;
} dbg_mode_color_t;


static dbg_mode_color_t dbg_colortab[] = {
    DBG_COLORTAB
};

static dbg_mode_t dbg_nametab[] = {
    DBG_NAMETAB
};

static inline 
const char* DCOLOR(unsigned long long d_mode)
{
    dbg_mode_color_t *mode;

    for (mode = dbg_colortab; mode->d_mode != 0; mode++)
    {
        if (mode->d_mode & d_mode)
            return mode->color;
    }

    return _BWHITE_;
}

static void dbg_init()
{
    const char *dbg_env;
    dbg_initiated = 1;

    dbg_modes = DBG_DEFAULT;

    dbg_env = getenv(DBG_ENV);
    if (!dbg_env) {
        return;
    } else {
        char env[256];
        char *name;

        strncpy(env, dbg_env, sizeof(env));
        for (name = strtok(env,","); name; name = strtok(NULL, ",")) {
            int cancel;
            dbg_mode_t *mode;

            if (*name == '-') {
                cancel = 1;
                name++;
            }
            else
                cancel = 0;

            for (mode = dbg_nametab; mode->d_name != NULL; mode++)
                if (strcmp(name, mode->d_name) == 0)
                    break;
            if (mode->d_name == NULL) {
                fprintf(stderr, "Warning: Unknown debug option: "
                        "\"%s\"\n", name);
                return;
            }

            if (cancel) 
            {
                dbg_modes &= ~mode->d_mode;
            }
            else
            {
                dbg_modes = dbg_modes | mode->d_mode;    
            }

        }
    }
}

#ifndef NO_DBG

#define dbg(mode, args...) { \
    if( !dbg_initiated) dbg_init(); \
    if( dbg_modes & (mode) ) { \
        fprintf(stderr, "%s", DCOLOR(mode)); \
        fprintf(stderr, args); \
        fprintf(stderr, _NORMAL_); \
    } \
}
#define dbgl(mode, args...) { \
    if( !dbg_initiated) dbg_init(); \
    if( dbg_modes & (mode) ) { \
        fprintf(stderr, "%s%s:%d ", DCOLOR(mode), __FILE__, __LINE__); \
        fprintf(stderr, args); \
        fprintf(stderr, _NORMAL_); \
    } \
}
#define dbg_active(mode) (dbg_modes & (mode))

#else

#define dbg(mode, args...) 
#define dbg_active(mode) false
#define cdbg(mode,color,dtag,arg) 

#endif

#define cprintf(color, args...) { printf(color); printf(args); \
    printf(_NORMAL_); }



#ifdef __cplusplus
}
#endif
#endif
