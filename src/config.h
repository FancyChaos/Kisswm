// Tags on which clients will be managed on
char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

// Demnu font
static const char dmenufont[]       = "Fira Code Nerd Font:pixelsize=18:antialias=true";

// Statusbar font
static const char *barfont       = "Fira Code Nerd Font:pixelsize=18:antialias=true";

// Height of the statusbar in pixels
static int barheight = 26;

// Witdh of client borders in pixels
static int borderwidth = 2;

// Colors in RGB representation (alpha, red, green, blue)
static unsigned long barbg = 0;
static unsigned long barfg = 0xFFFAEBD7;
static unsigned long bordercolor = 0xFFFAEBD7;
static unsigned long bordercolor_inactive = 0xC56F6C69;
static unsigned long bordercolor_urgent = 0xFFCC3333;

// Usable layouts:
// MASTER_STACK_LAYOUT, SIDE_BY_SIDE_LAYOUT,
// STACK_LAYOUT, NULL (For future floating only mode)
static Layout_Func layouts_available[] = {
    MASTER_STACK_LAYOUT,
    SIDE_BY_SIDE_LAYOUT,
    STACK_LAYOUT
};

// Buttons (Mouse clicks) to which windows gets focused
static const unsigned int buttons[] = {Button1, Button2};

// Default commands to spawn
static const char *term[] = {"st", NULL};
static const char *lock[] = {"fxlock", NULL};
static const char *dmenucmd[] = {"dmenu_run", "-fn", dmenufont, NULL};

// Keybindings which utilize all tags
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
        { MODKEY,                       KEY,      key_focus_tag,                 {.ui = TAG} }, \
        { MODKEY|ShiftMask,             KEY,      key_move_client_to_tag,         {.ui = TAG} }, \

// General keybindings
Key keys[] = {
        { MODKEY,                     XK_Return,              key_spawn,                        {.v = term} },
        { MODKEY,                     XK_d,                   key_spawn,                        {.v = dmenucmd} },
        { MODKEY|ShiftMask,           XK_l,                   key_spawn,                        {.v = lock} },
        { MODKEY,                     XK_q,                   key_kill_client,                  {0} },
        { MODKEY,                     XK_f,                   key_fullscreen,                   {0} },

        { MODKEY,                     XK_k,                   key_cycle_client,                 {.i = +1} },
        { MODKEY,                     XK_Right,               key_cycle_client,                 {.i = +1} },
        { MODKEY,                     XK_j,                   key_cycle_client,                 {.i = -1} },
        { MODKEY,                     XK_Left,                key_cycle_client,                 {.i = -1} },

        { MODKEY|ShiftMask,           XK_Right,               key_follow_client_to_tag,         {.i = 1} },
        { MODKEY|ShiftMask,           XK_x,                   key_follow_client_to_tag,         {.i = 1} },
        { MODKEY|ShiftMask,           XK_Left,                key_follow_client_to_tag,         {.i = -1} },
        { MODKEY|ShiftMask,           XK_y,                   key_follow_client_to_tag,         {.i = -1} },

        { MODKEY|ControlMask,         XK_Right,               key_cycle_tag,                    {.i = 1} },
        { MODKEY|ControlMask,         XK_k,                   key_cycle_tag,                    {.i = 1} },
        { MODKEY|ControlMask,         XK_Left,                key_cycle_tag,                    {.i = -1} },
        { MODKEY|ControlMask,         XK_j,                   key_cycle_tag,                    {.i = -1} },

        { MODKEY,                     XK_period,              key_cycle_monitor,                {.i = 1 } },
        { MODKEY,                     XK_comma,               key_cycle_monitor,                {.i = -1 } },

        { MODKEY|ShiftMask,           XK_period,              key_move_client_to_monitor,       {.i = 1 } },
        { MODKEY|ShiftMask,           XK_comma,               key_move_client_to_monitor,       {.i = -1 } },

        { MODKEY|ShiftMask,           XK_k,                   key_move_client,                  {.i = 1} },
        { MODKEY|ShiftMask,           XK_j,                   key_move_client,                  {.i = -1} },

        { MODKEY,                     XK_l,                   key_update_master_offset,         {.i = 50} },
        { MODKEY,                     XK_h,                   key_update_master_offset,         {.i = -50} },

        { MODKEY,                     XK_m,                   key_change_layout,                {0} },

        { MODKEY|ShiftMask,           XK_w,                   key_create_workspace,             {0} },
        { MODKEY|ShiftMask,           XK_p,                   key_delete_workspace,             {0} },
        { MODKEY,                     XK_Up,                  key_cycle_workspace,              {.i = 1} },
        { MODKEY,                     XK_Down,                key_cycle_workspace,              {.i = -1} },

        /* Tag keys */
        TAGKEYS(                      XK_1,                                                     1)
        TAGKEYS(                      XK_2,                                                     2)
        TAGKEYS(                      XK_3,                                                     3)
        TAGKEYS(                      XK_4,                                                     4)
        TAGKEYS(                      XK_5,                                                     5)
        TAGKEYS(                      XK_6,                                                     6)
        TAGKEYS(                      XK_7,                                                     7)
        TAGKEYS(                      XK_8,                                                     8)
        TAGKEYS(                      XK_9,                                                     9)
};
