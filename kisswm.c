#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#include <bsd/string.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

typedef struct Monitor Monitor;
typedef struct Client Client;
typedef struct Tag Tag;
typedef struct Colors Colors;
typedef struct Statusbar Statusbar;

struct Statusbar {
        Window win;
        XftDraw *xdraw;
        Visual *visual;
        int depth;
        int width;
        int height;
};

struct Colors {
        XftColor xbarbg;
        XftColor xbarfg;
        XftColor xalpha;
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
        // Overall clients
        int clientnum;
        // Tiling clients
        int tclientnum;
        // Floating clients
        int fclientnum;
        int masteroffset;
        Monitor *mon;
        Client *clients;
        Client *focusclients;
        // Fullscreen client
        Client *fsclient;
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

void    run(void);
int     onxerror(Display*, XErrorEvent*);
int     wm_detected(Display*, XErrorEvent*);
void    grabbutton(Client *c);
void    ungrabbutton(Client *c);
void    createcolor(const char*, XftColor*);
void    setborder(Window, int, unsigned long);
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
void    unsetfullscreen(Tag*);
void    closeclient(Window);
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
        [ButtonPress] = buttonpress
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

XftFont *xfont;
XGlyphInfo xglyph;
Colors colors;
int screen;
int sw;
int currentmonnum;
long long monupdatetime = 0;

unsigned int tags_num = sizeof(tags)/sizeof(tags[0]);

char barstatus[256];


/*** X11 Eventhandling ****/

void
buttonpress(XEvent *e)
{
        XButtonPressedEvent *ev = &e->xbutton;
        if (selc && ev->window == selc->win) return;

        Client *c = wintoclient(ev->window);
        if (!c) return;

        if (selmon != c->mon) focusmon(c->mon);
        focusclient(c, false);
}

void
mappingnotify(XEvent *e)
{
        XMappingEvent *ev = &e->xmapping;
        if (ev->request != MappingModifier && ev->request != MappingKeyboard) return;

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

                            Client *fc = c->tag->fsclient;
                            if (!fc && ev->data.l[0] == 1) {
                                    // Enable fullscreen for request window
                                    setfullscreen(c);
                            } else if (fc == c && ev->data.l[0] == 0) {
                                    // Disable fullscreen for request window
                                    unsetfullscreen(c->tag);
                            } else {
                                    return;
                            }
                            remaptag(c->tag);
                            arrangemon(c->tag->mon);
                            setborders(c->tag);
                            drawbar(c->tag->mon);
                }
        }

}

void
destroynotify(XEvent *e)
{
        XDestroyWindowEvent *ev = &e->xdestroywindow;
        if (ev->window) closeclient(ev->window);
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
        arrangemon(c->mon);
        remaptag(c->tag);

        // Focus new client
        focusclient(c, true);
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

        focusclient(selmon->tag->focusclients, true);

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
        if (wintype != net_win_types[NET_UTIL] && wintype != net_win_types[NET_DIALOG])
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
        fprintf(stderr, "#####[ERROR]: An Error occurred#####\n");
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
        if (t->fsclient) {
                XSync(dpy, 0);
                return;
        }

        if (*barbg != '\0')
                XftDrawRect(
                        statusbar.xdraw,
                        &colors.xbarbg,
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
                &colors.xbarfg,
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
                &colors.xbarfg,
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
        if (pm->tag->clientnum) pm->bartags[pm->tag->num * 2] = '*';
        else pm->bartags[pm->tag->num * 2] = ' ';
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
        focusclient(t->focusclients, true);

        // Update statusbar (Due to bartags change)
        drawbar(m);
}

void
setfullscreen(Client *c)
{
        // Already in fullscreen
        if (!c || c->tag->fsclient || c->cf & CL_DIALOG) return;

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
        c->tag->fsclient = c;
}

void
unsetfullscreen(Tag *t)
{
        if (!t->fsclient) return;

        // Set fullscreen property
        XDeleteProperty(
                dpy,
                t->fsclient->win,
                net_atoms[NET_STATE]);
        t->fsclient = NULL;
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
        } else if (t->fsclient) {
                // only map focused client and dialogs on fullscreen
                for (Client *c = t->clients; c; c = c->next) {
                        if (c == t->fsclient || c->cf & CL_DIALOG) mapclient(c);
                        else unmapclient(c);
                }
        } else {
                for (Client *c = t->clients; c; c = c->next) mapclient(c);
        }
        XSync(dpy, 0);
}

void
closeclient(Window w)
{
        Client *c = NULL;
        if (!(c = wintoclient(w))) return;

        Monitor *m = c->mon;
        Tag *t = c->tag;

        // Reset fullscreen client on current tag when a window closes
        if (!(c->cf & CL_DIALOG)) unsetfullscreen(c->tag);

        detach(c);
        focusdetach(c);

        // Clear statusbar if last client and not focused
        if (!t->clientnum && t != m->tag) m->bartags[t->num * 2] = ' ';

        free(c);

        // if the tag where client closed is active (seen)
        if (t == m->tag) {
                remaptag(t);
                arrangemon(m);
                focusclient(t->focusclients, true);
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
                XWarpPointer(dpy, 0, w, 0, 0, 0, 0, selmon->x + selmon->width / 2, selmon->y + selmon->height / 2);
        else if (c && c->win == w)
                XWarpPointer(dpy, 0, w, 0, 0, 0, 0,  c->width / 2, c->height / 2);

        XSync(dpy, 0);
}

void
focusclient(Client *c, bool warp)
{
        // Try to focus client on the active tag
        // of the currently focused monitor
        if (!c) {
                c = selmon->tag->focusclients;
                if (c && c != selc) {
                        focusclient(c, warp);
                } else if (!c) {
                        if (selc) grabbutton(selc);
                        selc = NULL;
                        focus(0, NULL, warp);
                }
                return;
        }

        if (c != selc) grabbutton(selc);
        ungrabbutton(c);

        Tag *t = c->tag;
        if (c != t->focusclients) {
                if (c->prevfocus)
                        c->prevfocus->nextfocus = c->nextfocus;
                if (c->nextfocus)
                        c->nextfocus->prevfocus = c->prevfocus;

                c->prevfocus = t->focusclients;
                if (c->prevfocus)
                        c->prevfocus->nextfocus = c;
        }
        c->nextfocus = NULL;
        t->focusclients = c;
        c->cf &= ~CL_URGENT;

        selc = c;

        setborders(c->tag);

        focus(0, c, warp);
        XSync(dpy, 0);
}

void
focusattach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        // Reset fullscreen if not dialog
        if (!(c->cf & CL_DIALOG)) unsetfullscreen(c->tag);

        c->nextfocus = NULL;
        c->prevfocus = t->focusclients;

        if (c->prevfocus) c->prevfocus->nextfocus = c;

        t->focusclients = c;
}

void
focusdetach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        if (c == t->focusclients)
                t->focusclients = c->prevfocus;

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
        if (c->cf & CL_FLOAT) t->fclientnum -= 1;
        else t->tclientnum -= 1;

        // If this was the last open client on the tag
        if (!t->clientnum) {
                t->clients = NULL;
                c->next = c->prev = NULL;
                return;
        }

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
        if (c->cf & CL_FLOAT) t->fclientnum += 1;
        else t->tclientnum += 1;

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
        if (t->tclientnum < 2) return;

        int halfwidth = m->width / 2;
        int updatedmasteroffset = offset ? (t->masteroffset + offset) : 0;

        // Do not adjust if masteroffset already too small/big
        if ((halfwidth + updatedmasteroffset) < 100) return;
        else if ((halfwidth + updatedmasteroffset) > (m->width - 100)) return;

        t->masteroffset = updatedmasteroffset;
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
        if (!t->tclientnum) return;

        if (t->tclientnum == 1) t->masteroffset = 0;

        XWindowChanges wc;
        // We have a fullscreen client on the tag
        if (t->fsclient) {
                t->fsclient->width = wc.width = m->width;
                t->fsclient->height = wc.height = m->height;
                t->fsclient->x = wc.x = m->x;
                t->fsclient->y = wc.y = m->y;

                XConfigureWindow(dpy, t->fsclient->win, CWX|CWY|CWWidth|CWHeight, &wc);
                XSync(dpy, 0);
                return;
        }

        int borderoffset = borderwidth * 2;
        int masterarea = (m->width / 2) + t->masteroffset;

        // Get first client which is NOT a floating client
        Client *fc = t->clients;
        for (; fc && fc->cf & CL_FLOAT; fc = fc->next);
        if (!fc) return;

        fc->width = wc.width = ((t->tclientnum == 1) ? m->width : masterarea) - borderoffset;
        fc->height = wc.height = m->height - statusbar.height - borderwidth - borderoffset;
        fc->x = wc.x = m->x;
        fc->y = wc.y = m->y + statusbar.height + borderwidth;
        XConfigureWindow(dpy, fc->win, CWY|CWX|CWWidth|CWHeight, &wc);

        if (!fc->next || t->tclientnum == 1) {
                XSync(dpy, 0);
                return;
        }

        // Draw rest of the clients to the right of the screen
        int rightheight = (m->height - statusbar.height - borderwidth) / (t->tclientnum - 1);
        int prevheight = fc->y;
        for (Client *c = fc->next; c; c = c->next) {
                if (c->cf & CL_FLOAT) continue;
                c->width = wc.width = m->width - masterarea - borderoffset;
                c->height = wc.height = rightheight - borderoffset;
                c->x = wc.x = masterarea + m->x;
                c->y = wc.y = prevheight;
                XConfigureWindow(dpy, c->win, CWY|CWX|CWWidth|CWHeight, &wc);

                prevheight += rightheight;
        }

        XSync(dpy, 0);
}

void
grabkeys(void)
{
        unsigned int modifiers[] = {0, LockMask, Mod2Mask, LockMask|Mod2Mask};
        for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i)
                for (int j = 0; j < sizeof(modifiers)/sizeof(modifiers[0]); ++j)
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
        updatetagmasteroffset(selmon, arg->i);
        arrangemon(selmon);
}

void
key_killclient(Arg *arg)
{
        if (!selc || selc->tag->fsclient == selc) return;

        XGrabServer(dpy);

        if (!sendevent(selc->win, icccm_atoms[ICCCM_DEL_WIN]))
                XKillClient(dpy, selc->win);

        XUngrabServer(dpy);
}

void
key_spawn(Arg *arg)
{
        // Dont allow on fullscreen
        Tag *t = selmon->tag;
        if (t->fsclient) return;

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
        if (!t->fsclient) setfullscreen(selc);
        else if (t->fsclient == selc) unsetfullscreen(t);
        else return;

        remaptag(t);
        arrangemon(t->mon);
        focusclient(t->focusclients, true);
        drawbar(t->mon);
}

void
key_mvwintomon(Arg *arg)
{
        if (!selc || !mons->next) return;

        Tag *t = selc->tag;
        if (t->fsclient) return;

        // Target monitor to move window to
        Monitor *tm;
        if (arg->i == 1) tm = selmon->next ? selmon->next : mons;
        else if (arg->i == -1) tm = selmon->prev ? selmon->prev : lastmon;
        else return;

        if (tm == selmon) return;
        if (tm->tag->fsclient) return;

        // Move client (win) to target monitor
        mvwintomon(selc, tm, NULL);

        // Update bartags of target monitor
        tm->bartags[tm->tag->num * 2] = '*';
        drawbar(tm);

        setborders(tm->tag);

        arrangemon(tm);
        arrangemon(selmon);

        // Focus next client on current monitor/tag
        focusclient(t->focusclients, true);
}

void
key_mvwintotag(Arg *arg)
{
        if (!selc) return;
        if (arg->ui < 1 || arg->ui > tags_num) return;
        if ((arg->ui - 1) == selmon->tag->num) return;

        // Dont allow on fullscreen
        Tag *t = selc->tag;
        if (t->fsclient) return;

        // Get tag to move the window to
        Tag *tm = selmon->tags + (arg->ui -1);

        // Move the client to tag (detach, attach)
        mvwintotag(selc, tm);

        //Unmap moved client
        unmapclient(selc);

        // Update bartags
        selmon->bartags[(arg->ui - 1) * 2] = '*';
        drawbar(selmon);

        // Arrange the monitor
        arrangemon(selmon);

        // Focus previous client
        focusclient(t->focusclients, true);
}

void
key_followwintotag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;
        if (!selc) return;

        Tag *t = selc->tag;
        if (t->fsclient) return;

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
        if (t->fsclient) return;

        // Client to move
        Client *ctm = selc;

        if (arg->i == 1) {
                // Move to right
                if (!ctm->next) return;

                // Client to switch
                Client *cts = ctm->next;

                if (ctm->prev) ctm->prev->next = cts;
                if (cts->next) cts->next->prev = ctm;

                ctm->next = cts->next;
                cts->prev = ctm->prev;
                ctm->prev = cts;
                cts->next = ctm;

                if (ctm == t->clients) t->clients = cts;
        } else {
                // Move to left
                if (!ctm->prev) return;

                // Client to switch
                Client *cts = ctm->prev;

                if (ctm->next) ctm->next->prev = cts;
                if (cts->prev) cts->prev->next = ctm;

                ctm->prev = cts->prev;
                cts->next = ctm->next;
                ctm->next = cts;
                cts->prev = ctm;

                if (cts == t->clients) t->clients = ctm;
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
        focusclient(m->tag->focusclients, true);
        XSync(dpy, 0);
}

void
key_cycleclient(Arg *arg)
{
        if (!selc) return;
        if (arg->i != 1 && arg->i != -1) return;

        // Dont allow on fullscreen
        Tag *t = selc->tag;
        if (t->fsclient) return;
        if (t->clientnum < 2) return;

        Client *tofocus = NULL;

        if (arg->i == 1) {
                // Focus to next element or to first in stack
                tofocus = selc->next;
                if (!tofocus) tofocus = t->clients;
        } else {
                // Focus to previous element or last in the stack
                tofocus = selc->prev;
                if (!tofocus)
                        for (tofocus = selc; tofocus->next; tofocus = tofocus->next);
        }

        focusclient(tofocus, true);
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
grabbutton(Client *c)
{
    if (!c) return;
    XGrabButton(dpy, Button1, AnyModifier, c->win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

}

void
ungrabbutton(Client *c)
{
    if (!c) return;
    XUngrabButton(dpy, Button1, AnyModifier, c->win);
}

unsigned int
cleanmask(unsigned int mask)
{
        // Thanks to dwm
        return mask & (unsigned int) ~(Mod2Mask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask);
}

void
createcolor(const char *color, XftColor *dst)
{
        if (*color == '\0') return;

        if (!XftColorAllocName(
                    dpy,
                    DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen),
                    color,
                    dst))
                die("Could not load color: %s\n", color);
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
        if (t->fsclient) {
                setborder(t->fsclient->win, 0, 0);
                return;
        }

        // Set borders for selected tag
        for (Client *c = t->clients; c; c = c->next) {
                if (c->cf & CL_DIALOG) continue;
                if (c == selc)
                        setborder(c->win, borderwidth, bordercolor);
                else if (c->cf & CL_URGENT)
                        setborder(c->win, borderwidth, bordercolor_urgent);
                else
                        setborder(c->win, borderwidth, bordercolor_inactive);
        }
}

void
setborder(Window w, int width, unsigned long color)
{
        XWindowChanges wc;
        wc.border_width = width;
        XConfigureWindow(dpy, w, CWBorderWidth, &wc);

        if (color) XSetWindowBorder(dpy, w, color);
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

        // Sometimes XGetWMProtocols generate an X11 error.
        // Happens when a client closes and do not know why yet
        // Returns 0 on error
        if (XGetWMProtocols(dpy, w, &avail, &protcount) == 0) return false;
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

        // We assume we ALWAYS have one monitor
        Monitor *m = mons;
        selmon = m;
        lastmon = m;
        for (int n = 0; n < mn; ++n) {
                if (!m) {
                        m = createmon(info + n);
                        lastmon->next = m;
                        m->prev = lastmon;
                } else {
                        populatemon(m, info + n);
                        updatetagmasteroffset(m, 0);
                }
                // Change bartags accordingly
                m->snum = n;
                snprintf(m->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);
                m->bartags[m->tag->num * 2] = '>';

                lastmon = m;
                m = m->next;
        }
        lastmon->next = NULL;
        currentmonnum = mn;

        // Move any client of previous active monitors to the main one
        if (m) {
                // These monitors are not active anymore
                // Move any window to the first mon
                for (Monitor *nm = m; nm; m = nm) {
                        nm = m->next;
                        destroymon(m, mons);
                }

                // Update masteroffset of tag because we could have more clients now
                updatetagmasteroffset(mons, 0);

                // Only map client of active tag within the first monitor (selmon/mons)
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
                SubstructureRedirectMask|SubstructureNotifyMask|StructureNotifyMask|KeyPressMask|PropertyChangeMask);
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

        // Setup statusbar font
        xfont = XftFontOpenName(dpy, screen, barfont);
        if (!xfont) die("Cannot load font: %s\n", barfont);

        // Set up colors
        XRenderColor alpha = {0x0000, 0x0000, 0x0000, 0xffff};
        if (!XftColorAllocValue(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &alpha, &colors.xalpha))
                die("Could not load color: alpha\n");
        createcolor(barbg, &colors.xbarbg);
        createcolor(barfg, &colors.xbarfg);


        // Setup monitors and Tags
        initmons();

        // Create statusbar window
        sw = DisplayWidth(dpy, screen);

        statusbar.width = sw;
        statusbar.height = barheight;

        XSetWindowAttributes wa;
        wa.background_pixel = 0;
        wa.border_pixel = 0;

        unsigned long valuemask = CWBackPixel | CWBorderPixel;
        statusbar.depth = DefaultDepth(dpy, screen);
        statusbar.visual = DefaultVisual(dpy, screen);

        XVisualInfo vinfo;
        if (XMatchVisualInfo(
                dpy,
                screen,
                32,
                TrueColor,
                &vinfo)) {
            statusbar.depth = vinfo.depth;
            statusbar.visual = vinfo.visual;
            wa.colormap = XCreateColormap(
                            dpy,
                            root,
                            statusbar.visual,
                            AllocNone);
            valuemask = valuemask | CWColormap;
        }

        statusbar.win = XCreateWindow(
                        dpy,
                        root,
                        0,
                        0,
                        (unsigned int) statusbar.width,
                        (unsigned int) barheight,
                        0,
                        statusbar.depth,
                        InputOutput,
                        statusbar.visual,
                        valuemask,
                        &wa);
        statusbar.xdraw = XftDrawCreate(
                            dpy,
                            statusbar.win,
                            statusbar.visual,
                            DefaultColormap(dpy, screen));
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
