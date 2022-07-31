#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "util.h"

#ifdef DODEBUG
        #define DEBUG(...) fprintf(stderr, "DEBUG: "__VA_ARGS__"\n")
#else
        #define DEBUG(...) do {} while (0)
#endif

enum { ICCCM_PROTOCOLS, ICCCM_DEL_WIN, ICCCM_FOCUS, ICCCM_END };
enum { NET_SUPPORTED, NET_SUPPORTING, NET_WM_NAME, NET_STATE, NET_ACTIVE, NET_CLOSE, NET_FULLSCREEN, NET_END};

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
typedef struct Barwin Barwin;
typedef struct Colors Colors;

struct Barwin {
        Window w;
        XftDraw *xdraw;
};

struct Colors {
        XftColor black;
        XftColor white;
};

struct Client {
        Window win;
        Monitor *m;
        Tag *tag;
        Client *next, *prev;
        Client *nextfocus, *prevfocus;
        int y;
        int x;
        int width;
        int height;
        int bw;
};

struct Tag {
        int clientnum;
        Client *clients;
        Client *focusclients;
        // Fullscreen client
        Client *fsclient;
};

struct Monitor {
        Monitor *next;
        Barwin barwin;
        unsigned int tag;
        Tag *tags;
        int y;
        int x;
        int height;
        int width;
};

void mapclient(Client*);
void unmapclient(Client*);
void maptag(Tag*, int);
void unmaptag(Tag*);
void spawn(Arg*);
void mvwintotag(Arg*);
void followwintotag(Arg*);
void mvwin(Arg*);
void fullscreen(Arg*);
void focustag(Arg*);
void cycletag(Arg*);
void cycleclient(Arg*);
void killclient(Arg*);
void closeclient(Window);
void setup();
Client* wintoclient(Window);
Bool sendevent(Client*, Atom*);
Tag* currenttag(Monitor*);
void updatemons();
Client* _mvwintotag(unsigned int);
void togglefullscreen(Client*);
void attach(Client*);
void detach(Client*);
void focusattach(Client*);
void focusdetach(Client*);
void focusclient(Client*);
void focus();
void arrange(Monitor*);
void arrangemon(Monitor*);
void run();
void grabkeys(Window*);
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
void createbars();

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

Display *dpy;
Window root;
Monitor *mons, *selmon;
Client *selc;

XftDraw *xdraw;
XftFont *xfont;
XGlyphInfo xglyph;
Colors xcolors;

int screen;
int sw;
int sh;
unsigned long colormap;
Visual *visual;


unsigned int tags_num = sizeof(tags)/sizeof(tags[0]);

char *bartags;
unsigned long bartagssize;
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
                // TODO: Display an urgent mark inside bartags string
        } else if (ev->message_type == net_atoms[NET_STATE]) {
                if (ev->data.l[1] == net_atoms[NET_FULLSCREEN] ||
                    ev->data.l[2] == net_atoms[NET_FULLSCREEN]) {
                        fullscreen(NULL);
                }
        }

        DEBUG("---End: ClientMessage---");
}

void
destroynotify(XEvent *e)
{
        DEBUG("---Start: DestroyNotify---");
        XDestroyWindowEvent *ev = &e->xdestroywindow;
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
        // We assume the maprequest is on the current (selected) monitor
        // Get current tag
        XMapRequestEvent *ev = &e->xmaprequest;
        XWindowAttributes wa;

        if (!XGetWindowAttributes(dpy, ev->window, &wa))
                return;

        Tag *ct = currenttag(selmon);

        Client *c = malloc(sizeof(Client));
        c->next = c->prev = c->nextfocus = c->prevfocus = NULL;
        c->win = ev->window;
        c->m = selmon;
        Tag *t = ct;
        c->tag = t;

        attach(c);
        focusattach(c);
        selc = c;

        arrangemon(c->m);

        XMapWindow(dpy, c->win);
        grabkeys(&(c->win));

        focus();

        DEBUG("---End: MapRequest---");
}

void
configurenotify(XEvent *e)
{
        DEBUG("---Start: ConfigureNotify---");
        XConfigureEvent *ev = &e->xconfigure;
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
                    && keys[i].f) {
                        keys[i].f(&(keys[i].arg));
                        XSync(dpy, 0);
                }
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
createbars()
{
        DEBUG("---Start: createbars---");
        Monitor *m = NULL;
        for (m = selmon; m; m = m->next) {
                m->barwin.w = XCreateSimpleWindow(
                        dpy,
                        root,
                        0,
                        0,
                        (unsigned int) selmon->width,
                        (unsigned int) barheight,
                        0,
                        0,
                        0);

                m->barwin.xdraw = XftDrawCreate(
                        dpy,
                        selmon->barwin.w,
                        visual,
                        colormap);

                XMapWindow(dpy, m->barwin.w);
        }

        DEBUG("---End: createbars---");
}

void
drawbar(Monitor *m)
{
        DEBUG("---Start: drawbar---");

        // TODO: Finally implement cleanup after all this
        // TODO: Renam stuff that doesnt make sense

        // TODO: Extra function to draw text onto the statusbar
        //      draw_to_statusbar(text, font, x, y)
        // TODO: Only draw bartags if needed
        // TODO: Extra

        // Clear bar
        XftDrawRect(
                m->barwin.xdraw,
                &xcolors.black,
                0,
                0,
                (unsigned int) m->width,
                (unsigned int) barheight);

        int baroffset = (barheight - xfont->height) / 2;
        int glyphheight = 0;

        int bartagslen = (int) strnlen(bartags, bartagssize);
        int barstatuslen = (int) strnlen(barstatus, sizeof(barstatus));

        // Draw Tags first
        XftTextExtentsUtf8 (
                dpy,
                xfont,
                (XftChar8 *) bartags,
                bartagslen,
                &xglyph);

        glyphheight = xglyph.height;

        XftDrawStringUtf8(
                m->barwin.xdraw,
                &xcolors.white,
                xfont,
                0,
                glyphheight + baroffset,
                (XftChar8 *) bartags,
                bartagslen);

        // Draw statubarstatus
        XftTextExtentsUtf8 (
                dpy,
                xfont,
                (XftChar8 *) barstatus,
                barstatuslen,
                &xglyph);

        XftDrawStringUtf8(
                m->barwin.xdraw,
                &xcolors.white,
                xfont,
                m->width - xglyph.width,
                glyphheight + baroffset,
                (XftChar8 *) barstatus,
                barstatuslen);

        DEBUG("---End: drawbar---");
}

void
updatestatustext()
{
        // Reset barstatus
        barstatus[0] = '\0';

        XTextProperty pwmname;
        int success = XGetWMName(dpy, root, &pwmname);
        if (!success) {
                // TODO: Try different method for name retrival
                // https://specifications.freedesktop.org/wm-spec/1.3/

        }


        if (success && pwmname.encoding == XA_STRING) {
                strlcpy(barstatus, (char*)pwmname.value, sizeof(barstatus));
                XFree(pwmname.value);
        } else {
                strcpy(barstatus, "Kisswm V0.1");
        }
}

void
updatebars()
{
        DEBUG("---Start: updatebars---");

        updatestatustext();

        for (Monitor *m = selmon; m; m = m->next)
                drawbar(m);

        DEBUG("---End: updatebars---");
}

/*** WM state changing functions ***/

void
togglefullscreen(Client *ct)
{
        DEBUG("---Start: togglefullscreen---");
        Tag *t = ct->tag;

        // set fullscreen client
        t->fsclient = (t->fsclient) ? NULL : ct;

        for (Client *c = t->clients; c; c = c->next) {
                if (c == ct )
                        continue;

                if (t->fsclient)
                        unmapclient(c);
                else
                        mapclient(c);
        }

        if (t->fsclient)
                XUnmapWindow(dpy, selmon->barwin.w);
        else
                XMapWindow(dpy, selmon->barwin.w);

        updatebars();

        DEBUG("---End: togglefullscreen---");
}

Client*
_mvwintotag(unsigned int tag)
{
        // TODO: Maybe accept the client to move as argument?

        DEBUG("---Start: _mvwintotag---");
        if (tag < 1 || tag > tags_num)
                return NULL;

        if (!selc)
                return NULL;

        // Current Tag
        Tag *tc = currenttag(selmon);
        // New Tag
        Tag *tn = &selmon->tags[tag - 1];

        // Client to move
        Client *ctm = tc->focusclients;

        // Detach client from current tag
        detach(ctm);

        // Detach client from focus
        if (ctm == selc) {
                selc = ctm->prevfocus;
        }
        focusdetach(ctm);

        // Assign client to chosen tag
        ctm->tag = tn;
        attach(ctm);
        focusattach(ctm);

        // Dont set selc because we do not follow the client

        DEBUG("---End: _mvwintotag---");
        return ctm;
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
        for(Client *c = t->clients; c; c = c->next)
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
        for(Client *c = t->clients; c; c = c->next)
                mapclient(c);
}

void
closeclient(Window w)
{
        DEBUG("---Start: closeclient---");
        Client *c = (selc && selc->win == w) ? selc : NULL;
        if (!c && !(c = wintoclient(w)))
                return;

        Monitor *m = c->m;

        detach(c);

        // Detach client from focus
        if (c == selc)
                selc = c->prevfocus;

        focusdetach(c);

        // Reset fsclient
        if (c->tag->fsclient == c)
                togglefullscreen(c);

        free(c);

        arrangemon(m);

        focus();

        DEBUG("---End: closeclient---");
}

void
focus()
{
        DEBUG("---Start: focus---");

        if (!selc) {
                XDeleteProperty(
                        dpy,
                        root,
                        net_atoms[NET_ACTIVE]);
                return;
        }

        XSetInputFocus(dpy, selc->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_ACTIVE],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &(selc->win),
                1);
        sendevent(selc, &icccm_atoms[ICCCM_FOCUS]);
        DEBUG("---End: focus---");
}

void
focusclient(Client *c)
{
        DEBUG("---Start: focusclient---");
        if (!c)
                return;

        Tag *t = c->tag;
        if (!t)
                return;

        if (c == t->focusclients) {
                selc = c;
                selmon = c->m;
                focus();
                return;
        }

        if (c->prevfocus)
                c->prevfocus->nextfocus = c->nextfocus;
        if (c->nextfocus)
                c->nextfocus->prevfocus = c->prevfocus;

        t->focusclients->nextfocus = c;
        c->prevfocus = t->focusclients;

        c->nextfocus = NULL;

        t->focusclients = c;

        selc = c;
        selmon = c->m;

        focus();
        DEBUG("---End: focusclient---");
}

void
focusattach(Client *c)
{
        DEBUG("---Start: focusattach---");
        Tag *t = c->tag;
        if (!t)
                return;

        // TODO: Insert into focus if there is a fullscreen client on this tag
        // Insert between fsclient (focusclients) and the previous focus
        // Maybe look at focusclient() for inspiration
        // For now we just dont allow a window to move to a tag with fullscreen client

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
arrange(Monitor *m)
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
        if (t && !t->clientnum)
                return;

        XWindowChanges wc;

        // We have a fullscreen client on the tag
        if (t->fsclient) {
                t->fsclient->width = wc.width = m->width;
                t->fsclient->height = wc.height = m->height;
                t->fsclient->x = wc.x = 0;
                t->fsclient->y = wc.y = 0;

                XConfigureWindow(dpy, t->fsclient->win, CWX|CWY|CWWidth|CWHeight, &wc);
                XSync(dpy, 0);
                DEBUG("---End: arrangemon (FULLSCREEN)---");
                return;
        }

        int setwidth = m->width / t->clientnum;
        int setheight = m->height - barheight;
        int setx = m->x;
        int sety = barheight;

        int clientpos = 0;
        for (Client *c = t->clients; c; c = c->next) {
                c->width = wc.width = setwidth;
                c->height = wc.height = setheight;
                c->x = wc.x = setx = (c->width * clientpos++);
                c->y = wc.y = sety;

                XConfigureWindow(dpy, c->win, CWY|CWX|CWWidth|CWHeight, &wc);
        }


        XSync(dpy, 0);
        DEBUG("---End: arrangemon---");
}


void
grabkeys(Window *w)
{
        DEBUG("---Start: grabkeys---");
        for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i)
                XGrabKey(
                        dpy,
                        XKeysymToKeycode(dpy, keys[i].keysym),
                        keys[i].modmask,
                        *w,
                        0,
                        GrabModeAsync,
                        GrabModeAsync
                );
        DEBUG("---End: grabkeys---");
}

/*** Keybinding fuctions ***/

void
killclient(Arg *arg)
{
        DEBUG("---Start: killclient---");
        if (!selc)
                return;

        XGrabServer(dpy);

        if (!sendevent(selc, &icccm_atoms[ICCCM_DEL_WIN])) {
                DEBUG("Killing client by force");
                XKillClient(dpy, selc->win);
        }

        XUngrabServer(dpy);
        DEBUG("---End: killclient---");
}

void
spawn(Arg *arg)
{
        DEBUG("---Start: spawn---");

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient)
                return;

        // TODO: Rad about fork(), dup2(), etc. and rework if neccessary
        if (fork() == 0) {
                if (dpy)
                        close(ConnectionNumber(dpy));
                setsid();
                execvp(((char **)arg->v)[0], (char **)arg->v);
                fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
                perror(" failed");
                exit(EXIT_SUCCESS);
        }
        DEBUG("---End: spawn---");
}

void
fullscreen(Arg* arg)
{
        Tag *t = currenttag(selmon);
        if (!t->clientnum || !t->focusclients)
                return;

        togglefullscreen(t->focusclients);
        if (t->fsclient)
                focusclient(t->fsclient);

        arrangemon(selmon);
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
        if(t && t->fsclient)
                return;

        // Quickfix: Do not allow to move window when the target tag
        //           has a fullscreen window on it
        if (selmon->tags[arg->ui - 1].fsclient)
                return;

        // Returns client which was moved
        Client *c = _mvwintotag(arg->ui);

        //Unmap moved client
        unmapclient(c);

        // Arrange the monitor
        arrangemon(selmon);

        // Focus previous client
        focus();

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

        if (!(arg->i == 1) && !(arg->i == -1))
                return;

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

        _mvwintotag(totag);

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
        if(t && t->fsclient)
                return;

        if (!(arg->i == 1) && !(arg->i == -1))
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
        if (!(arg->i == 1) && !(arg->i == -1))
                return;

        // Chill for a bit
        usleep(10*1000);

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

void cycleclient(Arg *arg)
{
        DEBUG("---Start: cycleclient---");

        // Dont allow on fullscreen
        Tag *t = currenttag(selmon);
        if(t && t->fsclient)
                return;

        if (!(arg->i == 1) && !(arg->i == -1))
                return;

        if (!selc)
                return;

        if  (t->clientnum < 2)
                return;

        // Chill for a bit
        usleep(10*1000);

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

        // Clear old tag identifier in the statusbar
        bartags[selmon->tag*2] = ' ';

        // Get current tag
        Tag *tc = currenttag(selmon);
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
        // map/unmap barwin depending on fullscreen on new tag
        if (tn->fsclient)
                XUnmapWindow(dpy, selmon->barwin.w);
        else
                XMapWindow(dpy, selmon->barwin.w);

        // Set the current selected client to the current one on the tag
        selc = tn->focusclients;

        focus();

        // Create new tag identifier in the statusbar
        bartags[selmon->tag*2] = '>';

        updatebars();
        DEBUG("---End: focustag---");
}


/*** Util functions ***/

Bool
sendevent(Client *c, Atom *prot)
{
        Window w = c->win;
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
                                if (c->win == w) {
                                        return c;
                                        DEBUG("---End: wintoclient successfully---");
                                }

        DEBUG("---End: wintoclient with NULL---");

        return NULL;
}

Tag *
currenttag(Monitor *m)
{
        if (!m)
                m = selmon;
        if (!selmon)
                return NULL;

        return &m->tags[m->tag];
}

int
wm_detected(Display *dpy, XErrorEvent *ee)
{
        die("Different WM already running");
        // Ignored
        return 0;
}

void
updatemons()
{
        // TODO: Get multiple monitors with Xinerama
        // Either initialize monitors or update them (When plugged/unplugged)
        // TODO: Get all monitors and calculate with them
        Monitor *m = malloc(sizeof(Monitor));
        memset(m, 0, sizeof(Monitor));

        int snum = DefaultScreen(dpy);
        m->height = DisplayHeight(dpy, snum);
        m->width = DisplayWidth(dpy, snum);

        printf("Monitor: %dx%d on %d.%d\n", m->width, m->height, m->x, m->y);


        // Init the tags
        unsigned long tags_bytes = (tags_num * sizeof(Tag));
        m->tags = malloc(tags_bytes);
        memset(m->tags, 0, tags_bytes);

        mons = m;
        selmon = m;
}

void
setup()
{
        // Check that no other WM is running
        XSetErrorHandler(wm_detected);
        XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|KeyPressMask|ButtonPressMask|PropertyChangeMask);
        XSync(dpy, 0);

        // Set the error handler
        XSetErrorHandler(onxerror);

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
        net_atoms[NET_ACTIVE] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
        net_atoms[NET_CLOSE] = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
        net_atoms[NET_FULLSCREEN] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

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


        // Setup colors
        if (!XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), "white", &xcolors.white))
                die("Could not load colors\n");
        if (!XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), "black", &xcolors.black))
                die("Could not load colors\n");


        // Create the bartags string which will be displayed in the statusbar
        bartagssize = (tags_num * 2) + 2;
        bartags = ecalloc(bartagssize, 1);
        // Add spaces to all tags
        //  1 2 3 4 5 6 7 8 9
        for (int i = 0, j = 0; i < tags_num; ++i) {
                bartags[++j] = *tags[i];
                bartags[i*2] = ' ';
                j += 1;
        }
        bartags[bartagssize - 1] = '\0';

        // We start on first tag
        bartags[0] = '>';

        // Setup monitors and Tags
        updatemons();

        // Get screen attributes
        screen = DefaultScreen(dpy);
        sw = DisplayWidth(dpy, screen);
        sh = DisplayHeight(dpy, screen);
        visual = DefaultVisual(dpy, screen);
        colormap = DefaultColormap(dpy, screen);

        // Create statusbars
        createbars();
        updatebars();

        // Set supporting net atoms on one of the barwin
        XChangeProperty(
                dpy,
                root,
                net_atoms[NET_SUPPORTING],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &(selmon->barwin.w),
                1);
        XChangeProperty(
                dpy,
                selmon->barwin.w,
                net_atoms[NET_SUPPORTING],
                XA_WINDOW,
                32,
                PropModeReplace,
                (unsigned char*) &(selmon->barwin.w),
                1);
        XChangeProperty(
                dpy,
                selmon->barwin.w,
                net_atoms[NET_WM_NAME],
                ATOM_UTF8,
                8,
                PropModeReplace,
                (unsigned char*) "kisswm",
                6);


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


int main()
{
        if(!(dpy = XOpenDisplay(NULL)))
                die("Can not open Display");
        root = DefaultRootWindow(dpy);

        setup();

        run();

        XCloseDisplay(dpy);
        return 0;
}
