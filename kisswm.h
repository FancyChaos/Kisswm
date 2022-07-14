char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
static const char dmenufont[]       = "Fira Code Nerd Font:pixelsize=28:antialias=true";

char *term[] = { "st", NULL };
static const char *dmenucmd[] = { "dmenu_run", "-fn", dmenufont, NULL };

#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
        { MODKEY,                       KEY,      focustag,           {.ui = TAG} }, \
        { MODKEY|ShiftMask,            KEY,      mvwintotag,         {.ui = TAG} }, \

Key keys[] = {
        { MODKEY,                     XK_Return,              spawn,                  {.v = term} },
        { MODKEY,                     XK_d,                   spawn,                  {.v = dmenucmd} },
        { MODKEY,                     XK_q,                   killclient,             {0} },

        { MODKEY,                     XK_j,                   cycleclient,            {.i = -1} },
        { MODKEY,                     XK_Left,                cycleclient,            {.i = -1} },
        { MODKEY,                     XK_k,                   cycleclient,            {.i = +1} },
        { MODKEY,                     XK_Right,               cycleclient,            {.i = +1} },
        
        { MODKEY|ShiftMask,           XK_Left,                followwintotag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_Right,               followwintotag,         {.i = 1} },

        { MODKEY|ControlMask,         XK_Left,                cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_j,                   cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_Right,               cycletag,               {.i = 1} },
        { MODKEY|ControlMask,         XK_k,                   cycletag,               {.i = 1} },

        { MODKEY|ShiftMask,           XK_j,                   mvwin,             {.i = -1} },
        { MODKEY|ShiftMask,           XK_k,                   mvwin,             {.i = 1} },

        /* Tag keys */
        TAGKEYS(                      XK_1,                                           1)
        TAGKEYS(                      XK_2,                                           2)
        TAGKEYS(                      XK_3,                                           3)
        TAGKEYS(                      XK_4,                                           4)
        TAGKEYS(                      XK_5,                                           5)
        TAGKEYS(                      XK_6,                                           6)
        TAGKEYS(                      XK_7,                                           7)
        TAGKEYS(                      XK_8,                                           8)
        TAGKEYS(                      XK_9,                                           9)
};
