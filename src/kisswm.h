#define VERSION "v1.2.0"

enum ICCM_ATOMS {
        ICCCM_PROTOCOLS, ICCCM_DEL_WIN, ICCCM_FOCUS,
        ICCCM_END
};

enum EWMH_WINDOW_ATOMS {
        NET_SUPPORTED, NET_SUPPORTING, NET_WM_NAME, NET_STATE,
        NET_TYPE, NET_ACTIVE, NET_CLOSE,
        NET_END
};

enum EWMH_WINDOW_TYPES {
        NET_DESKTOP, NET_DOCK, NET_TOOLBAR, NET_MENU,
        NET_UTIL, NET_SPLASH, NET_DIALOG, NET_NORMAL,
        NET_TYPES_END
};

enum EWMH_WINDOW_STATES {
        NET_HIDDEN, NET_FULLSCREEN,
        NET_STATES_END
};

enum CLIENT_FLAGS {
        CL_URGENT       = 1 << 0,
        CL_DIALOG       = 1 << 1,
        CL_MANAGED      = 1 << 2,
        CL_HIDDEN       = 1 << 3
};

typedef union {
        int i;
        unsigned int ui;
        float f;
        void *v;
} Arg;

typedef struct {
        unsigned int modmask;
        KeySym keysym;
        void (*f)(Arg*);
        Arg arg;
} Key;

typedef struct {
        XftColor xft_color;
        Colormap cmap;
} Color;

struct Colors {
        Color barbg;
        Color barfg;
        Color bordercolor;
        Color bordercolor_inactive;
        Color bordercolor_urgent;
};

typedef struct Monitor Monitor;
typedef struct Workspace Workspace;
typedef struct Client Client;
typedef struct Tag Tag;
typedef struct Colors Colors;
typedef struct Statusbar Statusbar;
typedef struct Layout Layout;
typedef struct Layout_Meta Layout_Meta;
typedef void (*Layout_Func) (Monitor*, Layout_Meta*);

struct Layout_Meta {
        int master_offset;
};

struct Layout {
        Layout_Meta meta;
        Layout_Func f;
        size_t index;
};

struct Statusbar {
        Window win;
        XftDraw *xdraw;
        Colormap cmap;
        int depth;
        int width;
        int height;
        char wm_name[256];
};

struct Client {
        Window win;
        Monitor *mon;
        Workspace *ws;
        Tag *tag;
        Client *next, *prev;
        Client *nextfocus, *prevfocus;
        // Client flags
        int cf;
        int x;
        int y;
        int width;
        int height;
        int bw;
};

struct Tag {
        // Tag number
        int num;
        // Client counters
        int clientnum;
        int clientnum_managed;
        // Assigned workspace
        Workspace *ws;
        // Assigned layout
        Layout *layout;
        // Clients
        Client *clients;
        Client *client_last;
        Client *client_fullscreen;
        // Client focus order
        Client *clients_focus;
};

struct Workspace {
        Workspace *next;
        Workspace *prev;
        // Assigned monitor
        Monitor *mon;
        Tag *tags;
        Tag *tag;
        // Workspace number
        uint8_t num;
        // State of the workspace as string. Will be displayed in the statusbar
        char *state;
        // Size of the state string
        unsigned long state_size;
};

struct Monitor {
        // Name of monitor (atom)
        char *aname;
        Monitor *next;
        Monitor *prev;
        // Workspaces
        Workspace *wss;
        // Current workspace
        Workspace *ws;
        uint8_t ws_count;
        int snum;
        int x;
        int y;
        int height;
        int width;
};

void            MASTER_STACK_LAYOUT(Monitor*, Layout_Meta*);
void            SIDE_BY_SIDE_LAYOUT(Monitor*, Layout_Meta*);
void            STACK_LAYOUT(Monitor*, Layout_Meta*);

void            key_spawn(Arg*);
void            key_move_client_to_tag(Arg*);
void            key_move_client_to_monitor(Arg*);
void            key_follow_client_to_tag(Arg*);
void            key_move_client(Arg*);
void            key_fullscreen(Arg*);
void            key_focus_tag(Arg*);
void            key_cycle_tag(Arg*);
void            key_cycle_client(Arg*);
void            key_cycle_monitor(Arg*);
void            key_create_workspace(Arg*);
void            key_move_client_to_workspace(Arg*);
void            key_cycle_workspace(Arg*);
void            key_delete_workspace(Arg*);
void            key_kill_client(Arg*);
void            key_update_master_offset(Arg*);
void            key_change_layout(Arg*);

void            keypress(XEvent*);
void            configurenotify(XEvent*);
void            propertynotify(XEvent*);
void            configurerequest(XEvent*);
void            maprequest(XEvent*);
void            destroynotify(XEvent*);
void            clientmessage(XEvent*);
void            mappingnotify(XEvent*);
void            buttonpress(XEvent*);
void            enternotify(XEvent*);

void            run(void);
int             onxerror(Display*, XErrorEvent*);
int             wm_detected(Display*, XErrorEvent*);
void            grabbutton(Window w, unsigned int button);
void            grabbuttons(Window w);
void            ungrabbutton(Window w, unsigned int button);
void            ungrabbuttons(Window w);
void            createcolor(unsigned long color, Color*);
void            setborder(Window, int, Color*);
void            setborders(Tag*);
void            setup(void);
void            initialize_monitors(void);
void            grabkeys(void);
Client*         get_first_managed_client(Tag*);
Client*         get_last_managed_client(Tag*);
Atom            getwinprop(Window, Atom);
Client*         wintoclient(Window);
bool            sendevent(Window, Atom);
unsigned int    cleanmask(unsigned int);

Monitor*        monitor_create(XRRMonitorInfo*);
void            monitor_update(Monitor *m, XRRMonitorInfo*);
void            monitor_destroy(Monitor*, Monitor*);
void            monitor_free(Monitor*);

Workspace*      workspace_create(Monitor*);
Workspace*      workspace_add(Monitor*);
void            workspace_delete(Workspace*);
void            workspace_update_state(Workspace*);
void            workspace_free(Workspace*);

void            hide(Client*);
void            unhide(Client*);
void            focustag(Tag*);
void            updatetagmasteroffset(Monitor*, int);
void            focusmon(Monitor*);
void            refresh_monitors(void);
void            remaptag(Tag*);
void            mapclient(Client*);
void            unmapclient(Client*);
void            setfullscreen(Client*);
void            unsetfullscreen(Client*);
void            closeclient(Client*);
void            move_client_to_tag(Client*, Tag*);
void            move_tag_to_tag(Tag*, Tag*);
void            attach(Client*);
void            detach(Client*);
void            focusattach(Client*);
void            focusdetach(Client*);
void            focusclient(Client*, bool);
void            focus(Window, Client*, bool);
void            arrange(void);
void            arrangemon(Monitor*);
void            set_window_size(Window, int, int, int, int);
void            set_window_dimension(Window, int, int);
void            set_window_position(Window, int, int);
void            set_client_size(Client*, int, int, int, int);
void            set_client_dimension(Client*, int, int);
void            set_client_position(Client*, int, int);

void            statusbar_update(void);
void            statusbar_update_wm_name(void);
void            statusbar_draw(Monitor*);

void (*handler[LASTEvent])(XEvent*) = {
        [KeyPress] = keypress,
        [ConfigureNotify] = configurenotify,
        [ConfigureRequest] = configurerequest,
        [PropertyNotify] = propertynotify,
        [MapRequest] = maprequest,
        [MappingNotify] = mappingnotify,
        [DestroyNotify] = destroynotify,
        [ClientMessage] = clientmessage,
        [ButtonPress] = buttonpress,
        [EnterNotify] = enternotify
};

Atom ATOM_UTF8;
Atom icccm_atoms[ICCCM_END];
Atom net_atoms[NET_END];
Atom net_win_types[NET_TYPES_END];
Atom net_win_states[NET_STATES_END];

Display *dpy;
Window root;
Monitor *mons, *selmon, *lastmon;
Client *selc;
Statusbar statusbar;

Colors colors;
XftFont *xfont;
XGlyphInfo xglyph;
XVisualInfo xvisual_info;

int screen;
int sw;
int currentmonnum;
long long monupdatetime = 0;
