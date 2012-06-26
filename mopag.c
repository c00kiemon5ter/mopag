/**
 * Matus Telgarsky <chisel@gmail.com>
 * gcc -o mopag -std=c99 -pedantic -Wall -Wextra -Os mopag.c -lX11
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/select.h>
#include <X11/Xlib.h>

#define HEIGHT      4
#define FATTICK_W   6
#define TOP         True        /* show on top or bottom of the screen */
#define GAP         (0)         /* space above when on top. space below when on bottom */
#define BG          "#004020"
#define FG          "#90f0c0"
#define URFG        "#f00000"
#define URBG        "#900000"
#define WINFG       "#ffffff"
#define WINBG       "#000000"

typedef struct {
    unsigned int nwins;
    unsigned int mode;
    unsigned int urgent;
} DeskInfo;

enum { BORED, ERROR, RERENDER };

static void cleanup();
static int parse();
static void render();
static void setup();

static long fg, bg, urfg, urbg, winfg, winbg;
static unsigned int currdesk, ndesks = 0;
static int screen, scrwidth;
static Display *dis;
static Window root, w;
static Pixmap pm;
static GC gc;
static DeskInfo *di = NULL;

void setup()
{
    assert((dis = XOpenDisplay(NULL)));
    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);
    scrwidth  = DisplayWidth(dis, screen);

    long *col_vars[] = { &fg, &bg, &urfg, &urbg, &winfg, &winbg, NULL };
    const char *col_strs[] = { FG, BG, URFG, URBG, WINFG, WINBG, NULL };
    XColor c;
    for (unsigned int i = 0; col_vars[i]; ++i) {
        assert(XAllocNamedColor(dis, DefaultColormap(dis, screen), col_strs[i], &c, &c));
        *col_vars[i] = c.pixel;
    }

    XSetWindowAttributes wa = { .background_pixel = bg, .override_redirect = 1, .event_mask = ExposureMask, };
    w = XCreateWindow(dis, root, 0,
            (TOP) ? GAP : DisplayHeight(dis, screen) - HEIGHT - GAP,
            scrwidth, HEIGHT, 1, CopyFromParent, InputOutput, CopyFromParent,
            CWBackPixel | CWOverrideRedirect | CWEventMask, &wa);
    XMapWindow(dis, w);
    XSetWindowBorderWidth(dis, w, 0);

    XGCValues gcv = { .graphics_exposures = 0, }; /* otherwise get NoExpose on XCopyArea */
    gc = XCreateGC(dis, root, GCGraphicsExposures, &gcv);
    pm = XCreatePixmap(dis, w, scrwidth, HEIGHT, DefaultDepth(dis,screen));
}

int parse()
{
    static DeskInfo *di_temp;
    static unsigned int cur_desk_temp;

    char buf2[2048];
    ssize_t ret = read(STDIN_FILENO, buf2, sizeof(buf2));
    assert(buf2[ret - 1] == '\n');
    assert(ret > 2); /* XXX I had this fail once! (after 1 week of use?) */
    unsigned int pos2 = ret - 2;
    while (pos2 > 0 && buf2[pos2] != '\n') pos2 -= 1;
    printf("pos2 %u\n", pos2);
    char *buf = buf2 + pos2;

    int rerender = 0;

    if (!di) {
        char *s;
        assert(s = strrchr(buf, ' '));
        ndesks = atoi(s) + 1;
        assert((di = malloc(ndesks * sizeof(DeskInfo))));
        assert((di_temp = malloc(ndesks * sizeof(DeskInfo))));
        rerender = 1;
    }

    char *pos = buf;
    for (unsigned int i = 0; i < ndesks; ++i)
    {
        unsigned int is_cur, d;
        int offset;
        int res = sscanf(pos, "%u:%u:%u:%u:%u%n", &d, &di_temp[i].nwins,
                &di_temp[i].mode, &is_cur, &di_temp[i].urgent, &offset);
        printf("[%u, %u:%u:%u:%u:%u] ", res, d, di_temp[i].nwins,
                di_temp[i].mode, is_cur, di_temp[i].urgent);
        if (res < 5 || d != i) { /* %n doesn't count */
            fprintf(stderr, "Ignoring malformed input.\n");
            return ERROR;
        }

        if (is_cur)
            cur_desk_temp = i;

        if (!rerender &&
                (di_temp[i].nwins != di[i].nwins
                 || di_temp[i].mode != di[i].mode
                 || di_temp[i].urgent != di[i].urgent
                 || (is_cur != (currdesk == i)) ) )
            rerender = 1;
        printf("(re %u) ", rerender);

        pos += offset; /* okay if goes off end */
    }

    if (rerender) {
        currdesk = cur_desk_temp;
        DeskInfo *t = di;
        di = di_temp;
        di_temp = t;
        return RERENDER;
    } else return BORED;
}

void render()
{
    XSetForeground(dis, gc, bg);
    XFillRectangle(dis, pm, gc, 0, 0, scrwidth, HEIGHT);

    for (unsigned int i = 0; i < ndesks; ++i)
    {
        unsigned int start = i * scrwidth/ ndesks;
        unsigned int end = (i + 1) * scrwidth / ndesks;
        unsigned int width = end - start;

        if (i == currdesk || di[i].urgent) {
            XSetForeground(dis, gc, di[i].urgent ? ((i == currdesk) ? urfg : urbg) : fg);
            XFillRectangle(dis, pm, gc, start, 0, width, HEIGHT);
        }


        printf("[%u, %u] ", i, di[i].nwins);
        if (di[i].nwins) {
            //XSetForeground(dis, gc, (i == currdesk) ? bg : fg);

            unsigned int tick_width = width / di[i].nwins / 4;
            tick_width = (tick_width > FATTICK_W) ? FATTICK_W : tick_width;
            unsigned int nticks = di[i].nwins;
            if (!tick_width) {
                tick_width = 1;
                nticks = width / 4;
            }

            for (unsigned int j = 0; j < nticks; ++j) {
                XSetForeground(dis, gc, winbg);
                XFillRectangle(dis, pm, gc, start + tick_width * (2 * j + 1), 0,
                        tick_width,HEIGHT);
                if (tick_width > 2 && HEIGHT > 2) {
                    XSetForeground(dis, gc, winfg);
                    XFillRectangle(dis, pm, gc, start + tick_width * (2 * j + 1) +1, 1,
                            tick_width - 2, HEIGHT - 2);
                }
            }
        } else {
            /* XXX this debugging check shows the status indicator bug is in monster, not with me */
            //XSetForeground(dis, gc, winbg);
            //XFillRectangle(dis, pm, gc, start+FATTICK_W*5, 0, FATTICK_W * 5, HEIGHT);
        }
    }
    printf("\n");
}

void cleanup()
{
    //di not free()'d for solidarity with di_temp
    XFreeGC(dis, gc);
    XFreePixmap(dis, pm);
    XDestroyWindow(dis, w);
    XCloseDisplay(dis);
}

int main()
{
    setup();

    int xfd = ConnectionNumber(dis);
    int nfds = 1 + ((xfd > STDIN_FILENO) ? xfd : STDIN_FILENO);
    while (1) {
        int redraw = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        FD_SET(STDIN_FILENO, &fds);
        select(nfds, &fds, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            if (parse() == RERENDER) {
                render();
                redraw = 1;
            }
        }

        if (FD_ISSET(xfd, &fds)) {
            /* XXX I still (2012-02-28) can drop some exposes
             * test: start xscreensaver, leave it, BAM, mopag not redrawn
             */
            XEvent xev;
            while (XPending(dis)) {
                XNextEvent(dis, &xev);
                printf("EXPOSE\n");
                if (xev.type == Expose)
                    redraw = 1;
                else
                    fprintf(stderr, "weird event of type %u\n", xev.type);
            }
        }

        if (redraw)
            XCopyArea(dis, pm, w, gc, 0, 0, scrwidth, HEIGHT, 0, 0);

        XSync(dis, False);
    }

    cleanup();
}
