#include "maixpy.h"
#include "sdcard.h"
void * __dso_handle = 0 ;
extern sdcard_config_t config;

int main()
{
    sdcard_config_t amigo = { 33, 35, 34, 32, SD_CS_PIN };
    config = amigo;
    maixpy_main();
    return 0;
}
