#include "sdcard.h"
#include "dmac.h"
#include "fpioa.h"
#include "global_config.h"
#include "gpiohs.h"
#include "sleep.h"
#include "spi.h"
#include "sysctl.h"
#include "syslog.h"
#include "utils.h"
#include <stdio.h>

/*
 * @brief  Start Data tokens:
 *         Tokens (necessary because at nop/idle (and CS active) only 0xff is
 *         on the data/command line)
 */
/*!< Data token start byte, Start Single Block Read */
#define SD_START_DATA_SINGLE_BLOCK_READ 0xFE
/*!< Data token start byte, Start Multiple Block Read */
#define SD_START_DATA_MULTIPLE_BLOCK_READ 0xFE
/*!< Data token start byte, Start Single Block Write */
#define SD_START_DATA_SINGLE_BLOCK_WRITE 0xFE
/*!< Data token start byte, Start Multiple Block Write */
#define SD_START_DATA_MULTIPLE_BLOCK_WRITE 0xFC

/*
 * @brief  Commands: CMDxx = CMD-number | 0x40
 */
#define SD_CMD0 0    /*!< CMD0 = 0x40 */
#define SD_CMD8 8    /*!< CMD8 = 0x48 */
#define SD_CMD9 9    /*!< CMD9 = 0x49 */
#define SD_CMD10 10  /*!< CMD10 = 0x4A */
#define SD_CMD12 12  /*!< CMD12 = 0x4C */
#define SD_CMD16 16  /*!< CMD16 = 0x50 */
#define SD_CMD17 17  /*!< CMD17 = 0x51 */
#define SD_CMD18 18  /*!< CMD18 = 0x52 */
#define SD_ACMD23 23 /*!< CMD23 = 0x57 */
#define SD_CMD24 24  /*!< CMD24 = 0x58 */
#define SD_CMD25 25  /*!< CMD25 = 0x59 */
#define SD_ACMD41 41 /*!< ACMD41 = 0x41 */
#define SD_CMD55 55  /*!< CMD55 = 0x55 */
#define SD_CMD58 58  /*!< CMD58 = 0x58 */
#define SD_CMD59 59  /*!< CMD59 = 0x59 */

sdcard_config_t config = {
// see struct sdcard_config_t
#ifdef CONFIG_BOARD_M5STICK
    33, 31, 30, 32, SD_CS_PIN,
#else
    28, 26, 27, 29, SD_CS_PIN,
#endif
};

SD_CardInfo cardinfo;
int sd_version = 0;

void SD_CS_HIGH(void) { gpiohs_set_pin(config.cs_gpio_num, GPIO_PV_HIGH); }

void SD_CS_LOW(void) { gpiohs_set_pin(config.cs_gpio_num, GPIO_PV_LOW); }

void SD_HIGH_SPEED_ENABLE(void) { spi_set_clk_rate(SD_SPI_DEVICE, 25000000); }

void SD_LOW_SPEED_ENABLE(void) { spi_set_clk_rate(SD_SPI_DEVICE, 400000); }

/*
 * @brief  Calculate CRC-7 for SD card commands
 * @param  cmd: The command byte (without 0x40 prefix)
 * @param  arg: The 32-bit argument
 * @retval The calculated CRC-7 value (with stop bit)
 */
static uint8_t sd_calculate_crc7(uint8_t cmd, uint32_t arg) {
  uint8_t crc = 0;
  uint8_t data[5];

  // Construct the 5-byte command
  data[0] = cmd | 0x40;
  data[1] = (uint8_t)(arg >> 24);
  data[2] = (uint8_t)(arg >> 16);
  data[3] = (uint8_t)(arg >> 8);
  data[4] = (uint8_t)(arg);

  // Calculate CRC-7 over the 5 bytes (40 bits)
  for (int i = 0; i < 5; i++) {
    for (int j = 7; j >= 0; j--) {
      crc <<= 1;
      if (((data[i] >> j) & 1) ^ ((crc >> 7) & 1)) {
        crc ^= 0x09; // Polynomial: x^7 + x^3 + 1
      }
    }
  }

  // Return CRC-7 shifted left by 1 with stop bit (LSB = 1)
  return ((crc & 0x7F) << 1) | 0x01;
}

static void sd_lowlevel_init(uint8_t spi_index) {
  gpiohs_set_drive_mode(config.cs_gpio_num, GPIO_DM_OUTPUT);
  spi_set_clk_rate(SD_SPI_DEVICE, 200000); /*set clk rate*/
}

static void sd_write_data(uint8_t *data_buff, uint32_t length) {
  spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
  spi_send_data_standard(SD_SPI_DEVICE, SD_SS, NULL, 0, data_buff, length);
}

static void sd_read_data(uint8_t *data_buff, uint32_t length) {

  spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
  spi_receive_data_standard(SD_SPI_DEVICE, SD_SS, NULL, 0, data_buff, length);
}

static void sd_write_data_dma(uint8_t *data_buff) {
  // spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 32, 1);
  // misaligned error
  spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
  spi_send_data_standard_dma(SD_DMA_CH, SD_SPI_DEVICE, SD_SS, NULL, 0,
                             (uint8_t *)(data_buff), 128 * 4);
}

static void sd_read_data_dma(uint8_t *data_buff) {
  // spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 32, 1);
  // misaligned error
  spi_init(SD_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
  spi_receive_data_standard_dma(-1, SD_DMA_CH, SD_SPI_DEVICE, SD_SS, NULL, 0,
                                data_buff, 128 * 4);
}

/*
 * @brief  Send 5 bytes command to the SD card with auto-calculated CRC.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @retval None
 */
static void sd_send_cmd(uint8_t cmd, uint32_t arg) {
  uint8_t frame[6];
  uint8_t dummy = 0xFF;
  /*!< Construct byte 1 */
  frame[0] = (cmd | 0x40);
  /*!< Construct byte 2 */
  frame[1] = (uint8_t)(arg >> 24);
  /*!< Construct byte 3 */
  frame[2] = (uint8_t)(arg >> 16);
  /*!< Construct byte 4 */
  frame[3] = (uint8_t)(arg >> 8);
  /*!< Construct byte 5 */
  frame[4] = (uint8_t)(arg);
  /*!< Construct CRC: byte 6 - auto-calculated */
  frame[5] = sd_calculate_crc7(cmd, arg);
  /*!< SD chip select low */
  SD_CS_LOW();
  /*!< Send dummy byte for synchronization */
  sd_write_data(&dummy, 1);
  /*!< Send the Cmd bytes */
  sd_write_data(frame, 6);
}

/*
 * @brief  Send 5 bytes command to the SD card.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @param  Crc: The CRC.
 * @retval None
 */
static void sd_end_cmd(void) {
  uint8_t frame[1] = {0xFF};
  /*!< SD chip select high */
  SD_CS_HIGH();
  /*!< Send the Cmd bytes */
  sd_write_data(frame, 1);
}

/*
 * @brief  Returns the SD response.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_response(void) {
  uint8_t result;
  // was set to 0xFFFF (values from 95 to 65535)
  uint16_t timeout = 0xFDE; // min: 0x5f
  /*!< Check if response is got or a timeout is happen */
  while (timeout--) {
    sd_read_data(&result, 1);
    /*!< Right response got */
    if (result != 0xFF)
      return result;
  }
  /*!< After time out */
  return 0xFF;
}

/*
 * @brief  Get SD card data response.
 * @param  None
 * @retval The SD status: Read data response xxx0<status>1
 *         - status 010: Data accecpted
 *         - status 101: Data rejected due to a crc error
 *         - status 110: Data rejected due to a Write error.
 *         - status 111: Data rejected due to other error.
 */
static uint8_t sd_get_dataresponse(void) {
  uint8_t response;
  /*!< Read resonse */
  sd_read_data(&response, 1);
  /*!< Mask unused bits */
  response &= 0x1F;
  if (response != 0x05)
    return 0xFF;
  /*!< Wait null data */
  sd_read_data(&response, 1);
  while (response == 0)
    sd_read_data(&response, 1);
  /*!< Return response */
  return 0;
}

/*
 * @brief  Read the CSD card register
 *         Reading the contents of the CSD register in SPI mode is a simple
 *         read-block transaction.
 * @param  SD_csd: pointer on an SCD register structure
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_csdregister(SD_CSD *SD_csd) {
  uint8_t csd_tab[18];
  /*!< Send CMD9 (CSD register) or CMD10(CSD register) */
  sd_send_cmd(SD_CMD9, 0);
  /*!< Wait for response in the R1 format (0x00 is no errors) */
  uint8_t resp = sd_get_response();
  if (resp != 0x00) {
    printk("[MaixPy] %s | != 0x00 resp = %x \r\n", __func__, resp);
    sd_end_cmd();
    return 0xFF;
  }
  resp = sd_get_response();
  if (resp != SD_START_DATA_SINGLE_BLOCK_READ) {
    printk("[MaixPy] %s | != SD_START_DATA_SINGLE_BLOCK_READ resp = %x \r\n", __func__, resp);
    sd_end_cmd();
    return 0xFF;
  }
  /*!< Store CSD register value on csd_tab */
  /*!< Get CRC bytes (not really needed by us, but required by SD) */
  sd_read_data(csd_tab, 18);
  sd_end_cmd();
  /*!< Byte 0 */
  SD_csd->CSDStruct = (csd_tab[0] & 0xC0) >> 6;
  SD_csd->SysSpecVersion = (csd_tab[0] & 0x3C) >> 2;
  SD_csd->Reserved1 = csd_tab[0] & 0x03;
  /*!< Byte 1 */
  SD_csd->TAAC = csd_tab[1];
  /*!< Byte 2 */
  SD_csd->NSAC = csd_tab[2];
  /*!< Byte 3 */
  SD_csd->MaxBusClkFrec = csd_tab[3];
  /*!< Byte 4 */
  SD_csd->CardComdClasses = csd_tab[4] << 4;
  /*!< Byte 5 */
  SD_csd->CardComdClasses |= (csd_tab[5] & 0xF0) >> 4;
  SD_csd->RdBlockLen = csd_tab[5] & 0x0F;
  /*!< Byte 6 */
  SD_csd->PartBlockRead = (csd_tab[6] & 0x80) >> 7;
  SD_csd->WrBlockMisalign = (csd_tab[6] & 0x40) >> 6;
  SD_csd->RdBlockMisalign = (csd_tab[6] & 0x20) >> 5;
  SD_csd->DSRImpl = (csd_tab[6] & 0x10) >> 4;
  SD_csd->Reserved2 = 0; /*!< Reserved */
  if (2 == sd_version) {
    SD_csd->DeviceSize = (csd_tab[6] & 0x03) << 10;
    /*!< Byte 7 */
    SD_csd->DeviceSize = (csd_tab[7] & 0x3F) << 16;
    /*!< Byte 8 */
    SD_csd->DeviceSize |= csd_tab[8] << 8;
    /*!< Byte 9 */
    SD_csd->DeviceSize |= csd_tab[9];
  } else if (1 == sd_version) {
    SD_csd->DeviceSize |= (csd_tab[6] & 0x03) << 10;
    /*!< Byte 7 */
    SD_csd->DeviceSize |= csd_tab[7] << 2;
    /*!< Byte 8 */
    SD_csd->DeviceSize |= ((csd_tab[8] & 0xC0) >> 6);
    /*!< Byte 9 10*/
    SD_csd->CSizeMlut = ((csd_tab[10] & 128) >> 7) + ((csd_tab[9] & 0x03) << 1);
  }
  /*!< Byte 10 */
  SD_csd->EraseGrSize = (csd_tab[10] & 0x40) >> 6;
  SD_csd->EraseGrMul = (csd_tab[10] & 0x3F) << 1;
  /*!< Byte 11 */
  SD_csd->EraseGrMul |= (csd_tab[11] & 0x80) >> 7;
  SD_csd->WrProtectGrSize = (csd_tab[11] & 0x7F);
  /*!< Byte 12 */
  SD_csd->WrProtectGrEnable = (csd_tab[12] & 0x80) >> 7;
  SD_csd->ManDeflECC = (csd_tab[12] & 0x60) >> 5;
  SD_csd->WrSpeedFact = (csd_tab[12] & 0x1C) >> 2;
  SD_csd->MaxWrBlockLen = (csd_tab[12] & 0x03) << 2;
  /*!< Byte 13 */
  SD_csd->MaxWrBlockLen |= (csd_tab[13] & 0xC0) >> 6;
  SD_csd->WriteBlockPaPartial = (csd_tab[13] & 0x20) >> 5;
  SD_csd->Reserved3 = 0;
  SD_csd->ContentProtectAppli = (csd_tab[13] & 0x01);
  /*!< Byte 14 */
  SD_csd->FileFormatGrouop = (csd_tab[14] & 0x80) >> 7;
  SD_csd->CopyFlag = (csd_tab[14] & 0x40) >> 6;
  SD_csd->PermWrProtect = (csd_tab[14] & 0x20) >> 5;
  SD_csd->TempWrProtect = (csd_tab[14] & 0x10) >> 4;
  SD_csd->FileFormat = (csd_tab[14] & 0x0C) >> 2;
  SD_csd->ECC = (csd_tab[14] & 0x03);
  /*!< Byte 15 */
  SD_csd->CSD_CRC = (csd_tab[15] & 0xFE) >> 1;
  SD_csd->Reserved4 = 1;
  /*!< Return the reponse */
  return 0;
}

/*
 * @brief  Read the CID card register.
 *         Reading the contents of the CID register in SPI mode is a simple
 *         read-block transaction.
 * @param  SD_cid: pointer on an CID register structure
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_cidregister(SD_CID *SD_cid) {
  uint8_t cid_tab[18];
  /*!< Send CMD10 (CID register) */
  sd_send_cmd(SD_CMD10, 0);
  /*!< Wait for response in the R1 format (0x00 is no errors) */
  uint8_t resp = sd_get_response();
  if (resp != 0x00) {
    printk("[MaixPy] %s | sd_get_response() != 0x00 = %x \r\n", __func__, resp);
    sd_end_cmd();
    return 0xFF;
  }
  resp = sd_get_response();
  if (resp != SD_START_DATA_SINGLE_BLOCK_READ) {
    printk("[MaixPy] %s | sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ = %x \r\n", __func__, resp);
    sd_end_cmd();
    return 0xFF;
  }
  /*!< Store CID register value on cid_tab */
  /*!< Get CRC bytes (not really needed by us, but required by SD) */
  sd_read_data(cid_tab, 18);
  sd_end_cmd();
  /*!< Byte 0 */
  SD_cid->ManufacturerID = cid_tab[0];
  /*!< Byte 1 */
  SD_cid->OEM_AppliID = cid_tab[1] << 8;
  /*!< Byte 2 */
  SD_cid->OEM_AppliID |= cid_tab[2];
  /*!< Byte 3 */
  SD_cid->ProdName1 = cid_tab[3] << 24;
  /*!< Byte 4 */
  SD_cid->ProdName1 |= cid_tab[4] << 16;
  /*!< Byte 5 */
  SD_cid->ProdName1 |= cid_tab[5] << 8;
  /*!< Byte 6 */
  SD_cid->ProdName1 |= cid_tab[6];
  /*!< Byte 7 */
  SD_cid->ProdName2 = cid_tab[7];
  /*!< Byte 8 */
  SD_cid->ProdRev = cid_tab[8];
  /*!< Byte 9 */
  SD_cid->ProdSN = cid_tab[9] << 24;
  /*!< Byte 10 */
  SD_cid->ProdSN |= cid_tab[10] << 16;
  /*!< Byte 11 */
  SD_cid->ProdSN |= cid_tab[11] << 8;
  /*!< Byte 12 */
  SD_cid->ProdSN |= cid_tab[12];
  /*!< Byte 13 */
  SD_cid->Reserved1 |= (cid_tab[13] & 0xF0) >> 4;
  SD_cid->ManufactDate = (cid_tab[13] & 0x0F) << 8;
  /*!< Byte 14 */
  SD_cid->ManufactDate |= cid_tab[14];
  /*!< Byte 15 */
  SD_cid->CID_CRC = (cid_tab[15] & 0xFE) >> 1;
  SD_cid->Reserved2 = 1;
  /*!< Return the reponse */
  return 0;
}

/*
 * @brief  Returns information about specific card.
 * @param  cardinfo: pointer to a SD_CardInfo structure that contains all SD
 *         card information.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_cardinfo(SD_CardInfo *cardinfo) {
  if (sd_get_csdregister(&(cardinfo->SD_csd))) {
    printk("[MaixPy] %s | sd_get_csdregister failed\r\n", __func__);
    return 0xFF;
  }
  if (sd_get_cidregister(&(cardinfo->SD_cid))) {
    printk("[MaixPy] %s | sd_get_cidregister failed\r\n", __func__);
    return 0xFF;
  }
  if (2 == sd_version) {
    cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) * 1024;
    cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
    cardinfo->CardCapacity *= cardinfo->CardBlockSize;
  } else if (1 == sd_version) {
    cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
    cardinfo->CardCapacity =
        (cardinfo->SD_csd.DeviceSize + 1)
        << (cardinfo->SD_csd.CSizeMlut + 2 + cardinfo->SD_csd.RdBlockLen);
  }
  /*!< Returns the reponse */
  return 0;
}

/*
 * @brief  Initializes the SD/SD communication.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_init(void) {
  uint8_t frame[10], index, result;
  cardinfo.active = 0;

  fpioa_set_function(config.sclk_pin, FUNC_SPI1_SCLK);
  fpioa_set_function(config.mosi_pin, FUNC_SPI1_D0);
  fpioa_set_function(config.miso_pin, FUNC_SPI1_D1);
  fpioa_set_function(config.cs_pin, FUNC_GPIOHS0 + config.cs_gpio_num);

  /*!< Initialize SD_SPI */
  sd_lowlevel_init(0);
  /*!< SD chip select high */
  SD_CS_HIGH();
  /*!< Send dummy byte 0xFF, 10 times with CS high */
  /*!< Rise CS and MOSI for 80 clocks cycles */
  /*!< Send dummy byte 0xFF */
  for (index = 0; index < 10; index++)
    frame[index] = 0xFF;
  sd_write_data(frame, 10);
  /*------------Put SD in SPI mode--------------*/
  /*!< SD initialized and set to SPI mode properly */

  sd_send_cmd(SD_CMD0, 0);
  result = sd_get_response();
  sd_end_cmd();
  if (result != 0x01) {
    // Avoid printing this error while navigating and SD is not present
    // printk("[MaixPy] %s | SD_CMD0 is %X\r\n", __func__, result);
    return 0xFF;
  }

  sd_send_cmd(SD_CMD8, 0x01AA);
  /*!< 0x01 or 0x05 */
  result = sd_get_response();
  sd_read_data(frame, 4);
  sd_end_cmd();
  if (result != 0x01) {
    printk("[MaixPy] %s | SD_CMD8 is %X\r\n", __func__, result);
    return 0xFF;
  }
  index = 0xFF;
  while (index--) {
    sd_send_cmd(SD_CMD55, 0);
    result = sd_get_response();
    sd_end_cmd();
    if (result != 0x01 && result != 0x00) {
      printk("SD_CMD55 ack %X\r\n", result);
      return 0xFF;
    }
    sd_send_cmd(SD_ACMD41, 0x40000000);
    result = sd_get_response();
    sd_end_cmd();
    if (result == 0x00)
      break;
  }
  if (index == 0) {
    printk("SD_CMD55 is %X\r\n", result);
    return 0xFF;
  }
  sd_send_cmd(SD_CMD58, 0);
  result = sd_get_response();
  sd_read_data(frame, 4);
  sd_end_cmd();
  if (result != 0x00 && result != 0x01) {
    printk("[MaixPy] %s | SD_CMD58 is %X\r\n", __func__, result);
    return 0xFF;
  }
  if ((frame[0] & 0x40) == 0) {
#if CONFIG_SPI_SD_CARD_FORCE_HIGH_SPEED
    SD_HIGH_SPEED_ENABLE();
#endif
    sd_version = 1;
  } else {
    sd_version = 2;
    SD_HIGH_SPEED_ENABLE();
  }
  if (1 == sd_version) {
    sd_send_cmd(SD_CMD16, 512);
    if (sd_get_response() != 0x00) {
      sd_end_cmd();
      return 0xFF;
    }
  }
  if (0 == sd_get_cardinfo(&cardinfo)) {
    cardinfo.active = 1;
    return 0;
  }
  return 0xFF;
}

/*
 * @brief  Reads a block of data from the SD.
 * @param  data_buff: pointer to the buffer that receives the data read from the
 *                  SD.
 * @param  sector: SD's internal address to read from.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count) {
  uint8_t frame[2], flag;
  /*!< Send CMD17 (SD_CMD17) to read one block */
  if (count == 1) {
    flag = 0;
    sd_send_cmd(SD_CMD17, sector);
  } else {
    flag = 1;
    sd_send_cmd(SD_CMD18, sector);
  }
  /*!< Check if the SD acknowledged the read block command: R1 response (0x00:
   * no errors) */
  if (sd_get_response() != 0x00) {
    sd_end_cmd();
    return 0xFF;
  }
  while (count) {
    if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
      break;
    /*!< Read the SD block data : read NumByteToRead data */
    sd_read_data(data_buff, 512);
    /*!< Get CRC bytes (not really needed by us, but required by SD) */
    sd_read_data(frame, 2);
    data_buff += 512;
    count--;
  }
  sd_end_cmd();
  if (flag) {
    sd_send_cmd(SD_CMD12, 0);
    sd_get_response();
    sd_end_cmd();
    sd_end_cmd();
  }
  /*!< Returns the reponse */
  return count > 0 ? 0xFF : 0;
}

/*
 * @brief  Writes a block on the SD
 * @param  data_buff: pointer to the buffer containing the data to be written on
 *                  the SD.
 * @param  sector: address to write on.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count) {
  configASSERT(((uint32_t)data_buff) % 4 == 0);
  uint8_t frame[2] = {0xFF};
  if (count == 1) {
    frame[1] = SD_START_DATA_SINGLE_BLOCK_WRITE;
    sd_send_cmd(SD_CMD24, sector);
  } else {
    frame[1] = SD_START_DATA_MULTIPLE_BLOCK_WRITE;
    sd_send_cmd(SD_ACMD23, count);
    sd_get_response();
    sd_end_cmd();
    sd_send_cmd(SD_CMD25, sector);
  }
  /*!< Check if the SD acknowledged the write block command: R1 response (0x00:
   * no errors) */
  if (sd_get_response() != 0x00) {
    sd_end_cmd();
    return 0xFF;
  }
  while (count--) {
    /*!< Send the data token to signify the start of the data */
    sd_write_data(frame, 2);
    /*!< Write the block data to SD : write count data by block */
    sd_write_data(data_buff, 512);
    /*!< Put CRC bytes (not really needed by us, but required by SD) */
    sd_write_data(frame, 2);
    data_buff += 512;
    /*!< Read data response */
    if (sd_get_dataresponse() != 0x00) {
      sd_end_cmd();
      return 0xFF;
    }
  }
  sd_end_cmd();
  sd_end_cmd();
  /*!< Returns the reponse */
  return 0;
}

uint8_t sd_read_sector_dma(uint8_t *data_buff, uint32_t sector,
                           uint32_t count) {
  uint8_t frame[2], flag;
  if (1 == sd_version)
    sector = sector << 9;
  /*!< Send CMD17 (SD_CMD17) to read one block */
  if (count == 1) {
    flag = 0;
    sd_send_cmd(SD_CMD17, sector);
  } else {
    flag = 1;
    sd_send_cmd(SD_CMD18, sector);
  }
  /*!< Check if the SD acknowledged the read block command: R1 response (0x00:
   * no errors) */
  if (sd_get_response() != 0x00) {
    printk("%s sd_get_response() != 0x00 %d\r\n", __func__, flag);
    sd_end_cmd();
    return 0xFF;
  }
  while (count) {
    if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
      break;
    /*!< Read the SD block data : read NumByteToRead data */
    sd_read_data_dma(data_buff); // close recv dma
    /*!< Get CRC bytes (not really needed by us, but required by SD) */
    sd_read_data(frame, 2);
    data_buff += 512;
    count--;
  }
  sd_end_cmd();
  if (flag) {
    sd_send_cmd(SD_CMD12, 0);
    sd_get_response();
    sd_end_cmd();
    sd_end_cmd();
  }
  /*!< Returns the reponse */
  return count > 0 ? 0xFF : 0;
}

uint8_t sd_write_sector_dma(uint8_t *data_buff, uint32_t sector,
                            uint32_t count) {
  uint8_t frame[2] = {0xFF};
  uint32_t shift = 0;
  frame[1] = SD_START_DATA_SINGLE_BLOCK_WRITE;
  uint32_t i = 0;
  if (1 == sd_version)
    sector = sector << 9;
  while (count--) {
    if (1 == sd_version)
      shift = i << 9;
    else
      shift = i;
    sd_send_cmd(SD_CMD24, sector + shift);
    /*!< Check if the SD acknowledged the write block command: R1 response
     * (0x00: no errors) */
    if (sd_get_response() != 0x00) {
      sd_end_cmd();
      return 0xFF;
    }

    /*!< Send the data token to signify the start of the data */
    sd_write_data(frame, 2);
    /*!< Write the block data to SD : write count data by block */
    sd_write_data_dma(data_buff);
    /*!< Put CRC bytes (not really needed by us, but required by SD) */
    sd_write_data(frame, 2);
    data_buff += 512;
    /*!< Read data response */
    if (sd_get_dataresponse() != 0x00) {
      sd_end_cmd();
      return 0xFF;
    }
    i++;
  }
  sd_end_cmd();
  sd_end_cmd();
  /*!< Returns the reponse */
  return 0;
}
