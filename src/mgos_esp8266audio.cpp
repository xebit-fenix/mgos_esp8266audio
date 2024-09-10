#include "mgos.h"
#include "esp32/clk.h"
#include "esp_pm.h"
#include "mgos_esp8266audio.h"

/*union out {
  AudioOutputI2S *out;
  AudioOutputI2SNoDAC *out;
  AudioOutputSPDIF *out;
  AudioOutputULP *out;
};*/
AudioOutput* out;
AudioOutputMixer *mixer;
AudioOutputMixerStub* stub[2];
AudioGenerator* gen[2];
AudioFileSource* source[2] = {nullptr, nullptr};
AudioFileSource* buffer[2];
AudioFileSourceID3 *id3 = nullptr;

extern "C" {

bool mgos_esp8266audio_init(void) {
  // Change cpu freq to 240
  int curr_freq = esp_clk_cpu_freq();
  LOG(LL_INFO, ("Current CPU freq: %d", curr_freq / 1000000));

  int freq = 240;
  if (freq != 20 && freq != 40 && freq != 80 && freq != 160 && freq != 240) {
    LOG(LL_ERROR, ("Frequency must be 20MHz, 40MHz, 80Mhz, 160MHz or 240MHz"));
  } else {
    esp_pm_config_esp32_t config;
    esp_pm_get_configuration(&config);
    if (config.max_freq_mhz != 0) {
      LOG(LL_INFO, ("CPU config: max freq:%d, min freq:%d, light_sleep:%d", config.max_freq_mhz , config.min_freq_mhz , config.light_sleep_enable ? 1 : 0));
      config.max_freq_mhz = freq;
      //config.min_freq_mhz = freq;
      //config.light_sleep_enable = false;
      esp_err_t err = esp_pm_configure(&config);
      if (err == ESP_OK) {
        while ((curr_freq = esp_clk_cpu_freq()) != freq * 1000000) vTaskDelay(1);
        LOG(LL_INFO, ("New CPU freq: %d", curr_freq / 1000000));
      } else {
        LOG(LL_ERROR, ("Error while change PM config: %03X", err));
      }
    } else {
      LOG(LL_ERROR, ("PM config changing is not supported"));
    }
  }

  // Init audio
  console_t* console = new console_t();
  audioLogger = (Print*) console;

  if (strcmp(mgos_sys_config_get_audio_output(), "i2s") == 0) {
    LOG(LL_INFO, ("Auduo output I2S init"));
    AudioOutputI2S* i2s;
    i2s = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, 10, /*AudioOutputI2S::APLL_DISABLE*/ mgos_sys_config_get_audio_apll());
    i2s->SetPinout(mgos_sys_config_get_audio_i2s_bclk_pin(), mgos_sys_config_get_audio_i2s_wclk_pin(), mgos_sys_config_get_audio_i2s_dout_pin());
    i2s->SetOutputModeMono(mgos_sys_config_get_audio_mono());
    i2s->SetRate(44100);
    out = i2s;
  }
  if (strcmp(mgos_sys_config_get_audio_output(), "pdm") == 0) {
    AudioOutputI2SNoDAC* pdm;
  }
  if (strcmp(mgos_sys_config_get_audio_output(), "dac") == 0) {
    AudioOutputULP* dac;
  }
  if (strcmp(mgos_sys_config_get_audio_output(), "spdif") == 0) {
    AudioOutputSPDIF* spdif;
  }

  mixer = new AudioOutputMixer(64, out);
  stub[0] = mixer->NewInput();
  stub[1] = mixer->NewInput();
  stub[0]->SetGain(1);
  stub[1]->SetGain(1);

  audio_main_volume(mgos_sys_config_get_audio_volume());

  mgos_set_timer(2, MGOS_TIMER_REPEAT, [](void *arg) {
    if (gen[0] && gen[0]->isRunning()) {
      if (!gen[0]->loop()) {
        LOG(LL_ERROR, ("Audio channel MUSIC stop!"));
        gen[0]->stop();
        stub[0]->stop();
      }
    }
    if (gen[1] && gen[1]->isRunning()) {
      if (!gen[1]->loop()) {
        LOG(LL_ERROR, ("Audio channel FX stop!"));
        gen[1]->stop();
        stub[1]->stop();
      }
    }
  }, NULL);

  return true;
}

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  LOG(LL_INFO, ("ID3 callback for: %s = '", type));

  if (isUnicode) {
    string += 2;
  }

  LOG(LL_INFO, ("%.*s", strlen(string), string));
}


void set_audio_source(audio_channel_t channel, audio_source_t source_type) {
  if (channel >= 2) return;
  if (source[(int) channel] != nullptr) delete source[(int) channel];

  switch (source_type) {
    case audio_source_t::SOURCE_PROGMEM: source[(int) channel] = new AudioFileSourcePROGMEM(); break;
    case audio_source_t::SOURCE_FILE: source[(int) channel] = new AudioFileSourceSTDIO(); break;
    //case audio_source_t::SOURCE_HTTP: source[(int) channel] = new AudioFileSourceHTTPStream(); break;
    //case audio_source_t::SOURCE_ICY: source[(int) channel] = new AudioFileSourceICYStream(); break;
    default: LOG(LL_ERROR, ("Audio source not supported"));
  }
  //if (buffer[(int) channel] == nullptr) buffer[(int) channel] = source[(int) channel];
  if (source[(int) channel] == nullptr) LOG(LL_ERROR, ("Audio source [%d] not created", channel));
}

void set_audio_buffer(audio_channel_t channel, audio_buffer_t buffer_type, size_t size = 2048) {
  if (channel >= 2) return;
  if (buffer[(int) channel] != nullptr && buffer[(int) channel] != source[(int) channel]) delete buffer[(int) channel];

  switch (buffer_type) {
    case audio_buffer_t::BUFFER_NONE: buffer[(int) channel] = nullptr; break;
    case audio_buffer_t::BUFFER_INTERNAL: buffer[(int) channel] = new AudioFileSourceBuffer(source[(int) channel], size); break;
    //case audio_buffer_t::BUFFER_SPIRAM: buffer[(int) channel] = new AudioFileSourceSPIRAMBuffer(source[(int) channel], cs_pin, size); break;
    default: LOG(LL_ERROR, ("Audio buffer not supported"));
  }
  if (buffer[(int) channel] == nullptr) LOG(LL_ERROR, ("Audio buffer [%d] disabled", channel));
}

void set_audio_codec(audio_channel_t channel, audio_codec_t codec) {
  if (channel >= 2) return;
  if (gen[(int) channel] != nullptr) delete gen[(int) channel];

  switch (codec) {
    case audio_codec_t::CODEC_WAV: gen[(int) channel] = new AudioGeneratorWAV(); break;
    case audio_codec_t::CODEC_MP3: gen[(int) channel] = new AudioGeneratorMP3(); break;
    //case audio_codec_t::CODEC_AAC: gen[(int) channel] = new AudioGeneratorAAC(); break;
    case audio_codec_t::CODEC_RTTTL: gen[(int) channel] = new AudioGeneratorRTTTL(); break;
    case audio_codec_t::CODEC_TALKIE: gen[(int) channel] = new AudioGeneratorTalkie(); break;
    default: LOG(LL_ERROR, ("Audio codec not supported"));
  }
  if (gen[(int) channel] == nullptr) LOG(LL_ERROR, ("Audio codec [%d] not created", channel));

  if (codec == audio_codec_t::CODEC_MP3 && channel == 0 && source[0]) {
    if (id3) delete id3;
    id3 = new AudioFileSourceID3(buffer[0] ? buffer[0] : source[0]);
    id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  }

  if (codec == audio_codec_t::CODEC_TALKIE) if (source[(int) channel] != nullptr) delete source[(int) channel];
}


void audio_play(audio_channel_t channel, const char *filename_or_url) {
  LOG(LL_INFO, ("Audio play"));
  if (channel >= 2) return;
  if (gen[(int) channel] && stub[(int) channel] && source[(int) channel]) {
    source[(int) channel]->open(filename_or_url);
    AudioFileSource* src = (channel == 0) && id3 ? id3 : buffer[(int) channel] ? buffer[(int) channel] : source[(int) channel];
    gen[(int) channel]->begin(src, stub[(int) channel]);
  }
  LOG(LL_INFO, ("end of Audio play"));
}

void audio_play_progmem(audio_channel_t channel, const uint8_t *data, size_t len) {
  LOG(LL_INFO, ("Audio play progmem"));
  if (channel >= 2) return;
  if (gen[(int) channel] && stub[(int) channel] /*&& source[(int) channel]*/) {
    if (source[(int) channel]) {
      ((AudioFileSourcePROGMEM*) source[(int) channel])->open(data, len);
      AudioFileSource* src = (channel == 0) && id3 ? id3 : buffer[(int) channel] ? buffer[(int) channel] : source[(int) channel];
      gen[(int) channel]->begin(src, stub[(int) channel]);
    } else {
      gen[(int) channel]->begin(nullptr, stub[(int) channel]);
      ((AudioGeneratorTalkie*) gen[(int) channel])->say(data, len, true);
    }
  }
  LOG(LL_INFO, ("end of Audio play progmem"));
}

void audio_stop(audio_channel_t channel) {
  LOG(LL_INFO, ("Audio stop"));
  if (channel >= 2) return;
  if (gen[(int) channel] && stub[(int) channel]) {
    gen[(int) channel]->stop();
    stub[(int) channel]->stop();
  }
  LOG(LL_INFO, ("end of Audio stop"));
}

void audio_main_volume (uint16_t percent) {
  if (out) out->SetGain(((float) percent) / 100);
}

void audio_channel_volume (audio_channel_t channel, uint16_t percent) {
  if (channel >= 2) return;
  if (stub[(int) channel]) stub[(int) channel]->SetGain(((float) percent) / 100);
}

}