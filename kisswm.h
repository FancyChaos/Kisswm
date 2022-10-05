char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
static const char dmenufont[]       = "Fira Code Nerd Font:pixelsize=18:antialias=true";

static const char *barfont       = "Fira Code Nerd Font:pixelsize=18:antialias=true";
static int barheight = 40;

static const char *term[] = { "st", NULL };
static const char *lock[] = { "fxlock", NULL };
static const char *dmenucmd[] = { "dmenu_run", "-fn", dmenufont, NULL };

#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
        { MODKEY,                       KEY,      focustag,           {.ui = TAG} }, \
        { MODKEY|ShiftMask,             KEY,      mvwintotag,         {.ui = TAG} }, \

Key keys[] = {
        { MODKEY,                     XK_Return,              spawn,                  {.v = term} },
        { MODKEY,                     XK_d,                   spawn,                  {.v = dmenucmd} },
        { MODKEY|ShiftMask,           XK_l,                   spawn,                  {.v = lock} },
        { MODKEY,                     XK_q,                   killclient,             {0} },
        { MODKEY,                     XK_f,                   fullscreen,             {0} },

        { MODKEY,                     XK_j,                   cycleclient,            {.i = -1} },
        { MODKEY,                     XK_Left,                cycleclient,            {.i = -1} },
        { MODKEY,                     XK_k,                   cycleclient,            {.i = +1} },
        { MODKEY,                     XK_Right,               cycleclient,            {.i = +1} },

        { MODKEY|ShiftMask,           XK_Left,                followwintotag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_y,                   followwintotag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_Right,               followwintotag,         {.i = 1} },
        { MODKEY|ShiftMask,           XK_x,                   followwintotag,         {.i = 1} },

        { MODKEY|ControlMask,         XK_Left,                cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_j,                   cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_Right,               cycletag,               {.i = 1} },
        { MODKEY|ControlMask,         XK_k,                   cycletag,               {.i = 1} },

        { MODKEY,                     XK_comma,               cyclemon,               {.i = -1 } },
        { MODKEY,                     XK_period,              cyclemon,               {.i = 1 } },

        { MODKEY|ShiftMask,           XK_j,                   mvwin,                  {.i = -1} },
        { MODKEY|ShiftMask,           XK_k,                   mvwin,                  {.i = 1} },

        { MODKEY,                     XK_h,                   updatemasteroffset,     {.i = -50} },
        { MODKEY,                     XK_l,                   updatemasteroffset,     {.i = 50} },

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
