#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef __linux__
#include <bsd/string.h>
#endif
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
};

void focusmon(Monitor*, bool);
Monitor* createmon(XineramaScreenInfo*);
void resizemons(XineramaScreenInfo*, int);
void createcolor(const char*, XftColor*);
bool alreadymapped(Window);
void setborder(Window, int, unsigned long);
void setborders(Monitor*);
void mapclient(Client*);
void drawdialog(Window, XWindowAttributes*);
void unmapclient(Client*);
void maptag(Tag*, int);
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
void setup();
Atom getwinprop(Window, Atom);
Client* wintoclient(Window);
Bool sendevent(Window, Atom*);
Tag* currenttag(Monitor*);
void freemons();
void initmons();
void generatebartags(Monitor*);
void _mvwintotag(Client*, Tag*);
void togglefullscreen(Client*);
void attach(Client*);
void detach(Client*);
void focusattach(Client*);
void focusdetach(Client*);
void focusclient(Client*);
void focus(Window, Client*);
void arrange();
void arrangemon(Monitor*);
void updatemasteroffset(Arg*);
void run();
void cleanup();
void grabkeys(Window);
int wm_detected(Display*, XErrorEvent*);
int onxerror(Display*, XErrorEvent*);
void buttonpress(XEvent*);
void keypress(XEvent*);
void configurenotify(XEvent*);
void propertynotify(XEvent*);
void configurerequest(XEvent*);
void maprequest(XEvent*);
void unmapnotify(XEvent*);
void destroynotify(XEvent*);
void clientmessage(XEvent*);

void updatebars();
void updatestatustext();
void drawbar();

void (*handler[LASTEvent])(XEvent*) = {
        [ButtonPress] = buttonpress,
        [KeyPress] = keypress,
        [ConfigureNotify] = configurenotify,
        [ConfigureRequest] = configurerequest,
        [PropertyNotify] = propertynotify,
        [MapRequest] = maprequest,
        [UnmapNotify] = unmapnotify,
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
Monitor *mons, *selmon;
Client *selc;
Statusbar statusbar;

XftFont *xfont;
XGlyphInfo xglyph;
Colors colors;
int screen;
int sh, sw;

unsigned int tags_num = sizeof(tags)/sizeof(tags[0]);

char barstatus[256];


/*** X11 Eventhandling ****/
void
clientmessage(XEvent *e)
{
        DEBUG("---Start: ClientMessage---");
        XClientMessageEvent *ev = &e->xclient;
        Client *c = wintoclient(ev->window);
        if (!c)
                return;

        if (ev->message_type == net_atoms[NET_ACTIVE]) {
                c->m->bartags[c->m->tag * 2] = '!';
                c->tag->urgentclient = c;
                if (c->tag == currenttag(c->m))
                        setborders(c->m);
        } else if (ev->message_type == net_atoms[NET_STATE]) {
                if (ev->data.l[1] == net_atoms[NET_FULLSCREEN] ||
                    ev->data.l[2] == net_atoms[NET_FULLSCREEN]) {
                            if (c->tag->fsclient == c && ev->data.l[0] == 0)
                                    togglefullscreen(c);
                            else if (!c->tag->fsclient && ev->data.l[0] == 1)
                                    togglefullscreen(c);
                }
        }

        DEBUG("---End: ClientMessage---");
}

void
destroynotify(XEvent *e)
{
        DEBUG("---Start: DestroyNotify---");
        XDestroyWindowEvent *ev = &e->xdestroywindow;
        if (ev->window)
                closeclient(ev->window);
        XSync(dpy, 0);
        DEBUG("---End: DestroyNotify---");
}

void
unmapnotify(XEvent *e)
{
        DEBUG("---Start: UnmapNotify---");
        XUnmapEvent *ev = &e->xunmap;
        DEBUG("---End: UnmapNotify---");
}

void
maprequest(XEvent *e)
{
        DEBUG("---Start: MapRequest---");

        XMapRequestEvent *ev = &e->xmaprequest;

        if (alreadymapped(ev->window))
                return;

        XWindowAttributes wa;

        if (!XGetWindowAttributes(dpy, ev->window, &wa))
                return;

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

        // We can only focus of the windows was mapped
        focusclient(c);

        grabkeys(c->win);

        DEBUG("---End: MapRequest---");
}

void
configurenotify(XEvent *e)
{
        DEBUG("---Start: ConfigureNotify---");
        XConfigureEvent *ev = &e->xconfigure;

        if (ev->window != root) return;

        sh = DisplayHeight(dpy, screen);
        sw = DisplayWidth(dpy, screen);

        int monitornum;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &monitornum);

        resizemons(info, monitornum);

        Arg a = { .i = 0 };
        updatemasteroffset(&a);

        arrange();

        statusbar.width = sw;
        XWindowChanges wc = { .width = sw };
        XConfigureWindow(dpy, statusbar.win, CWWidth, &wc);
        updatebars();

        XFree(info);

        XSync(dpy, 0);

        DEBUG("---End: ConfigureNotify---");
}

void
propertynotify(XEvent *e)
{
        DEBUG("---Start: PropertyNotify---");
        XPropertyEvent *ev = &e->xproperty;

        if ((ev->window == root) && (ev->atom == XA_WM_NAME))
                updatebars();

        DEBUG("---End: PropertyNotify---");
}

void
configurerequest(XEvent *e)
{
        DEBUG("---Start: ConfigureRequest---");
        XConfigureRequestEvent *ev = &e->xconfigurerequest;

        // Only allow custom sizes for dialog windows
        Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
        if (wintype != net_win_types[NET_UTIL] &&
            wintype != net_win_types[NET_DIALOG]) {
                return;
        }

        XWindowChanges wc;

        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;

        XConfigureWindow(dpy, ev->window, (unsigned int)ev->value_mask, &wc);
        DEBUG("---End: ConfigureRequest---");
}

void
buttonpress(XEvent *e)
{
        DEBUG("---Start: ButtonPress---");
        XButtonPressedEvent *ev = &e->xbutton;
        DEBUG("---End: ButtonPress---");
}


void
keypress(XEvent *e)
{
        XKeyEvent *ev = &e->xkey;
        KeySym keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);

        for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i)
                if (keysym == keys[i].keysym
                    && ev->state == (keys[i].modmask)
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
updatebars()
{
        DEBUG("---Start: updatebars---");

        updatestatustext();

        // Clear bar
        XClearArea(
                dpy,
                statusbar.win,
                0,
                0,
                (unsigned int) statusbar.width,
                (unsigned int) barheight,
                0);

        // Draw bar for each monitor
        for (Monitor *m = mons; m; m = m->next)
                drawbar(m);

        DEBUG("---End: updatebars---");
}

void
drawbar(Monitor *m)
{
        DEBUG("---Start: drawbar---");

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
                        (unsigned int) barheight);

        int baroffset = (barheight - xfont->height) / 2;
        int glyphheight = 0;

        int bartagslen = (int) strnlen(m->bartags, m->bartagssize);
        int barstatuslen = (int) strnlen(barstatus, sizeof(barstatus));

        // Draw Tags first
        XftTextExtentsUtf8(
                dpy,
                xfont,
                (XftChar8 *) m->bartags,
                bartagslen,
                &xglyph);

        glyphheight = xglyph.height;


        XftDrawStringUtf8(
                statusbar.xdraw,
                &colors.xbarfg,
                xfont,
                m->x,
                m->y + (glyphheight + baroffset),
                (XftChar8 *) m->bartags,
                bartagslen);

        if (barstatus[0] == '\0') {
                XSync(dpy, 0);
                return;
        }

        // Draw statubarstatus
        XftTextExtentsUtf8 (
                dpy,
                xfont,
                (XftChar8 *) barstatus,
                barstatuslen,
                &xglyph);

        XftDrawStringUtf8(
                statusbar.xdraw,
                &colors.xbarfg,
                xfont,
                m->x + (m->width - xglyph.width),
                m->y + (glyphheight + baroffset),
                (XftChar8 *) barstatus,
                barstatuslen);

        XSync(dpy, 0);
        DEBUG("---End: drawbar---");
}

void
updatestatustext()
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
                        strcpy(barstatus, "Kisswm V0.1");
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
        DEBUG("---Start: togglefullscreen---");
        if (!cc)
                return;

        Tag *t = cc->tag;

        if (t->fsclient)
                XDeleteProperty(
                        dpy,
                        t->fsclient->win,
                        net_atoms[NET_STATE]);

        // set fullscreen client
        t->fsclient = (t->fsclient) ? NULL : cc;

        for (Client *c = t->clients; c; c = c->next) {
                if (c == cc)
                        continue;

                if (t->fsclient)
                        unmapclient(c);
                else
                        mapclient(c);
        }

        if (t->fsclient)
                XChangeProperty(
                        dpy,
                        selc->win,
                        net_atoms[NET_STATE],
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) &net_atoms[NET_FULLSCREEN],
                        1);

        focusclient(cc);
        updatebars();
        arrangemon(cc->m);

        DEBUG("---End: togglefullscreen---");
}

void
_mvwintotag(Client *c, Tag *t)
{
        DEBUG("---Start: _mvwintotag---");

        // Detach client from current tag
        detach(c);

        // Detach client from focus
        focusdetach(c);

        // Assign client to chosen tag
        c->tag = t;
        attach(c);
        focusattach(c);

        // Dont set selc because we do not follow the client. Only Moving

        DEBUG("---End: _mvwintotag---");
}

void
mapclient(Client *c)
{
        if (!c)
                return;
        XMapWindow(dpy, c->win);
}

void
unmapclient(Client *c)
{
        if (!c)
                return;
        XUnmapWindow(dpy, c->win);
}

void
unmaptag(Tag *t)
{
        for (Client *c = t->clients; c; c = c->next)
                unmapclient(c);
}

void
maptag(Tag *t, int check_fullscreen)
{
        // If check_fullscreen is 1, only map a fullscreen window if present
        if (check_fullscreen && t->fsclient) {
                mapclient(t->fsclient);
                return;
        }

        // Map every window if not fullscreen client is present
        for (Client *c = t->clients; c; c = c->next)
                mapclient(c);
}

void
closeclient(Window w)
{
        DEBUG("---Start: closeclient---");
        Client *c = (selc && selc->win == w) ? selc : NULL;
        if (!c && !(c = wintoclient(w)))
                return;

        // Reset fsclient
        if (c->tag->fsclient == c)
                togglefullscreen(c);

        Monitor *m = c->m;

        detach(c);

        // Detach client from focus
        if (c == selc)
                selc = c->prevfocus;

        focusdetach(c);


        free(c);

        arrangemon(m);

        // Will remove focus if selc is NULL
        if (!selc)
                focus(0, NULL);

        focusclient(selc);

        DEBUG("---End: closeclient---");
}

void
focus(Window w, Client *c)
{
        DEBUG("---Start: focus---");

        if (!w && !c) {
                if (selc)
                        w = selc->win;
                else
                        w = root;
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
        DEBUG("---End: focus---");
}

void
focusclient(Client *c)
{
        DEBUG("---Start: focusclient---");
        if (!c)
                return;

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

        setborders(c->m);

        focus(0, selc);
        DEBUG("---End: focusclient---");
}

void
focusattach(Client *c)
{
        DEBUG("---Start: focusattach---");
        Tag *t = c->tag;
        if (!t)
                return;

        // Reset fullscreen if new client attaches
        t->fsclient = NULL;

        c->nextfocus = NULL;
        c->prevfocus = t->focusclients;

        if (c->prevfocus)
                c->prevfocus->nextfocus = c;

        t->focusclients = c;

        DEBUG("---End: focusattach---");

}

void
focusdetach(Client *c)
{
        DEBUG("---Start: focusdetach---");
        Tag *t = c->tag;
        if (!t)
                return;

        if (c == t->focusclients)
                t->focusclients = c->prevfocus;

        if (c->prevfocus)
                c->prevfocus->nextfocus = c->nextfocus;
        if (c->nextfocus)
                c->nextfocus->prevfocus = c->prevfocus;

        c->prevfocus = c->nextfocus = NULL;
        DEBUG("---End: focusdetach---");
}

void
detach(Client *c)
{
        DEBUG("---Start: detach---");
        Tag *t = c->tag;
        if (!t)
                return;
        t->clientnum -= 1;

        // If this was the last open client on the tag
        if (!t->clientnum) {
                t->clients = NULL;
                c->next = c->prev = NULL;
                return;
        }

        if (c->next)
                c->next->prev = c->prev;

        if (c->prev)
                c->prev->next = c->next;
        else
                // Detaching first client
                t->clients = c->next;

        c->next = c->prev = NULL;

        DEBUG("---End: detach---");
}

void
attach(Client *c)
{
        DEBUG("---Start: attach---");
        Tag *t = c->tag;
        if (!t)
                return;
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
        DEBUG("---End: attach---");
}

void
arrange()
{
        DEBUG("---Start: arrange---");
        // Arrange current viewable terminals
        for (Monitor *m = mons; m; m = m->next)
                arrangemon(m);
        DEBUG("---End: arrange---");
}

void
arrangemon(Monitor *m)
{
        DEBUG("---Start: arrangemon---");

        // Only arrange current focused Tag of the monitor
        Tag *t = currenttag(m);
        if (!t->clientnum)
                return;

        if (t->clientnum == 1)
                t->masteroffset = 0;

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
        fc->height = wc.height = m->height - barheight - borderoffset;
        fc->x = wc.x = m->x;
        fc->y = wc.y = m->y + barheight;
        XConfigureWindow(dpy, fc->win, CWY|CWX|CWWidth|CWHeight, &wc);

        if (!fc->next) {
                XSync(dpy, 0);
                return;
        }

        // Draw rest of the clients to the right of the screen
        int rightheight = (m->height - barheight) / (t->clientnum - 1);
        for (Client *c = fc->next; c; c = c->next) {
                c->width = wc.width = m->width - masterarea - borderoffset;
                c->height = wc.height = rightheight - borderoffset;
                c->x = wc.x = masterarea + m->x;
                c->y = wc.y = c->prev->y + (c->prev == fc ? 0 : rightheight);
                XConfigureWindow(dpy, c->win, CWY|CWX|CWWidth|CWHeight, &wc);
        }

        XSync(dpy, 0);
        DEBUG("---End: arrangemon---");
}

void
drawdialog(Window w, XWindowAttributes *wa)
{
        DEBUG("---Start: drawdialog---");

        XMapWindow(dpy, w);
        grabkeys(w);
        setborder(w, borderwidth, bordercolor);
        focus(w, NULL);

        DEBUG("---End: drawdialog---");
}

void
grabkeys(Window w)
{
        DEBUG("---Start: grabkeys---");
        for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i)
                XGrabKey(
                        dpy,
                        XKeysymToKeycode(dpy, keys[i].keysym),
                        keys[i].modmask,
                        w,
                        0,
                        GrabModeAsync,
                        GrabModeAsync);
        DEBUG("---End: grabkeys---");
}

/*** Keybinding fuctions ***/

void
updatemasteroffset(Arg *arg)
{
        if (!selmon)
                return;

        // Only allow masteroffset adjustment if at least 2 clients are present
        Tag *t = currenttag(selmon);
        if (t->clientnum < 2)
                return;

        int halfwidth = selmon->width / 2;
        int updatedmasteroffset = t->masteroffset + arg->i;

        // Do not adjust if masteroffset already too small/big
        if ((halfwidth + updatedmasteroffset) < 100)
                return;
        else if ((halfwidth + updatedmasteroffset) > (selmon->width - 100))
                return;

        t->masteroffset = updatedmasteroffset;
        arrangemon(selmon);
}

void
killclient(Arg *arg)
{
        DEBUG("---Start: killclient---");
        // TODO: Either get focused window (netatom) if selc is not set
        // or create special type and keep track of dialog windows
        if (!selc)
                return;

        XGrabServer(dpy);

        if (!sendevent(selc->win, &icccm_atoms[ICCCM_DEL_WIN])) {
                DEBUG("Killing client by force");
                XKillClient(dpy, selc->win);
        }

        XUngrabServer(dpy);
        DEBUG("---End: killclient---");
}

void
spawn(Arg *arg)
{
        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient)
                return;

        if (fork())
                return;

        if (dpy)
                close(ConnectionNumber(dpy));

        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        exit(0);
}

void
fullscreen(Arg* arg)
{
        if (!selc)
                return;

        togglefullscreen(selc);
}

void
mvwintomon(Arg *arg)
{
        DEBUG("---Start: mvwintomon---");

        if (!selc)
                return;
        else if (!mons->next)
                return;
        else if (arg->i != 1 && arg->i != -1)
                return;

        // Target monitor to move window to
        Monitor *tm;
        if (arg->i == 1 && selmon->next)
                tm = selmon->next;
        else if (arg->i == -1 && selmon->prev)
                tm = selmon->prev;
        else
                return;

        // Target client to move to target monitor
        Client *tc = selc;
        // Current tag of client to move
        Tag *ct = tc->tag;
        // Current tag of the target monitor
        Tag *tt = currenttag(tm);

        // Do not allow moving when in fullscreen
        if (ct->fsclient || tt->fsclient)
                return;

        detach(tc);
        focusdetach(tc);

        tc->m = tm;
        tc->tag = tt;

        attach(tc);
        focusattach(tc);

        setborders(tm);

        arrangemon(tm);
        arrangemon(selmon);

        selc = ct->focusclients;
        if (!selc)
                focus(0, NULL);
        focusclient(ct->focusclients);

        DEBUG("---END: mvwintomon---");
}

void
mvwintotag(Arg *arg)
{
        DEBUG("---Start: mvwintotag---");

        if (arg->ui < 1 || arg->ui > tags_num)
                return;

        if ((arg->ui - 1) == selmon->tag)
                return;

        if (!selc)
                return;

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t->fsclient)
                return;

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

        DEBUG("---End: mvwintotag---");
}

void
followwintotag(Arg *arg)
{
        DEBUG("---Start: followwintotag---");

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient)
                return;

        if (arg->i != 1 && arg->i != -1)
                return;

        if (!selc)
                return;

        // Client to follow
        Client *c = selc;

        // To which tag to move
        unsigned int totag = 1;

        // Follow window to right tag
        if (arg->i == 1) {
                if (selmon->tag == (tags_num - 1))
                        return;
                totag = selmon->tag + 2;
        } else {
                if (!selmon->tag)
                        return;
                totag = selmon->tag;
        }

        Tag *tmvto = &(selmon->tags[totag - 1]);
        _mvwintotag(c, tmvto);

        Arg a = { .ui = totag };
        focustag(&a);
        DEBUG("---End: followwintotag---");
}

void
mvwin(Arg *arg)
{
        DEBUG("---Start: mvwin---");

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if (t && t->fsclient)
                return;

        if (arg->i != 1 && arg->i != -1)
                return;

        // Client to move
        Client *ctm = t->focusclients;

        // Move to right
        if (arg->i == 1) {
                if (!ctm->next)
                        return;

                // Client to switch
                Client *cts = ctm->next;

                if (ctm->prev)
                        ctm->prev->next = cts;
                if (cts->next)
                        cts->next->prev = ctm;

                ctm->next = cts->next;
                cts->prev = ctm->prev;
                ctm->prev = cts;
                cts->next = ctm;

                if (ctm == t->clients)
                        t->clients = cts;
        }
        // Move to left
        else {
                if (!ctm->prev)
                        return;

                // Client to switch
                Client *cts = ctm->prev;

                if (ctm->next)
                        ctm->next->prev = cts;
                if (cts->prev)
                        cts->prev->next = ctm;

                ctm->prev = cts->prev;
                cts->next = ctm->next;
                ctm->next = cts;
                cts->prev = ctm;

                if (cts == t->clients)
                        t->clients = ctm;
        }

        arrangemon(selmon);
        DEBUG("---End: mvwin---");
}

void
cycletag(Arg *arg)
{
        DEBUG("---Start: cycletag---");
        if (arg->i != 1 && arg->i != -1)
                return;

        // New tag
        unsigned int tn = 0;

        // Focus the next tag
        if (arg->i == 1) {
                tn = selmon->tag + 2;
                if (tn > tags_num)
                        tn = 1;
        } else {
                tn = selmon->tag;
                if (tn == 0)
                        tn = tags_num;
        }

        Arg a = { .ui = tn };
        focustag(&a);

        DEBUG("---Stop: cycletag---");
}

void
focusmon(Monitor *m, bool setfocus)
{
        DEBUG("---Start: focusmon---");
        if (!m) return;

        // Previous tag
        Tag *pt = currenttag(selmon);

        // Unfocus everything on previous monitor
        if (!pt->fsclient)
                for (Client *c = pt->clients; c; c = c->next)
                        setborder(c->win, borderwidth, bordercolor_inactive);
        XDeleteProperty(
                dpy,
                root,
                net_atoms[NET_ACTIVE]);
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);

        selmon = m;

        if (!setfocus) return;

        // Focus window on current tag
        Tag *t = currenttag(selmon);
        selc = t->focusclients;
        if (!selc) focus(0, NULL);
        focusclient(selc);

        DEBUG("---End: focusmon---");
}

void
cyclemon(Arg *arg)
{
        DEBUG("---Start: cyclemon---");

        if (!mons->next)
                return;

        if (arg->i != 1 && arg->i != -1)
                return;

        // Focus monitor if available
        if (arg->i == 1 && selmon->next)
                 focusmon(selmon->next, true);
        else if (arg->i == -1 && selmon->prev)
                focusmon(selmon->prev, true);

        DEBUG("---Stop: cyclemon---");
}

void
cycleclient(Arg *arg)
{
        DEBUG("---Start: cycleclient---");

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient)
                return;

        if (arg->i != 1 && arg->i != -1)
                return;

        if (!selc)
                return;

        if  (t->clientnum < 2)
                return;

        Client *tofocus = NULL;

        if (arg->i == 1) {
                // Focus to next element or to first in stack
                tofocus = selc->next;
                if (!tofocus)
                        tofocus = t->clients;
        } else {
                // Focus to previous element or last in the stack
                tofocus = selc->prev;
                if (!tofocus)
                        for (tofocus = selc; tofocus->next; tofocus = tofocus->next);

        }

        focusclient(tofocus);

        DEBUG("---End: cycleclient---");
}

void
focustag(Arg *arg)
{
        DEBUG("---Start: focustag---");
        if (arg->ui < 1 || arg->ui > tags_num)
                return;

        unsigned int tagtofocus = arg->ui - 1;
        if (tagtofocus == selmon->tag)
                return;

        // Get current tag
        Tag *tc = currenttag(selmon);

        // Clear old tag identifier in the statusbar
        if (tc->clientnum)
                selmon->bartags[selmon->tag*2] = '*';
        else
                selmon->bartags[selmon->tag*2] = ' ';

        // Update current tag of the current monitor
        selmon->tag = tagtofocus;
        // Get new tag
        Tag *tn = currenttag(selmon);

        // Arrange the new clients on the newly selected tag
        arrangemon(selmon);

        // Unmap current clients
        unmaptag(tc);
        // Map new clients
        maptag(tn, 1);

        // Focus the selected client on selected tag
        selc = tn->focusclients;
        focusclient(selc);

        // Create new tag identifier in the statusbar
        selmon->bartags[selmon->tag*2] = '>';

        updatebars();
        DEBUG("---End: focustag---");
}


/*** Util functions ***/

void
createcolor(const char *color, XftColor *dst)
{
        if (*color == '\0')
                return;

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
setborders(Monitor *m)
{
        DEBUG("---Start: SetBorders---");
        Tag *t = currenttag(m);
        if (!t || !t->clients) return;

        // Do not set border when fullscreen client
        if (t->fsclient) {
                setborder(t->fsclient->win, 0, 0);
                return;
        }

        // Border inactive when given monitor is not selected
        if (selmon != m) {
                for (Client *c = t->clients; c; c = c->next)
                        setborder(c->win, borderwidth, bordercolor_inactive);
                return;
        }

        // Set borders for selected tag
        for (Client *c = t->clients; c; c = c->next) {
                if (c == t->focusclients)
                        setborder(c->win, borderwidth, bordercolor);
                else if (c == t->urgentclient)
                        setborder(c->win, borderwidth, bordercolor_urgent);
                else
                        setborder(c->win, borderwidth, bordercolor_inactive);
        }

        DEBUG("---End: SetBorders---");
}

void
setborder(Window w, int width, unsigned long color)
{
        DEBUG("---Start: SetBorder---");
        XWindowChanges wc;
        wc.border_width = width;
        XConfigureWindow(dpy, w, CWBorderWidth, &wc);

        if (color)
                XSetWindowBorder(dpy, w, color);

        DEBUG("---END: SetBorder---");
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

        if (ret != Success || !data)
                return None;

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
        DEBUG("---Start: wintoclient---");
        Monitor *m;
        Client *c;
        for (m = mons; m; m = m->next)
                for (int i = 0; i < tags_num; ++i)
                        for (c = m->tags[i].clients; c; c = c->next)
                                if (c->win == w)
                                        return c;

        DEBUG("---End: wintoclient with NULL---");
        return NULL;
}

Tag *
currenttag(Monitor *m)
{
        if (!m)
                return NULL;

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
freemons()
{
        // Free monitors, tags //
        if (!mons)
                return;

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
        m->bartagssize = (tags_num * 2) + 2;
        m->bartags = (char*)ecalloc(m->bartagssize, 1);

        // Add spaces to all tags
        //  1 2 3 4 5 6 7 8 9
        for (int i = 0, j = 0; i < tags_num; ++i) {
                m->bartags[++j] = *tags[i];
                m->bartags[i*2] = ' ';
                j += 1;
        }
        m->bartags[m->bartagssize - 1] = '\0';

        // We start on first tag
        m->bartags[0] = '>';
}

void
resizemons(XineramaScreenInfo *info, int mn)
{
        DEBUG("---Start: ResizeMons---");

        Monitor *m = mons;
        for (int n = 0; n < mn; ++n) {
                if(!m) m = createmon(info + n);

                m->snum = info[n].screen_number;
                m->x = info[n].x_org;
                m->y = info[n].y_org;
                m->width = info[n].width;
                m->height = info[n].height;

                m = m->next;
        }

        XSync(dpy, 0);
        DEBUG("---End: ResizeMons---");
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
        m->prev = NULL;
        m->next = NULL;

        // Init the tags
        unsigned long tags_bytes = (tags_num * sizeof(Tag));
        m->tags = (Tag*)ecalloc(tags_bytes, 1);

        // Generate the bartags string which is displayed inside the statusbar
        generatebartags(m);

        return m;
}

void
initmons()
{
        if (!XineramaIsActive(dpy))
                die("Just build with Xinerama mate\n");

        int monitornum;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &monitornum);
        for (int n = 0; n < monitornum; ++n) {
                Monitor *m = createmon(info + n);

                if (!mons) {
                        mons = m;
                        selmon = m;
                } else {
                        Monitor *lastmon;
                        for (lastmon = mons; lastmon->next; lastmon = lastmon->next);
                        lastmon->next = m;
                        m->prev = lastmon;
                }
        }

        XFree(info);
}

void
setup()
{
        root = DefaultRootWindow(dpy);

        // Check that no other WM is running
        XSetErrorHandler(wm_detected);
        XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|StructureNotifyMask|KeyPressMask|ButtonPressMask|PropertyChangeMask);
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
        if (!xfont)
                die("Cannot load font: %s\n", barfont);

        // Set up colors
        XRenderColor alpha = {0x0000, 0x0000, 0x0000, 0xffff};
        if (!XftColorAllocValue(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &alpha, &colors.xalpha))
                die("Could not load color: alpha\n");
        createcolor(barbg, &colors.xbarbg);
        createcolor(barfg, &colors.xbarfg);


        // Setup monitors and Tags
        initmons();

        // Create statusbar window
        sh = DisplayHeight(dpy, screen);
        sw = DisplayWidth(dpy, screen);

        statusbar.width = sw;

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
        XSync(dpy, 0);
}

void
run()
{
        XEvent e;
        while(!XNextEvent(dpy, &e))
                if(handler[e.type])
                        handler[e.type](&e);
}


int
main()
{
        if(!(dpy = XOpenDisplay(NULL)))
                die("Can not open Display\n");

        signal(SIGCHLD, SIG_IGN);

        setup();
        run();

        XCloseDisplay(dpy);
        return 0;
}
