#ifndef IMAGE565_H
#define IMAGE565_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <math.h>
#include <Constant.h>
#include <httputil.h>
#include "ImagenGestor.h"
#include "image/uiimgdownloader.h"

#define RGBBUFFPIXEL 30

static const string OPENCYCLEMAP  = "http://b.tile.opencyclemap.org/cycle/";
static const string OPENSTREETMAP = "http://a.tile.openstreetmap.org/";

struct t_rle{
    uint16_t color;
    uint8_t  r, g, b;
    uint32_t n;
};

typedef enum {
    BMP,
    IM565,
    IM565RLE
}BMPTypes;

class t_mapSurface{

    public:
        t_mapSurface(){
            goodBmp = false;
            flip    = true;
            pos = 0;
            lastVLine = -1;
            bmpType = 0;
        }
        ~t_mapSurface(){};

        FILE *bmpFile;
        SDL_Surface *tmpSurface;
        int      bmpWidth, bmpHeight;   // W+H in pixels
        uint8_t  bmpDepth;              // Bit depth (currently must be 24)
        uint32_t bmpImageoffset;        // Start of image data in file
        uint32_t rowSize;               // Not always = bmpWidth; may have padding
        uint8_t  buffidx; // Current position in sdbuffer
        uint8_t  buffidxRle; // Current position in sdbuffer
        bool  goodBmp;       // Set to true on valid header parse
        bool  flip;        // BMP is stored bottom-to-top
        int      w, h, row, col;
        uint32_t pos;
        int lastVLine;
        int bmpType;
};

class Image565
{


    public:
        Image565();
        virtual ~Image565();

        SDL_Surface *screen;
        void downloadMap(string url, string diroutput);
        bool tileLoad(string imgLocation, t_mapSurface *surface);
        void tileDraw(t_mapSurface *surface, int x, int y, int offsetX, int offsetY);

    protected:
        unsigned long surfaceTo565(SDL_Surface *mySurface, string filename, bool rleFlag);
        int rleFileToScreen(string filename, int x, int y, int lcdw, int lcdh);
        unsigned long convertTo565Rle(string filename);
        int toScreen565(string filename, int x, int y, int lcdw, int lcdh);
        unsigned long convertTo565(string filename);
        void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
        Uint32 getpixel(SDL_Surface *surface, const int x, const int y);
        uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
        string existeFichero(string url, string diroutput);
        void bmpdraw(t_mapSurface *surface, int x, int y, int offsetX, int offsetY);
        bool cargarBmp(string imgLocation, t_mapSurface *surface);


        uint8_t read8(FILE *f);
        uint32_t read32(FILE *f);
        uint16_t read16(FILE *f);




    private:


};

#endif // IMAGE565_H
