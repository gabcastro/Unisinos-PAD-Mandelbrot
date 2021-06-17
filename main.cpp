#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <vector>

using namespace std;

#define X_RESN 800          /* x resolution */
#define Y_RESN 800          /* y resolution */
#define WORKER_SPACE 80     /* length of the square to compute maldelbrot calc */
#define NUM_THREADS 6       /* producer threads */

/* =================================== */
/* =========== GLOBAL VARS =========== */
/* =================================== */

pthread_mutex_t pmutex_wBuffer, pmutex_rBuffer;
pthread_cond_t pcond_compute_mandelbrot, pcond_plot_value;

int count_pop_buffer = 0;
bool goForward = true;

typedef struct complextype
{
    float real, imag;
} Compl;

/* where each task will be execute, mandelbrot calc. */
typedef struct {
  int c_init;
  int r_init;
  int c_term;
  int r_term;
} quadrant;

vector<quadrant> w_buffer;

typedef struct {
    int row;
    int column;
} map_point;

vector<map_point> r_buffer;

/* Mandlebrot variables */

int i, j, k;
Compl z, c;
float lengthsq, temp;

/* Xlib */

Window win;                        
Display *display;
GC gc;

/* =============================== */
/* =========== METHODS =========== */
/* =============================== */

unsigned long _RGB(int r,int g, int b);
void calculate_maldelbrot(quadrant *w_area_selected);
void worker_quadrants();
void *producer(void *str);
void create_threads();
void *consumer(void *str);
void draw_point(map_point *point);


int main()
{
    unsigned int width, height,        /* window size */
        x, y,                          /* window position */
        border_width,                  /*border width in pixels */
        display_width, display_height, /* size of screen */
        screen;                        /* which screen */

    char *window_name = "Mandelbrot Set", *display_name = NULL;
    unsigned long valuemask = 0;
    XGCValues values;
    XSizeHints size_hints;
    Pixmap bitmap;
    XPoint points[800];
    FILE *fp, *fopen();
    char str[100];

    XSetWindowAttributes attr[1];

    /* connect to Xserver */

    if ((display = XOpenDisplay(display_name)) == NULL)
    {
        fprintf(stderr, "drawon: cannot connect to X server %s\n",
                XDisplayName(display_name));
        exit(-1);
    }

    /* get screen size */

    screen = DefaultScreen(display);
    display_width = DisplayWidth(display, screen);
    display_height = DisplayHeight(display, screen);

    /* set window size */

    width = X_RESN;
    height = Y_RESN;

    /* set window position */

    x = 0;
    y = 0;

    /* create opaque window */

    border_width = 4;
    win = XCreateSimpleWindow(display, RootWindow(display, screen),
                              x, y, width, height, border_width,
                              BlackPixel(display, screen), _RGB(150,220,230));

    size_hints.flags = USPosition | USSize;
    size_hints.x = x;
    size_hints.y = y;
    size_hints.width = width;
    size_hints.height = height;
    size_hints.min_width = 300;
    size_hints.min_height = 300;

    XSetNormalHints(display, win, &size_hints);
    XStoreName(display, win, window_name);

    /* create graphics context */

    gc = XCreateGC(display, win, valuemask, &values);

    XSetBackground(display, gc, BlackPixel(display, screen));
    XSetForeground(display, gc, BlackPixel(display, screen));
    XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);

    attr[0].backing_store = Always;
    attr[0].backing_planes = 1;
    attr[0].backing_pixel = BlackPixel(display, screen);

    XChangeWindowAttributes(display, win, CWBackingStore | CWBackingPlanes | CWBackingPixel, attr);

    XMapWindow(display, win);
    XSync(display, 0);

    worker_quadrants();
    create_threads();

    cout << "tamanho do buffer com as coordenadas para plotar: " << r_buffer.size() << endl;

    XFlush(display);
    sleep(20);

    /* Program Finished */
}

unsigned long _RGB(int r,int g, int b)
{
    return b + (g<<8) + (r<<16);
}

void calculate_maldelbrot(quadrant *w_area_selected) {
    
    for (int row = w_area_selected->r_init; row <= w_area_selected->r_term; row++)
        for (int col = w_area_selected->c_init; col <= w_area_selected->c_term; col++) {
            
            z.real = z.imag = 0.0;
            c.real = ((float)col - 400.0) / 200.0; /* scale factors for 800 x 800 window */
            c.imag = ((float)row - 400.0) / 200.0;
            k = 0;

            do
            { /* iterate for pixel color */

                temp = z.real * z.real - z.imag * z.imag + c.real;
                z.imag = 2.0 * z.real * z.imag + c.imag;
                z.real = temp;
                lengthsq = z.real * z.real + z.imag * z.imag;
                k++;

            } while (lengthsq < 4.0 && k < 20);

            if (k == 20) {
                map_point plotPoint;
                plotPoint.column = col;
                plotPoint.row = row;

                r_buffer.push_back(plotPoint);
            }
        }
}


/* place where the task will be happen 
   like each 80x80 square in a window of 800x800, 
   will result in 10x10 tasks = 100 tasks
*/
void worker_quadrants() {    
    
    int total_quadrants = X_RESN / WORKER_SPACE;
    int total_tasks;

    int c_init, c_term, r_init, r_term;

    /* 
        get the area of all squares 
        columns per line
    */
    for (int r = 0; r < total_quadrants; r++)
        for (int c = 0; c < total_quadrants; c++) {
            c_init = c * WORKER_SPACE;
            c_term = ((c + 1) * WORKER_SPACE) - 1;

            r_init = r * WORKER_SPACE;
            r_term = ((r + 1) * WORKER_SPACE) - 1;

            quadrant w_area;
            w_area.c_init = c_init;
            w_area.c_term = c_term;
            w_area.r_init = r_init;
            w_area.r_term = r_term;

            w_buffer.push_back(w_area);
        }
}

void *producer(void *str) {
    while (1) {
        pthread_mutex_lock(&pmutex_wBuffer);

        if (w_buffer.size() == 0) {
            pthread_mutex_unlock(&pmutex_wBuffer);
            break;
        }
        
        cout << "consumindo valor do vetor de posicoes " << endl;

        quadrant w_area_selected = w_buffer.at(0);
        w_buffer.erase(w_buffer.begin(), w_buffer.begin()+1);

        pthread_mutex_unlock(&pmutex_wBuffer);

        pthread_mutex_lock(&pmutex_rBuffer);

        while (!goForward)
            pthread_cond_wait(&pcond_compute_mandelbrot, &pmutex_rBuffer);

        goForward = false;
        cout << "realiza o calculo mandelbrot" << endl;
        calculate_maldelbrot(&w_area_selected);
        goForward = true;  

        pthread_mutex_unlock(&pmutex_rBuffer);

        // sleep(3); // good to watch the prints on terminal

        pthread_cond_signal(&pcond_compute_mandelbrot);
        pthread_cond_signal(&pcond_plot_value);
    }
}

void *consumer(void *str) {
    while (1) {
        pthread_mutex_lock(&pmutex_rBuffer);

        while (r_buffer.size() == 0) 
            pthread_cond_wait(&pcond_plot_value, &pmutex_rBuffer);
        
        count_pop_buffer++;
        cout << count_pop_buffer << " - delete data from buffer result " << endl;

        map_point m_point = r_buffer.at(0);
        r_buffer.erase(r_buffer.begin(), r_buffer.begin()+1);

        draw_point(&m_point);

        pthread_mutex_unlock(&pmutex_rBuffer);

        pthread_cond_signal(&pcond_compute_mandelbrot);
    }
}

void create_threads() {

    pthread_mutex_init(&pmutex_wBuffer, NULL);
    pthread_mutex_init(&pmutex_rBuffer, NULL);

    pthread_cond_init(&pcond_compute_mandelbrot, NULL);
    pthread_cond_init(&pcond_plot_value, NULL);

    pthread_t producer_threads[NUM_THREADS];
    pthread_t consumer_thread;

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&producer_threads[i], NULL, producer, NULL);
    }
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    pthread_join(consumer_thread, NULL);
}

void draw_point(map_point *point) {
    XDrawPoint(display, win, gc, point->column, point->row);
}