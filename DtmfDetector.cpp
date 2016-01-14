/** Author:       Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#include <cassert>
#include "DtmfDetector.hpp"

#include "IDtmfDetectorCallback.hpp"    // IDtmfDetectorCallback

#if DEBUG
#include <cstdio>
#endif

namespace dtmf
{

// This is the same function as in DtmfGenerator.cpp
static inline int32_t MPY48SR( int16_t o16, int32_t o32 )
{
    uint32_t Temp0;
    int32_t Temp1;
    Temp0 = ( ( (uint16_t)o32 * o16 ) + 0x4000 ) >> 15;
    Temp1 = (int16_t)( o32 >> 16 ) * o16;
    return ( Temp1 << 1 ) + Temp0;
}

// The Goertzel algorithm.
// For a good description and walkthrough, see:
// https://sites.google.com/site/hobbydebraj/home/goertzel-algorithm-dtmf-detection
//
// Koeff0           Coefficient for the first frequency.
// Koeff1           Coefficient for the second frequency.
// arraySamples     Input samples to process.  Must be COUNT elements long.
// Magnitude0       Detected magnitude of the first frequency.
// Magnitude1       Detected magnitude of the second frequency.
// COUNT            The number of elements in arraySamples.  Always equal to
//                  SAMPLES in practice.
static void goertzel_filter(
        int16_t         Koeff0,
        int16_t         Koeff1,
        const int16_t   arraySamples[],
        int32_t         *Magnitude0,
        int32_t         *Magnitude1,
        uint32_t        COUNT )
{
    int32_t Temp0, Temp1;
    uint16_t ii;
    // Vk1_0    prev (first frequency)
    // Vk2_0    prev_prev (first frequency)
    //
    // Vk1_1    prev (second frequency)
    // Vk2_0    prev_prev (second frequency)
    int32_t Vk1_0 = 0, Vk2_0 = 0, Vk1_1 = 0, Vk2_1 = 0;

    // Iterate over all the input samples
    // For each sample, process the two frequencies we're interested in:
    // output = Input + 2*coeff*prev - prev_prev
    // N.B. bit-shifting to the left achieves the multiplication by 2.
    for( ii = 0; ii < COUNT; ++ii )
    {
        Temp0 = MPY48SR( Koeff0, Vk1_0 << 1 ) - Vk2_0 + arraySamples[ii], Temp1 = MPY48SR( Koeff1, Vk1_1 << 1 ) - Vk2_1 + arraySamples[ii];
        Vk2_0 = Vk1_0, Vk2_1 = Vk1_1;
        Vk1_0 = Temp0, Vk1_1 = Temp1;
    }

    // Magnitude: prev_prev**prev_prev + prev*prev - coeff*prev*prev_prev

    // TODO: what does shifting by 10 bits to the right achieve?  Probably to
    // make room for the magnitude calculations.
    Vk1_0 >>= 10, Vk1_1 >>= 10, Vk2_0 >>= 10, Vk2_1 >>= 10;
    Temp0 = MPY48SR( Koeff0, Vk1_0 << 1 ), Temp1 = MPY48SR( Koeff1, Vk1_1 << 1 );
    Temp0 = (int16_t)Temp0 * (int16_t)Vk2_0, Temp1 = (int16_t)Temp1 * (int16_t)Vk2_1;
    Temp0 = (int16_t)Vk1_0 * (int16_t)Vk1_0 + (int16_t)Vk2_0 * (int16_t)Vk2_0 - Temp0;
    Temp1 = (int16_t)Vk1_1 * (int16_t)Vk1_1 + (int16_t)Vk2_1 * (int16_t)Vk2_1 - Temp1;
    *Magnitude0 = Temp0, *Magnitude1 = Temp1;
    return;
}

// This is a GSM function, for concrete processors she may be replaced
// for same processor's optimized function (norm_l)
// This is a GSM function, for concrete processors she may be replaced
// for same processor's optimized function (norm_l)
//
// This function is used for normalization. TODO: how exactly does it work?
static inline int16_t norm_l( int32_t L_var1 )
{
    int16_t var_out;

    if( L_var1 == 0 )
    {
        var_out = 0;
    }
    else
    {
        if( L_var1 == (int32_t)0xffffffff )
        {
            var_out = 31;
        }
        else
        {
            if( L_var1 < 0 )
            {
                L_var1 = ~L_var1;
            }

            for( var_out = 0; L_var1 < (int32_t)0x40000000; var_out++ )
            {
                L_var1 <<= 1;
            }
        }
    }

    return ( var_out );
}

const unsigned DtmfDetector::COEFF_NUMBER;
// These frequencies are slightly different to what is in the generator.
// More importantly, they are also different to what is described at:
// http://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling
//
// Some frequencies seem to match musical notes:
// http://www.geocities.jp/pyua51113/artist_data/ki_setsumei.html
// http://en.wikipedia.org/wiki/Piano_key_frequencies
//
// It seems this is done to simplify harmonic detection.
//
const int16_t DtmfDetector::CONSTANTS_8KHz[COEFF_NUMBER] =
{
        27860,  // 0: 706Hz, harmonics include: 78Hz, 235Hz, 3592Hz
        26745,  // 1: 784Hz, apparently a high G, harmonics: 78Hz
        25529,  // 2: 863Hz, harmonics: 78Hz
        24216,  // 3: 941Hz, harmonics: 78Hz, 235Hz, 314Hz
        19747,  // 4: 1176Hz, harmonics: 78Hz, 235Hz, 392Hz, 3529Hz
        16384,  // 5: 1333Hz, harmonics: 78Hz
        12773,  // 6: 1490Hz, harmonics: 78Hz, 2980Hz
        8967,   // 7: 1547Hz, harmonics: 314Hz, 392Hz
                // The next coefficients correspond to frequencies of harmonics of the
                // near-DTMF frequencies above, as well as of other frequencies listed
                // below.
        21319,  // 1098Hz
        29769,  // 549Hz
                // 549Hz is:
                // - half of 1098Hz (see above)
                // - 1/3 of 1633Hz, a real DTMF frequency (see DtmfGenerator)
        32706,  // 78Hz, a very low D# on a piano
                // 78Hz is a very convenient frequency, since its (approximately):
                // - 1/3 of 235Hz (not a DTMF frequency, but we do detect it, see below)
                // - 1/4 of 314Hz (not a DTMF frequency, but we do detect it, see below)
                // - 1/5 of 392Hz (not a DTMF frequency, but we do detect it, see below)
                // - 1/7 of 549Hz
                // - 1/9 of 706Hz
                // - 1/10 of 784Hz
                // - 1/11 of 863Hz
                // - 1/12 of 941Hz
                // - 1/14 of 1098Hz (not a DTMF frequency, but we do detect it, see above)
                // - 1/15 of 1176Hz
                // - 1/17 of 1333Hz
                // - 1/19 of 1490Hz
        32210,  // 235Hz
                // 235Hz is:
                // - 1/3 of 706Hz
                // - 1/4 of 941Hz
                // - 1/5 of 1176Hz
                // - 1/15 of 3529Hz (not a DTMF frequency, but we do detect it, see below)
        31778,  // 314Hz
                // 314Hz is:
                // - 1/3 of 941Hz
                // - 1/5 of 1547Hz
                // - 1/8 of 2510Hz (not a DTMF frequency, but we do detect it, see below)
        31226,  // 392Hz, apparently a middle-2 G
                // 392Hz is:
                // - 1/2 of 794Hz
                // - 1/3 of 1176Hz
                // - 1/4 of 1547Hz
                // - 1/9 of 3529Hz
        -1009,  // 2039Hz TODO: why is this frequency useful?
        -12772, // 2510Hz, which is 8*314Hz
        -22811, // 2980Hz, which is 2*1490Hz
        -30555  // 3529Hz, 3*1176Hz, 5*706Hz
        };

const int16_t DtmfDetector::CONSTANTS_16KHz[COEFF_NUMBER] =
{
        31516,
        31226,
        30903,
        30555,
        29335,
        28379,
        27316,
        26149,
        29768,
        32008,
        32752,
        32628,
        32518,
        32380,
        22812,
        18097,
        12777,
        6026
        };

const int16_t DtmfDetector::CONSTANTS_44_1KHz[COEFF_NUMBER] =
{
        32601,
        32563,
        32520,
        32473,
        32308,
        32178,
        32031,
        31869,
        32367,
        32667,
        32765,
        32749,
        32734,
        32716,
        31394,
        30694,
        29858,
        28712,
        };

int32_t DtmfDetector::power_threshold_ = 328;
int32_t DtmfDetector::dial_tones_to_ohers_tones_ = 16;
int32_t DtmfDetector::dial_tones_to_ohers_dial_tones_ = 6;
//--------------------------------------------------------------------
DtmfDetector::DtmfDetector(
        int32_t frame_size,
        IDtmfDetectorCallback * callback,
        int32_t sampling_rate ) :
        frame_size_( frame_size ), callback_( callback ),
        CONSTANTS( nullptr )
{
    if( sampling_rate == 44100 )
    {
        CONSTANTS   = CONSTANTS_44_1KHz;
        SAMPLES     = 512;
    }
    else if( sampling_rate == 16000 )
    {
        CONSTANTS   = CONSTANTS_16KHz;
        SAMPLES     = 204;
    }
    else
    {
        CONSTANTS   = CONSTANTS_8KHz;
        SAMPLES     = 102;
    }

    //
    // This array is padded to keep the last batch, which is smaller
    // than SAMPLES, from the previous call to dtmfDetecting.
    //
    ptr_array_samples_  = new int16_t[frame_size_ + SAMPLES];
    internal_array_     = new int16_t[SAMPLES];
    frame_count_        = 0;
    prev_dial_button_   = ' ';
    permission_flag_    = 0;
}
//---------------------------------------------------------------------
DtmfDetector::~DtmfDetector()
{
    delete[] ptr_array_samples_;
    delete[] internal_array_;
}

void DtmfDetector::process( int16_t input_array[] )
{
    // ii                   Variable for iteration
    // temp_dial_button     A tone detected in part of the input_array
    uint32_t ii;
    char temp_dial_button;

    // Copy the input array into the middle of pArraySamples.
    // I think the first frameCount samples contain the last batch from the
    // previous call to this function.
    for( ii = 0; ii < frame_size_; ii++ )
    {
        ptr_array_samples_[ii + frame_count_] = input_array[ii];
    }

    frame_count_ += frame_size_;
    // Read index into pArraySamples that corresponds to the current batch.
    uint32_t temp_index = 0;
    // If don't have enough samples to process an entire batch, then don't
    // do anything.
    if( frame_count_ >= SAMPLES )
    {
        // Process samples while we still have enough for an entire
        // batch.
        while( frame_count_ >= SAMPLES )
        {
            // Determine the tone present in the current batch
            temp_dial_button = detect_dtmf( &ptr_array_samples_[temp_index] );

            // Determine if we should register it as a new tone, or
            // ignore it as a continuation of a previously
            // registered tone.
            //
            // This seems buggy.  Consider a sequence of three
            // tones, with each tone corresponding to the dominant
            // tone in a batch of SAMPLES samples:
            //
            // SILENCE TONE_A TONE_B will get registered as TONE_B
            //
            // TONE_A will be ignored.
            if( permission_flag_ )
            {
                if( temp_dial_button != ' ' )
                {
                    if( callback_ )
                        callback_->on_detect( temp_dial_button );
                }
                permission_flag_ = 0;
            }

            // If we've gone from silence to a tone, set the flag.
            // The tone will be registered in the next iteration.
            if( ( temp_dial_button != ' ' ) && ( prev_dial_button_ == ' ' ) )
            {
                permission_flag_ = 1;
            }

            // Store the current tone.  In light of the above
            // behaviour, all that really matters is whether it was
            // a tone or silence.  Finally, move on to the next
            // batch.
            prev_dial_button_ = temp_dial_button;

            temp_index += SAMPLES;
            frame_count_ -= SAMPLES;
        }

        //
        // We have frameCount samples left to process, but it's not
        // enough for an entire batch.  Shift these left-over
        // samples to the beginning of our array and deal with them
        // next time this function is called.
        //
        for( ii = 0; ii < frame_count_; ii++ )
        {
            ptr_array_samples_[ii] = ptr_array_samples_[ii + temp_index];
        }
    }

}
//-----------------------------------------------------------------
// Detect a tone in a single batch of samples (SAMPLES elements).
char DtmfDetector::detect_dtmf( int16_t short_array_samples[] )
{
    int32_t Dial = 32;
    char return_value = ' ';
    unsigned ii;
    int32_t Sum = 0;

    // Dial         TODO: what is this?
    // Sum          Sum of the absolute values of samples in the batch.
    // return_value The tone detected in this batch (can be silence).
    // ii           Iteration variable

    // Quick check for silence.
    for( ii = 0; ii < SAMPLES; ii++ )
    {
        if( short_array_samples[ii] >= 0 )
            Sum += short_array_samples[ii];
        else
            Sum -= short_array_samples[ii];
    }
    Sum /= SAMPLES;
    if( Sum < power_threshold_ )
        return ' ';

    //Normalization
    // Iterate over each sample.
    // First, adjusting Dial to an appropriate value for the batch.
    for( ii = 0; ii < SAMPLES; ii++ )
    {
        T[0] = static_cast<int32_t>( short_array_samples[ii] );
        if( T[0] != 0 )
        {
            if( Dial > norm_l( T[0] ) )
            {
                Dial = norm_l( T[0] );
            }
        }
    }

    Dial -= 16;

    // Next, utilize Dial for scaling and populate internalArray.
    for( ii = 0; ii < SAMPLES; ii++ )
    {
        T[0] = short_array_samples[ii];
        internal_array_[ii] = static_cast<int16_t>( T[0] << Dial );
    }

    //Frequency detection
    goertzel_filter( CONSTANTS[0], CONSTANTS[1], internal_array_, &T[0], &T[1], SAMPLES );
    goertzel_filter( CONSTANTS[2], CONSTANTS[3], internal_array_, &T[2], &T[3], SAMPLES );
    goertzel_filter( CONSTANTS[4], CONSTANTS[5], internal_array_, &T[4], &T[5], SAMPLES );
    goertzel_filter( CONSTANTS[6], CONSTANTS[7], internal_array_, &T[6], &T[7], SAMPLES );
    goertzel_filter( CONSTANTS[8], CONSTANTS[9], internal_array_, &T[8], &T[9], SAMPLES );
    goertzel_filter( CONSTANTS[10], CONSTANTS[11], internal_array_, &T[10], &T[11], SAMPLES );
    goertzel_filter( CONSTANTS[12], CONSTANTS[13], internal_array_, &T[12], &T[13], SAMPLES );
    goertzel_filter( CONSTANTS[14], CONSTANTS[15], internal_array_, &T[14], &T[15], SAMPLES );
    goertzel_filter( CONSTANTS[16], CONSTANTS[17], internal_array_, &T[16], &T[17], SAMPLES );

#if DEBUG
    for (ii = 0; ii < COEFF_NUMBER; ++ii)
    printf("%d ", T[ii]);
    printf("\n");
#endif

    int32_t Row = 0;
    int32_t Temp = 0;
    // Row      Index of the maximum row frequency in T
    // Temp     The frequency at the maximum row/column (gets reused
    //          below).
    //Find max row(low frequences) tones
    for( ii = 0; ii < 4; ii++ )
    {
        if( Temp < T[ii] )
        {
            Row = ii;
            Temp = T[ii];
        }
    }

    // Column   Index of the maximum column frequency in T
    int32_t Column = 4;
    Temp = 0;
    //Find max column(high frequences) tones
    for( ii = 4; ii < 8; ii++ )
    {
        if( Temp < T[ii] )
        {
            Column = ii;
            Temp = T[ii];
        }
    }

    Sum = 0;
    //Find average value dial tones without max row and max column
    for( ii = 0; ii < 10; ii++ )
    {
        Sum += T[ii];
    }
    Sum -= T[Row];
    Sum -= T[Column];
    // N.B. Divide by 8
    Sum >>= 3;

    // N.B. looks like avoiding a divide by zero.
    if( !Sum )
        Sum = 1;

    //If relations max row and max column to average value
    //are less then threshold then return
    // This means the tones are too quiet compared to the other, non-max
    // DTMF frequencies.
    if( T[Row] / Sum < dial_tones_to_ohers_dial_tones_ )
        return ' ';
    if( T[Column] / Sum < dial_tones_to_ohers_dial_tones_ )
        return ' ';

    // Next, check if the volume of the row and column frequencies
    // is similar.  If they are different, then they aren't part of
    // the same tone.
    //
    // In the literature, this is known as "twist".
    //If relations max colum to max row is large then 4 then return
    if( T[Row] < ( T[Column] >> 2 ) )
        return ' ';
    //If relations max colum to max row is large then 4 then return
    // The reason why the twist calculations aren't symmetric is that the
    // allowed ratios for normal and reverse twist are different.
    if( T[Column] < ( ( T[Row] >> 1 ) - ( T[Row] >> 3 ) ) )
        return ' ';

    // N.B. looks like avoiding a divide by zero.
    for( ii = 0; ii < COEFF_NUMBER; ii++ )
        if( T[ii] == 0 )
            T[ii] = 1;

    //If relations max row and max column to all other tones are less then
    //threshold then return
    // Check for the presence of strong harmonics.
    for( ii = 10; ii < COEFF_NUMBER; ii++ )
    {
        if( T[Row] / T[ii] < dial_tones_to_ohers_tones_ )
            return ' ';
        if( T[Column] / T[ii] < dial_tones_to_ohers_tones_ )
            return ' ';
    }

    //If relations max row and max column tones to other dial tones are
    //less then threshold then return
    for( ii = 0; ii < 10; ii++ )
    {
        // TODO:
        // The next two nested if's can be collapsed into a single
        // if-statement.  Basically, he's checking that the current
        // tone is NOT the maximum tone.
        //
        // A simpler check would have been (ii != Column && ii != Row)
        //
        if( T[ii] != T[Column] )
        {
            if( T[ii] != T[Row] )
            {
                if( T[Row] / T[ii] < dial_tones_to_ohers_dial_tones_ )
                    return ' ';
                if( Column != 4 )
                {
                    // Column == 4 corresponds to 1176Hz.
                    // TODO: what is so special about this frequency?
                    if( T[Column] / T[ii] < dial_tones_to_ohers_dial_tones_ )
                        return ' ';
                }
                else
                {
                    if( T[Column] / T[ii] < ( dial_tones_to_ohers_dial_tones_ / 3 ) )
                        return ' ';
                }
            }
        }
    }

    //We are choosed a push button
    // Determine the tone based on the row and column frequencies.
    switch( Row )
    {
    case 0:
        switch( Column )
        {
        case 4:
            return_value = '1';
            break;
        case 5:
            return_value = '2';
            break;
        case 6:
            return_value = '3';
            break;
        case 7:
            return_value = 'A';
            break;
        }
        ;
        break;
    case 1:
        switch( Column )
        {
        case 4:
            return_value = '4';
            break;
        case 5:
            return_value = '5';
            break;
        case 6:
            return_value = '6';
            break;
        case 7:
            return_value = 'B';
            break;
        }
        ;
        break;
    case 2:
        switch( Column )
        {
        case 4:
            return_value = '7';
            break;
        case 5:
            return_value = '8';
            break;
        case 6:
            return_value = '9';
            break;
        case 7:
            return_value = 'C';
            break;
        }
        ;
        break;
    case 3:
        switch( Column )
        {
        case 4:
            return_value = '*';
            break;
        case 5:
            return_value = '0';
            break;
        case 6:
            return_value = '#';
            break;
        case 7:
            return_value = 'D';
            break;
        }
    }

    return return_value;
}

} // namespace dtmf
