// Program to control alsa master volume from rotary control conected to two gpio pins...
// gcc -lwiringPi -lasound rotary_alsa_volume.c -o rotary_alsa_volume

#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct 
{
    int pin_a;
    int pin_b;
    long vol_min, vol_max, vol_initial;
    volatile long value;
    volatile int lastEncoded;
} encoder_t;

encoder_t encoder;
encoder_t *setupencoder(int pin_a, int pin_b); 

snd_mixer_elem_t* elem;
long min, max;
snd_mixer_selem_id_t *sid;

void updateEncoder()
{
  int MSB = digitalRead(encoder.pin_a);
  int LSB = digitalRead(encoder.pin_b);

  int encoded = (MSB << 1) | LSB;
  int sum = (encoder.lastEncoded << 2) | encoded;

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoder.value++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoder.value--;

  encoder.lastEncoded = encoded;

  if (encoder.value < encoder.vol_min) encoder.value = encoder.vol_min;
  if (encoder.value > encoder.vol_max) encoder.value = encoder.vol_max;

  snd_mixer_selem_set_playback_volume_all(elem, encoder.value * max / 100);
}

int main(int argc, char *argv[])
{
  snd_mixer_t *handle;
  
  if (argc != 6) {
    printf("Usage: %s pin_a pin_b min_volume max_volume initial_volume\n",argv[0]);
    printf("Note!  Pin pin_a and pin_b numbers use wiringPi not Broadcomm numbering convention.\nType 'gpio readall' for comparison table...\n");
    printf("       *_volume are in range 0-100\n");
    return 1;
  }

  wiringPiSetup();

  // setup encoder...
  encoder.pin_a       = atoi(argv[1]);
  encoder.pin_b       = atoi(argv[2]);
  encoder.vol_min     = (long)atoi(argv[3]);
  encoder.vol_max     = (long)atoi(argv[4]);
  encoder.vol_initial = (long)atoi(argv[5]);
  if (encoder.value < encoder.vol_min) encoder.value = encoder.vol_min;
  if (encoder.value > encoder.vol_max) encoder.value = encoder.vol_max;

  encoder.lastEncoded = 0;
  
  // setup alsa mixer
  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, "default");
  snd_mixer_selem_register(handle, nullptr, nullptr);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, "Master");
  
  elem = snd_mixer_find_selem(handle, sid);

  snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
  snd_mixer_selem_set_playback_volume_all(elem, encoder.value * max / 100);

  // setup GPIO pins and interrupt handler...
  pinMode(encoder.pin_a, INPUT);
  pinMode(encoder.pin_b, INPUT);
  pullUpDnControl(encoder.pin_a, PUD_UP);
  pullUpDnControl(encoder.pin_b, PUD_UP);
  wiringPiISR(encoder.pin_a,INT_EDGE_BOTH, updateEncoder);
  wiringPiISR(encoder.pin_b,INT_EDGE_BOTH, updateEncoder);

  while (1); // wait forever since updateEncoder() is interrupt driven

  snd_mixer_close(handle);

  return 0;
}