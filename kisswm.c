#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

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

struct Client {
        Window win;
        Monitor *m;
        Tag *tag;
        Client *next;
        Client *focusnext;
        int y;
        int x;
        int width;
        int height;
        int bw;
};

struct Tag {
        Client *clients;
        Client *focusclients;
        int clientnum;
};

struct Monitor {
        Monitor *next;
        unsigned int tag;
        Tag *tags;
        int y;
        int x;
        int height;
        int width;
};

void test(Arg*);
void spawn(Arg*);
void killclient(Arg*);
void closeclient(Window);
void die(char*, int);
void setup();
Client* wintoclient(Window);
Tag* currenttag(Monitor*);
void updatemons();
void attach(Client*);
void detach(Client*);
void focusattach(Client*);
void focusdetach(Client*);
void focus(Client*);
void arrange(Monitor*);
void arrangemon(Monitor*);
void run();
void grabkeys(Window*);
int wm_detected(Display*, XErrorEvent*);
int onxerror(Display*, XErrorEvent*);
void buttonpress(XEvent*);
void keypress(XEvent*);
void configurenotify(XEvent*);
void configurerequest(XEvent*);
void maprequest(XEvent*);
void unmapnotify(XEvent*);
void destroynotify(XEvent*);

void (*handler[LASTEvent])(XEvent*) = {
        [ButtonPress] = buttonpress,
        [KeyPress] = keypress,
        [ConfigureNotify] = configurenotify,
        [ConfigureRequest] = configurerequest,
        [MapRequest] = maprequest,
        [UnmapNotify] = unmapnotify,
        [DestroyNotify] = destroynotify
};

#include "kisswm.h"

unsigned long tags_num = sizeof(tags)/sizeof(tags[0]);

Display *dpy;
Window root;
Monitor *mons, *selmon;
Client *selc;

void
killclient(Arg *arg)
{
        if (!selc)
                return;

        XGrabServer(dpy);
        XKillClient(dpy, selc->win);
        XUngrabServer(dpy);
}

void
destroynotify(XEvent *e)
{
        XDestroyWindowEvent *ev = &e->xdestroywindow;
        printf("Destroy Notify!!\n");
        closeclient(ev->window);
        XSync(dpy, 0);
}

void
unmapnotify(XEvent *e)
{
        XUnmapEvent *ev = &e->xunmap;
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

void
closeclient(Window w)
{
        Client *c = selc->win == w ? selc : NULL;
        if (!c && !(c = wintoclient(w)))
                return;

        Monitor *m = c->m;
        Tag *t = currenttag(m);

        t->clientnum -= 1;
        detach(c);
        focusdetach(c);
        printf("Selc after delete: %p\n", selc);
        arrangemon(m);
        printf("Deleted client: %p\n", c);
        free(c);

        // TODO: What does this do exactly? (DWM)
        XEvent ev;
        while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
focusattach(Client *c)
{
        Tag *t = currenttag(c->m);
        if (!t)
                return;

        printf("Attaching Client to Tag: %p\n", t);
        selmon = c->m;
        selc = c;
        Client *beforefocus = t->focusclients;
        if (t->clientnum > 1 && beforefocus)
                c->focusnext = beforefocus;
        t->focusclients = c;
        printf("Client Attached: %p\n", t->focusclients);

}

void
focusdetach(Client *c)
{
    Tag *t = currenttag(c->m);
    if (!t)
            return;

    t->focusclients = c->focusnext;
    selc = t->focusclients;
    c->focusnext = NULL;
}

void
detach(Client *c)
{
        Client **todetach;
        for (todetach = &c->m->tags[c->m->tag].clients; *todetach && *todetach != c; todetach = &(*todetach)->next);
        *todetach = c->next;
        // TODO: Maybe set c->next to NULL?
        c->next = NULL;
}

void
attach(Client *c)
{
        c->next = NULL;

        Client **lastc;
        for(lastc = &c->m->tags[c->m->tag].clients; *lastc; lastc = &(*lastc)->next);
        *lastc = c;
}

void
focus(Client *c)
{
        // Focus client
        // TODO: Set netatom NetActiveWindow (XChangeProperty)
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
}

void
arrange(Monitor *m)
{
        // Arrange current viewable terminals
        for (Monitor *m = mons; m; m = m->next)
                arrangemon(m);
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

void
arrangemon(Monitor *m)
{

        // Only arrange current focused Tag of the monitor
        Tag *t = currenttag(m);
        if (t && !t->clientnum)
                return;

        printf("Arrange Monitor: %p\n", m);
        // TODO: Maybe call it arrangetag instead (We only arrange an active tag and it's client)
        // TODO: How to hide client/windows which are not an this tag/mon? Maybe I still need a big ass client linked list
        // Maybe we can iterate to all clients of a mon by looping through the tags first
        XWindowChanges wc;

        int setwidth = m->width / t->clientnum;
        int setheight = m->height;
        int setx = m->x;

        printf("Clientnum: %d\n", t->clientnum);
        printf("Width: %d, Height: %d, setx: %d\n", setwidth, setheight, setx);

        int clientpos = 0;
        for (Client *c = t->clients; c; c = c->next) {
                c->width = wc.width = setwidth;
                c->height = wc.height = setheight;
                c->x = wc.x = setx = (c->width * clientpos++);

                printf("Client: %p\n", c);
                printf("Resized Client: %dx%d, %d.%d\n", c->width, c->height, c->x, c->y);

                XConfigureWindow(dpy, c->win, CWX|CWWidth|CWHeight, &wc);
        }


        Tag *selectedtag = currenttag(selmon);
        printf("Selectedtag: %p\n", selectedtag);
        printf("Current mon tag: %p\n", t);
        printf("Focusclients: %p\n", selectedtag->focusclients);
        focus(selectedtag->focusclients);
        XSync(dpy, 0);
        printf("Done arranging\n");
}


void
maprequest(XEvent *e)
{
        XMapRequestEvent *ev = &e->xmaprequest;
        XWindowAttributes wa;

        if (!XGetWindowAttributes(dpy, ev->window, &wa))
                return;

        Client *c = malloc(sizeof(Client));
        c->win = ev->window;
        c->m = selmon;
        Tag *t = currenttag(c->m);
        c->tag = t;

        t->clientnum += 1;
        attach(c);
        focusattach(c);

        XMapWindow(dpy, c->win);
        grabkeys(&(c->win));

        XSync(dpy, 0);

        arrangemon(c->m);
}

void
configurenotify(XEvent *e)
{
        XConfigureEvent *ev = &e->xconfigure;
}

void
configurerequest(XEvent *e)
{
        XConfigureRequestEvent *ev = &e->xconfigurerequest;
        XWindowChanges wc;

        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;

        XConfigureWindow(dpy, ev->window, (unsigned int)ev->value_mask, &wc);
        XSync(dpy, 0);
}

void
buttonpress(XEvent *e)
{
        XButtonPressedEvent *ev = &e->xbutton;
}

void
grabkeys(Window *w)
{
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

void
spawn(Arg *arg)
{
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
}

void
test(Arg *arg)
{
        printf("%s\n", (char*)arg->v);
        fflush(NULL);
}

void
run()
{
        XEvent e;
        while(!XNextEvent(dpy, &e))
                if(handler[e.type])
                        handler[e.type](&e);
}

void
updatemons()
{
        // TODO: Get multiple monitors with Xinerama
        // Either initialize monitors or update them (When plugged/unplugged)
        // TODO: Get all monitors and calculate with them
        Monitor *m = malloc(sizeof(Monitor));
        m->next = NULL;
        m->tag = 0;

        int snum = DefaultScreen(dpy);
        m->height = DisplayHeight(dpy, snum);
        m->width = DisplayWidth(dpy, snum);
        m->y = 0;
        m->x = 0;

        printf("Monitor: %dx%d on %d.%d\n", m->width, m->height, m->x, m->y);


        // Init the tags
        unsigned long tags_bytes = (tags_num * sizeof(Tag));
        m->tags = malloc(tags_bytes);
        memset(m->tags, 0, tags_bytes);

        printf("Tags: %lu\n", tags_num);
        printf("Tags size: %lu\n", tags_bytes);

        mons = m;
        selmon = m;
}

void
setup()
{
        // Check that no other WM is running
        XSetErrorHandler(wm_detected);
        XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|KeyPressMask|ButtonPressMask);
        XSync(dpy, 0);

        // Setup of Kisswm
        XSetErrorHandler(onxerror);

        updatemons();
}

int
onxerror(Display *dpy, XErrorEvent *ee)
{
        printf("Error occured!\n");
        fflush(NULL);
        return 0;
}

int
wm_detected(Display *dpy, XErrorEvent *ee)
{
        die("Different WM already running", 2);
        // Ignored
        return 0;
}

void
die(char* msg, int exit_code)
{
        fprintf(stderr, "Kisswm [ERROR]: %s\n", msg);
        exit(exit_code);
}

int main()
{
        if(!(dpy = XOpenDisplay(NULL)))
                die("Can not open Display", 1);
        root = DefaultRootWindow(dpy);

        setup();

        run();

        XCloseDisplay(dpy);
        return 0;
}
