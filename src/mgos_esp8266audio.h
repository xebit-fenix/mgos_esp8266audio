#pragma once

#include "AudioLogger.h"

#include "AudioFileSourceBuffer.h"
//#include "AudioFileSourceSPIRAMBuffer.h"
#include "AudioFileSourceSTDIO.h"
#include "AudioFileSourcePROGMEM.h"
//#include "AudioFileSourceHTTPStream.h"
//#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceID3.h"

#include "AudioGeneratorRTTTL.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorMP3a.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorFLAC.h"
#include "AudioGeneratorMOD.h"
#include "AudioGeneratorWAV.h"
#include "AudioGeneratorTalkie.h"

#include "AudioOutputI2S.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputSPDIF.h"
#include "AudioOutputULP.h"
#include "AudioOutputMixer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum audio_source_t { SOURCE_PROGMEM, SOURCE_FILE, SOURCE_HTTP, SOURCE_ICY };
enum audio_buffer_t { BUFFER_NONE, BUFFER_INTERNAL, BUFFER_SPIRAM };
enum audio_codec_t { CODEC_WAV, CODEC_MP3, CODEC_MP3A, CODEC_AAC, CODEC_FLAC, CODEC_RTTTL, CODEC_MOD, CODEC_TALKIE };
enum audio_channel_t { CHANNEL_MUSIC, CHANNEL_FX };

void set_audio_source(audio_channel_t channel, audio_source_t source_type);
void set_audio_buffer(audio_channel_t channel, audio_buffer_t buffer_type, size_t size);
void set_audio_codec(audio_channel_t channel, audio_codec_t codec);

void audio_play(audio_channel_t channel, const char *filename_or_url);
void audio_play_progmem(audio_channel_t channel, const uint8_t *data, size_t len);
void audio_stop(audio_channel_t channel);
void audio_main_volume (uint16_t percent);
void audio_channel_volume (audio_channel_t channel, uint16_t percent);

#ifdef __cplusplus
}
#endif

class console_t : public Print {
  public:
  console_t() : Print() {}
  ~console_t(){}

  size_t write(uint8_t) {
    return 0;
  }

  size_t write(const uint8_t *buffer, size_t size) {
    for (int i = 0; i < size; i++) if (buffer[i] == 0x0d || buffer[i] == 0x0a) ((uint8_t*) buffer)[i] = 0x20;
    LOG(LL_INFO, ("%.*s", size, buffer));
    return size;
  }
};