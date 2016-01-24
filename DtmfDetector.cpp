/** Author:       Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
 *                Sergey Kolevatov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#include <cassert>
#include <stdexcept>                    // std::invalid_argument
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
        27978,
        26955,
        25700,
        24218,
        19072,
        16324,
        13084,
        9314,
        15013,
        11582,
        7549,
        3032,
        -10565,
        -16502,
        -22317,
        -27471
};

const int16_t DtmfDetector::CONSTANTS_16KHz[COEFF_NUMBER] =
{
        31547,
        31280,
        30950,
        30555,
        29142,
        28359,
        27408,
        26257,
        27978,
        26955,
        25700,
        24218,
        19072,
        16324,
        13084,
        9314
        };

const int16_t DtmfDetector::CONSTANTS_44_1KHz[COEFF_NUMBER] =
{
        32605,
        32570,
        32525,
        32472,
        32282,
        32175,
        32044,
        31884,
        32122,
        31981,
        31806,
        31596,
        30841,
        30421,
        29907,
        29283
};

int32_t DtmfDetector::power_threshold_ = 328;
int32_t DtmfDetector::dial_tones_to_ohers_tones_ = 16;
int32_t DtmfDetector::dial_tones_to_ohers_dial_tones_ = 6;
//--------------------------------------------------------------------
DtmfDetector::DtmfDetector(
        int32_t sampling_rate ) :
        callback_( nullptr ),
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
    else if( sampling_rate == 8000 )
    {
        CONSTANTS   = CONSTANTS_8KHz;
        SAMPLES     = 102;
    }
    else
    {
        throw std::invalid_argument( "unsupported sampling rate" );
    }

    //
    // This array is padded to keep the last batch, which is smaller
    // than SAMPLES, from the previous call to detect_dtmf.
    //
    internal_array_     = new int16_t[SAMPLES];
    prev_dial_button_   = tone_e::TONE_0;
    prev_tone_type_     = tone_type_e::SILENCE;
}
//---------------------------------------------------------------------
DtmfDetector::~DtmfDetector()
{
    delete[] internal_array_;
}

void DtmfDetector::init_callback(
        IDtmfDetectorCallback * callback )
{
    callback_   = callback;
}


void DtmfDetector::process( const int16_t * input_array, uint32_t frame_size )
{
    // Copy the input array into the middle of array_samples_.
    // I think the first frame_count samples contain the last batch from the
    // previous call to this function.

    array_samples_.insert( array_samples_.end(), & input_array[0], & input_array[frame_size] );

    // This gets used for a variety of purposes.  Most notably, it indicates
    // the start of the circular buffer at the start of ::detect_dtmf.

    uint32_t frame_count = array_samples_.size();

    // Read index into array_samples_ that corresponds to the current batch.
    uint32_t temp_index = 0;

    // If don't have enough samples to process an entire batch, then don't
    // do anything.
    if( frame_count >= SAMPLES )
    {
        // Process samples while we still have enough for an entire
        // batch.
        while( frame_count >= SAMPLES )
        {
            // Determine the tone present in the current batch

            // temp_dial_button     A tone detected in part of the input_array
            tone_e dial_button;
            tone_type_e type = detect_dtmf( &array_samples_[temp_index], dial_button );

            // Determine if we should register it as a new tone, or
            // ignore it as a continuation of a previously
            // registered tone.
            if( ( type == tone_type_e::UNDEF ) && ( prev_tone_type_ == tone_type_e::SILENCE ) )
            {
                // got something undefined, ignoring
            }
            else if( ( type == tone_type_e::TONE ) && ( prev_tone_type_ == tone_type_e::SILENCE ) )
            {
                // got a tone after silence, report it and update state
                if( callback_ )
                    callback_->on_detect( dial_button );

                prev_dial_button_   = dial_button;
                prev_tone_type_     = type;
            }
            else if( ( type == tone_type_e::TONE ) && ( prev_tone_type_ == tone_type_e::TONE ) )
            {
                // got a tone after tone, nothing to do
                if( prev_dial_button_ != dial_button )
                {
                    puts( "c" );
                    if( callback_ )
                        callback_->on_detect( dial_button );

                    prev_dial_button_   = dial_button;

                }
            }
            else if( ( type == tone_type_e::UNDEF ) && ( prev_tone_type_ == tone_type_e::TONE ) )
            {
                // got something undefined after tone, ignore it
                puts( "u" );
            }
            else if( ( type == tone_type_e::SILENCE ) && ( prev_tone_type_ != tone_type_e::SILENCE ) )
            {
                // got silence after non-silence, update state
                puts( "s" );
                prev_tone_type_ = type;
            }

            // Store the current tone.  In light of the above
            // behaviour, all that really matters is whether it was
            // a tone or silence.  Finally, move on to the next
            // batch.
            //prev_tone_type_ = type;

            temp_index += SAMPLES;
            frame_count -= SAMPLES;
        }

        //
        // We have frame_count samples left to process, but it's not
        // enough for an entire batch.  Shift these left-over
        // samples to the beginning of our array and deal with them
        // next time this function is called.
        //
        // move unprocessed elements to the beginning
        array_samples_.erase( array_samples_.begin(), array_samples_.begin() + temp_index );
    }

}
//-----------------------------------------------------------------
tone_e DtmfDetector::row_column_to_tone( int32_t row, int32_t column )
{
    tone_e return_value = tone_e::TONE_A;

    //We are choosed a push button
    // Determine the tone based on the row and column frequencies.
    switch( row )
    {
    case 0:
        switch( column )
        {
        case 4:
            return_value = tone_e::TONE_1;
            break;
        case 5:
            return_value = tone_e::TONE_2;
            break;
        case 6:
            return_value = tone_e::TONE_3;
            break;
        case 7:
            return_value = tone_e::TONE_A;
            break;
        }
        ;
        break;
    case 1:
        switch( column )
        {
        case 4:
            return_value = tone_e::TONE_4;
            break;
        case 5:
            return_value = tone_e::TONE_5;
            break;
        case 6:
            return_value = tone_e::TONE_6;
            break;
        case 7:
            return_value = tone_e::TONE_B;
            break;
        }
        ;
        break;
    case 2:
        switch( column )
        {
        case 4:
            return_value = tone_e::TONE_7;
            break;
        case 5:
            return_value = tone_e::TONE_8;
            break;
        case 6:
            return_value = tone_e::TONE_9;
            break;
        case 7:
            return_value = tone_e::TONE_C;
            break;
        }
        ;
        break;
    case 3:
        switch( column )
        {
        case 4:
            return_value = tone_e::TONE_STAR;
            break;
        case 5:
            return_value = tone_e::TONE_0;
            break;
        case 6:
            return_value = tone_e::TONE_HASH;
            break;
        case 7:
            return_value = tone_e::TONE_D;
            break;
        }
    }

    return return_value;
}
//-----------------------------------------------------------------
// Detect a tone in a single batch of samples (SAMPLES elements).
DtmfDetector::tone_type_e DtmfDetector::detect_dtmf( int16_t short_array_samples[], tone_e & tone )
{
    int32_t Dial = 32;
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
        return tone_type_e::SILENCE;

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

    // Next, utilize Dial for scaling and populate internal_array_.
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
        return tone_type_e::UNDEF;
    if( T[Column] / Sum < dial_tones_to_ohers_dial_tones_ )
        return tone_type_e::UNDEF;

    // Next, check if the volume of the row and column frequencies
    // is similar.  If they are different, then they aren't part of
    // the same tone.
    //
    // In the literature, this is known as "twist".
    //If relations max colum to max row is large then 4 then return
    if( T[Row] < ( T[Column] >> 2 ) )
        return tone_type_e::UNDEF;
    //If relations max colum to max row is large then 4 then return
    // The reason why the twist calculations aren't symmetric is that the
    // allowed ratios for normal and reverse twist are different.
    if( T[Column] < ( ( T[Row] >> 1 ) - ( T[Row] >> 3 ) ) )
        return tone_type_e::UNDEF;

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
            return tone_type_e::UNDEF;
        if( T[Column] / T[ii] < dial_tones_to_ohers_tones_ )
            return tone_type_e::UNDEF;
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
                    return tone_type_e::UNDEF;
                if( Column != 4 )
                {
                    // Column == 4 corresponds to 1176Hz.
                    // TODO: what is so special about this frequency?
                    if( T[Column] / T[ii] < dial_tones_to_ohers_dial_tones_ )
                        return tone_type_e::UNDEF;
                }
                else
                {
                    if( T[Column] / T[ii] < ( dial_tones_to_ohers_dial_tones_ / 3 ) )
                        return tone_type_e::UNDEF;
                }
            }
        }
    }

    tone = row_column_to_tone( Row, Column );

    return tone_type_e::TONE;
}

} // namespace dtmf
