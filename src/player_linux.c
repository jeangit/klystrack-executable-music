// gcc -g player.c ksnd.c -DENABLE_WAV_WRITER -I../klystron/src ../klystron/bin.size/libksndstatic.a `sdl2-config --libs --cflags` -lm -lSDL2_mixer -lncurses

// important: ajouter ceci au début de music.h (dans les sources de klystron):
// #define USESDL_RWOPS 1

#include "ksnd.h"
#include "snd/music.h"
#include <stdio.h>
#include "../temp/data.inc"

#include <ncurses.h>
#include<SDL.h>
#include<SDL_mixer.h>

#define SAMPLE_RATE 44100

int _kbhit()
{
  static WINDOW *scr = 0;
  if (!scr) {
    scr = initscr();
    timeout(5); //wait n ms (n==0 : non-blocking, n<0 : blocking)
  }

  int is_key = 0;
  if (getch() != ERR ) { 
    is_key = 1;
  }

  return is_key;
}

int sdl_initaudio()
{
  int is_err = 0;
  const char* driver_name = 0;


  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    is_err = 1;
  }

  driver_name = SDL_GetCurrentAudioDriver();
  if (driver_name) {
    fprintf(stderr,"[SDL] Audio subsystem initialized; driver = %s.\n", driver_name);
  } else {
    fprintf(stderr,"[SDL] no audio driver !\n");
    is_err = 1;
  }

  if( Mix_OpenAudio( SAMPLE_RATE, MIX_DEFAULT_FORMAT, 2, 2048 ) < 0 ) //SDL_mixer required
  {
    fprintf(stderr,"[SDL] SDL_mixer not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
    is_err = 1;
  }

  return is_err;
}

KSongInfo const * get_infos( KSong *song)
{
  KSongInfo const * infos = 0;
  infos = KSND_GetSongInfo( song, 0);
  return infos;
}


int main(int argc, char **argv)
{
  KPlayer *player;
  KSong * song;

#if ENABLE_WAV_WRITER
  int is_wave_writer = 0;
  //char output_file[80] = {0};
  char *output_file = 0;

  // Enable wav writer if output file is specified

  //  if (sscanf(GetCommandLine(), "%*s %79s", output_file) == 1)
  if (argc > 1) {
    output_file = argv[1];
    is_wave_writer = 1;
    fprintf(stderr, "will output to: %s\n", output_file);
  } else {
    sdl_initaudio();
  }

  if (is_wave_writer)
    player = KSND_CreatePlayerUnregistered(SAMPLE_RATE);
  else
    player = KSND_CreatePlayer(SAMPLE_RATE);
#else
  player = KSND_CreatePlayer(SAMPLE_RATE);
#endif

  song = KSND_LoadSongFromMemory(player, data, sizeof(data));

  KSND_PlaySong(player, song, 0);


  int p = 0, l =  KSND_GetSongLength(song), prev_p = 0;


#if ENABLE_WAV_WRITER
  short int buffer[16384];
  FILE *f = NULL;
  size_t riffsize = 0, chunksize = 0;

  if (is_wave_writer)
  {
    // Write the RIFF header with temporary size (will be finalized later)
    // 44.1 kHz 16-bit stereo
    f = fopen(output_file, "wb");

    const int channels = 2;
    unsigned int tmp = 0;

    fwrite("RIFF", 4, 1, f);
    riffsize = ftell(f);
    fwrite(&tmp, 4, 1, f);
    fwrite("WAVEfmt ", 8, 1, f);

    tmp = 16;
    fwrite(&tmp, 4, 1, f);

    tmp = 1;
    fwrite(&tmp, 2, 1, f);

    tmp = channels;
    fwrite(&channels, 2, 1, f);

    tmp = SAMPLE_RATE;
    fwrite(&tmp, 4, 1, f);

    tmp = SAMPLE_RATE * channels * sizeof(buffer[0]);
    fwrite(&tmp, 4, 1, f);

    tmp = channels * sizeof(buffer[0]);
    fwrite(&tmp, 2, 1, f);

    tmp = 16;
    fwrite(&tmp, 2, 1, f);
    fwrite("data", 4, 1, f);
    chunksize = ftell(f);

    tmp = 0;
    fwrite(&tmp, 4, 1, f);
  }
#endif

  int running = 1;
  KSongInfo const *infos = get_infos( song); 
  while ( running)
  {
    prev_p = p;
    p = KSND_GetPlayPosition(player);

    if (p >= l-1 || p < prev_p) {
      running = 0;
    }

#if ENABLE_WAV_WRITER
    if (is_wave_writer)
    {
      mvprintw(4,0,"*** Writing to %s", output_file);
      KSND_FillBuffer(player, buffer, sizeof(buffer));
      fwrite(buffer, sizeof(buffer[0]), sizeof(buffer) / sizeof(buffer[0]), f);
    } 
#else
#endif
    mvprintw(0,0,"=== %s ===\nPosition: %d\nPress the key [ANY] to quit",infos->song_title,p);
    refresh();
    if (_kbhit() ) {
      running = 0;
    }
  }

#if ENABLE_WAV_WRITER
  // Finalize the wave header with the final file size

  if (is_wave_writer)
  {
    size_t sz = ftell(f) - 8;

    fseek(f, riffsize, SEEK_SET);
    fwrite(&sz, sizeof(sz), 1, f);

    sz = sz + 8 - (chunksize + 4);
    fseek(f, chunksize, SEEK_SET);
    fwrite(&sz, sizeof(sz), 1, f);

    fclose(f);
  }
#endif

  endwin();
  if (song) { KSND_FreeSong( song); }
  if (player) { KSND_FreePlayer( player); }
  SDL_CloseAudio();
  SDL_Quit();

  return 0;
}

