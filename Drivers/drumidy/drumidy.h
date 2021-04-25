#ifndef DRUMIDY_H_
#define DRUMIDY_H_

#include "stm32g4xx_hal.h"
#include <string.h>


#define DRUMSET_CHANNEL 	9	// global midi property
#define RIM_BLOCKING_TIME 	150	// ms when rim blocks main and main blocks rim

#define PEAK_THRESHOLD		50	// adc digits for peak identification
#define AUX_PEAK2PEAK		50	// ms aux p2p


//#define OFF_TIMEOUT_TIME	50	// if no bigger peak within
// ===================================== //

//typedef enum
//{
//	EVENT_EMPTY = 0U,		// nothing
//	EVENT_NEW_PEAK = 1U,	// new peak on main
//	EVENT_NEW_ALT_PEAK,		// alternative event
//	EVENT_ALT2MAIN,			// updated peak to main
//	EVENT_MAIN_OFF,
//	EVENT_ALT_OFF
//} DRM_event;


typedef enum
{
	CHANNEL_STATUS_IDLE = 0U,	// channel doing nothing
	CHANNEL_STATUS_PEAK,		// peak evaluation stage
//	CHANNEL_STATUS_MUTE,		// post-peak mute stage
//	CHANNEL_STATUS_FILTER,		// post-peak filtering stage
	CHANNEL_STATUS_PRESSED		// pedal type pressed (on), idle - off
} DRM_status;

typedef enum
{
	CHANNEL_PAD_IDLE 	  = 0U,	// pad, no signal
	CHANNEL_PAD_TRIGGERED = 1U,	// pad - active peak
	CHANNEL_PEDAL_IDLE	  	,	// pedal released
	CHANNEL_PEDAL_PRESSED	 	// pedal pressed
} DRM_aux_state;


// Channel configuration
typedef enum
{
	MESH_PAD_AUTOAUX 	= 0U,	// normal pad, independent auto aux
	MESH_RIM_AUTOAUX	= 1U,	// normal pad with rim detection, independent auto aux
	CYMBAL_AUTOAUX		= 2U,	// cymbal input (faster peaks), independent auto aux
	CYMBAL_HIHAT		= 3U, 	// hihat input, aux - pedal
	CYMBAL_2_ZONE		= 4U, 	// cymbal input, aux - blocking pad
	CYMBAL_MUTE			= 5U, 	// cymbal input with muting pedal (button)
} DRM_type;

typedef enum
{
	AUX_TYPE_PAD	= 0U,	// aux channel input type pad (low rest)
	AUX_TYPE_PEDAL	= 1U,	// aux channel input type pedal (high rest)
} DRM_aux_input;


typedef enum {
  KICK   	= 0x24,
  SNARE  	= 0x26,
  SNRIM	 	= 0x25,
  TOM1   	= 0x30,
  TOM2   	= 0x2F,
  TOM3   	= 0x2D,
  TOMF   	= 0x2B,
  HHOPEN   	= 0x2E,
  HHCLOSE  	= 0x2A,
  CRASH  	= 0x31,
  RIDE   	= 0x33,
  HHPEDAL	= 0x2C,
  BELL 		= 0x35,

  x__X		= 0x25,
  MUTE		= 0x00,
  CR_MUTE   = 0x31
} DRM_voice;

// structure for a single channel both main and aux
typedef struct _drum
{
	// ### ==CONFIGURATION== ###
	// ### main input
	DRM_voice main_voice;			// main input voice
	DRM_voice alt_voice;			// pedal activated or rim hit voice
	DRM_voice aux_voice;			// aux input voice

	DRM_type  		chnl_type;		// channel inputs configuration
	DRM_aux_input	aux_type;		// pad/pedal input

	// ### ==PARAMETERS== ###
	uint16_t peak_volume_norm;		// 100 	100% volume point. 100=4096, 50 = 2048 etc
	uint16_t peak_max_length;		// 200 	x0.1=20ms
	uint16_t peak_min_length; 		// 8-mesh/3-cymbal 	x0.1=3ms
	uint16_t peak2peak;				// 2048 points full cycle

	// ### ==VARIABLES== ###
	uint16_t 	cooldown;			// cooldown counter for the peak-to-peak time
	uint8_t 	main_peaking;		// indicates whether there is an active peak

	uint8_t 	main_rdy;			// request to send new message
	uint8_t		main_rdy_usealt;	// flag to use alt voice on main

	// evaluation of current peak
	uint16_t 	main_active_max;	// current peak max height
	uint16_t 	main_active_length;	// current peak duration

	DRM_status aux_status;			// indicates current status of the aux channel

	// data to send to PC
	uint16_t main_rdy_height;		// ready peak height
	uint32_t main_rdy_time;			// ready peak time, DEBUG ONLY
	uint8_t  main_rdy_volume;		// calculated midi volume
	uint16_t main_rdy_length;		// ready peak duration, DEBUG ONLY


	uint8_t  aux_rdy;				// flag for aux event ready
	// aux input
	uint32_t aux_rdy_time;			// variable with active peak time
	DRM_aux_state aux_rdy_state;	// previous state of the aux

	uint32_t aux_active_time;		// last detected peak time
	DRM_aux_state aux_active_state;	// active state of the aux
	DRM_aux_state aux_last_state;	// active state of the aux

	uint32_t  main_last_on_time;	// tracking on-event time for delayed off
	uint8_t   main_last_on_voice;	// tracking on-event time for delayed off

	uint32_t  aux_last_on_time;		// tracking on-event time for delayed off
	uint8_t   aux_last_on_voice;	// tracking on-event time for delayed off
} DRUM;

uint32_t STEP_TIME;					// global var for current time

void 	initDrum		(DRUM* _chnl, DRM_voice _main_voice, DRM_voice _aux_voice, DRM_type _chnl_type, GPIO_PinState _aux_state);
//void 	initDrum		(DRUM* _chnl, DRM_voice _main_voice, DRM_voice _aux_voice, DRM_type _aux_type);
uint8_t Update_channel	(DRUM* _chnl, uint32_t _adc_reading, GPIO_PinState _aux_state);
//
//void sendMidiGEN(uint8_t voice, uint8_t volume);	// generic midi message
//void sendMidiHH (uint8_t voice, uint8_t volume); // hihat midi message with aftertouches
//void sendMidiAS ();		// active sense messages every 300ms

//void sendMidiMessage(uint8_t *_buffer, uint16_t len); // defined in main


#endif /* DRUMIDY_H_ */
