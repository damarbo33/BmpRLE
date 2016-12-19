#include "image565.h"

Image565::Image565()
{
    //ctor
}

Image565::~Image565()
{
    //dtor
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.
uint16_t Image565::read16(FILE *f) {
  uint16_t result;
  uint8_t buffer;
  fread(&buffer, 1, 1, f);
  ((uint8_t *)&result)[0] = buffer; // LSB
  fread(&buffer, 1, 1, f);
  ((uint8_t *)&result)[1] = buffer; // MSB
  return result;
}

uint32_t Image565::read32(FILE *f) {
  uint32_t result;
  if (fread(&result, 4, 1, f) != 0)
    return result;
  else
    return 0;
}

uint8_t Image565::read8(FILE *f) {
  uint8_t buffer;
  fread(&buffer, 1, 1, f);
  return buffer;
}


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t Image565::Color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
*
*/
Uint32 Image565::getpixel(SDL_Surface *surface, const int x, const int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        return *p;

    case 2:
        return *(Uint16 *)p;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;

    case 4:
        return *(Uint32 *)p;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}
/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 */
void Image565::putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

/**
* Converts a BMP into a 565 image
*/
unsigned long Image565::convertTo565(string filename){
    int      bmpWidth, bmpHeight;   // W+H in pixels
    uint8_t  bmpDepth;              // Bit depth (currently must be 24)
    uint32_t bmpImageoffset;        // Start of image data in file
    uint32_t rowSize;               // Not always = bmpWidth; may have padding
    uint8_t  sdbuffer[3*RGBBUFFPIXEL]; // pixel buffer (R+G+B per pixel)
    uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
    bool  flip    = true;        // BMP is stored bottom-to-top
    int      w, h, row, col;
    uint8_t  r, g, b;
    long pos = 0;
    int bmpSize = 0;

    FILE *bmpFile, *outBmpFile;
    bmpFile = fopen(filename.c_str(),"rb");

    unsigned int fpos = filename.find_last_of(".");
    string out;
    if (fpos != string::npos){
        out = filename.substr(0, fpos) + ".565";
    } else {
        out = filename + ".565";
    }
    outBmpFile = fopen(out.c_str(),"wb+");

    uint16_t color;
    size_t totalLength = 0;

    if(read16(bmpFile) == 0x4D42) { // BMP signature
        bmpSize = read32(bmpFile);
        cout << "bmpSize: " << bmpSize << endl;
        (void)read32(bmpFile); // Read & ignore creator bytes
        bmpImageoffset = read32(bmpFile); // Start of image data
        cout << "Image Offset: " << bmpImageoffset << endl;
        // Read DIB header
        cout << "Header size: " << read32(bmpFile) << endl;
        bmpWidth  = read32(bmpFile);
        bmpHeight = read32(bmpFile);
        cout << bmpWidth << "x" << bmpHeight << endl;
        int planes = read16(bmpFile);
        cout << "planes: " << planes << endl;
        if(planes == 1) { // # planes -- must be '1'
          bmpDepth = read16(bmpFile); // bits per pixel
          cout << "Bit Depth: " << bmpDepth << endl;
          if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
            rowSize = (bmpWidth * 3 + 3) & ~3;
            // If bmpHeight is negative, image is in top-down order.
            // This is not canon but has been observed in the wild.
            if(bmpHeight < 0) {
              bmpHeight = -bmpHeight;
              flip      = false;
            }
            // Crop area to be loaded
            w = bmpWidth;
            h = bmpHeight;

            for (row=0; row<h; row++) { // For each scanline...

              // Seek to start of scan line.  It might seem labor-
              // intensive to be doing this on every line, but this
              // method covers a lot of gritty details like cropping
              // and scanline padding.  Also, the seek only takes
              // place if the file position actually needs to change
              // (avoids a lot of cluster math in SD library).
              if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
                pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
              else     // Bitmap is stored top-to-bottom
                pos = bmpImageoffset + row * rowSize;


              if(ftell(bmpFile) != pos) { // Need seek?
                fseek(bmpFile, pos, SEEK_SET);
                buffidx = sizeof(sdbuffer); // Force buffer reload
              }

              for (col=0; col<w; col++) { // For each pixel...
                // Time to read more pixel data?
                if (buffidx >= sizeof(sdbuffer)) { // Indeed
                  fread(sdbuffer, 1, sizeof(sdbuffer), bmpFile);
                  buffidx = 0; // Set index to beginning
                }

                // Convert pixel from BMP to TFT format, push to display
                b = sdbuffer[buffidx++];
                g = sdbuffer[buffidx++];
                r = sdbuffer[buffidx++];
                //color = SDL_MapRGB(screen->format, r,g,b);
                color = Color565(r,g,b);
                fwrite(&color, sizeof(color), 1, outBmpFile);
              } // end pixel
            } // end scanline
          }
        }
    }

    fclose(bmpFile);
    fclose(outBmpFile);
    cout << endl;
    unsigned long totalKB = (totalLength * sizeof(color))/1024;
    cout << "totalLength: " << totalKB << "Kbytes" << endl;
    return totalKB;
}

/**
* Carga los datos de la cabecera del bmp y lo deja posicionado en
* la direccion de memoria correspondiente para poder leerlo
*/
bool Image565::cargarBmp(string imgLocation, t_mapSurface *surface){

	surface->bmpFile = fopen(imgLocation.c_str(),"rb");
	surface->goodBmp = false;

	if(surface->bmpFile != NULL && read16(surface->bmpFile) == 0x4D42) { // BMP signature
		(void)read32(surface->bmpFile); // Read & ignore file size
		(void)read32(surface->bmpFile); // Read & ignore creator bytes
		surface->bmpImageoffset = read32(surface->bmpFile); // Start of image data
		////Serial.print("Image Offset: "); //Serial.println(surface->bmpImageoffset, DEC);
		// Read DIB header
		(void)read32(surface->bmpFile); // Read & ignore header size
		surface->bmpWidth  = read32(surface->bmpFile);
		surface->bmpHeight = read32(surface->bmpFile);
		////Serial.print("bmpWidth: "); //Serial.println(surface->bmpWidth);
		////Serial.print("bmpHeight: "); //Serial.println(surface->bmpHeight);

		if(surface->bmpHeight < 0) {
		  surface->bmpHeight = -surface->bmpHeight;
		  surface->flip = false;
		}
		// BMP rows are padded (if needed) to 4-byte boundary
		surface->rowSize = (surface->bmpWidth * 3 + 3) & ~3;

		if(read16(surface->bmpFile) == 1) { // # planes -- must be '1'
		  surface->bmpDepth = read16(surface->bmpFile); // bits per pixel
		  ////Serial.print("Bit Depth: "); //Serial.println(surface->bmpDepth);
		  if((surface->bmpDepth == 24) && (read32(surface->bmpFile) == 0)) { // 0 = uncompressed
			surface->goodBmp = true; // Supported BMP format -- proceed!
		  }
		}
	}
//	if(!surface->goodBmp){
//		//Serial.println("BMP format not recognized.");
//	}
	return surface->goodBmp;
}

/**
* Converts a BMP into a 565 image
*/
unsigned long Image565::bmpdraw(t_mapSurface *surface, int x, int y,
                                int offsetX, int offsetY){

    uint8_t  sdbuffer[3*RGBBUFFPIXEL]; // pixel buffer (R+G+B per pixel)
    surface->buffidx = sizeof(sdbuffer); // Current position in sdbuffer
    uint8_t  r, g, b;
    int bmpSize = 0;
    uint16_t color;
    size_t totalLength = 0;

    const int finx = (screen->w - offsetX  - 4 > surface->bmpWidth ) ? surface->bmpWidth  : screen->w - offsetX - 4;
    const int finy = (screen->h - offsetY - 4 > surface->bmpHeight) ? surface->bmpHeight : screen->h - offsetY - 4;

	if(surface->goodBmp &&  finx > x && finy > y){

        cout << "(x,y): " << "(" << x << "," << y << ") - " << "(" << x + offsetX << "," << y + offsetY << "); (" << offsetX + finx << "," << offsetY + finy << ")" << endl;

        for (surface->row=y; surface->row < finy; surface->row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(surface->flip) // Bitmap is stored bottom-to-top order (normal BMP)
            surface->pos = x * 3 + surface->bmpImageoffset + (surface->bmpHeight - 1 - surface->row) * surface->rowSize;
          else     // Bitmap is stored top-to-bottom
            surface->pos = x * 3 + surface->bmpImageoffset + surface->row * surface->rowSize;

//          fpos_t t;
//          fgetpos(surface->bmpFile, &t);

          if(ftell(surface->bmpFile) != surface->pos) { // Need seek?
            fseek(surface->bmpFile, surface->pos, SEEK_SET);
            surface->buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (surface->col=x; surface->col < finx; surface->col++) { // For each pixel...
            // Time to read more pixel data?
            if (surface->buffidx >= sizeof(sdbuffer)) { // Indeed
              fread(sdbuffer, 1, sizeof(sdbuffer), surface->bmpFile);
              surface->buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[surface->buffidx++];
            g = sdbuffer[surface->buffidx++];
            r = sdbuffer[surface->buffidx++];
            color = SDL_MapRGB(screen->format, r,g,b);

            putpixel(screen, surface->col + offsetX, surface->row + offsetY, color);
            //color = Color565(r,g,b);
            //fwrite(&color, sizeof(color), 1, outBmpFile);
          } // end pixel
        } // end scanline
    }

    fclose(surface->bmpFile);
//    cout << endl;
    unsigned long totalKB = (totalLength * sizeof(color))/1024;
//    cout << "totalLength: " << totalKB << "Kbytes" << endl;
    return totalKB;
}

/**
* Converts a BMP into a 565 image with RLE compresion
*/
unsigned long Image565::convertTo565Rle(string filename){
    int      bmpWidth, bmpHeight;   // W+H in pixels
    uint8_t  bmpDepth;              // Bit depth (currently must be 24)
    uint32_t bmpImageoffset;        // Start of image data in file
    uint32_t rowSize;               // Not always = bmpWidth; may have padding
    uint8_t  sdbuffer[3*RGBBUFFPIXEL]; // pixel buffer (R+G+B per pixel)
    uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
    bool  flip    = true;        // BMP is stored bottom-to-top
    int      w, h, row, col;
    uint8_t  r, g, b;
    long pos = 0;
    int bmpSize = 0;

    FILE *bmpFile, *outBmpFile;
    bmpFile = fopen(filename.c_str(),"rb");

    unsigned int fpos = filename.find_last_of(".");
    string out;
    if (fpos != string::npos){
        out = filename.substr(0, fpos) + ".r65";
    } else {
        out = filename + ".r65";
    }
    outBmpFile = fopen(out.c_str(),"wb+");
    uint16_t color;
    t_rle lastColor;
    lastColor.n = 0;
    size_t totalLength = 0;
    int counter = 0;

    uint32_t maxRep = pow(2, sizeof(lastColor.n) * 8) - 1;
    cout << "maxRep: " << maxRep << "," << sizeof(lastColor.n) << "," << sizeof(lastColor.color) << endl;

    if(read16(bmpFile) == 0x4D42) { // BMP signature
        bmpSize = read32(bmpFile);
        cout << "bmpSize: " << bmpSize << endl;
        (void)read32(bmpFile); // Read & ignore creator bytes
        bmpImageoffset = read32(bmpFile); // Start of image data
        cout << "Image Offset: " << bmpImageoffset << endl;
        // Read DIB header
        cout << "Header size: " << read32(bmpFile) << endl;
        bmpWidth  = read32(bmpFile);
        bmpHeight = read32(bmpFile);
        cout << bmpWidth << "x" << bmpHeight << endl;
        int planes = read16(bmpFile);
        cout << "planes: " << planes << endl;
        if(planes == 1) { // # planes -- must be '1'
          bmpDepth = read16(bmpFile); // bits per pixel
          cout << "Bit Depth: " << bmpDepth << endl;
          if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

            //int fPos = ftell(bmpFile);
            //fseek(bmpFile, 0, SEEK_SET);
            //unsigned char ftmp[fPos];
            //Guardamos en el destino toda la cabecera leida hasta ahora
            //fread(ftmp, 1, fPos, bmpFile);

            /**Se escribe la cabecera*/
            //fwrite(ftmp, 1, fPos, outBmpFile);
            //fseek(bmpFile, fPos, SEEK_SET);
            // BMP rows are padded (if needed) to 4-byte boundary
            rowSize = (bmpWidth * 3 + 3) & ~3;

            // If bmpHeight is negative, image is in top-down order.
            // This is not canon but has been observed in the wild.
            if(bmpHeight < 0) {
              bmpHeight = -bmpHeight;
              flip      = false;
            }

            // Crop area to be loaded
            w = bmpWidth;
            h = bmpHeight;


            for (row=0; row<h; row++) { // For each scanline...
              // Seek to start of scan line.  It might seem labor-
              // intensive to be doing this on every line, but this
              // method covers a lot of gritty details like cropping
              // and scanline padding.  Also, the seek only takes
              // place if the file position actually needs to change
              // (avoids a lot of cluster math in SD library).
              if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
                pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
              else     // Bitmap is stored top-to-bottom
                pos = bmpImageoffset + row * rowSize;


              if(ftell(bmpFile) != pos) { // Need seek?
                fseek(bmpFile, pos, SEEK_SET);
                buffidx = sizeof(sdbuffer); // Force buffer reload
              }

              for (col=0; col<w; col++) { // For each pixel...
                // Time to read more pixel data?
                if (buffidx >= sizeof(sdbuffer)) { // Indeed
                  fread(sdbuffer, 1, sizeof(sdbuffer), bmpFile);
                  buffidx = 0; // Set index to beginning
                }
                // Convert pixel from BMP to TFT format, push to display
                b = sdbuffer[buffidx++];
                g = sdbuffer[buffidx++];
                r = sdbuffer[buffidx++];
                //tft.pushColor(tft.Color565(r,g,b));
                //color = SDL_MapRGB(screen->format, r,g,b);
                color = Color565(r,g,b);
                //putpixel(screen, col, row, color);
                //fwrite(&pix, 4, 1, outBmpFile);
                if (row == 0 && col == 0){
                    lastColor.n = 1;
                    lastColor.color = color;
                } else if (lastColor.color != color || lastColor.n >= maxRep){
                    fwrite(&lastColor.n, sizeof(lastColor.n), 1, outBmpFile);
                    fwrite(&lastColor.color, sizeof(lastColor.color), 1, outBmpFile);
                    counter += lastColor.n;
                    lastColor.n = 1;
                    lastColor.color = color;
                    totalLength++;
                } else {
                    lastColor.n++;
                }
              } // end pixel
            } // end scanline

            if ((lastColor.color == color || lastColor.n >= maxRep)){
                fwrite(&lastColor.n, sizeof(lastColor.n),1, outBmpFile);
                fwrite(&lastColor.color, sizeof(lastColor.color),1, outBmpFile);
                lastColor.n = 1;
                lastColor.color = color;
                totalLength++;
            }
          }
        }
    }

    fclose(bmpFile);
    fclose(outBmpFile);
    cout << endl;

    unsigned long totalKB = (totalLength * sizeof(lastColor.n) + totalLength * sizeof(lastColor.color))/1024;
    cout << "totalLength: " << totalKB << "Kbytes" << endl;
    cout << "pixels leidos: " << counter << " counter: " << counter <<  endl;
    return totalKB;
}

/**
* Show a 565 image into a SDL screen
*/
int Image565::toScreen565(string filename, int x, int y, int lcdw, int lcdh){
    uint16_t pixelValue = 0;
    int scrX = 0;
    int scrY = 0;

    FILE *bmpFile = fopen(filename.c_str(),"rb");
    if (bmpFile == NULL){
        return 0;
    }

    for (scrY=y; scrY < lcdh; scrY++){
        for (scrX=x; scrX < lcdw; scrX++){
            pixelValue = read16(bmpFile);
            putpixel(screen, scrX, scrY, pixelValue);
        }
    }
    fclose(bmpFile);
    return 1;
}


/**
* Shows an rle 565 image into the screen
*/
int Image565::rleFileToScreen(string filename, int x, int y, int lcdw, int lcdh){
    uint32_t repetitions = 0;
    uint16_t pixelValue = 0;

    int counter = 0;
    int scrX = 0;
    int scrY = 0;

    FILE *bmpFile = fopen(filename.c_str(),"rb");
    if (bmpFile == NULL){
        return 0;
    }

    int posFile = 0;
    while (counter < lcdw * lcdh){
        repetitions = read32(bmpFile);
        pixelValue = read16(bmpFile);
        //cout << "repetitions: " << repetitions << endl;
        if (repetitions == 0){
            cout << "problema en counter: " << counter << endl;
            return 0;
        }

        while(repetitions > 0){
            scrX = counter % (lcdw);
            scrY = counter / (lcdw);
            //cout << "scrX: " << scrX << " scrY: " << scrY << endl;
            putpixel(screen, scrX, scrY, pixelValue);
            repetitions--;
            counter++;
        }
        posFile++;
    }
    fclose(bmpFile);
    return 1;
}

/**
* Convert a surface into a 565 image
*/
unsigned long Image565::surfaceTo565(SDL_Surface *mySurface, string filename, bool rleFlag){
    int bpp = mySurface->format->BytesPerPixel;
    Uint8 r, g, b;
    Uint32 pixel;
    uint16_t color;
    t_rle lastColor;
    int counter = 0;
    uint32_t maxRep = pow(2, sizeof(lastColor.n) * 8) - 1;
    size_t totalLength = 0;
    unsigned long totalBytes = 0;

    FILE *outBmpFile = fopen(filename.c_str(),"wb+");

    Traza::print("recorriendo imagen de : " + Constant::TipoToStr(mySurface->w) + "," + Constant::TipoToStr(mySurface->h), W_DEBUG);

    for (int j=0; j < mySurface->h; j++){
        for (int i=0; i < mySurface->w; i++){
            SDL_GetRGB(getpixel(mySurface, i, j), mySurface->format, &r, &g, &b);
            color = Color565(r,g,b);
            //putpixel(screen, i, j, color);

            if (!rleFlag){
                fwrite(&color, sizeof(color), 1, outBmpFile);
                totalLength++;
            } else {
                if (j == 0 && i == 0){
                    lastColor.n = 1;
                    lastColor.color = color;
                } else if (lastColor.color != color || lastColor.n >= maxRep){
                    fwrite(&lastColor.n, sizeof(lastColor.n), 1, outBmpFile);
                    fwrite(&lastColor.color, sizeof(lastColor.color), 1, outBmpFile);
                    counter += lastColor.n;
                    lastColor.n = 1;
                    lastColor.color = color;
                    totalLength++;
                } else {
                    lastColor.n++;
                }
            }
        }
    }

    if (rleFlag && (lastColor.color == color || lastColor.n >= maxRep)){
        fwrite(&lastColor.n, sizeof(lastColor.n),1, outBmpFile);
        fwrite(&lastColor.color, sizeof(lastColor.color),1, outBmpFile);
        lastColor.n = 1;
        lastColor.color = color;
        totalLength++;
    }

    if (rleFlag)
        totalBytes = (totalLength * sizeof(lastColor.n) + totalLength * sizeof(lastColor.color));
    else
        totalBytes = totalLength * sizeof(color);

    fclose(outBmpFile);
    return totalBytes;
}

/**
*
*/
string Image565::existeFichero(string url, string diroutput){
    /*Filename(url) format is /zoom/x/y.png*/
    string tmp, zoom, x, y;
    tmp = url;
    int pos = tmp.find_last_of("/");
    if (pos != string::npos)
    {
        y = tmp.substr(pos + 1);
        Traza::print("y: " + y, W_DEBUG);
        tmp = tmp.substr(0, pos);
        pos = tmp.find_last_of("/");
        if (pos != string::npos)
        {
            x = tmp.substr(pos + 1);
            Traza::print("x: " + x, W_DEBUG);
        }
        tmp = tmp.substr(0, pos);
        pos = tmp.find_last_of("/");
        if (pos != string::npos)
        {
            zoom = tmp.substr(pos + 1);
            Traza::print("zoom: " + zoom, W_DEBUG);
        }
    }

    Dirutil dir;
    string directorioSalida = diroutput + Constant::getFileSep() + zoom
                              + Constant::getFileSep() + x;

    if (!dir.existe(directorioSalida)){
        dir.mkpath(directorioSalida.c_str(), 0777);
    }

    string fileSalida = directorioSalida + Constant::getFileSep() + dir.getFileNameNoExt(y);

    if (!dir.existe(fileSalida + ".r65") && !dir.existe(fileSalida + ".565")){
        return directorioSalida + Constant::getFileSep() + dir.getFileNameNoExt(y);
    } else {
        return "";
    }
}

/**
* Download an image into a 565 image
*/
void Image565::downloadMap(string url, string diroutput){
    HttpUtil utilHttp;
    utilHttp.setTimeout(10);

    string fichImagen = existeFichero(url, diroutput);
    if (fichImagen.empty()){
        Traza::print("La imagen ya existia en el disco: " + url, W_DEBUG);
        return;
    }
    bool downloaded = false;
    int retries = 0;

    while(!downloaded && retries < 3){
        bool ret = utilHttp.download(url);
        utilHttp.writeToFile(fichImagen + ".png");

        if (!ret){
            Traza::print("Imposible conectar a: " + url, W_ERROR);
            downloaded = true;
        } else {
            //Redimensionamos la imagen al tamanyo indicado
            SDL_Surface *mySurface = NULL;
            ImagenGestor imgGestor;
            Traza::print("Cargando imagen de bytes: ",utilHttp.getDataLength() , W_DEBUG);
            imgGestor.loadImgFromMem(utilHttp.getRawData(), utilHttp.getDataLength(), &mySurface);
			utilHttp.writeToFile(fichImagen + ".png");

            if (mySurface != NULL){
                unsigned long tam = 0;
                Traza::print("Imagen obtenida correctamente: " + url, W_DEBUG);
                tam = surfaceTo565(mySurface, fichImagen + ".r65", true);

                if (tam > mySurface->w * mySurface->h * 2){
                    Traza::print("Imagen 565 con rle de tamanyo excesivo (KBytes)", tam / 1024, W_DEBUG);
                    tam = surfaceTo565(mySurface, fichImagen + ".565", false);
                    Traza::print("Imagen 565 sin rle de tamanyo (KBytes)", tam / 1024, W_DEBUG);
                    //toScreen565(fichImagen + ".565", 0,0,256,256);
                    Dirutil dir;
                    dir.borrarArchivo(fichImagen + ".r65");
                } else {
                    Traza::print("Imagen 565 con rle de tamanyo (KBytes)", tam / 1024, W_DEBUG);
                    //rleFileToScreen(fichImagen + ".r65", 0,0,256,256);
                }
                downloaded = true;

            } else {
                Traza::print("Error al obtener la imagen: " + url, W_ERROR);
            }
        }
        retries++;
    }

//    cout << utilHttp.getDataLength() << endl;
}
