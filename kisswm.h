char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

char *term[] = { "st", NULL };

Key keys[] = {
        { Mod4Mask,                     XK_Return,              spawn,                  {.v = term} },
        { Mod4Mask,                     XK_q,                   killclient,             {0 } },
        { Mod4Mask|ShiftMask,           XK_Return,              test,                   {.v = "This is a test with Both modifier"} },
        { ShiftMask,                    XK_Return,              test,                   {.v = "This is a Shiftmask test modifier"} },
        { Mod4Mask,                     XK_j,                   cycleclient,            {.i = +1} },
        { Mod4Mask,                     XK_Left,                cycleclient,            {.i = +1} },
        { Mod4Mask,                     XK_k,                   cycleclient,            {.i = -1} },
        { Mod4Mask,                     XK_Right,               cycleclient,            {.i = -1} },

        /* Tag keys */
        { Mod4Mask,                     XK_1,                   focustag,               {.ui = 1} },
        { Mod4Mask,                     XK_2,                   focustag,               {.ui = 2} },
        { Mod4Mask,                     XK_3,                   focustag,               {.ui = 3} },
        { Mod4Mask,                     XK_4,                   focustag,               {.ui = 4} },
        { Mod4Mask,                     XK_5,                   focustag,               {.ui = 5} },
        { Mod4Mask,                     XK_6,                   focustag,               {.ui = 6} },
        { Mod4Mask,                     XK_7,                   focustag,               {.ui = 7} },
        { Mod4Mask,                     XK_8,                   focustag,               {.ui = 8} },
        { Mod4Mask,                     XK_9,                   focustag,               {.ui = 9} },
};
