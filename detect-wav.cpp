/*

Detect DTMF tones in WAV file.
The file must be 8KHz, mono.

Copyright (C) 2016 Sergey Kolevatov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

// $Revision: 3180 $ $Date:: 2016-01-13 #$ $Author: serge $

#include <iostream>
#include <sstream>

#include "../wave/wave.h"       // wave::Wave

#include "DtmfDetector.hpp"
#include "IDtmfDetectorCallback.hpp"    // IDtmfDetectorCallback


// The size of the buffer we use for reading & processing the audio samples.
//
#define BUFLEN 256

std::string to_string( const wave::Wave &h )
{
    std::stringstream ss;
    ss << h.get_data_size() << " samples, " << h.get_samples_per_sec() << "Hz, "
            << h.get_channels() << " channels, " << h.get_avg_bytes_per_sec() <<
            " avg bytes per sec";
    return ss.str();
}

class Callback: public dtmf::IDtmfDetectorCallback
{
public:

    virtual void on_detect( const char button )
    {
        std::cout << "detected '" << button << "'" << std::endl;
    }
};

int main( int argc, char **argv )
{
    if( argc != 2 )
    {
        std::cerr << "usage: " << argv[0] << " filename.wav" << std::endl;
        return 1;
    }

    wave::Wave header;

    try
    {
        wave::Wave v( argv[1] );

        header = v;
    }
    catch( std::exception & e)
    {
        std::cerr << argv[1] << ": unable to open file" << std::endl;
        return 1;
    }

    std::cout << argv[1] << ": " << to_string( header ) << std::endl;

    // This example only supports a specific type of WAV format:
    //
    // - 8KHz sample rate
    // - mono
    //
    if( header.get_samples_per_sec() != 8000 || header.get_channels() != 1 )
    {
        std::cerr << argv[1] << ": unsupported WAV format" << std::endl;
        return 1;
    }

    Callback callback;

    std::vector<char> cbuf(BUFLEN * 2);
    short sbuf[BUFLEN];
    dtmf::DtmfDetector detector( BUFLEN, & callback );

    auto data_size = header.get_data_size();

    for( auto i = 0; i < data_size; i += BUFLEN*2 )
    {
        cbuf.clear();

        header.get_samples( i, BUFLEN * 2, cbuf );

        // copy char data into short array
        for( int j = 0; j < BUFLEN; ++j )
        {
            sbuf[j] = (short)cbuf[j * 2 ] + ( (short)cbuf[j * 2 + 1] << 8 );
        }

        detector.process( sbuf );
    }

    std::cout << std::endl;

    return 0;
}
