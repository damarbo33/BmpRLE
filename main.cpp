#include <iostream>
#include "image565.h"

/**
*
*/
int main(int argc, char *argv[]){
    #ifdef WIN
        string appDir = argv[0];
        int pos = appDir.rfind(Constant::getFileSep());
        if (pos == string::npos){
            FILE_SEPARATOR = FILE_SEPARATOR_UNIX;
            pos = appDir.rfind(FILE_SEPARATOR);
            tempFileSep[0] = FILE_SEPARATOR;
        }
        appDir = appDir.substr(0, pos);
        if (appDir[appDir.length()-1] == '.'){
            appDir.substr(0, appDir.rfind(Constant::getFileSep()));
        }
        Constant::setAppDir(appDir);
    #endif // WIN

    #ifdef UNIX
        Dirutil dir;
        Constant::setAppDir(dir.getDirActual());
    #endif // UNIX

    string rutaTraza = appDir + Constant::getFileSep() + "Traza.txt";

    Traza *traza = new Traza(rutaTraza.c_str());
    /* Initialize the SDL library */
    if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        fprintf(stderr,
                "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    /* Clean up on exit */
    atexit(SDL_Quit);

    /*
     * Initialize the display in a 640x480 8-bit palettized mode,
     * requesting a software surface
     */
    Image565 imagen;
    imagen.screen = SDL_SetVideoMode(256, 256, 16, SDL_SWSURFACE);
    if ( imagen.screen == NULL ) {
        fprintf(stderr, "Couldn't set 256x256x16 video mode: %s\n",
                        SDL_GetError());
        exit(1);
    }

    //convertTo565("C:\\Users\\dmarcobo\\Desktop\\Maperitive\\Tiles\\mapnikTiles\\16\\Imagen1.bmp");
    //convertTo565Rle("C:\\Users\\dmarcobo\\Desktop\\Maperitive\\Tiles\\mapnikTiles\\16\\Imagen1.bmp");
    //rleFileToScreen("C:\\Users\\dmarcobo\\Desktop\\Maperitive\\Tiles\\mapnikTiles\\16\\Imagen1.r65", 0, 0, 256, 256);
    //toScreen565("C:\\Users\\dmarcobo\\Desktop\\Maperitive\\Tiles\\mapnikTiles\\16\\Imagen1.565", 0, 0, 256, 256);

    //imagen.downloadMap("http://b.tile.opencyclemap.org/cycle/16/32692/25161.png", "D:\\PruebasMapas\\Tiles565");
    imagen.downloadMap("http://a.tile.openstreetmap.org/16/32692/25161.png", "D:\\PruebasMapas\\Tiles565");


    bool salir = false;
    while(!salir){
        SDL_Event event;
        if(SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN){
                salir = true;
            }
        }

        //rleToScreen("C:\\Users\\dmarcobo\\Desktop\\Maperitive\\Tiles\\mapnikTiles\\16\\25160.bmp_rle.bmp", 0, 0, 128, 160);
        //showMap();
        SDL_UpdateRect(imagen.screen, 0, 0, 0, 0);
    }


    return 0;
}

