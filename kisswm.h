char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

char *term[] = { "st", NULL };

Key keys[] = {
        { Mod4Mask,                     XK_Return,              spawn,                  { .v = term} },
        { Mod4Mask,                     XK_q,                   killclient,             {0} },
        { Mod4Mask|ShiftMask,           XK_Return,              test,                   { .v = "This is a test with Both modifier"} },
        { ShiftMask,                    XK_Return,              test,                   { .v = "This is a Shiftmask test modifier"} }
};
