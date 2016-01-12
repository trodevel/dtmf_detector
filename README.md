dtmf_detector
=============

C++ DTMF detector and generator classes

The original code was written by Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
and is available in its original form at http://sourceforge.net/projects/dtmf/

Extended by Sergey Kolevatov.

Main features:

- Portable fixed-point implementation
- Detection of DTMF tones from 8KHz PCM8 signal

Installation
------------

    git clone https://github.com/trodevel/dtmf_detector.git
    cd dtmf_detector
    make
    bin/detect-au.out test-data/Dtmf0.au