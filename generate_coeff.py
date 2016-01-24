#!/usr/bin/python

'''

Generate coefficents for Goertzel filter.

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

'''

# $Revision: 3277 $ $Date:: 2016-01-24 #$ $Author: serge $


import math

def generate_coeff( freq, sample_rate ):
    normalized_freq = freq / sample_rate
    # inspired by https://github.com/hfeeki/dtmf/blob/master/dtmf-decoder.py
    coeff           = 2.0 * math.cos( 2.0 * math.pi * normalized_freq )
    coeff_int16     = int( 16383.5 * coeff )

    return coeff_int16

def generate_coeffs( sample_rate ):
    goertzel_freq = [ 697.0, 770.0, 852.0, 941.0, 1209.0, 1336.0, 1477.0, 1633.0 ]

    for freq in goertzel_freq:
        coeff = generate_coeff( freq, sample_rate )
        print coeff

    for freq in goertzel_freq:
        coeff = generate_coeff( freq * 2.0, sample_rate )
        print coeff

def generate_coeffs_nice( sample_rate ):
    print "sampling rate", sample_rate
    generate_coeffs( sample_rate )
    print

if __name__ == '__main__':
    generate_coeffs_nice( 8000 )
    generate_coeffs_nice( 16000 )
    generate_coeffs_nice( 44000 )
    generate_coeffs_nice( 44100 )
