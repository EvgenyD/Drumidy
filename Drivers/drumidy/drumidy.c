#include "drumidy.h"
/*
char* ASCIILOGO = "\n"
".-.,      ,--. ,--. \n"
"`/|~\\     \\__/T`--'     . \n"
"x |`' __   ,-~^~-.___ ==I== \n"
"  |  |--| /       \\__}  | \n"
"  |  |  |{   /~\\   }    | \n"
" /|\\ \\__/ \\  \\_/  /|   /|\\ \n"
"/ | \\|  | /`~-_-~'X.\\ //| \\ \n";

char* ASCIILABEL = "\n"
" |   \\ _ _ _  _ _ __ (_)__| |_  _ \n"
" | |) | '_| || | '  \\| / _` | || |\n"
" |___/|_|  \\_,_|_|_|_|_\\__,_|\\_, |\n"
"                             |__/ \n";
*/

void initDrum(DRUM* _chnl, DRM_voice _main_voice, DRM_voice _aux_voice, DRM_type _chnl_type, GPIO_PinState _aux_state)
{
	// main configuration
	_chnl->main_voice 			= _main_voice;
	_chnl->aux_voice  			= _aux_voice;
	_chnl->chnl_type   			= _chnl_type;

	// parameters default values for cymbal
	_chnl->peak_volume_norm		= 50;		// full volume at 4096*50%=2048
	_chnl->peak_max_length		= 201;		// 201 	x0.1=20.1ms
	_chnl->peak_min_length		= 3; 		// 15 	x0.1=1.5ms
	_chnl->peak2peak 			= 1500;//2048; 	//512 mute and 2048 ramp-down
	// if the main pad is mesh, peaks are much longer
	if ((_chnl_type == MESH_PAD_AUTOAUX) || (_chnl_type == MESH_RIM_AUTOAUX))
		_chnl->peak_min_length	= 8; 		// 8 	x0.1=0.8ms

	// likely to overwrite
	_chnl->alt_voice  			= _main_voice;

	// update all variables
	_chnl->cooldown		 		= 0;
	_chnl->main_peaking			= 0;
	_chnl->main_rdy	 			= 0;
	_chnl->main_rdy_usealt		= 0;

	_chnl->main_active_max 	 	= 0;
	_chnl->main_active_length	= 0;


	_chnl->main_rdy_height 	 	= 0;
	_chnl->main_rdy_time	 	= 0;
	_chnl->main_rdy_volume	 	= 0;
	_chnl->main_rdy_length	 	= 0;


	_chnl->aux_rdy				= 0;
	_chnl->aux_rdy_time			= 0;
	_chnl->aux_active_time		= 0;

	_chnl->aux_status 			= CHANNEL_STATUS_IDLE;

	if ((_aux_state == GPIO_PIN_RESET)||(_chnl_type == CYMBAL_2_ZONE)){
		// LOW state, pad input
		_chnl->aux_type = AUX_TYPE_PAD;
		_chnl->aux_active_state	= CHANNEL_PAD_IDLE;
		_chnl->aux_rdy_state	= CHANNEL_PAD_IDLE;
	}
	if ((_aux_state == GPIO_PIN_SET)||(_chnl_type == CYMBAL_HIHAT)||(_chnl_type == CYMBAL_MUTE)){
		// HIGH state, pedal input
		_chnl->aux_type = AUX_TYPE_PEDAL;
		_chnl->aux_active_state	= CHANNEL_PEDAL_IDLE;
		_chnl->aux_rdy_state	= CHANNEL_PEDAL_IDLE;
	}

	_chnl->aux_last_state	= _aux_state;

	_chnl->main_last_on_voice 	= 0;
	_chnl->main_last_on_time 	= 0;
	_chnl->aux_last_on_voice 	= 0;
	_chnl->aux_last_on_time 	= 0;

}

// V 4.0
uint8_t Update_channel(DRUM* _chnl, uint32_t _adc_reading, GPIO_PinState _aux_state){

	// ### MAIN INPUT ###
	uint16_t thresh = PEAK_THRESHOLD;
	if (_chnl->cooldown) {
		_chnl->cooldown--;

		// if the peak is happening - no threshold
		if ( _chnl->main_peaking )
			thresh = 1;
		// until 50ms threshold = (2x max height)
		else if ( _chnl->cooldown > (_chnl->peak2peak - 512) )
			thresh = (_chnl->main_rdy_height<<1);
		// after 50ms, gradually lower the threshold from 75% until 0 after 150ms
		else
			thresh = (uint16_t)(_chnl->main_rdy_height>>5)*(uint16_t)(_chnl->cooldown>>6);
	}


	if (_adc_reading > thresh){
		// new peak, restart cooldown timer
		if (_chnl->cooldown < (_chnl->peak2peak - 512)){
			_chnl->main_peaking  = 1;

			_chnl->cooldown = _chnl->peak2peak;
			_chnl->main_active_length 	= 0;
			_chnl->main_active_max 		= 0;
		}

		// increment peak length until max length is reached
		_chnl->main_active_length +=1;

		// if the value is bigger then max, update max
		if (_adc_reading > _chnl->main_active_max)
			_chnl->main_active_max  = _adc_reading;

		// CHECKING END CONDITION
		// End of peak, if the point is lower then half of the max
		if (_adc_reading < (_chnl->main_active_max>>1)) {
			_chnl->main_peaking = 0;

			// option 1. short peaks higher than 40% of max volume (50*8=400 ADC)
			if ((_chnl->main_active_length < _chnl->peak_min_length) && (_chnl->main_active_max > (_chnl->peak_volume_norm<<3)) ) {
				if ((_chnl->chnl_type == MESH_RIM_AUTOAUX)){
					_chnl->main_rdy 		= 1;
					_chnl->main_rdy_usealt 	= 0;
					_chnl->main_rdy_time	= STEP_TIME;
					_chnl->main_rdy_height 	= _chnl->main_active_max;
					_chnl->main_rdy_length	= _chnl->main_active_length;	//debug only
					_chnl->main_rdy_usealt = 1;
				}
			// option 2. normal length peaks
			} else if ( ((_chnl->main_active_length >= _chnl->peak_min_length)  ) //|| (_chnl->main_active_max < (_chnl->peak_volume_norm<<2))
					  && (_chnl->main_active_length <  _chnl->peak_max_length)) {
				_chnl->main_rdy 		= 1;
				_chnl->main_rdy_usealt 	= 0;
				_chnl->main_rdy_time	= STEP_TIME;
				_chnl->main_rdy_height 	= _chnl->main_active_max;
				_chnl->main_rdy_length	= _chnl->main_active_length;	//debug only

				// handle hihat case
				if ((_chnl->chnl_type == CYMBAL_HIHAT) && (_chnl->aux_status == CHANNEL_STATUS_PRESSED))
					_chnl->main_rdy_usealt = 1;
			// option 3 - inconsistent peak:
			}else{
//				if (_chnl->cooldown < (_chnl->peak2peak - 512)){
					_chnl->cooldown = 0;
					_chnl->main_rdy_height 	= 0;

//				}
			}

			// reset process
			_chnl->main_active_length 	= 0;
			_chnl->main_active_max 		= 0;

		}//end adc<max/2
	}

	// ### AUX INPUT ###

	if (_chnl->aux_type == AUX_TYPE_PAD){

		if (_aux_state == GPIO_PIN_RESET)
			_chnl->aux_active_state = CHANNEL_PAD_IDLE;
		else
			_chnl->aux_active_state = CHANNEL_PAD_TRIGGERED;

		// new peak started
		if ((_chnl->aux_active_state == CHANNEL_PAD_TRIGGERED) && (_chnl->aux_last_state == CHANNEL_PAD_IDLE)){
			_chnl->aux_status		 = CHANNEL_STATUS_PEAK;
		}

		// peak ended
		if ((_chnl->aux_active_state == CHANNEL_PAD_IDLE) && (_chnl->aux_last_state == CHANNEL_PAD_TRIGGERED)){
			if (STEP_TIME - _chnl->aux_rdy_time > AUX_PEAK2PEAK) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
				_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			}
			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
		}
		_chnl->aux_last_state = _chnl->aux_active_state;

	} else { //AUX_TYPE_PEDAL

		if (_aux_state == GPIO_PIN_SET)
			_chnl->aux_active_state = CHANNEL_PEDAL_IDLE;
		else
			_chnl->aux_active_state = CHANNEL_PEDAL_PRESSED;


		// pedal pressed
		if ((_chnl->aux_active_state == CHANNEL_PEDAL_PRESSED) && (_chnl->aux_last_state == CHANNEL_PEDAL_IDLE)){
			if (STEP_TIME - _chnl->aux_rdy_time > AUX_PEAK2PEAK) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
			}
			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			_chnl->aux_status		 = CHANNEL_STATUS_PRESSED;

		}

		// pedal released
		if ((_chnl->aux_active_state == CHANNEL_PEDAL_IDLE) && (_chnl->aux_last_state == CHANNEL_PEDAL_PRESSED)){
			if (STEP_TIME - _chnl->aux_rdy_time > AUX_PEAK2PEAK) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
			}
			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
		}
		_chnl->aux_last_state = _chnl->aux_active_state;
	}

	// for fun return 1 if there is data to report
	return (_chnl->main_rdy | _chnl->aux_rdy);
}


//// TODO: change algorithm to something with timer
////void sendMidiAS ();
//// V 3.0, no rim
//// still noisy
//uint8_t Update_channel(DRUM* _chnl, uint32_t _adc_reading, GPIO_PinState _aux_state){
//	// ### MAIN INPUT ###
//	uint16_t thresh = PEAK_THRESHOLD;
//
//	if (_chnl->cooldown) {
//		_chnl->cooldown--;
//
//		if (_chnl->cooldown == (_chnl->peak2peak>>1)){
//			// TODO: send OFF event
//			if (_chnl->main_rdy_event == EVENT_NEW_PEAK){
//				_chnl->main_rdy = 1;
//				_chnl->main_rdy_event  = EVENT_MAIN_OFF;
//				_chnl->main_active_length = 0;
//			}
//		}
//
//		if (_chnl->cooldown > (_chnl->peak2peak>>1))
//			thresh = (_chnl->main_rdy_height   ) + PEAK_THRESHOLD;
//		else
//			thresh = (_chnl->main_rdy_height>>1) + PEAK_THRESHOLD;
//
//		if ( _chnl->main_peaking )
//			thresh = 1;
//	}
//
//	if (_adc_reading > thresh){
//		// there is a peak going on
//
//		// new peak, restart cooldown timer
//		if (_chnl->main_peaking == 0){
//			_chnl->cooldown = _chnl->peak2peak;
//			_chnl->main_active_length = 0;
//			_chnl->main_active_max = 0;
//			_chnl->main_rdy_event = EVENT_EMPTY;
//		}
//
//		_chnl->main_peaking = 1;
//
//		// increment peak length untill max length is reached
//		_chnl->main_active_length += 1;
//
//		// if the value is bigger then max, update max
//		if (_adc_reading > _chnl->main_active_max)
//			_chnl->main_active_max  = _adc_reading;
//
//		// CHECKING END CONDITION
//		// End of peak, if the point is lower then half of the max
//		if (_adc_reading < (_chnl->main_active_max>>1)) {
//			_chnl->main_peaking = 0;
//			// if peak is too long, or single point - ignore it and restart
//			if ((_chnl->main_active_length > _chnl->peak_max_length) || (_chnl->main_active_length < 3)){
//				_chnl->main_active_length = 0;
//				_chnl->main_active_max = 0;
//			}else{
//				// if normal length, or very weak
//				if ((_chnl->main_active_length > _chnl->peak_min_length) || (_chnl->main_active_max < (PEAK_THRESHOLD << 2)) ) {
//					_chnl->main_rdy 		=  1;
//					_chnl->main_rdy_time	=  STEP_TIME;
//					_chnl->main_rdy_height 	= _chnl->main_active_max;
//					_chnl->main_rdy_length	= _chnl->main_active_length ;
//
//					// handle hihat case
//					if ((_chnl->chnl_type == CYMBAL_HIHAT) && (_chnl->aux_status == CHANNEL_STATUS_PRESSED))
//						_chnl->main_rdy_event = EVENT_NEW_ALT_PEAK;
//					//== default case ==//
//					else
//						_chnl->main_rdy_event = EVENT_NEW_PEAK;
//				}
//				_chnl->main_active_length = 0;
//				_chnl->main_active_max = 0;
//			}
//
//		}
//	}
//
//	// ### AUX INPUT ###
//
//	if (_chnl->aux_type == AUX_TYPE_PAD){
//
//		if (_aux_state == GPIO_PIN_RESET)
//			_chnl->aux_active_state = CHANNEL_PAD_IDLE;
//		else
//			_chnl->aux_active_state = CHANNEL_PAD_TRIGGERED;
//
//		// new peak started
//		if ((_chnl->aux_active_state == CHANNEL_PAD_TRIGGERED) && (_chnl->aux_last_state == CHANNEL_PAD_IDLE)){
//			_chnl->aux_status		 = CHANNEL_STATUS_PEAK;
//		}
//
//		// peak ended
//		if ((_chnl->aux_active_state == CHANNEL_PAD_IDLE) && (_chnl->aux_last_state == CHANNEL_PAD_TRIGGERED)){
//			if (STEP_TIME - _chnl->aux_rdy_time > _chnl->time_between_peaks) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
////				_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			}
//			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
//		}
//		_chnl->aux_last_state = _chnl->aux_active_state;
//
//	} else { //AUX_TYPE_PEDAL
//
//		if (_aux_state == GPIO_PIN_SET)
//			_chnl->aux_active_state = CHANNEL_PEDAL_IDLE;
//		else
//			_chnl->aux_active_state = CHANNEL_PEDAL_PRESSED;
//
//
//		// pedal pressed
//		if ((_chnl->aux_active_state == CHANNEL_PEDAL_PRESSED) && (_chnl->aux_last_state == CHANNEL_PEDAL_IDLE)){
//			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
//			}
//			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			_chnl->aux_status		 = CHANNEL_STATUS_PRESSED;
//
//			// pedal changes voice for hihat
////			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 1;
//
//		}
//
//		// pedal released
//		if ((_chnl->aux_active_state == CHANNEL_PEDAL_IDLE) && (_chnl->aux_last_state == CHANNEL_PEDAL_PRESSED)){
//			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
//			}
//			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
//
//			// pedal changes voice for hihat
////			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 0;
//		}
//		_chnl->aux_last_state = _chnl->aux_active_state;
//	}
//
//	// for fun return 1 if there is data to report
//	return (_chnl->main_rdy | _chnl->aux_rdy);
//}

// V2.0
/*
uint8_t Update_channel(DRUM* _chnl, uint32_t _adc_reading, GPIO_PinState _aux_state){
	// ### MAIN INPUT ###
	uint16_t thresh = PEAK_THRESHOLD;

	if (_chnl->cooldown) {
		_chnl->cooldown--;

		if (_chnl->cooldown == (_chnl->peak2peak>>1)){
			// TODO: send OFF event
			if (_chnl->main_rdy_event == EVENT_NEW_ALT_PEAK){
				_chnl->main_rdy = 1;
				_chnl->main_rdy_event  = EVENT_ALT_OFF;
				_chnl->main_active_length = 0;
			}

			if (_chnl->main_rdy_event == EVENT_NEW_PEAK){
				_chnl->main_rdy = 1;
				_chnl->main_rdy_event  = EVENT_MAIN_OFF;
				_chnl->main_active_length = 0;
			}

		}

		if (_chnl->cooldown > (_chnl->peak2peak>>1))
			thresh = (_chnl->main_rdy_height   ) + PEAK_THRESHOLD;
		else
			thresh = (_chnl->main_rdy_height>>1) + PEAK_THRESHOLD;

		if ( _chnl->main_peaking )
			thresh = 1;
	}

	if (_adc_reading > thresh){
		// there is a peak going on

		// new peak, restart cooldown timer
		if (_chnl->main_peaking == 0){
			_chnl->cooldown = _chnl->peak2peak;
			_chnl->main_active_length = 0;
			_chnl->main_active_max = 0;
			_chnl->main_rdy_event = EVENT_EMPTY;
		}

		_chnl->main_peaking = 1;

//		if (_chnl->cooldown < (_chnl->peak2peak>>1)){
//		}

		// increment peak length untill max length is reached
		_chnl->main_active_length += 1;

		// if the value is bigger then max, update max
		if (_adc_reading > _chnl->main_active_max)
			_chnl->main_active_max  = _adc_reading;

		// CHECKING END CONDITION
		// End of peak, if the point is lower then half of the max
		if (_adc_reading < (_chnl->main_active_max>>1)) {
			_chnl->main_peaking = 0;
			// if peak is too long, or single point - ignore it and restart
			if ((_chnl->main_active_length > _chnl->peak_max_length) || (_chnl->main_active_length < 3)){
				_chnl->main_active_length = 0;
				_chnl->main_active_max = 0;
			}else{
				// if normal length, or very weak
				if ((_chnl->main_active_length > _chnl->peak_min_length) || (_chnl->main_active_max < (PEAK_THRESHOLD << 2)) ) {
					_chnl->main_rdy 		=  1;
					_chnl->main_rdy_time	=  STEP_TIME;
					_chnl->main_rdy_height 	= _chnl->main_active_max;
					_chnl->main_rdy_length	= _chnl->main_active_length ;

					// handle rim hit case
					if ( (_chnl->chnl_type == MESH_RIM_AUTOAUX) && (_chnl->main_rdy_event == EVENT_NEW_ALT_PEAK) )
						_chnl->main_rdy_event = EVENT_ALT2MAIN;
					// handle hihat case
					else if ((_chnl->chnl_type == CYMBAL_HIHAT) && (_chnl->aux_status == CHANNEL_STATUS_PRESSED))
						_chnl->main_rdy_event = EVENT_NEW_ALT_PEAK;
					//== default case ==//
					else
						_chnl->main_rdy_event = EVENT_NEW_PEAK;


				}else{
					if ((_chnl->chnl_type == MESH_RIM_AUTOAUX)&&(STEP_TIME- _chnl->main_rdy_time > (_chnl->peak2peak<<1))){
					_chnl->main_rdy 		=  1;
					_chnl->main_rdy_height 	= _chnl->main_active_max;
					_chnl->main_rdy_event = EVENT_NEW_ALT_PEAK;
					_chnl->main_rdy_length	= _chnl->main_active_length ;
				}
				}
				_chnl->main_active_length = 0;
				_chnl->main_active_max = 0;
			}

		}
	}

	// ### AUX INPUT ###

	if (_chnl->aux_type == AUX_TYPE_PAD){

		if (_aux_state == GPIO_PIN_RESET)
			_chnl->aux_active_state = CHANNEL_PAD_IDLE;
		else
			_chnl->aux_active_state = CHANNEL_PAD_TRIGGERED;

		// new peak started
		if ((_chnl->aux_active_state == CHANNEL_PAD_TRIGGERED) && (_chnl->aux_last_state == CHANNEL_PAD_IDLE)){
			_chnl->aux_status		 = CHANNEL_STATUS_PEAK;
		}

		// peak ended
		if ((_chnl->aux_active_state == CHANNEL_PAD_IDLE) && (_chnl->aux_last_state == CHANNEL_PAD_TRIGGERED)){
			if (STEP_TIME - _chnl->aux_rdy_time > _chnl->time_between_peaks) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
//				_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			}
			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
		}
		_chnl->aux_last_state = _chnl->aux_active_state;

	} else { //AUX_TYPE_PEDAL

		if (_aux_state == GPIO_PIN_SET)
			_chnl->aux_active_state = CHANNEL_PEDAL_IDLE;
		else
			_chnl->aux_active_state = CHANNEL_PEDAL_PRESSED;


		// pedal pressed
		if ((_chnl->aux_active_state == CHANNEL_PEDAL_PRESSED) && (_chnl->aux_last_state == CHANNEL_PEDAL_IDLE)){
			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
			}
			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			_chnl->aux_status		 = CHANNEL_STATUS_PRESSED;

			// pedal changes voice for hihat
//			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 1;

		}

		// pedal released
		if ((_chnl->aux_active_state == CHANNEL_PEDAL_IDLE) && (_chnl->aux_last_state == CHANNEL_PEDAL_PRESSED)){
			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
				_chnl->aux_rdy			 = 1;
				_chnl->aux_rdy_time		 = STEP_TIME;
			}
			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;

			// pedal changes voice for hihat
//			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 0;
		}
		_chnl->aux_last_state = _chnl->aux_active_state;
	}

	// for fun return 1 if there is data to report
	return (_chnl->main_rdy | _chnl->aux_rdy);
}
*/
// old version

// TODO: change algorithm to something with timer
//void sendMidiAS ();
//uint8_t Update_channel(DRUM* _chnl, uint32_t _adc_reading, GPIO_PinState _aux_state){
//	// ### MAIN INPUT ###
//	//	set threshold level
//	uint16_t thresh = PEAK_THRESHOLD;
//	if ((STEP_TIME - _chnl->main_rdy_time) < _chnl->time_between_peaks){
//		thresh = (_chnl->main_rdy_height>>1) +  PEAK_THRESHOLD;
//		// if sooner then half way between peaks, accept only higher peaks
//		if ((STEP_TIME - _chnl->main_rdy_time) < (_chnl->time_between_peaks>>1))
//			thresh =_chnl->main_rdy_height;
//	}else{
//		if (_chnl->main_status == CHANNEL_STATUS_MUTED)
//			_chnl->main_status  = CHANNEL_STATUS_IDLE;
//	}
//
//	if (_adc_reading > thresh){
//		// increment peak length untill max length is reached
//		_chnl->main_active_length += 1;
//
//		// if this is first point in the peak, update starting time
//		if (_chnl->main_active_time == 0){
//			_chnl->main_active_time = STEP_TIME;
//			_chnl->main_status 		= CHANNEL_STATUS_PEAK;
//		}
//		// if the value is bigger then max, update max
//		if (_adc_reading > _chnl->main_active_max)
//					       _chnl->main_active_max  = _adc_reading;
//
//
//		// if the point is lower then half of the peak and not too long
//		if ( ((_adc_reading < (_chnl->main_active_max>>1)) ) && (_chnl->main_active_length < _chnl->peak_max_length) ) {
//
//			if ((_chnl->main_active_length > _chnl->peak_min_length) || (_chnl->main_active_max < (PEAK_THRESHOLD<<3))){
//				// set values to report
//				_chnl->main_rdy 		=  1;
//				_chnl->main_rdy_height 	= _chnl->main_active_max;
//				_chnl->main_rdy_length 	= _chnl->main_active_length;
//				_chnl->main_rdy_time 	= _chnl->main_active_time;
//
//				// if the hihat pedal is presed - send alt voice
//				if ((_chnl->chnl_type == CYMBAL_HIHAT) && (_chnl->aux_status == CHANNEL_STATUS_PRESSED))
//					_chnl->main_alt_rdy		=  1;
//				else
//					_chnl->main_alt_rdy		=  0;
//
//				//clean variables for next cycle
//				_chnl->main_active_max    = 0;
//				_chnl->main_active_time	  = 0;
//				_chnl->main_active_length = 0;
//				_chnl->main_status		  = CHANNEL_STATUS_MUTED;
//
//			} else // when peak too short then process the rim event
//				if (_chnl->chnl_type == MESH_RIM_AUTOAUX){
//					// check if the main event was long ago enough, or if it was also a rim shoot
//					if ((STEP_TIME - _chnl->main_rdy_time > (_chnl->time_between_peaks<<1))||(_chnl->main_alt_rdy == 1)){
//						// set values to report
//						_chnl->main_rdy 			=  1;
//						_chnl->main_alt_rdy			=  1;
//						_chnl->main_rdy_height 		= _chnl->main_active_max;//<<2;
//						_chnl->main_rdy_length 		= _chnl->main_active_length;
//						_chnl->main_rdy_time 		= _chnl->main_active_time;
//
//						//clean variables for next cycle
//						_chnl->main_active_max		= 0;
//						_chnl->main_active_time		= 0;
//						_chnl->main_active_length	= 0;
//						_chnl->main_status 			= CHANNEL_STATUS_MUTED;
//					}
//			}
//		}
//
//	} else {
//		if (_chnl->main_active_length > 2){
//			_chnl->main_rdy 		=  1;
//			_chnl->main_rdy_height 	= _chnl->main_active_max;
//			_chnl->main_rdy_length 	= _chnl->main_active_length;
//			_chnl->main_rdy_time 	= _chnl->main_active_time;
//
//			// if the hihat pedal is presed - send alt voice
//			if ((_chnl->chnl_type == CYMBAL_HIHAT) && (_chnl->aux_status == CHANNEL_STATUS_PRESSED))
//				_chnl->main_alt_rdy		=  1;
//			else
//				_chnl->main_alt_rdy		=  0;
//		}
//		_chnl->main_active_length = 0;
//		_chnl->main_active_time	  = 0;
//		_chnl->main_active_max    = 0;
//		_chnl->main_status		  = CHANNEL_STATUS_MUTED;
//	}
//
//	// ### AUX INPUT ###
//
//	if (_chnl->aux_type == AUX_TYPE_PAD){
//
//		if (_aux_state == GPIO_PIN_RESET)
//			_chnl->aux_active_state = CHANNEL_PAD_IDLE;
//		else
//			_chnl->aux_active_state = CHANNEL_PAD_TRIGGERED;
//
//		// new peak started
//		if ((_chnl->aux_active_state == CHANNEL_PAD_TRIGGERED) && (_chnl->aux_last_state == CHANNEL_PAD_IDLE)){
//			_chnl->aux_status		 = CHANNEL_STATUS_PEAK;
//		}
//
//		// peak ended
//		if ((_chnl->aux_active_state == CHANNEL_PAD_IDLE) && (_chnl->aux_last_state == CHANNEL_PAD_TRIGGERED)){
//			if (STEP_TIME - _chnl->aux_rdy_time > _chnl->time_between_peaks) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
////				_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			}
//			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
//		}
//		_chnl->aux_last_state = _chnl->aux_active_state;
//
//	} else { //AUX_TYPE_PEDAL
//
//		if (_aux_state == GPIO_PIN_SET)
//			_chnl->aux_active_state = CHANNEL_PEDAL_IDLE;
//		else
//			_chnl->aux_active_state = CHANNEL_PEDAL_PRESSED;
//
//
//		// pedal pressed
//		if ((_chnl->aux_active_state == CHANNEL_PEDAL_PRESSED) && (_chnl->aux_last_state == CHANNEL_PEDAL_IDLE)){
//			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
//			}
//			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			_chnl->aux_status		 = CHANNEL_STATUS_PRESSED;
//
//			// pedal changes voice for hihat
////			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 1;
//
//		}
//
//		// pedal released
//		if ((_chnl->aux_active_state == CHANNEL_PEDAL_IDLE) && (_chnl->aux_last_state == CHANNEL_PEDAL_PRESSED)){
//			if (STEP_TIME - _chnl->aux_rdy_time > (_chnl->time_between_peaks<<1)) {
//				_chnl->aux_rdy			 = 1;
//				_chnl->aux_rdy_time		 = STEP_TIME;
//			}
//			_chnl->aux_rdy_state	 = _chnl->aux_active_state ;
//			_chnl->aux_status		 = CHANNEL_STATUS_IDLE;
//
//			// pedal changes voice for hihat
////			if (_chnl->chnl_type == CYMBAL_HIHAT)	_chnl->main_alt_rdy = 0;
//
//		}
//		_chnl->aux_last_state = _chnl->aux_active_state;
//
//
//	}
//
//	// for fun return 1 if there is data to report
//	return (_chnl->main_rdy | _chnl->aux_rdy);
//}
