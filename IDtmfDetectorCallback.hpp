/** Author:       Sergey Kolevatov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#ifndef DTMF_DETECTOR_CALLBACK
#define DTMF_DETECTOR_CALLBACK

namespace dtmf
{

class IDtmfDetectorCallback
{
public:
    virtual ~IDtmfDetectorCallback() {};

    virtual void on_detect( const char button ) = 0;
};

} // namespace dtmf

#endif // DTMF_DETECTOR_CALLBACK
