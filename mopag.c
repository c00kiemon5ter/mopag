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


//XXX DEBUG
#include <sys/time.h>

#define HEIGHT 4
#define POS (HEIGHT)
#define FATTICK_W 6
#define BG "#004020"
#define FG "#90f0c0"
#define URFG "#f00000"
#define URBG "#900000"
#define WINFG "#ffffff"
#define WINBG "#000000"

typedef struct {
    unsigned int nwins;
    unsigned int mode;
    unsigned int urgent;
} DeskInfo;

enum { BORED, ERROR, RERENDER };

static void setup();
static int parse();
static void render();
static void render2();
static void cleanup();

static Display *dis;
static int screen;
static Window root, w;
static Pixmap p;
static GC gc;
static long fg, bg, urfg, urbg, winfg, winbg;
static DeskInfo *di = NULL;
static unsigned int cur_desk;
static unsigned int ndesks = 0;

void setup()
{
    assert((dis = XOpenDisplay(NULL)));
    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    long *col_vars[] = { &fg, &bg, &urfg, &urbg, &winfg, &winbg, NULL };
    const char *col_strs[] = { FG, BG, URFG, URBG, WINFG, WINBG, NULL };
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
            0, (POS >= 0) ? POS : DisplayHeight(dis, screen) + POS,
            DisplayWidth(dis, screen), HEIGHT,
            1, CopyFromParent,
            InputOutput, CopyFromParent,
            CWBackPixel | CWOverrideRedirect | CWEventMask, &wa);
    XMapWindow(dis, w);
    XSetWindowBorderWidth(dis, w, 0);

    XGCValues gcv;
    gcv.graphics_exposures = 0; //otherwise get NoExpose on XCopyArea
    gc = XCreateGC(dis, root, GCGraphicsExposures, &gcv);

    p = XCreatePixmap(dis, w,
            DisplayWidth(dis, screen), HEIGHT, DefaultDepth(dis,screen));
}

int parse()
{
    static DeskInfo *di_temp;
    static unsigned int cur_desk_temp;
    //version one: gets confused by multiple lines, which are consumed by
    //stdio and make select not work as desired.
  //char buf[1024];
  //if (fgets(buf, sizeof(buf), stdin) == NULL) {
  //    fprintf(stderr, "received no characters, exiting..\n");
  //    exit(1);
  //}

    //version two: seems to work fine: circumvents buffering issue mentioned above.
    char buf2[4096];
    ssize_t ret = read(STDIN_FILENO, buf2, sizeof(buf2));
    assert(buf2[ret - 1] == '\n');
    assert(ret > 2); //XXX I had this fail once! (after 1 week of use?)
    unsigned int pos2 = ret - 2;
    while (pos2 > 0 && buf2[pos2] != '\n')
        pos2 -= 1;
    printf("pos2 %u\n", pos2);
    char *buf = buf2 + pos2;

    //version three: suppose feof works properly?
    //XXX nopes doesn't do what I want because eof only happens on close, not on
    //availability
  //if (feof(stdin)) {
  //    fprintf(stderr, "received no characters, exiting..\n");
  //    exit(1);
  //}
  //char buf[1024];
  //while (!feof(stdin))
  //    assert(fgets(buf, sizeof(buf), stdin) != NULL);


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
        if (res < 5 || d != i) { //%n doesn't count
            fprintf(stderr, "Ignoring malformed input.\n");
            return ERROR;
        }

        if (is_cur)
            cur_desk_temp = i;

        if (!rerender &&
                (di_temp[i].nwins != di[i].nwins
                 || di_temp[i].mode != di[i].mode
                 || di_temp[i].urgent != di[i].urgent
                 || (is_cur != (cur_desk == i)) ) )
            rerender = 1;
        printf("(re %u) ", rerender);

        pos += offset; //okay if goes off end.
    }
    //XXX DEBUG
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("%lu.%lu", tv.tv_sec, tv.tv_usec);
    printf("\n");

    if (rerender) {
        cur_desk = cur_desk_temp;
        DeskInfo *t = di;
        di = di_temp;
        di_temp = t;
        return RERENDER;
    } else
        return BORED;

    return rerender ? RERENDER : BORED;
}

void render2()
{
    XSetForeground(dis, gc, bg);
    XFillRectangle(dis, p, gc, 0, 0, DisplayWidth(dis, screen), HEIGHT);

    for (unsigned int i = 0; i < ndesks; ++i)
    {
        unsigned int start = i * DisplayWidth(dis, screen) / ndesks;
        unsigned int end = (i + 1) * DisplayWidth(dis, screen) / ndesks;
        unsigned int width = end - start;

        if (i == cur_desk || di[i].urgent) {
            XSetForeground(dis, gc, di[i].urgent ? ((i == cur_desk) ? urfg : urbg) : fg);
            XFillRectangle(dis, p, gc, start, 0, width, HEIGHT);
        }


        printf("[%u, %u] ", i, di[i].nwins);
        if (di[i].nwins) {
            //XSetForeground(dis, gc, (i == cur_desk) ? bg : fg);

            unsigned int tick_width = width / di[i].nwins / 4;
            tick_width = (tick_width > FATTICK_W) ? FATTICK_W : tick_width;
            unsigned int nticks = di[i].nwins;
            if (!tick_width) {
                tick_width = 1;
                nticks = width / 4;
            }

            for (unsigned int j = 0; j < nticks; ++j) {
                XSetForeground(dis, gc, winbg);
                XFillRectangle(dis, p, gc, start + tick_width * (2 * j + 1), 0,
                        tick_width,HEIGHT);
                if (tick_width > 2 && HEIGHT > 2) {
                    XSetForeground(dis, gc, winfg);
                    XFillRectangle(dis, p, gc, start + tick_width * (2 * j + 1) +1, 1,
                            tick_width - 2, HEIGHT - 2);
                }
            }
        } else {
            //XXX this debugging check shows the status indicator bug is in monster,
            //not with me..
          //XSetForeground(dis, gc, winbg);
          //XFillRectangle(dis, p, gc, start+FATTICK_W*5, 0, FATTICK_W * 5, HEIGHT);
        }

    }
    printf("\n");
}

void render()
{
    XSetForeground(dis, gc, bg);
    XFillRectangle(dis, p, gc, 0, 0, DisplayWidth(dis, screen), HEIGHT);

    for (unsigned int i = 0; i < ndesks; ++i)
    {
        unsigned int start = i * DisplayWidth(dis, screen) / ndesks;
        unsigned int end = (i + 1) * DisplayWidth(dis, screen) / ndesks;
        unsigned int width = end - start;

        if (i == cur_desk || di[i].urgent) {
            XSetForeground(dis, gc, di[i].urgent ? ((i == cur_desk) ? urfg : urbg) : fg);
            XFillRectangle(dis, p, gc, start, 0, width, HEIGHT);
        }

        XSetForeground(dis, gc, fg);
        XFillRectangle(dis, p, gc, start - FATTICK_W / 2, 0, FATTICK_W, HEIGHT);
        if (i == ndesks - 1)
            XFillRectangle(dis, p, gc, end - FATTICK_W / 2, 0, FATTICK_W, HEIGHT);

        if (i == cur_desk)
            XSetForeground(dis, gc, bg);

        unsigned int nticks = (di[i].nwins > width / 3) ? width / 3 : di[i].nwins;
        for (unsigned int j = 0; j + 1 < nticks; ++j) {
            XDrawLine(dis, p, gc, start + width * (j + 1) / nticks, 0,
                    start + width * (j + 1) / nticks, HEIGHT);
        }
    }
}

void cleanup()
{
    //di not free()'d for solidarity with di_temp
    XFreeGC(dis, gc);
    XFreePixmap(dis, p);
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
                //render();
                render2();
                redraw = 1;
            }
        }

        if (FD_ISSET(xfd, &fds)) {
            //XXX I still (2012-02-28) can drop some exposes!! for instance:
            //start xscreensaver, leave it, BAM, mopag not redrawn
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
            XCopyArea(dis, p, w, gc, 0, 0, DisplayWidth(dis, screen), HEIGHT, 0, 0);

        //XFlush(dis);
        XSync(dis, False);
    }

    cleanup();
}
