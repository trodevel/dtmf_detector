/** Author:       Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#ifndef DTMF_DETECTOR
#define DTMF_DETECTOR

#include <cstdint>      // uint32_t

class IDtmfDetectorCallback
{
public:
    virtual ~IDtmfDetectorCallback() {};

    virtual void on_detect( const char button ) = 0;
};

// DTMF detector object

class DtmfDetector
{
public:

    // frameSize_ - input frame size
    DtmfDetector( int32_t frameSize_, IDtmfDetectorCallback * callback );
    ~DtmfDetector();

    void process( int16_t inputFrame[] ); // The DTMF detection.
    // The size of a inputFrame must be equal of a frameSize_, who
    // was set in constructor.

protected:

    // This protected function determines the tone present in a single frame.
    char DTMF_detection( int16_t short_array_samples[] );

protected:
    // These coefficients include the 8 DTMF frequencies plus 10 harmonics.
    static const unsigned COEFF_NUMBER = 18;

    // A fixed-size array to hold the coefficients
    static const int16_t CONSTANTS[COEFF_NUMBER];

    // This array keeps the entire buffer PLUS a single batch.
    int16_t *pArraySamples;

    // The magnitude of each coefficient in the current frame.  Populated
    // by goertzel_filter
    int32_t T[COEFF_NUMBER];

    // An array of size SAMPLES.  Used as input to the Goertzel function.
    int16_t *internalArray;

    // The size of the entire buffer used for processing samples.  
    // Specified at construction time.
    const int32_t frameSize; //Size of a frame is measured in int16_t(word)

    // The number of samples to utilize in a single call to Goertzel.
    // This is referred to as a frame.
    static const int32_t SAMPLES;

    // This gets used for a variety of purposes.  Most notably, it indicates
    // the start of the circular buffer at the start of ::dtmfDetecting.
    int32_t frameCount;

    // The tone detected by the previous call to DTMF_detection.
    char prevDialButton;

    // This flag is used to aggregate adjacent tones and spaces, i.e.
    //
    // 111111   222222 -> 12 (where a space represents silence)
    //
    // 1 means that during the previous iteration, we entered a tone 
    // (gone from silence to non-silence).
    // 0 means otherwise.
    //
    // While this is a member variable, it can by all means be a local
    // variable in dtmfDetecting.
    //
    // N.B. seems to not work.  In practice, you get this:
    //
    // 111111   222222 -> 1111122222
    char permissionFlag;

    // Used for quickly determining silence within a batch.
    static int32_t powerThreshold;
    //
    // dialTonesToOhersTone is the higher ratio.
    // dialTonesToOhersDialTones is the lower ratio.
    //
    // It seems like the aim of this implementation is to be more tolerant
    // towards strong "dial tones" than "tones".  The latter include
    // harmonics.
    //
    static int32_t dialTonesToOhersTones;
    static int32_t dialTonesToOhersDialTones;

private:

    IDtmfDetectorCallback * callback_;
};

#endif
