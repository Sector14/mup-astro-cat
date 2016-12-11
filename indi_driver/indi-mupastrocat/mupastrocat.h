#include "libindi/defaultdevice.h"

class MUPAstroCAT : public INDI::DefaultDevice
{
public:
    MUPAstroCAT();
    virtual ~MUPAstroCAT();

    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
};
