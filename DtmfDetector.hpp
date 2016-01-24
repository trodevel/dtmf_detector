/** Author:       Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
 *                Sergey Kolevatov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#ifndef DTMF_DETECTOR
#define DTMF_DETECTOR

#include <cstdint>      // uint32_t
#include <vector>       // std::vector

#include "IDtmfDetectorCallback.hpp"    // tone_e

namespace dtmf
{

class IDtmfDetectorCallback;

// DTMF detector object

class DtmfDetector
{
public:

    // frame_size - input frame size
    DtmfDetector(
            int32_t sampling_rate = 8000 );
    ~DtmfDetector();

    void init_callback( IDtmfDetectorCallback * callback );

    // The DTMF detection.
    // Size of a frame is measured in int16_t(word)
    void process( const int16_t * input_frame, uint32_t frame_size );

protected:

    enum class tone_type_e
    {
        UNDEF,
        SILENCE,
        TONE,
    };


    // This protected function determines the tone present in a single frame.
    tone_type_e detect_dtmf( int16_t short_array_samples[], tone_e & tone );

    tone_e row_column_to_tone( int32_t row, int32_t column );

protected:
    // These coefficients include the 8 DTMF frequencies plus 10 harmonics.
    static const unsigned COEFF_NUMBER = 18;

    // A fixed-size array to hold the coefficients
    static const int16_t CONSTANTS_8KHz[COEFF_NUMBER];
    static const int16_t CONSTANTS_16KHz[COEFF_NUMBER];
    static const int16_t CONSTANTS_44_1KHz[COEFF_NUMBER];

    // This array keeps the entire buffer PLUS a single batch.
    std::vector<int16_t> array_samples_;

    // The magnitude of each coefficient in the current frame.  Populated
    // by goertzel_filter
    int32_t T[COEFF_NUMBER];

    // An array of size SAMPLES.  Used as input to the Goertzel function.
    int16_t *internal_array_;

    // The number of samples to utilize in a single call to Goertzel.
    // This is referred to as a frame.
    uint32_t SAMPLES;

    // The tone detected by the previous call to DTMF_detection.
    tone_e      prev_dial_button_;
    tone_type_e prev_tone_type_;

    // Used for quickly determining silence within a batch.
    static int32_t power_threshold_;
    //
    // dial_tones_to_ohers_tones_ is the higher ratio.
    // dial_tones_to_ohers_dial_tones_ is the lower ratio.
    //
    // It seems like the aim of this implementation is to be more tolerant
    // towards strong "dial tones" than "tones".  The latter include
    // harmonics.
    //
    static int32_t dial_tones_to_ohers_tones_;
    static int32_t dial_tones_to_ohers_dial_tones_;

private:

    IDtmfDetectorCallback   * callback_;

    const int16_t           * CONSTANTS;
};

} // namespace dtmf

#endif // DTMF_DETECTOR
