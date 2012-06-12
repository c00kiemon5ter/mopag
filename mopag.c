/**
 * Matus Telgarsky <chisel@gmail.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/select.h>
#include <X11/Xlib.h>

#define TOP True
#define HEIGHT 4
#define FATTICK_W 6
#define BG "#404040"
#define FG "#40a0d0"
#define URFG "#f00000"
#define URBG "#900000"

typedef struct {
    unsigned int nwins;
    unsigned int mode;
    unsigned int urgent;
} DeskInfo;

enum { BORED, ERROR, RERENDER };

static void setup();
static int parse();
static void render();
static void cleanup();

static Display *dis;
static int screen, swidth, sheight;
static Window root, w;
static Pixmap p;
static GC gc;
static long fg, bg, urfg, urbg;
static DeskInfo *di = NULL;
static unsigned int cur_desk;
static unsigned int ndesks = 0;

void setup() {
    assert((dis = XOpenDisplay(NULL)));
    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);
    swidth  = DisplayWidth(dis, screen);
    sheight = DisplayHeight(dis, screen);

    long *col_vars[] = { &fg, &bg, &urfg, &urbg, NULL };
    const char *col_strs[] = { FG, BG, URFG, URBG, NULL };
    XColor c;

    for (unsigned int i = 0; col_vars[i]; ++i) {
        assert(XAllocNamedColor(dis, DefaultColormap(dis, screen), col_strs[i], &c, &c));
        *col_vars[i] = c.pixel;
    }

    XSetWindowAttributes wa;
    wa.background_pixel = bg;
    wa.override_redirect = 1;
    wa.event_mask = ExposureMask;
    w = XCreateWindow(dis, root,
            0, TOP ? 0 : sheight - HEIGHT, swidth, HEIGHT,
            1, CopyFromParent, InputOutput, CopyFromParent,
            CWBackPixel | CWOverrideRedirect | CWEventMask, &wa);

    XMapWindow(dis, w);
    XSetWindowBorderWidth(dis, w, 0);

    XGCValues gcv;
    gcv.graphics_exposures = 0; //otherwise get NoExpose on XCopyArea
    gc = XCreateGC(dis, root, GCGraphicsExposures, &gcv);

    p = XCreatePixmap(dis, w, swidth, HEIGHT, DefaultDepth(dis,screen));
}

int parse() {
    static DeskInfo *di_temp;
    static unsigned int cur_desk_temp;
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        fprintf(stderr, "received no characters, exiting..\n");
        exit(1);
    }

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
    for (unsigned int i = 0; i < ndesks; ++i) {
        unsigned int is_cur, d;
        int offset;
        int res = sscanf(pos, "%u:%u:%u:%u:%u%n", &d, &di_temp[i].nwins,
                &di_temp[i].mode, &is_cur, &di_temp[i].urgent, &offset);

        if (res < 5 || d != i) { //%n doesn't count
            fprintf(stderr, "Ignoring malformed input.\n");
            return ERROR;
        }

        if (is_cur) cur_desk_temp = i;

        if (!rerender && (di_temp[i].nwins != di[i].nwins
                       || di_temp[i].mode != di[i].mode
                       || di_temp[i].urgent != di[i].urgent
                       || (is_cur != (cur_desk == i))))
            rerender = 1;

        pos += offset; //okay if goes off end.
    }

    if (rerender) {
        cur_desk = cur_desk_temp;
        DeskInfo *t = di;
        di = di_temp;
        di_temp = t;
        return RERENDER;
    } else return BORED;

    return rerender ? RERENDER : BORED;
}

void render() {
    XSetForeground(dis, gc, bg);
    XFillRectangle(dis, p, gc, 0, 0, swidth, HEIGHT);

    for (unsigned int i = 0; i < ndesks; ++i) {
        unsigned int start = i * swidth / ndesks;
        unsigned int end = (i + 1) * swidth / ndesks;
        unsigned int width = end - start;

        if (i == cur_desk || di[i].urgent) {
            XSetForeground(dis, gc, di[i].urgent ? ((i == cur_desk) ? urfg : urbg) : fg);
            XFillRectangle(dis, p, gc, start, 0, width, HEIGHT);
        }

        XSetForeground(dis, gc, fg);
        XFillRectangle(dis, p, gc, start - FATTICK_W / 2, 0, FATTICK_W, HEIGHT);

        if (i == ndesks - 1)
            XFillRectangle(dis, p, gc, end - FATTICK_W / 2, 0, FATTICK_W, HEIGHT);

        if (i == cur_desk) XSetForeground(dis, gc, bg);

        unsigned int nticks = (di[i].nwins > width / 3) ? width / 3 : di[i].nwins;
        for (unsigned int j = 0; j + 1 < nticks; ++j) {
            XDrawLine(dis, p, gc, start + width * (j + 1) / nticks, 0,
                            start + width * (j + 1) / nticks, HEIGHT);
        }
    }
}

void cleanup() {
    //di not free()'d for solidarity with di_temp
    XFreeGC(dis, gc);
    XFreePixmap(dis, p);
    XDestroyWindow(dis, w);
    XCloseDisplay(dis);
}

int main() {
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
            XEvent xev;
            while (XPending(dis)) {
                XNextEvent(dis, &xev);
                if (xev.type == Expose) redraw = 1;
                else fprintf(stderr, "weird event of type %u\n", xev.type);
            }
        }

        if (redraw) XCopyArea(dis, p, w, gc, 0, 0, swidth, HEIGHT, 0, 0);

        XFlush(dis);
    }
    cleanup();
}

