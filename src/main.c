#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/mpeg2convert.h>

#define WIDTH 768
#define HEIGHT 480

#define BUFFER_SIZE 4096
#define VID "data/test.mpg"

int main(void) {
  InitWindow(WIDTH, HEIGHT, "raylib example - basic ffmpeg video edit");
  mpeg2dec_t *decoder = mpeg2_init();
  if (!decoder) {
    TraceLog(LOG_ERROR, "could not init the decoder");
    exit(EXIT_FAILURE);
  }
  const mpeg2_info_t *info = mpeg2_info(decoder);
  mpeg2_state_t state;
  FILE *mpegfile = fopen(VID, "rb");
  if (!mpegfile) {
    TraceLog(LOG_ERROR, "could not open the video file!");
    exit(EXIT_FAILURE);
  }

  FILE *ffmpeg =
      popen("ffmpeg -loglevel verbose -y -f rawvideo -pix_fmt rgba -s 768x480 "
            "-r 30 -i - -vf \"vflip\" -c:v libx264 -pix_fmt yuv420p output.mp4",
            "w");

  if (!ffmpeg) {
    TraceLog(LOG_ERROR, "Could not open FFmpeg pipe");
    exit(EXIT_FAILURE);
  }

  RenderTexture2D screen = LoadRenderTexture(WIDTH, HEIGHT);
  int frameCount, lastFrame = 0;
  Image img = {0};
  Texture texture = {0};
  size_t size;
  uint8_t buffer[BUFFER_SIZE];
  bool gotDims = false;
  bool endRecording = false;

  SetTargetFPS(30);
  while (!WindowShouldClose() && !endRecording) {
    lastFrame = frameCount;
    while (lastFrame == frameCount) {
      state = mpeg2_parse(decoder);
      switch (state) {
      case STATE_BUFFER:
        size = fread(buffer, 1, BUFFER_SIZE, mpegfile);
        mpeg2_buffer(decoder, buffer, buffer + BUFFER_SIZE);
        if (size == 0) {
          endRecording = true;
          frameCount = 0;
        }
        break;
      case STATE_SEQUENCE:
        mpeg2_convert(decoder, mpeg2convert_rgb24, NULL);
        break;
      case STATE_SLICE:
      case STATE_END:
      case STATE_INVALID_END:
        if (info->display_fbuf) {
          if (gotDims == false) {
            gotDims = true;
            img.width = info->sequence->width;
            img.height = info->sequence->height;
            img.mipmaps = 1;
            img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
            img.data = (unsigned char *)malloc(img.width * img.height * 3);

            texture = LoadTextureFromImage(img);
            UnloadImage(img);
          }
          UpdateTexture(texture, info->display_fbuf->buf[0]);
          frameCount++;

          BeginDrawing();
          ClearBackground(PURPLE);
          DrawFPS(20, 20);

          BeginTextureMode(screen);
          DrawTexturePro(texture,
                         (Rectangle){0, 0, texture.width, texture.height},
                         (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
                         (Vector2){0, 0}, 0, WHITE);

          DrawText("Added this text on top of the video on raylib!", 10,
                   GetScreenHeight() / 2.0f, 20, WHITE);

          DrawText("Then, rendered the video with FFMPEG!", 10,
                   GetScreenHeight() / 2.0f + 30, 20, WHITE);

          EndTextureMode();
          DrawTextureRec(
              screen.texture,
              (Rectangle){0, 0, screen.texture.width, -screen.texture.height},
              (Vector2){0, 0}, WHITE);
          EndDrawing();
          Image image = LoadImageFromTexture(screen.texture);
          fwrite(image.data, 1, WIDTH * HEIGHT * sizeof(uint32_t), ffmpeg);
          UnloadImage(image);
        }
        break;
      default:
        break;
      }
    }
  }

  UnloadTexture(texture);
  mpeg2_close(decoder);
  fclose(mpegfile);
  pclose(ffmpeg);
  CloseWindow();
  return EXIT_SUCCESS;
}
