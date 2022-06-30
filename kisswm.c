#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
        Window w;
        Monitor *m;
        unsigned int tag;
        Client *next;
};

struct Tag {
        Client *clients;
        Client *selc;
};

struct Monitor {
        Monitor *next;
        unsigned int tag;
        Tag *tags;
};

void test(Arg*);
void spawn(Arg*);
void die(char*, int);
void setup();
void updatemons();
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

void (*handler[LASTEvent])(XEvent*) = { [ButtonPress] = buttonpress,
        [KeyPress] = keypress,
        [ConfigureNotify] = configurenotify,
        [ConfigureRequest] = configurerequest,
        [MapRequest] = maprequest,
        [UnmapNotify] = unmapnotify,
        [DestroyNotify] = destroynotify
};

#include "kisswm.h"

Display *dpy;
Window root;
Monitor *mons, *selmon;

void
destroynotify(XEvent *e)
{
        XDestroyWindowEvent *ev = &e->xdestroywindow;
}

void
unmapnotify(XEvent *e)
{
        XUnmapEvent *ev = &e->xunmap;
}

void
attach(Client *c)
{
        c->next = NULL;

        Client **lastc;
        for(lastc = &c->m->tags[selmon->tag].clients; *lastc; lastc = &(*lastc)->next);
        *lastc = c;
}

void
arrange(Monitor *m)
{
        // Arrange current viewable terminals
        for (Monitor *m = mons; m; m = m->next)
                arrangemon(m);
}

void
arrangemon(Monitor *m)
{
        // TODO: Maybe call it arrangetag instead (We only arrange an active tag and it's client)
        // TODO: How to hide client/windows which are not an this tag/mon? Maybe I still need a big ass client linked list
        // Maybe we can iterate to all clients of a mon by looping through the tags first
}


void
maprequest(XEvent *e)
{
        XMapRequestEvent *ev = &e->xmaprequest;

        Client *c = malloc(sizeof(Client));
        c->w = ev->window;
        // TODO: Will lead to bugs when app opens and monitor is changed quickly
        // We need to calculate which monitor is responsible for the new window (x,y) calculation
        c->m = selmon;
        c->tag = selmon->tag;
        attach(c);

        // TODO: Call an arrange function which arranges all clients
        arrangemon(c->m);

        XMapWindow(dpy, c->w);
        grabkeys(&(c->w));
        XSync(dpy, 0);
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
        // TODO: Get all monitors and calculate with them
        Monitor *m = malloc(sizeof(Monitor));
        m->next = NULL;
        m->tag = 0;

        // Init the tags
        m->tags = calloc(sizeof(tags)/sizeof(tags[0]), sizeof(Tag));

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
