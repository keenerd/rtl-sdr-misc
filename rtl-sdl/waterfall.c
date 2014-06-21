
/*

SDL powered waterfall

at the moment this everything is hard-coded for a single platform
the BeagleboneBlack with an LCD7 touchscreen (framebuffer mode)

it can run on other platforms, but will not autodetect anything
the keybinds are laid out for the touchscreen face buttons

on the BBB:
full screen double buffered blits seem to perform at 140 fps (cpu limited)

to run automatically:
@reboot sleep 1 && cd /the/install/path && ./waterfall

todo:
benchmark against fftw3
replace defines with options
autodetect things like screen resolution
change the displayed bandwidth
audio demodulation
fix screen blanking
a real make file

*/

#include <stdio.h>
#include <stdlib.h>

#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_ttf.h"

#include "rtl_power_lite.c"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
char* font_path = "./din1451alt.ttf";
#define FONT_SIZE 24
#define FRAME_MS 30
#define FRAME_LINES 10
#define MAX_STRING 100
#define BIG_JUMP 50000000

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
static const Uint32 r_mask = 0xFF000000;
static const Uint32 g_mask = 0x00FF0000;
static const Uint32 b_mask = 0x0000FF00;
static const Uint32 a_mask = 0x000000FF;
#else
static const Uint32 r_mask = 0x000000FF;
static const Uint32 g_mask = 0x0000FF00;
static const Uint32 b_mask = 0x00FF0000;
static const Uint32 a_mask = 0xFF000000;
#endif

static SDL_Surface* img_surface;
static SDL_Surface* scroll_surface;
static SDL_Surface* future_surface;
static const SDL_VideoInfo* info = 0;
SDL_Surface* screen;
TTF_Font *font;
int do_flip;  // todo, cond
int credits_toggle;

struct text_bin
{
    char string[MAX_STRING];
    int x, y;
    int i;
    int dirty;
    SDL_Surface* surf_fg;
    SDL_Surface* surf_bg;
};

struct text_bin credits[6];
struct text_bin freq_labels[5];

int init_video()
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      fprintf(stderr, "Video initialization failed: %s\n",
               SDL_GetError());
      return 0;
    }

  info = SDL_GetVideoInfo();

  if( !info ) {
    fprintf( stderr, "Video query failed: %s\n",
             SDL_GetError( ) );
    return 0;
  }

  return 1;
}

int set_video( Uint16 width, Uint16 height, int bpp, int flags)
{
  if (init_video())
    {
      if((screen = SDL_SetVideoMode(width,height,bpp,flags))==0)
        {
          fprintf( stderr, "Video mode set failed: %s\n",
                   SDL_GetError( ) );
          return 0;
        }
    }
  return 1;
}

int init_ttf()
{
  if (TTF_Init() != 0)
  {
    fprintf( stderr, "TTF init failed: %s\n",
             SDL_GetError( ) );
    return 1;
  }
  font = TTF_OpenFont(font_path, FONT_SIZE);
  if (font == NULL)
  {
    fprintf( stderr, "TTF load failed: %s\n",
             TTF_GetError( ) );
    return 1;
  }
  return 0;
}

void quit( int code )
{
  SDL_FreeSurface(scroll_surface);
  SDL_FreeSurface(future_surface);
  SDL_FreeSurface(img_surface);

  TTF_Quit( );
  SDL_Quit( );

  exit( code );
}

void handle_key_down(SDL_keysym* keysym)
{
    switch(keysym->sym)
    {
        case SDLK_ESCAPE:
            quit(0);
            break;
        case SDLK_RETURN:
            credits_toggle = !credits_toggle;
            break;
        case SDLK_DOWN:
        case SDLK_UP:
        case SDLK_LEFT:
        case SDLK_RIGHT:
         default:
            break;
      }
}

void process_events( void )
{
    SDL_Event event;

    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            handle_key_down( &event.key.keysym );
            break;
        case SDL_QUIT:
            quit( 0 );
            break;
        }
    }
}

void init()
{
  SDL_Surface* tmp;
  int i;
  SDL_Color colors[256];

  tmp = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH,
       SCREEN_HEIGHT, 8, r_mask, g_mask, b_mask, a_mask);
  scroll_surface = SDL_DisplayFormat(tmp);
  SDL_FreeSurface(tmp);

  tmp = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH,
       SCREEN_HEIGHT, 8, r_mask, g_mask, b_mask, a_mask);
  future_surface = SDL_DisplayFormat(tmp);
  SDL_FreeSurface(tmp);

  img_surface = IMG_Load("8-bit-arch.pcx");
  for (i = 0; i < SDL_NUMEVENTS; ++i)
    {
      if (i != SDL_KEYDOWN && i != SDL_QUIT)
        {
          SDL_EventState(i, SDL_IGNORE);
        }
    }

    for(i=0; i<256; i++)
    {
        colors[i].r = i;
        colors[i].g = i;
        colors[i].b = 50;
    }
    colors[0].r = 0; colors[0].g = 0; colors[0].b = 0;
    colors[255].r = 255; colors[255].g = 255; colors[255].b = 255;

  SDL_SetPalette(future_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

  SDL_ShowCursor(SDL_DISABLE);
}

void putpixel(SDL_Surface *surface, int x, int y, uint32_t pixel)
/* taken from some stackoverflow post */
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(uint16_t *)p = pixel;
        break;

    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        }
        else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(uint32_t *)p = pixel;
        break;

   default:
        break;           /* shouldn't happen, but avoids warnings */
    }
}

int pretty_text(SDL_Surface* surface, struct text_bin* text)
{
    SDL_Color fg_color = {255, 255, 255};
    SDL_Color bg_color = {0, 0, 0};
    SDL_Rect fg_rect = {text->x + 0, text->y + 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_Rect bg_rect = {text->x + 2, text->y + 2, SCREEN_WIDTH, SCREEN_HEIGHT};

    if (text->dirty)
    { 
        // this leaks, but freeing segfaults?
        // in practice, it leaks an MB an hour under very heavy use
        //if (text->surf_fg != NULL)
        //    {SDL_FreeSurface(text->surf_fg);}
        //if (text->surf_bg != NULL)
        //    {SDL_FreeSurface(text->surf_bg);}
        text->surf_fg = TTF_RenderText_Solid(font, text->string, fg_color);
        text->surf_bg = TTF_RenderText_Solid(font, text->string, bg_color);
        text->dirty = 0;
    }

    SDL_BlitSurface(text->surf_bg, NULL, surface, &bg_rect);
    SDL_BlitSurface(text->surf_fg, NULL, surface, &fg_rect);

    return 0;
}

void build_credits(void)
{
    int i;
    int xs[] = {300, 300, 300, 300, 300, 300};
    int ys[] = {100, 150, 200, 250, 300, 350};
    strncpy(credits[0].string, "board: BeagleBone Black", MAX_STRING);
    strncpy(credits[1].string, "display: CircuitCo LCD7", MAX_STRING);
    strncpy(credits[2].string, "radio: rtl-sdr", MAX_STRING);
    strncpy(credits[3].string, "graphics: SDL", MAX_STRING);
    strncpy(credits[4].string, "os: Arch Linux ARM", MAX_STRING);
    strncpy(credits[5].string, "glue: Kyle Keen", MAX_STRING);
    for (i=0; i<6; i++)
    {
        credits[i].x = xs[i];
        credits[i].y = ys[i];
        credits[i].dirty = 1;
    }
}

void show_credits(SDL_Surface* surface)
{
    int i;
    for (i=0; i<6; i++)
        {pretty_text(surface, &(credits[i]));}
}

void build_labels(void)
{
    // very similar to the lines code
    int f, i, x, drift, center;
    drift = (frequency % 1000000) / (SAMPLE_RATE / FFT_SIZE);
    center = frequency - (frequency % 1000000);
    for (i=-2; i<=2; i++)
    {
        x = SCREEN_WIDTH / 2 + -drift + i * 1000000 / (SAMPLE_RATE / FFT_SIZE);
        f = center + i * 1000000;
        freq_labels[i+2].x = x - FONT_SIZE/2;
        freq_labels[i+2].y = 10;
        if (freq_labels[i+2].i == f)
            {continue;}
        freq_labels[i+2].dirty = 1;
        freq_labels[i+2].i = f;
        snprintf(freq_labels[i+2].string, MAX_STRING, "%i", f/1000000);
    }
}

void static_events(void)
{
    SDL_Rect blank = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    uint8_t* keystate = SDL_GetKeyState(NULL);
    if (keystate[SDLK_LEFT])
    {
        frequency -= BIG_JUMP;
        frequency_set();
        build_labels();
        SDL_FillRect(scroll_surface, &blank, 0);
    }
    if (keystate[SDLK_RIGHT])
    {
        frequency += BIG_JUMP;
        frequency_set();
        build_labels();
        SDL_FillRect(scroll_surface, &blank, 0);
    }
    if (keystate[SDLK_UP])
        {gain_decrease();}
    if (keystate[SDLK_DOWN])
        {gain_increase();}
}

uint32_t frame_callback(uint32_t interval, void* param)
{
    do_flip = 1;
    return interval;
}

uint32_t rgb(uint32_t i)
{
    return ((b_mask/255)*20 | (r_mask/255)*i | (g_mask/255)*i);
}

int mouse_stuff(void)
// returns X scroll offset
// kind of crap with variable framerate
{
    static double prev_x = -100;
    static double velo = 0;
    double deaccel = 10;
    int x, y, buttons;
    buttons = SDL_GetMouseState(&x, &y);
    if (buttons & SDL_BUTTON_LMASK)
    {
        if (prev_x < 0)
        {
            prev_x = x;
        }
        velo = x - prev_x;
        prev_x = x;
        //fprintf(stdout, "%i %f\n", x, velo);
    } else {
        prev_x = -100;
        if (velo > deaccel)
            {velo -= deaccel;}
        if (velo < -deaccel)
            {velo += deaccel;}
        if (velo >= -deaccel && velo <= deaccel)
            {velo *= 0.5;}
     }
     return (int)round(velo);
}

int main( int argc, char* argv[] )
{
  int i, c, x, y, v, line;
  int blits = 0;
  uint32_t pixel = 0;
  struct text_bin text;
  SDL_Rect ScrollFrom   = {0, 1, SCREEN_WIDTH, SCREEN_HEIGHT};
  if (!set_video(SCREEN_WIDTH, SCREEN_HEIGHT, 8,
      SDL_HWSURFACE | SDL_HWACCEL | SDL_HWPALETTE /*| SDL_FULLSCREEN*/))
   quit(1);
  init_ttf();
  //SDL_Init(SDL_INIT_TIMER);

  SDL_WM_SetCaption("Demo", "");

    init();

    build_credits();
    build_labels();

    strncpy(text.string, "<<        >>         -          +           ?", MAX_STRING);
    text.x = 30;
    text.y = 450;
    text.dirty = 1;

  SDL_BlitSurface(img_surface, NULL, scroll_surface, NULL);
  //SDL_AddTimer(FRAME_MS, frame_callback, NULL);

    fft_launch();
    y = 0;
    SDL_LockSurface(future_surface);
  while(1)
    {
      process_events();
      //safe_cond_wait(&fft_out.ready, &fft_out.ready_m);
      if (!fft_out.ready_fast)
      {
          usleep(1000);
          continue;
      }
      fft_out.ready_fast = 0;
      pthread_rwlock_rdlock(&fft_out.rw);
      for (x=0; x<SCREEN_WIDTH; x++)
      {
        //putpixel(future_surface, x, y, pixel);
        i = x + (FFT_SIZE - SCREEN_WIDTH) / 2;
        c = 40*fft_out.buf[i] + 1;
        if (c > 254)
            {c = 254;}
        if (c < 1)
            {c = 1;}
        //fprintf(stdout, "%i ", fft_out.buf[i]);
        putpixel(future_surface, x, y, 40*fft_out.buf[i] + 1);
        pixel++;
      }
      // lines every 100KHz
      line = (frequency % 100000) / (SAMPLE_RATE / FFT_SIZE);
      for (i=-15; i<15; i++)
      {
          if (y%4)
              {break;}
          x = SCREEN_WIDTH / 2 + -line + i * 100000 / (SAMPLE_RATE / FFT_SIZE);
          if (x < 0)
              {continue;}
          if (x > SCREEN_WIDTH)
              {continue;}
          putpixel(future_surface, x, y, 0xFF);
      }
      //fprintf(stdout, "\n");
      pthread_rwlock_unlock(&fft_out.rw);
      y++;
      if (!do_flip && y <= FRAME_LINES)
          {continue;}
      static_events();
      v = mouse_stuff();
      if (v != 0)
      {
          frequency += (-v * SAMPLE_RATE / FFT_SIZE);
          frequency_set();
          build_labels();
      }
      SDL_UnlockSurface(future_surface);
      // scroll
      ScrollFrom.x = -v;
      ScrollFrom.y = y;
      ScrollFrom.w = SCREEN_WIDTH;
      ScrollFrom.h = SCREEN_HEIGHT;
      SDL_BlitSurface(scroll_surface, &ScrollFrom, scroll_surface, NULL);
      // nuke edges
      if (v > 0)
      {
          ScrollFrom.x = 0;
          ScrollFrom.y = 0;
      }
      if (v < 0)
      {
          ScrollFrom.x = SCREEN_WIDTH+v;
          ScrollFrom.y = 0;
      }
      if (v != 0)
      {
          ScrollFrom.w = abs(v);
          ScrollFrom.h = SCREEN_HEIGHT-y;
          SDL_FillRect(scroll_surface, &ScrollFrom, 0);
      }
      // new stuff
      ScrollFrom.x = v;
      ScrollFrom.y = SCREEN_HEIGHT - y;
      ScrollFrom.w = SCREEN_WIDTH;
      ScrollFrom.h = SCREEN_HEIGHT;
      SDL_BlitSurface(future_surface, NULL, scroll_surface, &ScrollFrom);
      SDL_BlitSurface(scroll_surface, NULL, screen, NULL);
      // overlay
      pretty_text(screen, &text);
      if (credits_toggle)
          {show_credits(screen);}
      for (i=0; i<5; i++)
          {pretty_text(screen, &freq_labels[i]);}
      pretty_text(screen, &text);
      SDL_Flip(screen);
      // only way to keep the BBB from blanking the screen
      // (the 10 minute timeout can not be changed by any known means)
      if (blits % 2000 == 0)
          {system("setterm -blank poke");}
      blits++;
      do_flip = 0;
      y = 0;
      SDL_LockSurface(future_surface);
    }
    quit(0);
    fft_cleanup();

  return 0;
}


// vim:set tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab:
