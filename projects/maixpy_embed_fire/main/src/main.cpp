#include "maixpy.h"
#include "sdcard.h"
void * __dso_handle = 0 ;
extern sdcard_config_t config;

int main()
{
    sdcard_config_t fire = { 7, 9, 8, 6, 28};
    config = fire;
    maixpy_main();
    return 0;
}
