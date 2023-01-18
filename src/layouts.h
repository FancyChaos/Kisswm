void    lonely_client(Client*);

void
MASTER_STACK_LAYOUT(Monitor *m, Layout_Meta *meta)
{
        Tag *t = m->ws->tag;

        Client *c = get_first_managed_client(t);
        if (!c) return;

        int border_offset = borderwidth * 2;
        if (t->clientnum_managed == 1) {
                lonely_client(c);
                return;
        }

        Client *lc = get_last_managed_client(t);

        int base_height = m->height - statusbar.height;
        int master_area = (m->width / 2) + meta->master_offset;

        // Set client in master area
        set_client_size(
                c,
                master_area - border_offset,
                base_height - border_offset,
                m->x,
                m->y + statusbar.height);

        // sa = stack area
        int sa_clientnum = t->clientnum_managed - 1;
        int sa_client_x = m->x + master_area;
        int sa_client_width = m->width - master_area - border_offset;
        int sa_client_height = base_height / sa_clientnum;
        int sa_client_render_height = sa_client_height - border_offset;
        int sa_client_last_height = sa_client_render_height +
            base_height - (sa_client_height * sa_clientnum);

        int prev_y = c->y;
        for (c = c->next; c; c = c->next) {
                if (!(c->cf & CL_MANAGED)) continue;
                set_client_size(
                        c,
                        sa_client_width,
                        c == lc ? sa_client_last_height : sa_client_render_height,
                        sa_client_x,
                        prev_y);

                prev_y += sa_client_height;
        }
}

void
SIDE_BY_SIDE_LAYOUT(Monitor *m, Layout_Meta *meta)
{
        Tag *t = m->ws->tag;

        Client *c = get_first_managed_client(t);

        int border_offset = borderwidth * 2;
        if (t->clientnum_managed == 1) {
                lonely_client(c);
                return;
        }

        Client *lc = get_last_managed_client(t);

        int client_height = m->height - statusbar.height - border_offset;
        int base_width = m->width / t->clientnum_managed;
        int client_width = base_width - border_offset;
        int client_last_width = client_width +
            m->width - (base_width * t->clientnum_managed);

        int prev_x = m->x;
        for (; c; c = c->next) {
                if (!(c->cf & CL_MANAGED)) continue;
                set_client_size(
                        c,
                        c == lc ? client_last_width : client_width,
                        client_height,
                        prev_x,
                        m->y + statusbar.height);

                prev_x += base_width;
        }
}

void
STACK_LAYOUT(Monitor *m, Layout_Meta *meta)
{
        Tag *t = m->ws->tag;

        Client *c = get_first_managed_client(t);

        int border_offset = borderwidth * 2;
        if (t->clientnum_managed == 1) {
                lonely_client(c);
                return;
        }

        Client *lc = get_last_managed_client(t);

        int client_width = m->width - border_offset;
        int base_height = m->height - statusbar.height;
        int client_height = base_height / t->clientnum_managed;
        int client_render_height = client_height - border_offset;
        int client_last_render_height = client_render_height +
            base_height - (client_height * t->clientnum_managed);

        int prev_y = m->y + statusbar.height;
        for (; c; c = c->next) {
                if (!(c->cf & CL_MANAGED)) continue;
                set_client_size(
                        c,
                        client_width,
                        c == lc ? client_last_render_height : client_render_height,
                        m->x,
                        prev_y);

                prev_y += client_height;
        }

}

// Render only one client
void
lonely_client(Client *c)
{
        if (!c) return;

        int border_offset = borderwidth * 2;
        set_client_size(
                c,
                c->mon->width - border_offset,
                c->mon->height - statusbar.height - border_offset,
                c->mon->x,
                c->mon->y + statusbar.height);
}
