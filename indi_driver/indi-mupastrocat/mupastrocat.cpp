/*
    Driver Type: MUP Astro CAT focuser and temperature INDI Driver

    Copyright © 2016 Gary Preston (gary@mups.co.uk)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Future TODO:- 
      - Switch to xml skeleton file to allow re-configuring driver. 
      - Temperature reading/alarm.
*/

#include <memory>

#include "mupastrocat.h"

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

const char* DEFAULT_DEVICE_NAME = "MUP Astro CAT";

//////////////////////////////////////////////////////////////////////
// Driver Instance
//////////////////////////////////////////////////////////////////////

std::unique_ptr<MUPAstroCAT> sgMupAstroCAT(new MUPAstroCAT());

//////////////////////////////////////////////////////////////////////
// INDI Framework Callbacks
//////////////////////////////////////////////////////////////////////

void ISGetProperties(const char *dev)
{
    sgMupAstroCAT->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    sgMupAstroCAT->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    sgMupAstroCAT->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    sgMupAstroCAT->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    sgMupAstroCAT->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////
// MUP Astro CAT Construction/Destruction
//////////////////////////////////////////////////////////////////////

MUPAstroCAT::MUPAstroCAT()
{

}

MUPAstroCAT::~MUPAstroCAT()
{

}

//////////////////////////////////////////////////////////////////////
// INDI Framework Callbacks
//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::Connect()
{
    IDMessage(getDeviceName(), "Connected to device.");

    return true;
}

bool MUPAstroCAT::Disconnect()
{
    IDMessage(getDeviceName(), "Disconnected from device.");

    return true;
}

const char * MUPAstroCAT::getDefaultName()
{
    return DEFAULT_DEVICE_NAME;
}
