char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const char dmenufont[]       = "Fira Code Nerd Font:pixelsize=18:antialias=true";
static const char *barfont       = "Fira Code Nerd Font:pixelsize=18:antialias=true";

static int barheight = 26;
static int borderwidth = 3;

// Colors in RGB representation (alpha, red, green, blue)
static unsigned long barbg = 0;
static unsigned long barfg = 0xFFFAEBD7;
static unsigned long bordercolor = 0xFFFAEBD7;
static unsigned long bordercolor_inactive = 0xFF6F6C69;
static unsigned long bordercolor_urgent = 0xFFCC3333;

static const char *term[] = {"st", NULL};
static const char *lock[] = {"fxlock", NULL};
static const char *dmenucmd[] = {"dmenu_run", "-fn", dmenufont, NULL};

// Buttons (Mouse clicks) to which windows gets focused
static const unsigned int buttons[] = {Button1, Button2};

#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
        { MODKEY,                       KEY,      key_focustag,           {.ui = TAG} }, \
        { MODKEY|ShiftMask,             KEY,      key_mvwintotag,         {.ui = TAG} }, \

Key keys[] = {
        { MODKEY,                     XK_Return,              key_spawn,                  {.v = term} },
        { MODKEY,                     XK_d,                   key_spawn,                  {.v = dmenucmd} },
        { MODKEY|ShiftMask,           XK_l,                   key_spawn,                  {.v = lock} },
        { MODKEY,                     XK_q,                   key_killclient,             {0} },
        { MODKEY,                     XK_f,                   key_fullscreen,             {0} },

        { MODKEY,                     XK_j,                   key_cycleclient,            {.i = -1} },
        { MODKEY,                     XK_Left,                key_cycleclient,            {.i = -1} },
        { MODKEY,                     XK_k,                   key_cycleclient,            {.i = +1} },
        { MODKEY,                     XK_Right,               key_cycleclient,            {.i = +1} },

        { MODKEY|ShiftMask,           XK_Left,                key_followwintotag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_y,                   key_followwintotag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_Right,               key_followwintotag,         {.i = 1} },
        { MODKEY|ShiftMask,           XK_x,                   key_followwintotag,         {.i = 1} },

        { MODKEY|ControlMask,         XK_Left,                key_cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_j,                   key_cycletag,               {.i = -1} },
        { MODKEY|ControlMask,         XK_Right,               key_cycletag,               {.i = 1} },
        { MODKEY|ControlMask,         XK_k,                   key_cycletag,               {.i = 1} },

        { MODKEY,                     XK_comma,               key_cyclemon,               {.i = -1 } },
        { MODKEY,                     XK_period,              key_cyclemon,               {.i = 1 } },

        { MODKEY|ShiftMask,           XK_comma,               key_mvwintomon,             {.i = -1 } },
        { MODKEY|ShiftMask,           XK_period,              key_mvwintomon,             {.i = 1 } },

        { MODKEY|ShiftMask,           XK_j,                   key_mvwin,                  {.i = -1} },
        { MODKEY|ShiftMask,           XK_k,                   key_mvwin,                  {.i = 1} },

        { MODKEY,                     XK_h,                   key_updatemasteroffset,     {.i = -50} },
        { MODKEY,                     XK_l,                   key_updatemasteroffset,     {.i = 50} },

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
