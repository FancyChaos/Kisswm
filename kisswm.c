#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#include <bsd/string.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>

#include "util.h"

enum { ICCCM_PROTOCOLS, ICCCM_DEL_WIN, ICCCM_FOCUS, ICCCM_END };
enum {
        NET_SUPPORTED, NET_SUPPORTING, NET_WM_NAME, NET_STATE,
        NET_TYPE, NET_ACTIVE, NET_CLOSE, NET_FULLSCREEN,
        NET_END
};

enum {
        NET_DESKTOP, NET_DOCK, NET_TOOLBAR, NET_MENU,
        NET_UTIL, NET_SPLAH, NET_DIALOG, NET_NORMAL,
        NET_TYPES_END
};

enum client_flags {
        CL_URGENT = 1 << 0,
        CL_DIALOG = 1 << 1,
        CL_FLOAT = 1 << 2
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
typedef struct Client Client;
typedef struct Tag Tag;
typedef struct Colors Colors;
typedef struct Statusbar Statusbar;

struct Statusbar {
        Window win;
        XftDraw *xdraw;
        Colormap cmap;
        int depth;
        int width;
        int height;
};

struct Client {
        Window win;
        Monitor *mon;
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
        int clientnum_tiling;
        int clientnum_floating;
        // Layout offset
        int master_offset;
        // Assigned monitor
        Monitor *mon;
        // Clients
        Client *clients;
        Client *client_last;
        Client *client_fullscreen;
        // Client focus order
        Client *clients_focus;
};

struct Monitor {
        // Name of monitor (atom)
        char *aname;
        Monitor *next;
        Monitor *prev;
        Tag *tags;
        Tag *tag;
        char *bartags;
        unsigned long bartagssize;
        int snum;
        int x;
        int y;
        int height;
        int width;
};

void    key_spawn(Arg*);
void    key_mvwintotag(Arg*);
void    key_mvwintomon(Arg*);
void    key_followwintotag(Arg*);
void    key_mvwin(Arg*);
void    key_fullscreen(Arg*);
void    key_focustag(Arg*);
void    key_cycletag(Arg*);
void    key_cycleclient(Arg*);
void    key_cyclemon(Arg*);
void    key_killclient(Arg*);
void    key_updatemasteroffset(Arg*);

void    keypress(XEvent*);
void    configurenotify(XEvent*);
void    propertynotify(XEvent*);
void    configurerequest(XEvent*);
void    maprequest(XEvent*);
void    destroynotify(XEvent*);
void    clientmessage(XEvent*);
void    mappingnotify(XEvent*);
void    buttonpress(XEvent*);
void    enternotify(XEvent*);

void    run(void);
int     onxerror(Display*, XErrorEvent*);
int     wm_detected(Display*, XErrorEvent*);
void    grabbutton(Window w, unsigned int button);
void    grabbuttons(Window w);
void    ungrabbutton(Window w, unsigned int button);
void    ungrabbuttons(Window w);
void    createcolor(unsigned long color, Color*);
void    setborder(Window, int, Color*);
void    setborders(Tag*);
void    populatemon(Monitor *m, XRRMonitorInfo*);
void    destroymon(Monitor*, Monitor*);
void    setup(void);
void    initmons(void);
void    grabkeys(void);
bool    alreadymapped(Window);
Atom    getwinprop(Window, Atom);
Client *wintoclient(Window);
bool    sendevent(Window, Atom);
Monitor *createmon(XRRMonitorInfo*);
unsigned int cleanmask(unsigned int);

void    focustag(Tag*);
void    updatetagmasteroffset(Monitor*, int);
void    focusmon(Monitor*);
void    updatemons(void);
void    remaptag(Tag*);
void    mapclient(Client*);
void    unmapclient(Client*);
void    setfullscreen(Client*);
void    unsetfullscreen(Client*);
void    closeclient(Client*);
void    generatebartags(Monitor*);
void    mvwintotag(Client*, Tag*);
void    mvwintomon(Client*, Monitor*, Tag*);
void    attach(Client*);
void    detach(Client*);
void    focusattach(Client*);
void    focusdetach(Client*);
void    focusclient(Client*, bool);
void    focus(Window, Client*, bool);
void    arrange(void);
void    arrangemon(Monitor*);

void    updatebars(void);
void    updatestatustext(void);
void    drawbar(Monitor*);

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

#include "kisswm.h"

Atom ATOM_UTF8;
Atom icccm_atoms[ICCCM_END];
Atom net_atoms[NET_END];
Atom net_win_types[NET_TYPES_END];

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

unsigned int tags_num = sizeof(tags)/sizeof(tags[0]);

char barstatus[256];


/*** X11 Eventhandling ****/

void
enternotify(XEvent *e)
{
        XCrossingEvent *ev = &e->xcrossing;
        if (root == ev->window) grabbuttons(root);
        else ungrabbuttons(root);
}

void
buttonpress(XEvent *e)
{
        XButtonPressedEvent *ev = &e->xbutton;
        if (selc && ev->window == selc->win) return;

        XAllowEvents(dpy, ReplayPointer, CurrentTime);

        if (ev->window == root) {
                for (Monitor *m = mons; m; m = m->next) {
                        if (ev->x_root <= (m->x + m->width)) {
                                if (m != selmon) {
                                        focusmon(m);
                                        if (selc) grabbuttons(selc->win);
                                        selc = NULL;
                                        focus(0, NULL, false);
                                }
                                break;
                        }
                }
                return;
        }

        Client *c = wintoclient(ev->window);
        if (!c) return;

        if (selmon != c->mon) focusmon(c->mon);
        focusclient(c, false);
        setborders(c->tag);
}

void
mappingnotify(XEvent *e)
{
        XMappingEvent *ev = &e->xmapping;
        if (ev->request != MappingModifier && ev->request != MappingKeyboard)
                return;

        XRefreshKeyboardMapping(ev);
        grabkeys();
}

void
clientmessage(XEvent *e)
{
        XClientMessageEvent *ev = &e->xclient;
        Client *c = wintoclient(ev->window);
        if (!c) return;

        if (ev->message_type == net_atoms[NET_ACTIVE] && c->tag != selmon->tag) {
                c->mon->bartags[c->tag->num * 2] = '!';
                c->cf |= CL_URGENT;
                setborders(c->tag);
        } else if (ev->message_type == net_atoms[NET_STATE]) {
                if (ev->data.l[1] == net_atoms[NET_FULLSCREEN] ||
                    ev->data.l[2] == net_atoms[NET_FULLSCREEN]) {
                            if (c->tag != c->mon->tag) return;

                            Client *fc = c->tag->client_fullscreen;
                            // Client is not allowed to interrupt
                            // different clients fullscreen state
                            if (fc && c != fc) return;
                            switch (ev->data.l[0]) {
                            case 2: // Toggle
                                    if (fc) unsetfullscreen(fc);
                                    else setfullscreen(c);
                                    break;
                            case 1: // Set fullscreen
                                    if (!fc) setfullscreen(c);
                                    break;
                            case 0: // Unset fullscreen
                                    if (fc) unsetfullscreen(fc);
                                    break;
                            default:
                                    return;
                            }
                            remaptag(c->tag);
                            arrangemon(c->tag->mon);
                            drawbar(c->tag->mon);
                }
        }

}

void
destroynotify(XEvent *e)
{
        XDestroyWindowEvent *ev = &e->xdestroywindow;
        Client *c = wintoclient(ev->window);
        closeclient(c);
        XSync(dpy, 0);
}

void
maprequest(XEvent *e)
{
        XMapRequestEvent *ev = &e->xmaprequest;
        if (alreadymapped(ev->window)) return;

        XWindowAttributes wa;
        if (!XGetWindowAttributes(dpy, ev->window, &wa)) return;

        // We assume the maprequest is on the current (selected) monitor
        // Get current tag
        Tag *ct = selmon->tag;

        Client *c = ecalloc(sizeof(Client), 1);
        c->next = c->prev = c->nextfocus = c->prevfocus = NULL;
        c->win = ev->window;
        c->mon = selmon;
        c->tag = ct;
        c->cf = 0;

        // Set floating if dialog
        Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
        if (net_win_types[NET_UTIL] == wintype ||
            net_win_types[NET_DIALOG] == wintype) {
                c->cf |= CL_DIALOG|CL_FLOAT;
        }

        attach(c);
        focusattach(c);

        // Already arrange monitor before new window is mapped
        // This will reduce flicker of client
        if (c->tag->client_fullscreen)
                unsetfullscreen(c->tag->client_fullscreen);
        arrangemon(c->mon);
        remaptag(c->tag);

        // Focus new client
        focusclient(c, true);

        drawbar(c->mon);

        XSelectInput(dpy, c->win, EnterWindowMask);
}

void
configurenotify(XEvent *e)
{
        XConfigureEvent *ev = &e->xconfigure;
        if (ev->window != root) return;

        // Only allow one monitor change event within a second
        struct timespec time;
        if (clock_gettime(CLOCK_BOOTTIME, &time) == 0) {
                if (time.tv_sec == monupdatetime) return;
                monupdatetime = time.tv_sec;
        }

        // Update monitor setup
        updatemons();
        focusmon(mons);

        // Calculate combined monitor width (screen width)
        sw = 0;
        for (Monitor *m = mons; m; m = m->next) sw += m->width;

        arrange();

        // Update statusbar to new width of combined monitors
        statusbar.width = sw;
        XWindowChanges wc = {
                .width = statusbar.width,
                .height = statusbar.height,
                .y = 0,
                .x = 0};
        XConfigureWindow(dpy, statusbar.win, CWX|CWY|CWWidth|CWHeight, &wc);
        updatebars();

        focusclient(selmon->tag->clients_focus, true);

        XSync(dpy, 0);
}

void
propertynotify(XEvent *e)
{
        XPropertyEvent *ev = &e->xproperty;
        if ((ev->window == root) && (ev->atom == XA_WM_NAME)) updatebars();
}

void
configurerequest(XEvent *e)
{
        XConfigureRequestEvent *ev = &e->xconfigurerequest;

        // Only allow custom sizes for dialog windows
        Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
        if (wintype != net_win_types[NET_UTIL] &&
            wintype != net_win_types[NET_DIALOG])
                return;

        XWindowChanges wc;

        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;

        XConfigureWindow(dpy, ev->window, (unsigned int)ev->value_mask, &wc);
}

void
keypress(XEvent *e)
{
        XKeyEvent *ev = &e->xkey;
        KeySym keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);

        for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i)
                if (keysym == keys[i].keysym
                    && cleanmask(ev->state) == cleanmask(keys[i].modmask)
                    && keys[i].f)
                        keys[i].f(&(keys[i].arg));
}

int
onxerror(Display *dpy, XErrorEvent *ee)
{
        char error[128] = {0};
        XGetErrorText(dpy, ee->error_code, error, 128);
        fprintf(stderr, "[ERROR]: %s\n\n", error);
        return 0;
}

/*** Statusbar functions ***/


void
updatebars(void)
{
        updatestatustext();
        for (Monitor *m = mons; m; m = m->next) drawbar(m);
}

void
drawbar(Monitor *m)
{
        // Clear bar section
        XClearArea(
                dpy,
                statusbar.win,
                m->x,
                0,
                (unsigned int) m->width,
                (unsigned int) statusbar.height,
                0);

        // Do not draw bar if fullscreen window on monitor
        Tag *t = m->tag;
        if (t->client_fullscreen) {
                XSync(dpy, 0);
                return;
        }

        XftDrawRect(
                statusbar.xdraw,
                &colors.barbg.xft_color,
                m->x,
                m->y,
                (unsigned int) m->width,
                (unsigned int) statusbar.height);

        int bartagslen = (int) strnlen(m->bartags, m->bartagssize);
        int barstatuslen = (int) strnlen(barstatus, sizeof(barstatus));

        // Get glyph information about statusbartags (Dimensions of text)
        XftTextExtentsUtf8(
                dpy,
                xfont,
                (XftChar8 *) m->bartags,
                bartagslen,
                &xglyph);
        int baroffset = statusbar.height - ((statusbar.height - xglyph.y) / 2);
        if (baroffset < 0) baroffset = xglyph.y;

        // Draw statusbartags text
        XftDrawStringUtf8(
                statusbar.xdraw,
                &colors.barfg.xft_color,
                xfont,
                m->x,
                m->y + baroffset,
                (XftChar8 *) m->bartags,
                bartagslen);

        if (barstatus[0] == '\0') {
                XSync(dpy, 0);
                return;
        }

        // Get glyph information about statusbarstatus (Dimensions of text)
        XftTextExtentsUtf8 (
                dpy,
                xfont,
                (XftChar8 *) barstatus,
                barstatuslen,
                &xglyph);

        // Draw statubarstatus
        XftDrawStringUtf8(
                statusbar.xdraw,
                &colors.barfg.xft_color,
                xfont,
                m->x + (m->width - xglyph.width),
                m->y + baroffset,
                (XftChar8 *) barstatus,
                barstatuslen);

        XSync(dpy, 0);
}

void
updatestatustext(void)
{
        // Reset barstatus
        barstatus[0] = '\0';

        XTextProperty pwmname;
        if (!XGetWMName(dpy, root, &pwmname)) {
                if (!XGetTextProperty(
                    dpy,
                    root,
                    &pwmname,
                    net_atoms[NET_WM_NAME])
                ) {
                        strlcpy(barstatus, "Kisswm\0", sizeof(barstatus));
                        return;
                }
        }

        if (pwmname.encoding == XA_STRING || pwmname.encoding == ATOM_UTF8)
                strlcpy(barstatus, (char*)pwmname.value, sizeof(barstatus));

        XFree(pwmname.value);
}

/*** WM state changing functions ***/

void
focusmon(Monitor *m)
{
        if (!m) return;

        // Previous monitor
        Monitor *pm = selmon;

        // Update bartags of previous focused monitor
        pm->bartags[pm->tag->num * 2] = '^';
        drawbar(pm);

        // Update bartags of monitor to focus
        m->bartags[m->tag->num * 2] = '>';
        drawbar(m);

        // Set current monitor
        selmon = m;

        // Set borders to inactive on previous monitor
        setborders(pm->tag);

        XSync(dpy, 0);
}
void
focustag(Tag *t)
{
        // Get monitor of tag and the current selected tag
        Monitor *m = t->mon;
        Tag *tc = m->tag;

        // Clear old tag identifier in the statusbar
        if (tc->clientnum) m->bartags[tc->num * 2] = '*';
        else m->bartags[tc->num * 2] = ' ';

        // Create new tag identifier in the statusbar
        m->bartags[t->num * 2] = '>';

        // Update the current active tag
        m->tag = t;

        // Redraw both tags
        remaptag(tc);
        remaptag(t);

        // arrange and focus
        arrangemon(m);
        focusclient(t->clients_focus, true);

        // Update statusbar (Due to bartags change)
        drawbar(m);
        XSync(dpy, 0);
}

void
setfullscreen(Client *c)
{
        // Already in fullscreen
        if (!c || c->tag->client_fullscreen || c->cf & CL_DIALOG) return;

        // Set fullscreen property
        XChangeProperty(
                dpy,
                c->win,
                net_atoms[NET_STATE],
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char*) &net_atoms[NET_FULLSCREEN],
                1);
        c->tag->client_fullscreen = c;
        XRaiseWindow(dpy, c->win);
        setborders(c->tag);
}

void
unsetfullscreen(Client *c)
{
        if (!c) return;

        // Set fullscreen property
        XDeleteProperty(
                dpy,
                c->win,
                net_atoms[NET_STATE]);
        c->tag->client_fullscreen = NULL;
        setborders(c->tag);
}

void
mvwintomon(Client *c, Monitor *m, Tag *t)
{
        if (!c || !m) return;

        // Tag of target monitor to move window to
        Tag *tt = t ? t : m->tag;

        detach(c);
        focusdetach(c);

        c->mon = m;
        c->tag = tt;

        attach(c);
        focusattach(c);
}

void
mvwintotag(Client *c, Tag *t)
{
        // Detach client from current tag
        detach(c);
        // Detach client from focus
        focusdetach(c);

        // Assign client to chosen tag
        c->tag = t;
        attach(c);
        focusattach(c);
}

void
mapclient(Client *c)
{
        if (!c) return;
        XMapWindow(dpy, c->win);
}

void
unmapclient(Client *c)
{
        if (!c) return;
        XUnmapWindow(dpy, c->win);
}

void
remaptag(Tag *t)
{
        if (!t) return;

        if (t != t->mon->tag) {
                // if tag is not the current active tag unmap everything
                for (Client *c = t->clients; c; c = c->next) unmapclient(c);
        } else if (t->client_fullscreen) {
                // only map focused client and dialogs on fullscreen
                for (Client *c = t->clients; c; c = c->next) {
                        if (c == t->client_fullscreen ||c->cf & CL_DIALOG) mapclient(c);
                        else unmapclient(c);
                }
        } else {
                for (Client *c = t->clients; c; c = c->next) mapclient(c);
        }
        XSync(dpy, 0);
}

void
closeclient(Client *c)
{
        if (!c) return;

        Monitor *m = c->mon;
        Tag *t = c->tag;

        if (c == selc) selc = NULL;
        if (c == t->client_fullscreen) t->client_fullscreen = NULL;

        // Detach and free
        detach(c);
        focusdetach(c);
        free(c);

        // Clear statusbar if last client and not focused
        if (!t->clientnum && t != m->tag) m->bartags[t->num * 2] = ' ';

        // if the tag where client closed is active (seen)
        if (t == m->tag) {
                remaptag(t);
                arrangemon(m);
                focusclient(t->clients_focus, true);
        }
        drawbar(m);
}

void
focus(Window w, Client *c, bool warp)
{
        if (!w && !c) {
                if (selc) w = selc->win;
                else w = root;
        } else if (!w && c) {
                w = c->win;
        }

        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
        sendevent(w, icccm_atoms[ICCCM_FOCUS]);
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_ACTIVE],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &w,
                1);
        XRaiseWindow(dpy, w);

        if (!warp) {
                XSync(dpy, 0);
                return;
        }

        if (w == root)
                XWarpPointer(
                        dpy, 0, w, 0, 0, 0, 0, selmon->x + selmon->width / 2,
                        selmon->y + selmon->height / 2);
        else if (c && c->win == w)
                XWarpPointer(
                        dpy, 0, w, 0, 0, 0, 0,  c->width / 2, c->height / 2);

        XSync(dpy, 0);
}

void
focusclient(Client *c, bool warp)
{
        // Try to focus client on the active tag
        // of the currently focused monitor
        if (!c) {
                c = selmon->tag->clients_focus;
                if (c && c != selc) {
                        focusclient(c, warp);
                } else if (!c) {
                        if (selc) grabbuttons(selc->win);
                        selc = NULL;
                        focus(0, NULL, warp);
                }
                return;
        }

        if (selc && c != selc) grabbuttons(selc->win);
        ungrabbuttons(c->win);

        Tag *t = c->tag;
        if (c != t->clients_focus) {
                if (c->prevfocus)
                        c->prevfocus->nextfocus = c->nextfocus;
                if (c->nextfocus)
                        c->nextfocus->prevfocus = c->prevfocus;

                c->prevfocus = t->clients_focus;
                if (c->prevfocus)
                        c->prevfocus->nextfocus = c;
        }
        c->nextfocus = NULL;
        t->clients_focus = c;
        c->cf &= ~CL_URGENT;

        selc = c;

        setborders(c->tag);

        focus(c->win, c, warp);
        XSync(dpy, 0);
}

void
focusattach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        // Reset fullscreen if not dialog
        if (!(c->cf & CL_DIALOG)) unsetfullscreen(c->tag->client_fullscreen);

        c->nextfocus = NULL;
        c->prevfocus = t->clients_focus;

        if (c->prevfocus) c->prevfocus->nextfocus = c;

        t->clients_focus = c;
}

void
focusdetach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        if (c == t->clients_focus)
                t->clients_focus = c->prevfocus;

        if (c->prevfocus)
                c->prevfocus->nextfocus = c->nextfocus;
        if (c->nextfocus)
                c->nextfocus->prevfocus = c->prevfocus;

        c->prevfocus = c->nextfocus = NULL;
}

void
detach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        t->clientnum -= 1;
        if (c->cf & CL_FLOAT) t->clientnum_floating -= 1;
        else t->clientnum_tiling -= 1;

        // If this was the last open client on the tag
        if (t->clientnum == 0) {
                t->clients = NULL;
                t->client_last = NULL;
                t->client_fullscreen = NULL;
                c->next = c->prev = NULL;
                return;
        }

        if (t->client_last == c) t->client_last = c->prev;

        if (c->next) c->next->prev = c->prev;

        if (c->prev) c->prev->next = c->next;
        else t->clients = c->next; // Detaching first client

        c->next = c->prev = NULL;
}

void
attach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        t->clientnum += 1;
        if (c->cf & CL_FLOAT) t->clientnum_floating += 1;
        else t->clientnum_tiling += 1;

        t->client_last = c;

        c->next = NULL;
        c->prev = NULL;

        // Only 1 client (This one)
        if (t->clientnum == 1) {
                t->clients = c;
                return;
        }

        Client *lastc;
        for (lastc = t->clients; lastc->next; lastc = lastc->next);
        lastc->next = c;
        c->prev = lastc;
}

void
updatetagmasteroffset(Monitor *m, int offset)
{
        if (!m) return;

        // Only allow masteroffset adjustment if at least 2 tiling clients are present
        Tag *t = m->tag;
        if (t->clientnum_tiling < 2) return;

        int halfwidth = m->width / 2;
        int updatedmasteroffset = offset ? (t->master_offset + offset) : 0;

        // Do not adjust if masteroffset already too small/big
        if ((halfwidth + updatedmasteroffset) < 100) return;
        else if ((halfwidth + updatedmasteroffset) > (m->width - 100)) return;

        t->master_offset = updatedmasteroffset;
}

void
arrange(void)
{
        for (Monitor *m = mons; m; m = m->next) arrangemon(m);
}

void
arrangemon(Monitor *m)
{
        // Only arrange current focused Tag of the monitor
        Tag *t = m->tag;
        if (!t->clientnum_tiling) return;

        if (t->clientnum_tiling == 1) t->master_offset = 0;

        XWindowChanges wc;
        // We have a fullscreen client on the tag
        if (t->client_fullscreen) {
                t->client_fullscreen->width = wc.width = m->width;
                t->client_fullscreen->height = wc.height = m->height;
                t->client_fullscreen->x = wc.x = m->x;
                t->client_fullscreen->y = wc.y = m->y;

                XConfigureWindow(
                        dpy, t->client_fullscreen->win,
                        CWX|CWY|CWWidth|CWHeight, &wc);
                XSync(dpy, 0);
                return;
        }

        // Get first tiling client
        Client *fc = t->clients;
        for (; fc && fc->cf & CL_FLOAT; fc = fc->next);
        if (!fc) return;

        int base_height = m->height - statusbar.height;
        int border_offset = borderwidth * 2;
        int master_area = (m->width / 2) + t->master_offset;

        fc->width = wc.width = (t->clientnum_tiling == 1 ? m->width : master_area) - border_offset;
        fc->height = wc.height = base_height - border_offset;
        fc->x = wc.x = m->x;
        fc->y = wc.y = m->y + statusbar.height;
        XConfigureWindow(dpy, fc->win, CWY|CWX|CWWidth|CWHeight, &wc);

        if (!fc->next || t->clientnum_tiling == 1) {
                XSync(dpy, 0);
                return;
        }

        // Get last tiling client
        Client *lc = t->client_last;
        for (; lc && lc->cf & CL_FLOAT; lc = lc->prev);

        // Draw rest of the clients to the right of the screen
        int right_clients_num = t->clientnum_tiling - 1;
        int right_height = base_height / right_clients_num;
        int prev_y = fc->y;
        for (Client *c = fc->next; c; c = c->next) {
                if (c->cf & CL_FLOAT) continue;
                c->width = wc.width = m->width - master_area - border_offset;
                c->height = wc.height = right_height - border_offset;
                if (c == lc)
                        c->height = wc.height = c->height +
                            base_height - (right_height * right_clients_num);
                c->x = wc.x = m->x + master_area;
                c->y = wc.y = prev_y;
                XConfigureWindow(dpy, c->win, CWY|CWX|CWWidth|CWHeight, &wc);

                prev_y += right_height;
        }

        XSync(dpy, 0);
}

void
grabkeys(void)
{
        unsigned int modifiers[] = {0, LockMask, Mod2Mask, LockMask|Mod2Mask};
        size_t keys_length = sizeof(keys)/sizeof(keys[0]);
        size_t modifiers_length = sizeof(modifiers)/sizeof(modifiers[0]);

        for (int i = 0; i < keys_length; ++i)
                for (int j = 0; j < modifiers_length; ++j)
                        XGrabKey(
                                dpy,
                                XKeysymToKeycode(dpy, keys[i].keysym),
                                keys[i].modmask | modifiers[j],
                                root,
                                True,
                                GrabModeAsync,
                                GrabModeAsync);
}

/*** Keybinding fuctions ***/

void
key_updatemasteroffset(Arg *arg)
{
        if (selmon->tag->client_fullscreen) return;
        updatetagmasteroffset(selmon, arg->i);
        arrangemon(selmon);
}

void
key_killclient(Arg *arg)
{
        if (!selc) return;

        XGrabServer(dpy);

        if (!sendevent(selc->win, icccm_atoms[ICCCM_DEL_WIN]))
                XKillClient(dpy, selc->win);

        XUngrabServer(dpy);
}

void
key_spawn(Arg *arg)
{
        if (fork()) return;

        if (dpy) close(ConnectionNumber(dpy));

        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        exit(0);
}

void
key_fullscreen(Arg* arg)
{
        if (!selc) return;

        Tag *t = selc->tag;
        if (!t->client_fullscreen) setfullscreen(selc);
        else if (t->client_fullscreen == selc) unsetfullscreen(selc);
        else return;

        remaptag(t);
        arrangemon(t->mon);
        drawbar(t->mon);
}

void
key_mvwintomon(Arg *arg)
{
        if (!selc || !mons->next) return;

        Client *c = selc;
        Tag *t = c->tag;
        if (t->client_fullscreen) return;

        // Target monitor to move window to
        Monitor *tm;
        if (arg->i == 1) tm = selmon->next ? selmon->next : mons;
        else if (arg->i == -1) tm = selmon->prev ? selmon->prev : lastmon;
        else return;

        if (tm == selmon) return;
        if (tm->tag->client_fullscreen) return;

        // Move client (win) to target monitor
        mvwintomon(c, tm, NULL);

        // Update bartags of target monitor
        tm->bartags[tm->tag->num * 2] = '*';
        drawbar(tm);

        setborders(tm->tag);

        arrangemon(tm);
        arrangemon(selmon);

        // Focus next client on current monitor/tag
        focusclient(t->clients_focus, true);
}

void
key_mvwintotag(Arg *arg)
{
        if (!selc) return;
        if (arg->ui < 1 || arg->ui > tags_num) return;
        if ((arg->ui - 1) == selmon->tag->num) return;

        Client *c = selc;
        Tag *t = c->tag;
        if (t->client_fullscreen) return;

        // Get tag to move the window to
        Tag *tm = selmon->tags + (arg->ui -1);

        // Move the client to tag (detach, attach)
        mvwintotag(c, tm);

        // Unmap moved client
        unmapclient(c);

        // Update bartags
        selmon->bartags[(arg->ui - 1) * 2] = '*';
        drawbar(selmon);

        // Arrange the monitor
        arrangemon(selmon);

        // Focus previous client
        focusclient(t->clients_focus, true);
}

void
key_followwintotag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;
        if (!selc) return;

        Tag *t = selc->tag;
        if (t->client_fullscreen) return;

        int totag = t->num + arg->i;
        if (totag < 0) totag = (int) (tags_num - 1);
        else if (totag == tags_num) totag = 0;

        Tag *tm = selmon->tags + totag;
        mvwintotag(selc, tm);
        focustag(tm);
}

void
key_mvwin(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;
        if (!selc) return;

        // Dont allow on fullscreen
        Tag *t = selc->tag;
        if (t->client_fullscreen) return;

        if (arg->i == 1) {
                // Move to right
                if (!selc->next) return;

                // Client to switch with
                Client *cs = selc->next;

                if (selc->prev) selc->prev->next = cs;
                if (cs->next) cs->next->prev = selc;

                selc->next = cs->next;
                cs->prev = selc->prev;
                selc->prev = cs;
                cs->next = selc;

                if (!selc->next) t->client_last = selc;
                if (selc == t->clients) t->clients = cs;
        } else {
                // Move to left
                if (!selc->prev) return;

                // Client to switch with
                Client *cs = selc->prev;

                if (selc->next) selc->next->prev = cs;
                if (cs->prev) cs->prev->next = selc;

                selc->prev = cs->prev;
                cs->next = selc->next;
                selc->next = cs;
                cs->prev = selc;

                if (!cs->next) t->client_last = cs;
                if (cs == t->clients) t->clients = selc;
        }

        arrangemon(selmon);
}

void
key_cycletag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        int totag = selmon->tag->num + arg->i;
        if (totag < 0) totag = (int) (tags_num - 1);
        else if (totag == tags_num) totag = 0;

        focustag(selmon->tags + totag);
}

void
key_cyclemon(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        // Focus monitor if available
        Monitor *m = NULL;
        if (arg->i == 1) m = selmon->next ? selmon->next : mons;
        else m = selmon->prev ? selmon->prev : lastmon;

        if (m == selmon) return;

        focusmon(m);
        focusclient(m->tag->clients_focus, true);
        XSync(dpy, 0);
}

void
key_cycleclient(Arg *arg)
{
        if (!selc) return;
        if (arg->i != 1 && arg->i != -1) return;

        // Dont allow on fullscreen
        Tag *t = selc->tag;
        if (t->client_fullscreen) return;
        if (t->clientnum < 2) return;

        // Client to focus
        Client *c = NULL;

        if (arg->i == 1) {
                // Focus to next element or to first in stack
                c = selc->next;
                if (!c) c = t->clients;
        } else {
                // Focus to previous element or last in the stack
                c = selc->prev;
                if (!c) c = t->client_last;
        }

        focusclient(c, true);
}

void
key_focustag(Arg *arg)
{
        if (arg->ui < 1 || arg->ui > tags_num) return;

        unsigned int tagtofocus = arg->ui - 1;
        if (tagtofocus == selmon->tag->num) return;

        focustag(selmon->tags + tagtofocus);
}


/*** Util functions ***/

void
grabbutton(Window w, unsigned int button)
{
        XGrabButton(
                dpy, button, AnyModifier, w, False, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);
}

void
grabbuttons(Window w)
{
        for (int i = 0; i < sizeof(buttons)/sizeof(buttons[0]); ++i)
                grabbutton(w, buttons[i]);
        XSync(dpy, 0);
}

void
ungrabbutton(Window w, unsigned int button)
{
        XUngrabButton(dpy, button, AnyModifier, w);
}

void
ungrabbuttons(Window w)
{
        for (int i = 0; i < sizeof(buttons)/sizeof(buttons[0]); ++i)
                ungrabbutton(w, buttons[i]);
        XSync(dpy, 0);
}

unsigned int
cleanmask(unsigned int mask)
{
        // Thanks to dwm
        return mask & (unsigned int) ~(Mod2Mask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask);
}

void
createcolor(unsigned long color, Color *dst_color)
{
        XRenderColor xrender_color = {
                .blue = (unsigned short) (color << 8) & 0xFF00,
                .green = (unsigned short) color & 0xFF00,
                .red = (unsigned short) ((color >> 8) & 0xFF00),
                .alpha = (unsigned short) ((color >> 16) & 0xFF00)
        };
         dst_color->cmap = XCreateColormap(
                                dpy,
                                root,
                                xvisual_info.visual,
                                AllocNone);
        if (!XftColorAllocValue(
                    dpy,
                    xvisual_info.visual,
                    dst_color->cmap,
                    &xrender_color,
                    &dst_color->xft_color))
                die("Could not create color: %lu\n", color);

        // Set alpha bits of the pixel. No idea why XftColorAllocValue()
        // does not do this automatically...
        dst_color->xft_color.pixel |=
            (unsigned long) (xrender_color.alpha >> 8) << 24;
}

bool
alreadymapped(Window w)
{
        // Checks if the window of MapRequest is already mapped (a client)
        // A specific application was observed to have that behavior
        for (Monitor *m = mons; m; m = m->next)
                for (int i = 0; i < tags_num; ++i)
                        for (Client *c = m->tags[i].clients; c; c = c->next)
                                if (c->win == w)
                                        return true;
        return false;
}

void
setborders(Tag *t)
{
        if (!t || !t->clients) return;
        if (t != t->mon->tag) return;

        // Do not set border when fullscreen client
        if (t->client_fullscreen) {
                setborder(t->client_fullscreen->win, 0, NULL);
                return;
        }

        // Set borders for selected tag
        for (Client *c = t->clients; c; c = c->next) {
                if (c->cf & CL_DIALOG) continue;
                if (c->mon == selmon && c == selc)
                        setborder(c->win, borderwidth, &colors.bordercolor);
                else if (c->cf & CL_URGENT)
                        setborder(c->win, borderwidth, &colors.bordercolor_urgent);
                else
                        setborder(c->win, borderwidth, &colors.bordercolor_inactive);
        }
}

void
setborder(Window w, int width, Color *color)
{
        XWindowChanges wc;
        wc.border_width = width;
        XConfigureWindow(dpy, w, CWBorderWidth, &wc);

        if (color && width) {
                XSetWindowBorder(dpy, w, color->xft_color.pixel);
        }
}

Atom
getwinprop(Window w, Atom a)
{
        Atom wintype;
        Atom actualtype;
        int actualformat;
        unsigned long nitems;
        unsigned long bytes;
        unsigned char *data = NULL;

        int ret = XGetWindowProperty(
                dpy,
                w,
                a,
                0,
                sizeof(wintype),
                False,
                XA_ATOM,
                &actualtype,
                &actualformat,
                &nitems,
                &bytes,
                &data);

        if (ret != Success || !data) return None;

        wintype = *(Atom *)data;
        XFree(data);

        return wintype;
}

bool
sendevent(Window w, Atom prot)
{
        bool protavail = false;
        int protcount = 0;
        Atom *avail;

        if (!XGetWMProtocols(dpy, w, &avail, &protcount)) return false;
        // Failsave
        if (!avail || !protcount) return false;

        // Check if the desired protocol/atom to send is available on the client
        for (int i = 0; i < protcount; ++i) {
                if (avail[i] == prot) {
                        protavail = true;
                        break;
                }
        }
        XFree(avail);

        if (!protavail) return false;

        XEvent ev;
        ev.type = ClientMessage;
        ev.xclient.window = w;
        ev.xclient.message_type = icccm_atoms[ICCCM_PROTOCOLS];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = (long) prot;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, w, False, NoEventMask, &ev);

        return true;
}

Client*
wintoclient(Window w)
{
        Monitor *m;
        Client *c;
        for (m = mons; m; m = m->next)
                for (int i = 0; i < tags_num; ++i)
                        for (c = m->tags[i].clients; c; c = c->next)
                                if (c->win == w)
                                        return c;

        return NULL;
}

int
wm_detected(Display *dpy, XErrorEvent *ee)
{
        die("Different WM already running\n");
        // Ignored
        return 0;
}

void
generatebartags(Monitor *m)
{
        // Create the bartags string which will be displayed in the statusbar
        m->bartagssize = (tags_num * 2) + 1;
        // Add monitor indicator ' | n'
        m->bartagssize += 4;

        m->bartags = (char*)ecalloc(m->bartagssize, 1);
        m->bartags[m->bartagssize - 1] = '\0';

        // Add spaces to all tags
        //  1 2 3 4 5 6 7 8 9
        for (int i = 0, j = 0; i < tags_num; ++i) {
                m->bartags[++j] = *tags[i];
                m->bartags[i*2] = ' ';
                j += 1;
        }

        // Show monitor number
        snprintf(m->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);

        // We start on first tag
        m->bartags[0] = '>';
}

void
populatemon(Monitor *m, XRRMonitorInfo *info)
{
        if (!m || !info) return;

        if (m->aname) XFree(m->aname);
        m->aname = XGetAtomName(dpy, info->name);
        m->snum = -1;
        m->x = info->x;
        m->y = info->y;
        m->width = info->width;
        m->height = info->height;
}

void
updatemons(void)
{

        int mn;
        XRRMonitorInfo *info = XRRGetMonitors(dpy, root, True, &mn);
        if (!info) die("Could not get monitors with Xrandr");

        int monitornum = 0;
        bool overlaying = false;
        // We assume we ALWAYS have one monitor
        Monitor *m = mons;
        selmon = m;
        lastmon = m;
        for (int n = 0; n < mn; ++n) {
                // ignore overlaying monitor
                overlaying = false;
                for (Monitor *pm = lastmon; n && pm; pm = pm->prev) {
                        // Check if monitor is overlaying with one of the previous ones
                        if (info[n].x == pm->x && info[n].y == pm->y)
                                overlaying = true;
                }
                if (overlaying) continue;

                if (!m) {
                        m = createmon(info + n);
                        lastmon->next = m;
                        m->prev = lastmon;
                } else {
                        populatemon(m, info + n);
                        updatetagmasteroffset(m, 0);
                }
                // Change bartags accordingly
                m->snum = monitornum;
                snprintf(m->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);
                m->bartags[m->tag->num * 2] = n ? '^' : '>';

                lastmon = m;
                m = m->next;
                ++monitornum;
        }
        lastmon->next = NULL;
        currentmonnum = monitornum;

        // Move any client of previous active monitors to the main one
        if (m) {
                // These monitors are not active anymore
                // Move any window to the first mon
                for (Monitor *nm = m; nm; m = nm) {
                        nm = m->next;
                        destroymon(m, mons);
                }

                // Update masteroffset of tag because
                // we could have more clients now
                updatetagmasteroffset(mons, 0);

                // Only map client of active tag within
                // the first monitor (selmon/mons)
                for (int i = 0; i < tags_num; ++i) remaptag(mons->tags + i);
                mons->bartags[mons->tag->num * 2] = '>';
        }

        XRRFreeMonitors(info);
        XSync(dpy, 0);
}

void
destroymon(Monitor *m, Monitor *tm)
{
        // Destroy monitor and move clients
        // to different monitor if wished
        if (tm && tm != m) {
                for (int i = 0; i < tags_num; ++i) {
                        Client *nc = NULL;
                        for (Client *c = m->tags[i].clients; c; c = nc) {
                                nc = c->next;
                                mvwintomon(c, tm, tm->tags + i);
                                tm->bartags[i * 2] = '*';
                        }
                }
        }

        m->prev = m->next = NULL;
        XFree(m->aname);
        free(m->tags);
        free(m->bartags);
        free(m);
}

Monitor*
createmon(XRRMonitorInfo *info)
{
        Monitor *m = (Monitor*)ecalloc(sizeof(Monitor), 1);

        populatemon(m, info);
        m->prev = NULL;
        m->next = NULL;

        // Init the tags
        unsigned long tags_bytes = (tags_num * sizeof(Tag));
        m->tags = (Tag*)ecalloc(tags_bytes, 1);
        for (int i = 0; i < tags_num; ++i) {
                m->tags[i].mon = m;
                m->tags[i].num = i;
        }
        m->tag = m->tags;

        // Generate the bartags string which is displayed inside the statusbar
        generatebartags(m);

        return m;
}

void
initmons(void)
{
        mons = NULL;
        lastmon = NULL;
        selmon = NULL;

        XRRMonitorInfo *info = XRRGetMonitors(dpy, root, True, &currentmonnum);
        if (!info) die("Could not get monitors with Xrandr");

        for (int n = 0; n < currentmonnum; ++n) {
                Monitor *m = createmon(info + n);
                m->snum = n;

                // Update monitor number in statusbar
                snprintf(m->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);

                if (!mons) {
                        mons = m;
                        selmon = m;
                        lastmon = m;
                } else {
                        m->bartags[0] = '^';
                        lastmon->next = m;
                        m->prev = lastmon;
                        lastmon = m;
                }
        }

        XRRFreeMonitors(info);
}

void
setup(void)
{
        root = DefaultRootWindow(dpy);

        // Check that no other WM is running
        XSetErrorHandler(wm_detected);
        XSelectInput(
                dpy,
                root,
                SubstructureRedirectMask|SubstructureNotifyMask|
                    StructureNotifyMask|KeyPressMask|
                    PropertyChangeMask|EnterWindowMask);
        XSync(dpy, 0);

        // Set the error handler
        XSetErrorHandler(onxerror);

        // Get screen attributes
        screen = DefaultScreen(dpy);

        // Setup atoms
        ATOM_UTF8 = XInternAtom(dpy, "UTF8_STRING", False);
        icccm_atoms[ICCCM_PROTOCOLS] = XInternAtom(dpy, "WM_PROTOCOLS", False);
        icccm_atoms[ICCCM_DEL_WIN] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        icccm_atoms[ICCCM_FOCUS] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

        // Setup extended NET atoms
        net_atoms[NET_SUPPORTED] = XInternAtom(dpy, "_NET_SUPPORTED", False);
        net_atoms[NET_SUPPORTING] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
        net_atoms[NET_WM_NAME] = XInternAtom(dpy, "_NET_WM_NAME", False);
        net_atoms[NET_STATE] = XInternAtom(dpy, "_NET_WM_STATE", False);
        net_atoms[NET_TYPE] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        net_atoms[NET_ACTIVE] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
        net_atoms[NET_CLOSE] = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
        net_atoms[NET_FULLSCREEN] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

        // Set atoms for window types
        net_win_types[NET_DESKTOP] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        net_win_types[NET_DOCK] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        net_win_types[NET_TOOLBAR] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
        net_win_types[NET_MENU] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
        net_win_types[NET_UTIL] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
        net_win_types[NET_SPLAH] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLAH", False);
        net_win_types[NET_DIALOG] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        net_win_types[NET_NORMAL] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

        // Set supported net atoms
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_SUPPORTED],
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char*) net_atoms,
                NET_END);

        // Create global visual
        if (!XMatchVisualInfo(
                        dpy,
                        screen,
                        32,
                        TrueColor,
                        &xvisual_info)) {
                die("Could not create visual");
        }

        // Setup statusbar font
        xfont = XftFontOpenName(dpy, screen, barfont);
        if (!xfont) die("Cannot load font: %s\n", barfont);

        // Setup bar colors
        createcolor(barbg, &colors.barbg);
        createcolor(barfg, &colors.barfg);

        // Setup border colors
        createcolor(bordercolor, &colors.bordercolor);
        createcolor(bordercolor_inactive, &colors.bordercolor_inactive);
        createcolor(bordercolor_urgent, &colors.bordercolor_urgent);

        // Setup monitors and Tags
        initmons();

        // Create statusbar window
        sw = DisplayWidth(dpy, screen);

        statusbar.width = sw;
        statusbar.height = barheight;

        XSetWindowAttributes wa;
        wa.background_pixel = colors.barbg.xft_color.pixel;
        wa.border_pixel = 0;

        statusbar.cmap = XCreateColormap(
                                dpy,
                                root,
                                xvisual_info.visual,
                                AllocNone);
        wa.colormap = statusbar.cmap;

        statusbar.win = XCreateWindow(
                        dpy,
                        root,
                        0,
                        0,
                        (unsigned int) statusbar.width,
                        (unsigned int) barheight,
                        0,
                        xvisual_info.depth,
                        InputOutput,
                        xvisual_info.visual,
                        CWBackPixel|CWBorderPixel|CWColormap,
                        &wa);

        statusbar.xdraw = XftDrawCreate(
                            dpy,
                            statusbar.win,
                            xvisual_info.visual,
                            statusbar.cmap);
        XMapRaised(dpy, statusbar.win);

        // Create statusbars
        barstatus[0] = '\0';
        updatebars();

        // Set supporting net atoms to the statusbar
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_SUPPORTING],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &statusbar.win,
                1);
        XChangeProperty(
                dpy,
                statusbar.win,
                net_atoms[NET_SUPPORTING],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &statusbar.win,
                1);
        XChangeProperty(
                dpy,
                statusbar.win,
                net_atoms[NET_WM_NAME],
                ATOM_UTF8,
                8,
                PropModeReplace,
                (unsigned char*) "kisswm",
                6);


        arrange();
        grabkeys();
        focusmon(selmon);
        focusclient(NULL, true);

        XSync(dpy, 0);
}

void
run(void)
{
        XEvent e;
        while (!XNextEvent(dpy, &e))
                if (handler[e.type]) handler[e.type](&e);
}


int
main(int argc, char *argv[])
{
        if (!(dpy = XOpenDisplay(NULL)))
                die("Can not open Display\n");

        signal(SIGCHLD, SIG_IGN);

        setup();
        run();

        XCloseDisplay(dpy);
        return 0;
}
