/*

DTMF detector callback interface.

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

// $Revision: 3241 $ $Date:: 2016-01-22 #$ $Author: serge $

#ifndef DTMF_DETECTOR_CALLBACK
#define DTMF_DETECTOR_CALLBACK

namespace dtmf
{

enum class tone_e
{
    UNDEF = -1,
    TONE_0 = 0,
    TONE_1,
    TONE_2,
    TONE_3,
    TONE_4,
    TONE_5,
    TONE_6,
    TONE_7,
    TONE_8,
    TONE_9,
    TONE_A,
    TONE_B,
    TONE_C,
    TONE_D,
    TONE_STAR,
    TONE_HASH,
};

class IDtmfDetectorCallback
{
public:
    virtual ~IDtmfDetectorCallback() {};

    virtual void on_detect( tone_e tone ) = 0;
};

} // namespace dtmf

#endif // DTMF_DETECTOR_CALLBACK
