#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#include <bsd/string.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "kisswm.h"

// Include of config and layouts at the end
#include "config.h"
#include "layouts.h"

size_t tags_num = sizeof(tags)/sizeof(tags[0]);
size_t layouts_num = sizeof(layouts_available)/sizeof(layouts_available[0]);


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

        if (ev->window != root) {
                Client *c = wintoclient(ev->window);
                if (!c) return;

                if (selmon != c->mon) focusmon(c->mon);
                focusclient(c, false);
                setborders(c->tag);

                return;
        }

        // Mouse click on root window
        // Focus appropriate monitor
        for (Monitor *m = mons; m; m = m->next) {
                if (selmon == m) continue;

                if (ev->x_root >= m->x && ev->x_root <= m->x + m->width) {
                        focusmon(m);
                        focusclient( m->ws->tag->clients_focus, false);
                        break;
                }
        }
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

        if (ev->message_type == net_atoms[NET_ACTIVE] &&
            c->tag != selmon->ws->tag) {
                c->ws->bartags[c->tag->num * 2] = '!';
                c->cf |= CL_URGENT;
                setborders(c->tag);
        } else if (ev->message_type == net_atoms[NET_STATE]) {
                if (ev->data.l[1] == net_win_states[NET_FULLSCREEN] ||
                    ev->data.l[2] == net_win_states[NET_FULLSCREEN]) {
                            // Ignore request if the client
                            // is not on an active tag
                            if (c->tag != c->mon->ws->tag) return;

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
                            arrangemon(c->mon);
                            drawbar(c->mon);
                } else if (ev->data.l[1] == net_win_states[NET_HIDDEN]) {
                        // Hide window if requested
                        switch (ev->data.l[0]) {
                        case 2: // Toggle
                                if (c->cf & CL_HIDDEN) unhide(c);
                                else hide(c);
                                break;
                        case 1: // Add (Set hidden)
                                hide(c);
                                break;
                        case 0: // Remove (Unset hidden)
                                unhide(c);
                                break;
                        default:
                                return;
                        }
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
        if (wintoclient(ev->window)) return;

        XWindowAttributes wa;
        if (!XGetWindowAttributes(dpy, ev->window, &wa)) return;

        // We assume the maprequest is on the current (selected) monitor
        // Get current tag
        Tag *ct = selmon->ws->tag;

        Client *c = ecalloc(sizeof(Client), 1);
        c->next = c->prev = c->nextfocus = c->prevfocus = NULL;
        c->win = ev->window;
        c->mon = selmon;
        c->ws = selmon->ws;
        c->tag = ct;
        c->cf = CL_MANAGED;

        c->width = wa.width;
        c->height = wa.height;
        c->x = wa.x;
        c->y = wa.y;

        Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
        if (net_win_types[NET_UTIL] == wintype ||
            net_win_types[NET_DIALOG] == wintype) {
                // Set dialog flag
                c->cf |= CL_DIALOG;
                // unset managed flag
                c->cf &= ~CL_MANAGED;
        }

        attach(c);
        focusattach(c);

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
        refresh_monitors();
        focusmon(mons);

        // Calculate combined monitor width (screen width)
        sw = 0;
        for (Monitor *m = mons; m; m = m->next) sw += m->width;

        arrange();

        // Update statusbar to new width of combined monitors
        statusbar.width = sw;
        set_window_size(statusbar.win, statusbar.width, statusbar.height, 0, 0);
        updatebars();

        focusclient(selmon->ws->tag->clients_focus, true);

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

        XWindowChanges wc;

        Client *c = wintoclient(ev->window);
        if (c) {
                // Do not allow custom sizes when a layout is enabled
                // Still allow for dialog windows
                if (c->tag->layout->f && !(c->cf & CL_DIALOG)) return;
                c->x = wc.x = ev->x;
                c->y = wc.y = ev->y;
                c->width = wc.width = ev->width;
                c->height = wc.height = ev->height;
        } else {
                Atom wintype = getwinprop(ev->window, net_atoms[NET_TYPE]);
                if (wintype != net_win_types[NET_UTIL] &&
                    wintype != net_win_types[NET_DIALOG])
                        return;
                wc.x = ev->x;
                wc.y = ev->y;
                wc.width = ev->width;
                wc.height = ev->height;
        }

        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, (unsigned int) ev->value_mask, &wc);

        XSync(dpy, 0);
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
        Tag *t = m->ws->tag;
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

        int bartagslen = (int) strnlen(m->ws->bartags, m->ws->bartagssize);
        int barstatuslen = (int) strnlen(barstatus, sizeof(barstatus));

        // Get glyph information about statusbartags (Dimensions of text)
        XftTextExtentsUtf8(
                dpy,
                xfont,
                (XftChar8 *) m->ws->bartags,
                bartagslen,
                &xglyph);
        int baroffset = xglyph.y + ((statusbar.height - xglyph.height) / 2);

        // Draw statusbartags text
        XftDrawStringUtf8(
                statusbar.xdraw,
                &colors.barfg.xft_color,
                xfont,
                m->x,
                m->y + baroffset,
                (XftChar8 *) m->ws->bartags,
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
hide(Client *c)
{
        if (!c || c->cf & CL_HIDDEN) return;

        if (c->tag->client_fullscreen == c) unsetfullscreen(c);

        // Add hidden flag and remove managed flag
        c->cf |= CL_HIDDEN;

        // Client will not be managed when hidden
        c->cf &= ~CL_MANAGED;
        --c->tag->clientnum_managed;

        remaptag(c->tag);
        arrangemon(c->mon);

        // Detach from focus
        focusdetach(c);
        focusclient(c->tag->clients_focus, true);
}

void
unhide(Client *c)
{
        if (!c || !(c->cf & CL_HIDDEN)) return;

        // Remove hidden flag and add managed flag
        c->cf &= ~CL_HIDDEN;

        c->cf |= CL_MANAGED;
        ++c->tag->clientnum_managed;

        remaptag(c->tag);
        arrangemon(c->mon);

        // Attach to focus
        focusattach(c);
        focusclient(c, true);
}

void
focusmon(Monitor *m)
{
        if (!m) return;

        // Previous monitor
        Monitor *pm = selmon;

        // Update bartags of previous focused monitor
        pm->ws->bartags[pm->ws->tag->num * 2] = '^';
        drawbar(pm);

        // Update bartags of monitor to focus
        m->ws->bartags[m->ws->tag->num * 2] = '>';
        drawbar(m);

        // Set current monitor
        selmon = m;

        // Set borders to inactive on previous monitor
        setborders(pm->ws->tag);

        XSync(dpy, 0);
}

void
focustag(Tag *t)
{
        // Get current selected tag
        Tag *tc = selmon->ws->tag;

        // Focus monitor if it differs from the current selected tag
        if (t->ws->mon != tc->ws->mon) focusmon(t->ws->mon);

        // Focus workspace
        t->ws->mon->ws = t->ws;

        // Clear old tag identifier in the statusbar
        if (tc != t && tc->clientnum) tc->ws->bartags[tc->num * 2] = '*';
        else if (tc != t) tc->ws->bartags[tc->num * 2] = ' ';

        // Create new tag identifier in the statusbar
        t->ws->bartags[t->num * 2] = '>';

        // Update the current active tag
        t->ws->tag = t;

        // Redraw both tags
        if (tc != t) remaptag(tc);
        remaptag(t);

        // arrange and focus
        arrangemon(t->ws->mon);
        focusclient(t->clients_focus, true);

        // Update statusbar (Due to bartags change)
        drawbar(t->ws->mon);
        XSync(dpy, 0);
}

void
setfullscreen(Client *c)
{
        // Already in fullscreen
        if (!c || c->tag->client_fullscreen || !(c->cf & CL_MANAGED)) return;

        // Set fullscreen property
        XChangeProperty(
                dpy,
                c->win,
                net_atoms[NET_STATE],
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char*) &net_win_states[NET_FULLSCREEN],
                1);
        c->tag->client_fullscreen = c;
        XRaiseWindow(dpy, c->win);
        setborders(c->tag);
        XSync(dpy, 0);
}

void
unsetfullscreen(Client *c)
{
        if (!c || c->tag->client_fullscreen != c) return;


        // Delete fullscreen property
        XDeleteProperty(
                dpy,
                c->win,
                net_atoms[NET_STATE]);
        c->tag->client_fullscreen = NULL;
        setborders(c->tag);
        XSync(dpy, 0);
}

void
move_tag_to_tag(Tag *t, Tag *tt)
{
        // Move all Clients of a tag to a different one
        Client *nc = NULL;
        for (Client *c = t->clients; c; c = nc) {
                nc = c->next;
                move_client_to_tag(c, tt);
                tt->ws->bartags[t->num * 2] = '*';
        }
}

void
move_client_to_tag(Client *c, Tag *t)
{
        // Unmapclient if moved to an inactive tag
        if (t != t->ws->mon->ws->tag) unmapclient(c);

        // Detach client from current tag
        detach(c);
        // Detach client from focus
        focusdetach(c);

        // Assign client to targets tag workspace and monitor
        c->mon = t->ws->mon;
        c->ws = t->ws;
        c->tag = t;

        // Update bartag if tag is on the active monitor
        if (selmon == t->ws->mon) t->ws->bartags[t->num * 2] = '*';

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

        if (t != t->ws->mon->ws->tag) {
                // if tag is not the current active tag unmap everything
                for (Client *c = t->clients; c; c = c->next) unmapclient(c);
        } else if (t->client_fullscreen) {
                // only map fullscreen and dialog clients
                for (Client *c = t->clients; c; c = c->next) {
                        if (c == t->client_fullscreen ||c->cf & CL_DIALOG) mapclient(c);
                        else unmapclient(c);
                }
        } else {
                for (Client *c = t->clients; c; c = c->next) {
                        if (c->cf & CL_HIDDEN) unmapclient(c);
                        else mapclient(c);
                }
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
        if (!t->clientnum && t != m->ws->tag) m->ws->bartags[t->num * 2] = ' ';

        // if the tag where client closed is active (seen)
        if (t == m->ws->tag) {
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
        if (!c) {
                c = selmon->ws->tag->clients_focus;
                if (c && c->cf & CL_HIDDEN) {
                        return;
                } else if (c && c != selc) {
                        focusclient(c, warp);
                } else if (!c) {
                        if (selc) grabbuttons(selc->win);
                        selc = NULL;
                        focus(0, NULL, warp);
                }
                return;
        }

        // Focus only clients on an active tag
        // and ignore hidden clients
        if (c->tag != c->mon->ws->tag || c->cf & CL_HIDDEN) return;

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

        // Reset fullscreen if managed client
        if (c->cf & CL_MANAGED) unsetfullscreen(t->client_fullscreen);

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

        if (c->cf & CL_MANAGED) --t->clientnum_managed;
        --t->clientnum;

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

        if (c->cf & CL_MANAGED) ++t->clientnum_managed;
        ++t->clientnum;

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

        Tag *t = m->ws->tag;
        if (t->layout->f != MASTER_STACK_LAYOUT) return;
        if (t->clientnum_managed < 2) return;

        int halfwidth = m->width / 2;
        int updatedmasteroffset = offset ? (t->layout->meta.master_offset + offset) : 0;

        // Do not adjust if masteroffset already too small/big
        if ((halfwidth + updatedmasteroffset) < 100) return;
        else if ((halfwidth + updatedmasteroffset) > (m->width - 100)) return;

        t->layout->meta.master_offset = updatedmasteroffset;
}

void
set_window_size(Window w, int width, int height, int x, int y)
{
        if (!w) return;

        XWindowChanges wc = {
                .width = width,
                .height = height,
                .x = x,
                .y = y};
        XConfigureWindow(dpy, w, CWX|CWY|CWWidth|CWHeight, &wc);
}

void
set_client_size(Client *c, int width, int height, int x, int y)
{
        if (!c || !(c->cf & CL_MANAGED)) return;

        c->width = width;
        c->height = height;
        c->x = x;
        c->y = y;

        set_window_size(c->win, width, height, x, y);
}

void
arrange(void)
{
        for (Monitor *m = mons; m; m = m->next) arrangemon(m);
}

void
arrangemon(Monitor *m)
{
        // Only arrange if we have managed clients
        Tag *t = m->ws->tag;
        if (t->clientnum_managed == 0) return;


        // We have a fullscreen client on the tag
        if (t->client_fullscreen) {
                set_client_size(
                        t->client_fullscreen,
                        m->width,
                        m->height,
                        m->x,
                        m->y);
                XSync(dpy, 0);
                return;
        }

        if (t->layout->f) t->layout->f(m, &(t->layout->meta));
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
key_create_workspace(Arg* arg)
{
        Workspace *ws = workspace_add(selmon);
        focustag(ws->tag);
}

void
key_move_client_to_workspace(Arg* arg)
{
        if (!selc || selmon->ws_count == 1) return;

        Workspace *ws = NULL;
        if (arg->i == 1) ws = selmon->ws->next;
        if (arg->i == -1) ws = selmon->ws->prev;

        if (ws) move_client_to_tag(selc, ws->tags + selc->tag->num);

        arrangemon(selmon);
        focusclient(selmon->ws->tag->clients_focus, true);
}

void
key_delete_workspace(Arg* arg)
{
        if (selmon->ws_count == 1) return;

        Workspace *ws = selmon->ws;
        workspace_delete(ws);
        selmon->ws = selmon->wss;

        focustag(selmon->wss->tag);
}

void
key_cycle_workspace(Arg* arg)
{
        if (selmon->ws_count == 1) return;

        Workspace *ws = NULL;
        if (arg->i == 1) {
                ws = selmon->ws->next ? selmon->ws->next : selmon->wss;
        } else if (arg->i == -1) {
                ws = selmon->ws->prev;
                if (!ws) for (ws = selmon->ws; ws->next; ws = ws->next);
        }

        focustag(ws->tag);
}

void
key_change_layout(Arg* arg)
{
        Tag *t = selmon->ws->tag;
        if (t->client_fullscreen) return;

        // Set new layout index
        t->layout->index =
            ((t->layout->index + 1) == layouts_num) ? 0 : t->layout->index + 1;

        t->layout->f = layouts_available[t->layout->index];

        if (t->clientnum_managed) arrangemon(selmon);

        XSync(dpy, 0);
}

void
key_update_master_offset(Arg *arg)
{
        if (selmon->ws->tag->client_fullscreen) return;
        updatetagmasteroffset(selmon, arg->i);
        arrangemon(selmon);
}

void
key_kill_client(Arg *arg)
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
        arrangemon(t->ws->mon);
        drawbar(t->ws->mon);
}

void
key_move_client_to_monitor(Arg *arg)
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
        if (tm->ws->tag->client_fullscreen) return;

        // Move client (win) to target monitor
        move_client_to_tag(c, tm->ws->tag);

        setborders(tm->ws->tag);

        arrangemon(tm);
        arrangemon(selmon);

        // Focus next client on current monitor/tag
        focusclient(t->clients_focus, true);
}

void
key_move_client_to_tag(Arg *arg)
{
        if (!selc) return;
        if (arg->ui < 1 || arg->ui > tags_num) return;
        if ((arg->ui - 1) == selmon->ws->tag->num) return;

        Client *c = selc;
        Tag *t = c->tag;
        if (t->client_fullscreen) return;

        // Get tag to move the window to
        Tag *tm = selmon->ws->tags + (arg->ui -1);

        // Move the client to tag (detach, attach)
        move_client_to_tag(c, tm);

        // Unmap moved client
        unmapclient(c);

        // Update bartags
        selmon->ws->bartags[(arg->ui - 1) * 2] = '*';
        drawbar(selmon);

        // Arrange the monitor
        arrangemon(selmon);

        // Focus previous client
        focusclient(t->clients_focus, true);
}

void
key_follow_client_to_tag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;
        if (!selc) return;

        Tag *t = selc->tag;
        if (t->client_fullscreen) return;

        int totag = t->num + arg->i;
        if (totag < 0) totag = (int) (tags_num - 1);
        else if (totag == tags_num) totag = 0;

        Tag *tm = selmon->ws->tags + totag;
        move_client_to_tag(selc, tm);
        focustag(tm);
}

void
key_move_client(Arg *arg)
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
key_cycle_tag(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        int totag = selmon->ws->tag->num + arg->i;
        if (totag < 0) totag = (int) (tags_num - 1);
        else if (totag == tags_num) totag = 0;

        focustag(selmon->ws->tags + totag);
}

void
key_cycle_monitor(Arg *arg)
{
        if (arg->i != 1 && arg->i != -1) return;

        // Focus monitor if available
        Monitor *m = NULL;
        if (arg->i == 1) m = selmon->next ? selmon->next : mons;
        else m = selmon->prev ? selmon->prev : lastmon;

        if (m == selmon) return;

        focusmon(m);
        focusclient(m->ws->tag->clients_focus, true);
        XSync(dpy, 0);
}

void
key_cycle_client(Arg *arg)
{
        if (!selc) return;
        if (arg->i != 1 && arg->i != -1) return;

        Tag *t = selc->tag;
        if (t->clientnum < 2) return;

        // Client to focus
        Client *c = NULL;

        // Allow switching to dialog windows in fullscreen
        if (t->client_fullscreen) {
                if (arg->i == 1) {
                        for (c = selc->next; c; c = c->next)
                                if (c->cf & CL_DIALOG) break;
                        if (!c) {
                                for (c = t->clients; c; c = c->next)
                                        if (c->cf & CL_DIALOG || c == selc)
                                                break;
                        }
                } else {
                        for (c = selc->prev; c; c = c->prev)
                                if (c->cf & CL_DIALOG) break;
                        if (!c) {
                                for (c = t->client_last; c; c = c->prev)
                                        if (c->cf & CL_DIALOG || c == selc)
                                                break;
                        }
                }

                if (c && c != selc)
                        focusclient(c, true);
                else if (c && c != t->client_fullscreen)
                        focusclient(t->client_fullscreen, true);

                return;
        }

        if (arg->i == 1) {
                // Focus to next element or to first in stack
                c = selc->next;
                if (!c) c = t->clients;

                // Get next client which is not hidden
                while (c->cf & CL_HIDDEN) {
                        if (c == selc) break;
                        if (c->next) c = c->next;
                        else c = t->clients;
                }
        } else {
                // Focus to previous element or last in the stack
                c = selc->prev;
                if (!c) c = t->client_last;

                // Get previous client which is not hidden
                while (c->cf & CL_HIDDEN) {
                        if (c == selc) break;
                        if (c->prev) c = c->prev;
                        else c = t->client_last;
                }
        }

        focusclient(c, true);
}

void
key_focus_tag(Arg *arg)
{
        if (arg->ui < 1 || arg->ui > tags_num) return;

        unsigned int tagtofocus = arg->ui - 1;
        if (tagtofocus == selmon->ws->tag->num) return;

        focustag(selmon->ws->tags + tagtofocus);
}


/*** Util functions ***/

Client*
get_first_managed_client(Tag *t)
{
        Client *c = t->clients;
        for (; c && !(c->cf & CL_MANAGED); c = c->next);
        return c;
}

Client*
get_last_managed_client(Tag *t)
{
        Client *c = t->client_last;
        // Move backwards to the last tiling client
        for (; c && !(c->cf & CL_MANAGED); c = c->prev);
        return c;
}

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

void
setborders(Tag *t)
{
        if (!t || !t->clients) return;
        if (t != t->ws->mon->ws->tag) return;

        // Do not set border when fullscreen client
        if (t->client_fullscreen) {
                setborder(t->client_fullscreen->win, 0, NULL);

                // Return if focus is on fullscreen client
                if (t->clients_focus == t->client_fullscreen) return;

                // Only dialogs could be in focus at this point
                for (Client *c = t->clients; c; c = c->next) {
                        if (c->cf & CL_DIALOG)
                                setborder(c->win, borderwidth, &colors.bordercolor);
                }

                return;
        }

        // Set borders for selected tag
        for (Client *c = t->clients; c; c = c->next) {
                if (c->cf & CL_HIDDEN) continue;

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
        for (Monitor *m = mons; m; m = m->next)
                for (Workspace *ws = m->wss; ws; ws = ws->next)
                        for (int i = 0; i < tags_num; ++i)
                                for (Client *c = ws->tags[i].clients; c; c = c->next)
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
generate_bartags(Workspace *ws, Monitor *m)
{
        // Create the bartags string which will be displayed in the statusbar
        ws->bartagssize = (tags_num * 2) + 1;
        // Add monitor indicator ' | n'
        ws->bartagssize += 4;

        ws->bartags = (char*)ecalloc(ws->bartagssize, 1);
        ws->bartags[ws->bartagssize - 1] = '\0';

        // Add spaces to all tags
        //  1 2 3 4 5 6 7 8 9
        for (int i = 0, j = 0; i < tags_num; ++i) {
                ws->bartags[++j] = *tags[i];
                ws->bartags[i*2] = ' ';
                j += 1;
        }

        // Show monitor number
        snprintf(ws->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);

        // We start on first tag
        ws->bartags[0] = '>';
}

void
monitor_update(Monitor *m, XRRMonitorInfo *info)
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
refresh_monitors(void)
{

        int mn;
        XRRMonitorInfo *info = XRRGetMonitors(dpy, root, True, &mn);
        if (!info) die("Could not get monitors with Xrandr");

        int monitornum = 0;
        bool overlaying = false;

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
                        m = monitor_create(info + n);
                        lastmon->next = m;
                        m->prev = lastmon;
                } else {
                        monitor_update(m, info + n);
                        updatetagmasteroffset(m, 0);
                }
                // Change bartags accordingly
                m->snum = monitornum++;
                snprintf(m->ws->bartags + (tags_num * 2), 5, " | %d", m->snum + 1);
                m->ws->bartags[m->ws->tag->num * 2] = n ? '^' : '>';

                lastmon = m;
                m = m->next;
        }
        lastmon->next = NULL;
        currentmonnum = monitornum;

        // Move any client of previous active monitors to the main one
        if (m) {
                // These monitors are not active anymore
                // Move any window to the first mon
                for (Monitor *nm = m; nm; m = nm) {
                        nm = m->next;
                        // Never destroy first monitor
                        if (m != mons) monitor_destroy(m, mons);
                }

                // Update masteroffset of tag because
                // we could have more clients now
                updatetagmasteroffset(mons, 0);

                // Only map client of active tag within
                // the first monitor (selmon/mons)
                for (int i = 0; i < tags_num; ++i) remaptag(mons->ws->tags + i);
                mons->ws->bartags[mons->ws->tag->num * 2] = '>';
        }

        XRRFreeMonitors(info);
        XSync(dpy, 0);
}

void
monitor_destroy(Monitor *m, Monitor *tm)
{
        // Destroy monitor and move clients
        // to different one
        if (!m || !tm || tm == m) return;

        for (Workspace *ws = m->wss; ws; ws = ws->next)
                for (int i = 0; i < tags_num; ++i)
                        move_tag_to_tag(ws->tags + i, tm->ws->tags + i);


        // Free monitor
        monitor_free(m);
}

Workspace*
workspace_create(Monitor *m)
{
        Workspace *ws = (Workspace*)ecalloc(sizeof(Workspace), 1);
        ws->mon = NULL;
        ws->next = NULL;
        ws->prev = NULL;

        // Init the tags
        ws->tags = (Tag*)ecalloc(tags_num * sizeof(Tag), 1);
        for (int i = 0; i < tags_num; ++i) {
                ws->tags[i].ws = ws;
                ws->tags[i].num = i;
                ws->tags[i].layout = (Layout*) ecalloc(sizeof(Layout), 1);
                ws->tags[i].layout->f = layouts_available[0];
                ws->tags[i].layout->index = 0;
        }
        ws->tag = ws->tags;

        return ws;
}

Workspace*
workspace_add(Monitor *m)
{
        m->ws_count++;

        Workspace *ws = workspace_create(m);
        ws->mon = m;
        generate_bartags(ws, m);

        if (!m->wss) {
                m->wss = ws;
                m->ws = ws;
                return ws;
        }

        Workspace *ws_last = m->ws;
        for (; ws_last->next; ws_last = ws_last->next);
        ws_last->next = ws;
        ws_last->next->prev = ws_last;

        return ws;
}

void
workspace_delete(Workspace *ws)
{
        // Do nothing if lonely workspace
        if (ws->mon->ws_count == 1) return;
        ws->mon->ws_count--;

        // Update first workspace
        if (ws == ws->mon->wss) ws->mon->wss = ws->next;

        // Move all leftover clients to the active tag of the first workspace
        for (int i = 0; i < tags_num; ++i)
                move_tag_to_tag(ws->tags + i, ws->mon->wss->tags + i);

        // Set active workspace of monitor to NULL if this was the one
        if (ws == ws->mon->ws) ws->mon->ws = NULL;

        if (ws->next) ws->next->prev = ws->prev;
        if (ws->prev) ws->prev->next = ws->next;

        workspace_free(ws);
}

void
workspace_free(Workspace *ws)
{
        // Free layouts on tags
        for (int i = 0; i < tags_num; ++i)
                free(ws->tags[i].layout);

        // Free tags and bartags
        free(ws->tags);
        free(ws->bartags);

        free(ws);
}

Monitor*
monitor_create(XRRMonitorInfo *info)
{
        Monitor *m = (Monitor*)ecalloc(sizeof(Monitor), 1);

        monitor_update(m, info);
        m->prev = NULL;
        m->next = NULL;
        m->wss = NULL;
        m->ws = NULL;
        m->ws_count = 0;

        workspace_add(m);

        return m;
}

void
monitor_free(Monitor *m)
{
        // Free workspaces on monitor
        Workspace *ws_next;
        for (Workspace *ws = m->wss; ws; ws = ws_next) {
                ws_next = ws->next;
                workspace_free(ws);
        }

        XFree(m->aname);
        free(m);
}

void
initialize_monitors(void)
{
        mons = NULL;
        lastmon = NULL;
        selmon = NULL;

        XRRMonitorInfo *info = XRRGetMonitors(dpy, root, True, &currentmonnum);
        if (!info) die("Could not get monitors with Xrandr");

        for (int n = 0; n < currentmonnum; ++n) {
                Monitor *m = monitor_create(info + n);
                m->snum = n;

                // Update monitor number in statusbar
                snprintf(
                        m->ws->bartags + (tags_num * 2),
                        5,
                        " | %d",
                        m->snum + 1);

                if (!mons) {
                        mons = m;
                        selmon = m;
                        lastmon = m;
                } else {
                        m->ws->bartags[0] = '^';
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

        // Set atoms for window types
        net_win_types[NET_DESKTOP] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        net_win_types[NET_DOCK] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        net_win_types[NET_TOOLBAR] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
        net_win_types[NET_MENU] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
        net_win_types[NET_UTIL] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
        net_win_types[NET_SPLASH] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
        net_win_types[NET_DIALOG] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        net_win_types[NET_NORMAL] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

        // Set atoms for window states
        net_win_states[NET_FULLSCREEN] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
        net_win_states[NET_HIDDEN] = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);

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
        initialize_monitors();

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
