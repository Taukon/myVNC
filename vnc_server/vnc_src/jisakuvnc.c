#include <stdio.h>

#include <X11/Xlib.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

Display *dpy;
Window root;
static rfbScreenInfoPtr rfbScreen;

static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl);
static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl);

static void check_update_rfbScreen_depth16(rfbScreenInfoPtr rfbScreen);
static void check_update_rfbScreen_depth24(rfbScreenInfoPtr rfbScreen);

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
/*****************************************************************************/
static void set_xcursor(rfbScreenInfoPtr rfbScreen)
{
    int width = 13, height = 22;
    char cursor[] =
        " xx          "
        " x x         "
        " x  x        "
        " x   x       "
        " x    x      "
        " x     x     "
        " x      x    "
        " x       x   "
        " x     xx x  "
        " x x   x xxx "
        " x xx  x   x "
        " xx x   x    "
        " xx  x  x    "
        " x    x  x   "
        " x    x  x   "
        "       x  x  "
        "        x  x "
        "        x  x "
        "         xx  "
        "             "
        "             ";
    char mask[] =
        "xxx          "
        "xxxx         "
        "xxxxx        "
        "xxxxxx       "
        "xxxxxxx      "
        "xxxxxxxx     "
        "xxxxxxxxx    "
        "xxxxxxxxxx   "
        "xxxxxxxxxxx  "
        "xxxxxxxxxxxx "
        "xxxxxxxxxxxxx"
        "xxxxxxxxxxxxx"
        "xxxxxxxxxx  x"
        "xxxxxxxxxx   "
        "xxx  xxxxxx  "
        "xxx  xxxxxx  "
        "xx    xxxxxx "
        "       xxxxx "
        "       xxxxxx"
        "        xxxxx"
        "         xxx "
        "             ";

    rfbCursorPtr c;

    c = rfbMakeXCursor(width, height, cursor, mask);
    c->xhot = 0;
    c->yhot = 0;

    rfbSetCursor(rfbScreen, c);
}

/*****************************************************************************/

static int init_fb_server(int argc, char **argv)
{
    printf("Initializing framebuffer...\n");

    dpy = XOpenDisplay(NULL);
    root = DefaultRootWindow(dpy);

    int windowHeight = XDisplayHeight(dpy, 0);
    int windowWidth = XDisplayWidth(dpy, 0);

    // Get dump of rfbScreen
    XImage *xi = XGetImage(dpy, root, 0, 0, windowWidth, windowHeight, AllPlanes, ZPixmap);

    int bits_per_color = guess_bits_per_color(xi->bits_per_pixel);

    printf("bits_per_pixel: %d\n", (int)xi->bits_per_pixel);
    printf("bytes_per_line: %d\n", (int)xi->bytes_per_line);
    printf("depth: %d\n", (int)xi->depth);
    printf("bits_per_color: %d\n", bits_per_color);

    printf("Initializing server...\n");

    rfbScreen = rfbGetScreen(&argc, argv, xi->width, xi->height, bits_per_color, 1, xi->bits_per_pixel / 8);

    if (!rfbScreen)
        return 1;

    ////
    rfbScreen->serverFormat.bitsPerPixel = xi->bits_per_pixel;
    rfbScreen->serverFormat.depth = xi->depth;
    rfbScreen->serverFormat.trueColour = TRUE;
    printf("height: %d, width: %d\n", rfbScreen->height, rfbScreen->width);

    rfbScreen->desktopName = "My VNC";
    rfbScreen->kbdAddEvent = keyevent;
    rfbScreen->ptrAddEvent = ptrevent;

    set_xcursor(rfbScreen);

    if (rfbScreen->serverFormat.depth == 16)
    {
        rfbScreen->frameBuffer = xi->data;

        /*
        default
        true colour: max r 31 g 31 b 31, shift r 0 g 5 b 10
        change
        true colour: max r 31 g 63 b 31, shift r 11 g 5 b 0
        */

        rfbScreen->serverFormat.redShift = 11;
        rfbScreen->serverFormat.greenShift = 5;
        rfbScreen->serverFormat.blueShift = 0;
        rfbScreen->serverFormat.redMax = 31;
        rfbScreen->serverFormat.greenMax = 63;
        rfbScreen->serverFormat.blueMax = 31;
    }
    else if (rfbScreen->serverFormat.depth == 24)
    {
        int bpp = rfbScreen->serverFormat.bitsPerPixel / 8;
        char *buffer;
        buffer = (char *)malloc(sizeof(char) * bpp * xi->width * xi->height);

        for (int y = 0; y < xi->height; y++)
        {
            for (int x = 0; x < xi->width; x++)
            {
                buffer[(y * xi->width + x) * bpp + 0] = xi->data[(y * xi->width + x) * bpp + 0] & 0xff;
                buffer[(y * xi->width + x) * bpp + 1] = xi->data[(y * xi->width + x) * bpp + 1] & 0xff;
                buffer[(y * xi->width + x) * bpp + 2] = xi->data[(y * xi->width + x) * bpp + 2] & 0xff;
            }
        }
        rfbScreen->frameBuffer = buffer;

        rfbScreen->serverFormat.redShift = 16;
        rfbScreen->serverFormat.greenShift = 8;
        rfbScreen->serverFormat.blueShift = 0;
        rfbScreen->serverFormat.redMax = 255;
        rfbScreen->serverFormat.greenMax = 255;
        rfbScreen->serverFormat.blueMax = 255;

        XDestroyImage(xi);
    }
    else
    {
        return 1;
    }

    if (!rfbScreen->frameBuffer)
        return 1;

    rfbInitServer(rfbScreen);

    return 0;
}

/*****************************************************************************/

static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{

    uint32_t keycode;
    keycode = XKeysymToKeycode(dpy, key);

    if (keycode == 0)
    {
        printf("error ");
    }
    else
    {
        XTestFakeKeyEvent(dpy, keycode, down, CurrentTime);
    }

    printf("mod: %d ", IsModifierKey(key));
    printf("key: %x code: %x down: %x\n", key, keycode, down);
}

/*****************************************************************************/

static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl)
{
    // if(buttonMask != 0)
    XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);

    if (buttonMask == 0x10)
    {
        printf("scroll down ");
        // buttonMask: 0x10 -> 0x5
        buttonMask = 0x5;
    }
    else if (buttonMask == 0x8)
    {
        printf("scroll up ");
        // buttonMask: 0x8 -> 0x4
        buttonMask = 0x4;
    }
    else if (buttonMask == 0x4)
    {
        printf("left click ");
        // buttonMask: 0x4 -> 0x2 0x3 ?
        buttonMask = 0x3;
    }
    else if (buttonMask == 0x2)
    {
        printf("wheel click ");
        // buttonMask: 0x2 -> ?
        // buttonMask = 0xa;
    }

    if (buttonMask != 0)
    {
        // XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
        XTestFakeButtonEvent(dpy, buttonMask, True, CurrentTime);
        XTestFakeButtonEvent(dpy, buttonMask, False, CurrentTime);
        printf("buttonMask: 0x%x\n", buttonMask);
    }
}

/****************************************************************************************/

static void check_update_rfbScreen_depth16(rfbScreenInfoPtr rfbScreen)
{
    XImage *xi = XGetImage(dpy, root, 0, 0, rfbScreen->width, rfbScreen->height, AllPlanes, ZPixmap);
    size_t size = rfbScreen->width * rfbScreen->height * rfbScreen->serverFormat.bitsPerPixel / 8;

    if (memcmp(xi->data, rfbScreen->frameBuffer, size) == 0)
    {
        free(xi->data);
        // printf("1\n");
    }
    else
    {
        free(rfbScreen->frameBuffer);
        rfbScreen->frameBuffer = xi->data;
        rfbMarkRectAsModified(rfbScreen, 0, 0, rfbScreen->width, rfbScreen->height);
        // printf("2 : %ld\n", size);
    }
}

static void check_update_rfbScreen_depth24(rfbScreenInfoPtr rfbScreen)
{
    XImage *xi = XGetImage(dpy, root, 0, 0, rfbScreen->width, rfbScreen->height, AllPlanes, ZPixmap);

    int bpp = rfbScreen->serverFormat.bitsPerPixel / 8;
    size_t size = rfbScreen->width * rfbScreen->height * bpp;
    // size_t size = rfbScreen->width * rfbScreen->height * xi->depth / 8;
    char *buffer;
    buffer = (char *)malloc(sizeof(char) * bpp * xi->width * xi->height);

    for (int y = 0; y < xi->height; y++)
    {
        for (int x = 0; x < xi->width; x++)
        {
            buffer[(y * xi->width + x) * bpp + 0] = xi->data[(y * xi->width + x) * bpp + 0] & 0xff;
            buffer[(y * xi->width + x) * bpp + 1] = xi->data[(y * xi->width + x) * bpp + 1] & 0xff;
            buffer[(y * xi->width + x) * bpp + 2] = xi->data[(y * xi->width + x) * bpp + 2] & 0xff;
        }
    }

    if (memcmp(buffer, rfbScreen->frameBuffer, size) == 0)
    {
        // free(xi->data);
        // xi->data = NULL;
        free(buffer);
        buffer = NULL;
        // printf("1\n");
    }
    else
    {
        free(rfbScreen->frameBuffer);
        rfbScreen->frameBuffer = buffer;
        rfbMarkRectAsModified(rfbScreen, 0, 0, rfbScreen->width, rfbScreen->height);
        // printf("2 : %ld\n", size);
    }

    XDestroyImage(xi);
}

/*****************************************************************************/

int main(int argc, char **argv)
{
    if (init_fb_server(argc, argv) == 1)
    {
        return 1;
    }

    if (rfbScreen->serverFormat.depth == 16)
    {
        while (rfbIsActive(rfbScreen))
        {
            while (rfbScreen->clientHead == NULL)
                rfbProcessEvents(rfbScreen, 100000);

            rfbProcessEvents(rfbScreen, 100000);
            check_update_rfbScreen_depth16(rfbScreen);
        }
    }
    else if (rfbScreen->serverFormat.depth == 24)
    {
        while (rfbIsActive(rfbScreen))
        {
            while (rfbScreen->clientHead == NULL)
                rfbProcessEvents(rfbScreen, 100000);

            rfbProcessEvents(rfbScreen, 100000);
            check_update_rfbScreen_depth24(rfbScreen);
        }
    }

    printf("Cleaning up...\n");
    free(rfbScreen->frameBuffer);
    rfbScreenCleanup(rfbScreen);

    return 0;
}