#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#include <bsd/string.h>
#endif
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
#include <X11/extensions/Xinerama.h>

#include "util.h"

#ifdef DODEBUG
        #define DEBUG(...) fprintf(stderr, "DEBUG: "__VA_ARGS__"\n")
#else
        #define DEBUG(...) do {} while (0)
#endif

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
        Monitor *m;
        Tag *tag;
        Client *next, *prev;
        Client *nextfocus, *prevfocus;
        int x;
        int y;
        int width;
        int height;
        int bw;
};

struct Tag {
        int clientnum;
        int masteroffset;
        Monitor *mon;
        Client *clients;
        Client *focusclients;
        Client *urgentclient;
        // Fullscreen client
        Client *fsclient;
};

struct Monitor {
        Monitor *next;
        Monitor *prev;
        unsigned long bartagssize;
        char *bartags;
        unsigned int tag;
        Tag *tags;
        int snum;
        int x;
        int y;
        int height;
        int width;
        bool inactive;
};

unsigned int cleanmask(unsigned int);
int gettagnum(Tag*);
void updatemonmasteroffset(Monitor*, int);
void focusmon(Monitor*);
Monitor* createmon(XineramaScreenInfo*);
void resizemons(XineramaScreenInfo*, int);
void createcolor(const char*, XftColor*);
bool alreadymapped(Window);
void setborder(Window, int, unsigned long);
void setborders(Tag*);
void mapclient(Client*);
void drawdialog(Window, XWindowAttributes*);
void unmapclient(Client*);
void maptag(Tag*);
void unmaptag(Tag*);
void spawn(Arg*);
void mvwintotag(Arg*);
void mvwintomon(Arg*);
void followwintotag(Arg*);
void mvwin(Arg*);
void fullscreen(Arg*);
void focustag(Arg*);
void cycletag(Arg*);
void cycleclient(Arg*);
void cyclemon(Arg*);
void killclient(Arg*);
void closeclient(Window);
void setup(void);
Atom getwinprop(Window, Atom);
Client* wintoclient(Window);
Bool sendevent(Window, Atom*);
Tag* currenttag(Monitor*);
void freemons(void);
void initmons(void);
void generatebartags(Monitor*);
void _mvwintotag(Client*, Tag*);
void togglefullscreen(Client*);
void attach(Client*);
void detach(Client*);
void focusattach(Client*);
void focusdetach(Client*);
void focusclient(Client*);
void focus(Window, Client*);
void arrange(void);
void arrangemon(Monitor*);
void updatemasteroffset(Arg*);
void run(void);
void grabkeys(void);
int wm_detected(Display*, XErrorEvent*);
int onxerror(Display*, XErrorEvent*);
void keypress(XEvent*);
void configurenotify(XEvent*);
void propertynotify(XEvent*);
void configurerequest(XEvent*);
void maprequest(XEvent*);
void destroynotify(XEvent*);
void clientmessage(XEvent*);
void mappingnotify(XEvent*);

void updatebars(void);
void updatestatustext(void);
void drawbar(Monitor*);

void (*handler[LASTEvent])(XEvent*) = {
        [KeyPress] = keypress,
        [ConfigureNotify] = configurenotify,
        [ConfigureRequest] = configurerequest,
        [PropertyNotify] = propertynotify,
        [MapRequest] = maprequest,
        [MappingNotify] = mappingnotify,
        [DestroyNotify] = destroynotify,
        [ClientMessage] = clientmessage
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
int currmonitornum;

unsigned int tags_num = sizeof(tags)/sizeof(tags[0]);

char barstatus[256];


/*** X11 Eventhandling ****/

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

        if (ev->message_type == net_atoms[NET_ACTIVE] && c->tag != selc->tag) {
                c->m->bartags[gettagnum(c->tag) * 2] = '!';
                c->tag->urgentclient = c;
                setborders(c->tag);
        } else if (ev->message_type == net_atoms[NET_STATE]) {
                if (ev->data.l[1] == net_atoms[NET_FULLSCREEN] ||
                    ev->data.l[2] == net_atoms[NET_FULLSCREEN]) {
                            if (c->tag->fsclient == c && ev->data.l[0] == 0)
                                    togglefullscreen(c);
                            else if (!c->tag->fsclient && ev->data.l[0] == 1)
                                    togglefullscreen(c);
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

        // Simply render a dialog window
        Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
        if (net_win_types[NET_UTIL] == wintype ||
            net_win_types[NET_DIALOG] == wintype) {
                drawdialog(ev->window, &wa);
                return;
        }

        // We assume the maprequest is on the current (selected) monitor
        // Get current tag
        Tag *ct = currenttag(selmon);

        Client *c = malloc(sizeof(Client));
        c->next = c->prev = c->nextfocus = c->prevfocus = NULL;
        c->win = ev->window;
        c->m = selmon;
        c->tag = ct;

        attach(c);
        focusattach(c);

        // Already arrange monitor before new window is mapped
        // This will reduce flicker of client
        arrangemon(c->m);

        mapclient(c);

        // Focus new client
        focusclient(c);
}

void
configurenotify(XEvent *e)
{
        XConfigureEvent *ev = &e->xconfigure;
        if (ev->window != root) return;

        int monitornum;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &monitornum);
        resizemons(info, monitornum);

        // Calculate combined monitor width
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

        XFree(info);
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
        DEBUG("#####[ERROR]: An Error occurred#####");
        fflush(NULL);
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
        Tag *t = currenttag(m);
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
                        strcpy(barstatus, "Kisswm V1.0.0");
                        return;
                }
        }

        if (pwmname.encoding == XA_STRING || pwmname.encoding == ATOM_UTF8)
                strlcpy(barstatus, (char*)pwmname.value, sizeof(barstatus));

        XFree(pwmname.value);
}

/*** WM state changing functions ***/

void
togglefullscreen(Client *cc)
{
        if (!cc) return;

        Tag *t = cc->tag;

        if (t->fsclient) {
                XDeleteProperty(
                        dpy,
                        t->fsclient->win,
                        net_atoms[NET_STATE]);
        }

        // set fullscreen client
        t->fsclient = (t->fsclient) ? NULL : cc;

        focusclient(cc);
        if (t->fsclient) {
                XChangeProperty(
                        dpy,
                        selc->win,
                        net_atoms[NET_STATE],
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) &net_atoms[NET_FULLSCREEN],
                        1);
        }

        drawbar(cc->m);
        for (Monitor *m = mons; m; m = m->next) setborders(currenttag(m));
        arrangemon(cc->m);

        // Unmap or map client depending if fullscreen
        for (Client *c = t->clients; c; c = c->next) {
                if (c == cc) continue;

                if (t->fsclient) unmapclient(c);
                else mapclient(c);
        }
}

void
_mvwintotag(Client *c, Tag *t)
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
unmaptag(Tag *t)
{
        for (Client *c = t->clients; c; c = c->next) unmapclient(c);
}

void
maptag(Tag *t)
{
        if (t->fsclient) {
                mapclient(t->fsclient);
                return;
        }

        // Map every window if no fullscreen client is present
        for (Client *c = t->clients; c; c = c->next) mapclient(c);
}

void
closeclient(Window w)
{
        Client *c = (selc && selc->win == w) ? selc : NULL;
        if (!c && !(c = wintoclient(w))) return;

        // Reset fsclient
        if (c->tag->fsclient == c) togglefullscreen(c);

        Monitor *m = c->m;

        detach(c);

        // Detach client from focus
        if (c == selc) selc = c->prevfocus;

        focusdetach(c);

        // Clear statusbar if last client and not focused
        if (c->tag->clientnum == 0 && currenttag(c->m) != c->tag)
                c->m->bartags[gettagnum(c->tag) * 2] = ' ';


        free(c);
        arrangemon(m);
        focusclient(selc);
        drawbar(m);
}

void
focus(Window w, Client *c)
{
        if (!w && !c) {
                if (selc) w = selc->win;
                else w = root;
        } else if (!w && c) {
                w = c->win;
        }

        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
        sendevent(w, &icccm_atoms[ICCCM_FOCUS]);
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_ACTIVE],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &w,
                1);

        if (w == root)
                XWarpPointer(dpy, 0, w, 0, 0, 0, 0, selmon->x + selmon->width / 2, selmon->y + selmon->height / 2);
        else if (c && c->win == w)
                XWarpPointer(dpy, 0, w, 0, 0, 0, 0,  c->width / 2, c->height / 2);

        XSync(dpy, 0);
}

void
focusclient(Client *c)
{
        if (!c) {
                selc = NULL;
                focus(0, NULL);
                return;
        }

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

        selc = c;
        selmon = c->m;

        if (c == t->urgentclient) t->urgentclient = NULL;

        setborders(c->tag);

        focus(0, selc);
}

void
focusattach(Client *c)
{
        Tag *t = c->tag;
        if (!t) return;

        // Reset fullscreen if new client attaches
        t->fsclient = NULL;

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
updatemonmasteroffset(Monitor *m, int offset)
{
        if (!m) return;

        // Only allow masteroffset adjustment if at least 2 clients are present
        Tag *t = currenttag(m);
        if (t->clientnum < 2) return;

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
        Tag *t = currenttag(m);
        if (!t->clientnum) return;

        if (t->clientnum == 1) t->masteroffset = 0;

        int borderoffset = borderwidth * 2;
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

        int masterarea = (m->width / 2) + t->masteroffset;

        // First client gets full or half monitor if multiple clients
        Client *fc = t->clients;
        fc->width = wc.width = ((t->clientnum == 1) ? m->width : masterarea) - borderoffset;
        fc->height = wc.height = m->height - statusbar.height - borderwidth - borderoffset;
        fc->x = wc.x = m->x;
        fc->y = wc.y = m->y + statusbar.height + borderwidth;
        XConfigureWindow(dpy, fc->win, CWY|CWX|CWWidth|CWHeight, &wc);

        if (!fc->next) {
                XSync(dpy, 0);
                return;
        }

        // Draw rest of the clients to the right of the screen
        int rightheight = (m->height - statusbar.height - borderwidth) / (t->clientnum - 1);
        for (Client *c = fc->next; c; c = c->next) {
                c->width = wc.width = m->width - masterarea - borderoffset;
                c->height = wc.height = rightheight - borderoffset;
                c->x = wc.x = masterarea + m->x;
                c->y = wc.y = c->prev->y + (c->prev == fc ? 0 : rightheight);
                XConfigureWindow(dpy, c->win, CWY|CWX|CWWidth|CWHeight, &wc);
        }

        XSync(dpy, 0);
}

void
drawdialog(Window w, XWindowAttributes *wa)
{
        XMapWindow(dpy, w);
        setborder(w, borderwidth, bordercolor);
        focus(w, NULL);
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
updatemasteroffset(Arg *arg)
{
        updatemonmasteroffset(selmon, arg->i);
        arrangemon(selmon);
}

void
killclient(Arg *arg)
{
        // TODO: Either get focused window (netatom) if selc is not set
        // or create special type and keep track of dialog windows
        if (!selc) return;

        XGrabServer(dpy);

        if (!sendevent(selc->win, &icccm_atoms[ICCCM_DEL_WIN]))
                XKillClient(dpy, selc->win);

        XUngrabServer(dpy);
}

void
spawn(Arg *arg)
{
        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient) return;

        if (fork()) return;

        if (dpy) close(ConnectionNumber(dpy));

        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        exit(0);
}

void
fullscreen(Arg* arg)
{
        if (!selc) return;
        togglefullscreen(selc);
}

void
mvwintomon(Arg *arg)
{
        if (!selc) return;
        if (!mons->next) return;

        // Target monitor to move window to
        Monitor *tm;
        if (arg->i == 1 && selmon->next) tm = selmon->next;
        else if (arg->i == -1 && selmon->prev) tm = selmon->prev;
        else return;

        // Do not do anything if in inactive state
        if (tm->inactive) return;

        // Target client to move to target monitor
        Client *tc = selc;
        // Current tag of client to move
        Tag *ct = tc->tag;
        // Current tag of the target monitor
        Tag *tt = currenttag(tm);

        // Do not allow moving when in fullscreen
        if (ct->fsclient || tt->fsclient) return;

        detach(tc);
        focusdetach(tc);

        tc->m = tm;
        tc->tag = tt;

        attach(tc);
        focusattach(tc);

        setborders(currenttag(tm));

        arrangemon(tm);
        arrangemon(selmon);

        focusclient(ct->focusclients);
}

void
mvwintotag(Arg *arg)
{
        if (!selc) return;
        if (arg->ui < 1 || arg->ui > tags_num) return;
        if ((arg->ui - 1) == selmon->tag) return;

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if (t->fsclient) return;

        // Get tag to move the window to
        Tag *tmvto = &(selmon->tags[arg->ui -1]);

        // Client to move
        Client *c = selc;
        // Previous client which gets focus after move
        Client *pc = c->prevfocus;

        // Move the client to tag (detach, attach)
        _mvwintotag(c, tmvto);

        //Unmap moved client
        unmapclient(c);

        // Arrange the monitor
        arrangemon(selmon);

        // Focus previous client
        focusclient(pc);
}

void
followwintotag(Arg *arg)
{
        if (!selc) return;

        Tag *t = currenttag(selmon);

        if (t->fsclient) return;
        if (arg->i != 1 && arg->i != -1) return;

        // Client to follow
        Client *c = selc;

        // To which tag to move
        unsigned int totag = 1;

        // Follow window to right tag
        if (arg->i == 1) {
                if (selmon->tag == (tags_num - 1)) return;
                totag = selmon->tag + 2;
        } else {
                if (!selmon->tag) return;
                totag = selmon->tag;
        }

        Tag *tmvto = &(selmon->tags[totag - 1]);
        _mvwintotag(c, tmvto);

        Arg a = { .ui = totag };
        focustag(&a);
}

void
mvwin(Arg *arg)
{
        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if (t->fsclient) return;

        if (arg->i != 1 && arg->i != -1) return;

        // Client to move
        Client *ctm = t->focusclients;

        // Move to right
        if (arg->i == 1) {
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
        }
        // Move to left
        else {
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
cycletag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        // New tag
        unsigned int tn = 0;

        // Focus the next tag
        if (arg->i == 1) {
                tn = selmon->tag + 2;
                if (tn > tags_num) tn = 1;
        } else {
                tn = selmon->tag;
                if (tn == 0) tn = tags_num;
        }

        Arg a = { .ui = tn };
        focustag(&a);
}

void
focusmon(Monitor *m)
{
        if (!m || m->inactive) return;

        // Previous monitor
        Monitor *pm = selmon;

        selmon = m;

        XDeleteProperty(
                dpy,
                root,
                net_atoms[NET_ACTIVE]);
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);

        // Set borders to inactive on previous monitor
        if (pm != selmon) setborders(currenttag(pm));

        // Focus window on current tag
        Tag *t = currenttag(selmon);
        focusclient(t->focusclients);
}

void
cyclemon(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        // Focus monitor if available
        if (arg->i == 1)
                focusmon(selmon->next ? selmon->next : mons);
        else if (arg->i == -1)
                focusmon(selmon->prev ? selmon->prev : lastmon);
}

void
cycleclient(Arg *arg)
{
        if (!selc) return;
        if (arg->i != 1 && arg->i != -1) return;

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(!t || t->fsclient) return;

        if  (t->clientnum < 2) return;

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

        focusclient(tofocus);
}

void
focustag(Arg *arg)
{
        if (arg->ui < 1 || arg->ui > tags_num) return;

        unsigned int tagtofocus = arg->ui - 1;
        if (tagtofocus == selmon->tag) return;

        // Get current tag
        Tag *tc = currenttag(selmon);

        // Clear old tag identifier in the statusbar
        if (tc->clientnum) selmon->bartags[selmon->tag*2] = '*';
        else selmon->bartags[selmon->tag*2] = ' ';

        // Update current tag of the current monitor
        selmon->tag = tagtofocus;
        // Get new tag
        Tag *tn = currenttag(selmon);

        // Arrange the new clients on the newly selected tag
        arrangemon(selmon);

        // Unmap current clients
        unmaptag(tc);
        // Map new clients
        maptag(tn);

        // Focus the selected client on selected tag
        focusclient(tn->focusclients);

        // Create new tag identifier in the statusbar
        selmon->bartags[selmon->tag*2] = '>';

        drawbar(selmon);
}


/*** Util functions ***/

unsigned int
cleanmask(unsigned int mask)
{
        // Thanks to dwm
        return mask & (unsigned int) ~(Mod2Mask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask);
}

int
gettagnum(Tag *t)
{
        if (!t) return -1;

        for (int i = 0; i < tags_num; ++i)
                if (&t->mon->tags[i] == t)
                        return i;

        return -1;
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

        // Do not set border when fullscreen client
        if (t->fsclient) {
                setborder(t->fsclient->win, 0, 0);
                return;
        }

        // Set borders for selected tag
        for (Client *c = t->clients; c; c = c->next) {
                if (c == t->focusclients && selmon == t->mon)
                        setborder(c->win, borderwidth, bordercolor);
                else if (c == t->urgentclient)
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

Bool
sendevent(Window w, Atom *prot)
{
        Bool protavail = False;
        int protcount = 0;
        Atom *avail;

        if (XGetWMProtocols(dpy, w, &avail, &protcount)) {
                for (int i = 0; i < protcount; ++i)
                        if (avail[i] == *prot)
                                protavail = True;
                XFree(avail);
        }

        if (protavail) {
                XEvent ev;
                ev.type = ClientMessage;
                ev.xclient.window = w;
                ev.xclient.message_type = icccm_atoms[ICCCM_PROTOCOLS];
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = (long) *prot;
                ev.xclient.data.l[1] = CurrentTime;
                XSendEvent(dpy, w, False, NoEventMask, &ev);
        }

        return protavail;
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

Tag *
currenttag(Monitor *m)
{
        if (!m) return NULL;
        return &m->tags[m->tag];
}

int
wm_detected(Display *dpy, XErrorEvent *ee)
{
        die("Different WM already running\n");
        // Ignored
        return 0;
}

void
freemons(void)
{
        // Free monitors, tags //
        if (!mons) return;

        Monitor *tmp = mons;
        for (Monitor *m = mons; m; m = tmp) {
                free(m->tags);
                free(m->bartags);
                m->bartagssize = 0;

                tmp = m->next;
                free(m);
        }

        mons = selmon = NULL;
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
resizemons(XineramaScreenInfo *info, int mn)
{
        Monitor *m = mons;

        int n = 0;
        for (; n < mn; ++n) {
                if (!m) m = createmon(info + n);

                m->snum = info[n].screen_number;
                m->x = info[n].x_org;
                m->y = info[n].y_org;
                m->width = info[n].width;
                m->height = info[n].height;
                updatemonmasteroffset(m, 0);
                // Change monitor num indicator
                snprintf(m->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);
                // Focus first mon
                if (!n) focusmon(m);

                m->inactive = false;
                m = m->next;
        }

        currmonitornum = mn;

        // Set dangling monitors to inactive (disconnected monitors)
        for (; m; m = m->next) m->inactive = true;
}

Monitor*
createmon(XineramaScreenInfo *info)
{
        Monitor *m = (Monitor*)ecalloc(sizeof(Monitor), 1);

        m->snum = info->screen_number;
        m->x = info->x_org;
        m->y = info->y_org;
        m->width = info->width;
        m->height = info->height;
        m->tag = 0;
        m->prev = NULL;
        m->next = NULL;
        m->inactive = false;

        // Init the tags
        unsigned long tags_bytes = (tags_num * sizeof(Tag));
        m->tags = (Tag*)ecalloc(tags_bytes, 1);
        for (int i = 0; i < tags_num; ++i) m->tags[i].mon = m;

        // Automatically append monitors to our list of monitors
        if (!mons) {
                mons = m;
                selmon = m;
                lastmon = m;
        } else {
                lastmon->next = m;
                m->prev = lastmon;
                lastmon = m;
        }

        // Generate the bartags string which is displayed inside the statusbar
        generatebartags(m);

        return m;
}

void
initmons(void)
{
        if (!XineramaIsActive(dpy)) die("Build with Xinerama\n");

        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &currmonitornum);
        for (int n = 0; n < currmonitornum; ++n) createmon(info + n);

        XFree(info);
}

void
setup(void)
{
        root = DefaultRootWindow(dpy);

        // Check that no other WM is running
        XSetErrorHandler(wm_detected);
        XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|StructureNotifyMask|KeyPressMask|PropertyChangeMask);
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

        // Set supporting net atoms on one of the statusbar
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
        XSync(dpy, 0);
}

void
run()
{
        XEvent e;
        while(!XNextEvent(dpy, &e))
                if(handler[e.type]) handler[e.type](&e);
}


int
main(int argc, char *argv[])
{
        if(!(dpy = XOpenDisplay(NULL)))
                die("Can not open Display\n");

        signal(SIGCHLD, SIG_IGN);

        setup();
        run();

        XCloseDisplay(dpy);
        return 0;
}
