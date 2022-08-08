#include <stdio.h>

#include <X11/Xlib.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>//

#include <X11/keysym.h>
#include <X11/extensions/XTest.h> //sudo apt-get install libxtst-dev


static int bits_per_color;
static int fb_bpp, fb_Bpl, fb_depth;
static int windowHeight;
static int windowWidth;
Display *dpy;
Window root;
XImage *xi;

static rfbScreenInfoPtr screen;


static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl);
static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl);

static int guess_bits_per_color(int bits_per_pixel)
{
    int bits_per_color;

    /* first guess, spread them "evenly" over R, G, and B */
    bits_per_color = bits_per_pixel / 3;
    if (bits_per_color < 1)
    {
        bits_per_color = 1;
    }

    /* choose safe values for usual cases: */
    if (bits_per_pixel == 8)
    {
        bits_per_color = 2;
    }
    else if (bits_per_pixel == 15 || bits_per_pixel == 16)
    {
        bits_per_color = 5;
    }
    else if (bits_per_pixel == 24 || bits_per_pixel == 32)
    {
        bits_per_color = 8;
    }
    return bits_per_color;
}

static void init_fb(void)
{
    printf("Initializing framebuffer...\n");

    dpy = XOpenDisplay(NULL);
    root = DefaultRootWindow(dpy);

    windowHeight = XDisplayHeight(dpy, 0);
    windowWidth = XDisplayWidth(dpy, 0);

    // Get dump of screen
    xi = XGetImage(dpy, root, 0, 0, windowWidth, windowHeight, AllPlanes, ZPixmap);

    fb_bpp = (int)xi->bits_per_pixel;
    fb_Bpl = (int)xi->bytes_per_line;
    fb_depth = (int)xi->depth;
    bits_per_color = guess_bits_per_color(fb_bpp);
}

/*****************************************************************************/

static int init_fb_server(int argc, char **argv)
{
    printf("Initializing server...\n");

    screen = rfbGetScreen(&argc, argv, xi->width, xi->height, bits_per_color, 1, fb_bpp / 8);

    if (!screen)
        return 1;
    screen->frameBuffer = xi->data;
    if (!screen->frameBuffer)
        return 1;

    ////
    screen->serverFormat.bitsPerPixel = fb_bpp;
    screen->serverFormat.depth = fb_depth;
    screen->serverFormat.trueColour = TRUE;

    /*
    default
    true colour: max r 31 g 31 b 31, shift r 0 g 5 b 10
    change
    true colour: max r 31 g 63 b 31, shift r 11 g 5 b 0
    */
    screen->serverFormat.redShift = 11;
    screen->serverFormat.greenShift = 5;
    screen->serverFormat.blueShift = 0;
    screen->serverFormat.redMax = 31;
    screen->serverFormat.greenMax = 63;
    screen->serverFormat.blueMax = 31;
    ////

    //
    screen->kbdAddEvent = keyevent;
    screen->ptrAddEvent = ptrevent;
    //
    rfbInitServer(screen);
    return 0;
}

/*****************************************************************************/


static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl){
    
    uint32_t keycode;
    keycode = XKeysymToKeycode(dpy, key);
    printf("key: %x code: %x down: %x\n",key,keycode, down);
    printf("mod: %d ", IsModifierKey(key));
    XTestFakeKeyEvent(dpy, keycode, down, CurrentTime);

}

/*****************************************************************************/

static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl)
{
    //printf("buttonMask: %x\n",buttonMask);
    //XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);

    if (buttonMask != 0){
        printf("buttonMask Press: %x\n", buttonMask);
        XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
        XTestFakeButtonEvent(dpy, buttonMask, True, CurrentTime);
        XTestFakeButtonEvent(dpy, buttonMask, False, CurrentTime);
        //press = buttonMask;
    }

}

/*****************************************************************************/

static void check_update_screen(rfbScreenInfoPtr screen)
{
    xi = XGetImage(dpy, root, 0, 0, windowWidth, windowHeight, AllPlanes, ZPixmap);
    size_t size = windowWidth * windowHeight * fb_bpp / 8;
    if (memcmp(xi->data, screen->frameBuffer, size) == 0)
    {
        free(xi->data);
    }
    else
    {
        free(screen->frameBuffer);
        screen->frameBuffer = xi->data;
        rfbMarkRectAsModified(screen, 0, 0, windowWidth, windowHeight);
    }
    }

    int main(int argc, char **argv)
    {

        init_fb();
        if (init_fb_server(argc, argv) == 1)
        {
            return 1;
        }

        while (rfbIsActive(screen))
        {
            while (screen->clientHead == NULL)
                rfbProcessEvents(screen, 100000);

            rfbProcessEvents(screen, 100000);
            check_update_screen(screen);
        }

        printf("Cleaning up...\n");
        free(screen->frameBuffer);
        rfbScreenCleanup(screen);

        return 0;
    }