#ifdef ARDUINO_ARCH_RP2040
#include "FileTransferModule.h"
#endif
#ifdef OKNXHW_REG2_DEVICE_DISPLAY
#include "GPIOModule.h"
#endif
#include "GpioBinaryInputModule.h"
#include "Logic.h"
#include "MeterModule.h"
#include "OpenKNX.h"
#include "SMLModule.h"
#ifdef ARDUINO_ARCH_RP2040
#include "UsbExchangeModule.h"
#endif
#include "VirtualButtonModule.h"
#include <SoftwareSerial.h>

#ifdef SML_TEST_STRINGS
    #include "SMLSamples.h"
#endif

#ifdef DEVICE_DISPLAY_MODULE
    #include "DeviceDisplay.h"
#endif

#ifdef DEVICE_RTC_MODULE
    #include "DeviceRTC.h"
#endif

#ifdef EXTERNAL_FLASH_MODULE
    #include "ExternalFlash.h"
#endif

// #define W25Q128_FLASH_TEST

#ifdef W25Q128_FLASH_TEST
    #define FLASH_CS 13
    #define FLASH_SCK 10
    #define FLASH_MOSI 11
    #define FLASH_MISO 12
    #define FLASH_WP 14
    #define FLASH_HOLD 15
    #define CMD_READ_ID 0x9F

/*
struct ChipID
{
    uint8_t manufacturerID;
    uint8_t memoryType;
    uint8_t capacity;
};
*/

ChipID readChipID()
{
    ChipID id = {0, 0, 0};
    uint8_t buf[3] = {0};

    digitalWrite(FLASH_CS, LOW);
    openknx.logger.log("Reading Chip ID...");

    SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    openknx.logger.log("CMD_READ_ID sent...");
    SPI1.transfer(CMD_READ_ID);
    buf[0] = SPI1.transfer(0); // Manufacturer ID
    buf[1] = SPI1.transfer(0); // Memory Type
    buf[2] = SPI1.transfer(0); // Capacity
    SPI1.endTransaction();

    digitalWrite(FLASH_CS, HIGH);

    id.manufacturerID = buf[0];
    id.memoryType = buf[1];
    id.capacity = buf[2];

    openknx.logger.log(String("Manufacturer ID: " + String(id.manufacturerID, HEX)).c_str());
    openknx.logger.log(String("Memory Type: " + String(id.memoryType, HEX)).c_str());
    openknx.logger.log(String("Capacity: " + String(id.capacity, HEX)).c_str());

    return id;
}

    #define CMD_WRITE_ENABLE 0x06 // Write Enable
    #define CMD_PAGE_PROGRAM 0x02 // Page Program
    #define CMD_SECTOR_ERASE 0x20 // 4KB Sector Erase
    #define CMD_READ_DATA 0x03    // Read Data

// Funktion zum Überprüfen, ob der Flash-Speicher beschäftigt ist
bool isFlashBusy()
{
    uint8_t status;
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(0x05); // Read Status Register-1 Command
    status = SPI1.transfer(0x00);
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);
    return (status & 0x01); // BUSY-Bit prüfen
}

// Funktion zum Löschen eines 4KB-Sektors
void eraseSector(uint32_t address)
{
    openknx.logger.log("Erasing 4KB sector...");

    // Write Enable
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_WRITE_ENABLE);
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Erase Command
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_SECTOR_ERASE);
    SPI1.transfer((address >> 16) & 0xFF); // MSB
    SPI1.transfer((address >> 8) & 0xFF);  // Middle Byte
    SPI1.transfer(address & 0xFF);         // LSB
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Warten, bis das Löschen abgeschlossen ist (überprüft den Busy-Status)
    while (isFlashBusy())
    {
        delay(10);
    }
    openknx.logger.log("Sector erased.");
}

// 2. Funktion zum Schreiben einer Seite (max. 256 Bytes)
void writePage(uint32_t address, uint8_t* data, size_t len)
{
    if (len > 256) len = 256; // Maximal 256 Bytes pro Page

    // Write Enable
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_WRITE_ENABLE);
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Page Program Command
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_PAGE_PROGRAM);
    SPI1.transfer((address >> 16) & 0xFF); // MSB
    SPI1.transfer((address >> 8) & 0xFF);  // Middle Byte
    SPI1.transfer(address & 0xFF);         // LSB

    // Daten schreiben
    for (size_t i = 0; i < len; i++)
    {
        SPI1.transfer(data[i]);
    }

    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Warten, bis der Schreibvorgang abgeschlossen ist
    while (isFlashBusy())
    {
        delay(10);
    }
    openknx.logger.log("Page written.");
}

// 4. Hauptfunktion zum Schreiben und Lesen
void testFlashMemory()
{
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t readBuf[4] = {0};
    uint32_t address = 0x000100; // Beispieladresse im ersten Sektor

    openknx.logger.log("Starting Flash memory test...");

    // Löschen des 4KB-Sektors
    eraseSector(address);

    // Schreiben der Daten
    writePage(address, data, sizeof(data));

    // Daten lesen
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_READ_DATA);
    SPI1.transfer((address >> 16) & 0xFF);
    SPI1.transfer((address >> 8) & 0xFF);
    SPI1.transfer(address & 0xFF);

    for (size_t i = 0; i < sizeof(readBuf); i++)
    {
        readBuf[i] = SPI1.transfer(0x00);
    }

    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Ausgabe der gelesenen Daten
    openknx.logger.log("Data read from Flash:");
    openknx.logger.log(String(String("Expected: DEADBEEF, Got: ") + String(readBuf[0], HEX) + String(readBuf[1], HEX) + String(readBuf[2], HEX) + String(readBuf[3], HEX)).c_str());

    if (memcmp(data, readBuf, sizeof(data)) == 0)
    {
        openknx.logger.log("Data match!");
    }
    else
    {
        openknx.logger.log("Data mismatch!");
    }
}

void _flashTest()
{

    // Initialisierung der GPIOs
    pinMode(FLASH_CS, OUTPUT);
    digitalWrite(FLASH_CS, HIGH);

    pinMode(FLASH_WP, OUTPUT);
    digitalWrite(FLASH_WP, HIGH);

    pinMode(FLASH_HOLD, OUTPUT);
    digitalWrite(FLASH_HOLD, HIGH);

    SPI1.setSCK(FLASH_SCK);
    SPI1.setTX(FLASH_MOSI);
    SPI1.setRX(FLASH_MISO);
    SPI1.begin();

    delay(100); // Warten auf Initialisierung

    // W25Q128 erkennen
    ChipID chipID = readChipID();
    if (chipID.manufacturerID == 0xEF && chipID.memoryType == 0x40 && chipID.capacity == 0x18)
    { // W25Q128 ID
        openknx.logger.log("W25Q128 erkannt!");
    }
    else
    {
        openknx.logger.log(String("W25Q128 nicht erkannt! Chip ID: " + String(chipID.manufacturerID, HEX) + " " + String(chipID.memoryType, HEX) + " " + String(chipID.capacity, HEX)).c_str());
        return;
    }
    delay(1000);

    // W25Q128 Flash testen
    openknx.logger.log("Testing W25Q128 Flash memory...");
    testFlashMemory();

    // openknx.logger.log("Erase sector...");
    // testEraseSector();
    // openknx.logger.log("Write and test full memory...");
    // writeAndTestFullMemory();
    // openknx.logger.log("Read and test full memory...");
    // readAndTestFullMemory();
    openknx.logger.log("Flash test done!");
}

    #define SECTOR_SIZE_W25Q128_4KB 4096          // 4KB
    #define PAGE_SIZE_W25Q128_256B 256            // 256 Bytes
    #define FLASH_SIZE_W25Q128 (16 * 1024 * 1024) // 16MB (128Mbit) --> 16 x 1024 x 1024 Bytes = 16777216 Bytes (Hex: 0x1000000)

// LittleFS Configuration
lfs_t lfs;
lfs_file_t file;

/**
 * @brief Function to check if the flash memory is busy.
 *        We need this function to check if the flash memory is busy before we can write or read data.
 *        The function sends a command to the flash memory to read the status register and checks the busy bit.
 *
 * @return true, if the flash memory is busy
 * @return false, if the flash memory is not busy
 */
bool lfs_isFlashBusy()
{
    uint8_t status;
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(0x05); // Read Status Register
    status = SPI1.transfer(0x00);
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);
    return (status & 0x01);
}

/**
 * @brief Function to read data from the flash memory
 *
 * @param c, LittleFS configuration (using the Port of LittleFS to RP2040 Arduino)
 * @param block, block number to read
 * @param off, offset in the block
 * @param buffer, buffer to store the read data
 * @param size, size of the data to read
 * @return int, 0 if successful
 */
int lfs_read(const struct lfs_config* c, lfs_block_t block,
             lfs_off_t off, void* buffer, lfs_size_t size)
{
    uint32_t addr = (block * c->block_size) + off;
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0)); // SPI-Settings: 8MHz, MSB first, Mode 0
    SPI1.transfer(CMD_READ_DATA);                                     // Read Data Command for W25Q128 Flash (0x03)
    SPI1.transfer((addr >> 16) & 0xFF);                               // MSB
    SPI1.transfer((addr >> 8) & 0xFF);                                // Middle Byte
    SPI1.transfer(addr & 0xFF);                                       // LSB
    for (uint32_t i = 0; i < size; i++)
    {
        ((uint8_t*)buffer)[i] = SPI1.transfer(0x00); // Read data
    }
    SPI1.endTransaction();        // End SPI transaction
    digitalWrite(FLASH_CS, HIGH); // Set CS high
    return 0;                     // Return 0 if successful, we just assume that the read operation is always successful
}

/**
 * @brief Function to write data to the flash memory
 *
 * @param c, LittleFS configuration (using the Port of LittleFS to RP2040 Arduino)
 * @param block, block number to write
 * @param off, offset in the block
 * @param buffer, buffer with the data to write
 * @param size, size of the data to write
 * @return int, 0 if successful
 */
int lfs_prog(const struct lfs_config* c, lfs_block_t block,
             lfs_off_t off, const void* buffer, lfs_size_t size)
{
    uint32_t addr = (block * c->block_size) + off;
    // Write Enable
    digitalWrite(FLASH_CS, LOW);                                      // Set CS low
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0)); // SPI-Settings: 8MHz, MSB first, Mode 0
    SPI1.transfer(CMD_WRITE_ENABLE);                                  // Write Enable Command for W25Q128 Flash (0x06)
    SPI1.endTransaction();                                            // End SPI transaction
    digitalWrite(FLASH_CS, HIGH);                                     // Set CS high

    // Page Program Command
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_PAGE_PROGRAM); // Page Program Command for W25Q128 Flash (0x02)
    SPI1.transfer((addr >> 16) & 0xFF);
    SPI1.transfer((addr >> 8) & 0xFF);
    SPI1.transfer(addr & 0xFF);
    for (uint32_t i = 0; i < size; i++)
    {
        SPI1.transfer(((const uint8_t*)buffer)[i]);
    }
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Lets wait until the write operation is finished. This is a blocking operation!!
    while (lfs_isFlashBusy())
        delay(1);
    return 0;
}

/**
 * @brief Function to erase a block in the flash memory
 *
 * @param c, LittleFS configuration (using the Port of LittleFS to RP2040 Arduino)
 * @param block, block number to erase
 * @return int, 0 if successful
 */
int lfs_erase(const struct lfs_config* c, lfs_block_t block)
{
    uint32_t addr = block * c->block_size;
    // Write Enable
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0)); // SPI-Settings: 8MHz, MSB first, Mode 0
    SPI1.transfer(CMD_WRITE_ENABLE);                                  // Write Enable Command for W25Q128 Flash (0x06)
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Erase Sector
    digitalWrite(FLASH_CS, LOW);
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    SPI1.transfer(CMD_SECTOR_ERASE); // Sector Erase Command for W25Q128 Flash (0x20)
    SPI1.transfer((addr >> 16) & 0xFF);
    SPI1.transfer((addr >> 8) & 0xFF);
    SPI1.transfer(addr & 0xFF);
    SPI1.endTransaction();
    digitalWrite(FLASH_CS, HIGH);

    // Lets wait until the erase operation is finished. This is a blocking operation!!
    while (lfs_isFlashBusy())
        delay(1);
    return 0;
}

int lfs_sync(const struct lfs_config* c)
{
    return 0; // Sync operation for SPI flash is not needed! We just return 0.
              // ToDo EC: Check the LittleFS documentation for more information about the sync operation.
}

// COnfiguration for LittleFS with the W25Q128 Flash
const struct lfs_config cfg = {
    .context = nullptr, // User defined context, we don't need it here

    // Lets set the callbacks for our W25Q128 Flash
    .read = lfs_read,   // read callback for our W25Q128 Flash
    .prog = lfs_prog,   // program callback for our W25Q128 Flash
    .erase = lfs_erase, // erase callback for our W25Q128 Flash
    .sync = lfs_sync,   // sync callback for pur W25Q128 Flash

    #ifdef LFS_THREADSAFE
    .lock = nullptr,   // If thread-safety is needed
    .unlock = nullptr, // If thread-safety is needed
    #endif

    // Size configuration for the W25Q128 Flash
    .read_size = PAGE_SIZE_W25Q128_256B,                         // Minimale read size
    .prog_size = PAGE_SIZE_W25Q128_256B,                         // Minimale program size
    .block_size = SECTOR_SIZE_W25Q128_4KB,                       // Sector size of the flash device (4KB)
    .block_count = FLASH_SIZE_W25Q128 / SECTOR_SIZE_W25Q128_4KB, // Number of sectors of the flash device

    .block_cycles = 500,                  // Number of write cycles per block
    .cache_size = PAGE_SIZE_W25Q128_256B, // Cache size
    .lookahead_size = 16,                 // Lookahead buffer size
    .compact_thresh = 0,                  // Default compaxt threshold

    // Static buffers for LittleFS (if needed) we don't need them here
    .read_buffer = nullptr,
    .prog_buffer = nullptr,
    .lookahead_buffer = nullptr,

    // Set maximum sizes
    .name_max = 255,   // Max filname length
    .file_max = 0,     // Max number of files open at the same time, 0 for default
    .attr_max = 0,     // Max number of attributes, 0 for default
    .metadata_max = 0, // Max metadata size, 0 for default
    .inline_max = 0    // Max inline data size. 0 for default

    #ifdef LFS_MULTIVERSION
    ,
    .disk_version = 0 // default disk version. 0 seems to be the recent version
    #endif
};

// Tesz function for LittleFS with the W25Q128 Flash
void _littlefsW25Q128Test()
{
    SPI1.begin();
    pinMode(FLASH_CS, OUTPUT);
    digitalWrite(FLASH_CS, HIGH);

    // Initialise the LittleFS filesystem using the LittleFS.h - Filesystem wrapper for LittleFS on the RP2040
    int err = lfs_mount(&lfs, &cfg);
    if (err)
    {
        openknx.logger.log("Error mounting LittleFS");
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    // Zeige die freien Speicherplatz
    lfs_ssize_t total_blocks = lfs_fs_size(&lfs);
    openknx.logger.log(String("LittleFS: Total Blocks: " + String(total_blocks)).c_str());
    openknx.logger.log(String("LittleFS: Total Block Size: " + String(total_blocks * cfg.block_size) + " Bytes").c_str());
    // ich will jetzt in KB freien und benutzten Speicherplatz anzeigen, berechne die Werte anhand der Blockgröße ohne lfs_fs_size
    uint32_t totalSizeKB = (cfg.block_count * cfg.block_size) / 1024;
    uint32_t usedSizeKB = (total_blocks * cfg.block_size) / 1024;
    uint32_t freeSizeKB = totalSizeKB - usedSizeKB;

    openknx.logger.log(String("LittleFS: Total Size: " + String(totalSizeKB) + " KB (" + String((float)totalSizeKB / 1024) + " MB)").c_str());
    openknx.logger.log(String("LittleFS: Used Size: " + String(usedSizeKB) + " KB (" + String((float)usedSizeKB / 1024) + " MB)").c_str());
    openknx.logger.log(String("LittleFS: Free Size: " + String(freeSizeKB) + " KB (" + String((float)freeSizeKB / 1024) + " MB)").c_str());

    // Mach ein speed test
    openknx.logger.log("Starting LittleFS speed test...");
    openknx.logger.log("Writing 1MB (250 blocks) to LittleFS this may take a while...");
    uint32_t start = millis();
    for (int i = 0; i < 250; i++)
    { // Schreibe 250 Blöcke. 1 Block = 4KB insgesamt werden 1MB geschrieben
        char buffer[32];
        sprintf(buffer, "Block %d", i);
        lfs_file_open(&lfs, &file, buffer, LFS_O_RDWR | LFS_O_CREAT);
        lfs_file_write(&lfs, &file, buffer, strlen(buffer));
        lfs_file_close(&lfs, &file);
        // Zeige den Fortschritt in % an. mach 5% Schritte
        if (i % 50 == 0)
        {
            openknx.logger.log(String("Writing block " + String(i) + " of 250").c_str());
        }
    }
    uint32_t end = millis();
    openknx.logger.log(String("Writing 1MB (250 blocks) took " + String(end - start) + "ms (" + String((float)(end - start) / 1000) + "s)").c_str());
    // Jetzt lesen wir die 1000 Blöcke
    openknx.logger.log("Reading  250 blocks (1MB) from LittleFS this may take a while...");
    start = millis();
    for (int i = 0; i < 250; i++)
    { // Lese 250 Blöcke. 1 Block = 4KB insgesamt werden 1MB gelesen
        char buffer[32];
        sprintf(buffer, "Block %d", i);
        lfs_file_open(&lfs, &file, buffer, LFS_O_RDONLY);
        lfs_file_read(&lfs, &file, buffer, sizeof(buffer));
        lfs_file_close(&lfs, &file);
        // Zeige den Fortschritt in % an. mach 10% Schritte
        if (i % 100 == 0)
        {
            openknx.logger.log(String("Reading block " + String(i) + " of 250").c_str());
        }
    }
    end = millis();
    openknx.logger.log(String("Reading 1MB (250 blocks) took " + String(end - start) + "ms (" + String((float)(end - start) / 1000) + "s)").c_str());

    // Write the hello.txt file
    openknx.logger.log("Writing 'hello.txt' with message 'Hello, W25Q128 Flash!' to LittleFS...");
    lfs_file_open(&lfs, &file, "hello.txt", LFS_O_RDWR | LFS_O_CREAT);
    const char* msg = "Hello, W25Q128 Flash!";
    lfs_file_write(&lfs, &file, msg, strlen(msg));
    lfs_file_close(&lfs, &file);

    // Read the hello.txt file
    openknx.logger.log("Reading 'hello.txt' from LittleFS...");
    lfs_file_open(&lfs, &file, "hello.txt", LFS_O_RDONLY);
    char buffer[32] = {0};
    lfs_file_read(&lfs, &file, buffer, sizeof(buffer));
    openknx.logger.log(String("Read: " + String(buffer)).c_str());
    lfs_file_close(&lfs, &file);

    // LittleFS unmount
    lfs_unmount(&lfs);
}

#endif // W25Q128_FLASH_TEST

// #define W25Q128_LITTLEFS_WITH_CONFIGURABLE
#ifdef W25Q128_LITTLEFS_WITH_CONFIGURABLE

// Instanz von ConfigurableLittleFS
void testLittleFSWithConfigurable()
{
    openknx.logger.log("Testing LittleFS with ConfigurableLittleFS...");

    ConfigurableLittleFS fs(true); // true = use external flash, false = use internal flash

    openknx.logger.log("Initializing ConfigurableLittleFS...");
    if (!fs.begin())
    {
        openknx.logger.log("Failed to initialize ConfigurableLittleFS.");
        return;
    }
    openknx.logger.log("ConfigurableLittleFS initialized.");

    openknx.logger.log("Start to test the onboard W25Q128 flash memory");
    //_flashTest();
    ChipID chipID = fs.externalFlash.readID();
    openknx.logger.log(String(String("Manufacturer ID: " + String(chipID.manufacturerID, HEX))).c_str());
    openknx.logger.log(String(String("Memory Type: " + String(chipID.memoryType, HEX))).c_str());
    openknx.logger.log(String(String("Capacity: " + String(chipID.capacity, HEX))).c_str());

    openknx.logger.log("Start Write/Read Test with External Flash...");
    if (fs.externalFlash.Test_BlockWriteRead())
    {
        openknx.logger.log("External Flash Test successful!");
    }
    else
    {
        openknx.logger.log("External Flash Test failed!");
    }
    openknx.logger.log("Start to test the onboard W25Q128 flash memory with LittleFS...");

    // Schreibgeschwindigkeit testen
    openknx.logger.log("Starting LittleFS speed test...");
    openknx.logger.log("Writing 1MB (250 blocks) to LittleFS...");
    uint32_t start = millis();
    for (int i = 0; i < 250; i++)
    {
        String filename = "/block" + String(i);
        File file = fs.open(filename.c_str(), "w"); // Öffnen im Schreibmodus ("w")
        if (!file)
        {
            openknx.logger.log(String("Failed to open file: " + filename).c_str());
            continue;
        }
        file.print("Block " + String(i));
        file.close();

        if (i % 50 == 0)
        {
            openknx.logger.log(String("Writing block " + String(i) + " of 250").c_str());
        }
    }
    uint32_t end = millis();
    openknx.logger.log(String("Writing 1MB took " + String(end - start) + "ms (" + String((float)(end - start) / 1000) + "s)").c_str());

    // Lesegeschwindigkeit testen
    openknx.logger.log("Reading 250 blocks (1MB) from LittleFS...");
    start = millis();
    for (int i = 0; i < 250; i++)
    {
        String filename = "/block" + String(i);
        File file = fs.open(filename.c_str(), "r"); // Öffnen im Lesemodus ("r")
        if (!file)
        {
            openknx.logger.log(String("Failed to open file: " + filename).c_str());
            continue;
        }
        char buffer[32];
        file.readBytes(buffer, sizeof(buffer));
        file.close();

        if (i % 100 == 0)
        {
            openknx.logger.log(String("Reading block " + String(i) + " of 250").c_str());
        }
    }
    end = millis();
    openknx.logger.log(String("Reading 1MB took " + String(end - start) + "ms (" + String((float)(end - start) / 1000) + "s)").c_str());

    // Schreiben und Lesen einer Beispieldatei
    openknx.logger.log("Writing 'hello.txt' with message 'Hello, ConfigurableLittleFS!'...");
    File file = fs.open("/hello.txt", "w");
    if (file)
    {
        file.print("Hello, ConfigurableLittleFS!");
        file.close();
    }
    else
    {
        openknx.logger.log("Failed to write 'hello.txt'.");
    }

    openknx.logger.log("Reading 'hello.txt' from LittleFS...");
    file = fs.open("/hello.txt", "r");
    if (file)
    {
        String content = file.readString();
        openknx.logger.log(String("Read: " + content).c_str());
        file.close();
    }
    else
    {
        openknx.logger.log("Failed to read 'hello.txt'.");
    }
}
#endif // W25Q128_LITTLEFS_WITH_CONFIGURABLE

void setup()
{
    const uint8_t firmwareRevision = 0;
    openknx.init(firmwareRevision);
    openknx.addModule(1, openknxLogic);
    openknx.addModule(2, openknxMeterModule);
    openknx.addModule(3, openknxSMLModule);
#if defined(OPENKNX_BI_GPIO_PINS) && OPENKNX_BI_GPIO_COUNT > 0 && BI_ChannelCount > 0
    openknx.addModule(6, openknxGpioBinaryInputModule);
#endif
    openknx.addModule(7, openknxVirtualButtonModule);
    #ifdef ARDUINO_ARCH_RP2040
    openknx.addModule(8, openknxUsbExchangeModule);
    openknx.addModule(9, openknxFileTransferModule);
    #endif
#ifdef DEVICE_DISPLAY_MODULE
    openknx.addModule(10, openknxDisplayModule);
#endif
#ifdef DEVICE_RTC_MODULE
    #ifdef OKNXHW_REG2_DEVICE_RTC
    openknxRTCModule.setI2CSettings(OKNXHW_DEVICE_RTC_I2C_INST, OKNXHW_DEVICE_RTC_I2C_SCL, OKNXHW_DEVICE_RTC_I2C_SDA,
                                    OKNXHW_DEVICE_RTC_I2C_ADDRESS, OKNXHW_DEVICE_RTC_EEPROM_I2C_ADDRESS, OKNXHW_REG2_HWRTC_I2C_EEPROM_SIZE);

    openknx.addModule(11, openknxRTCModule);
    #endif
#endif
#ifdef EXTERNAL_FLASH_MODULE
    openknx.addModule(12, extFlashModule);
#endif

#ifdef OKNXHW_REG2_DEVICE_DISPLAY
    openknx.addModule(20, openknxGPIOModule);
#endif

    openknx.setup();

#if defined(INFO3_LED_PIN)
    // openknx.info3Led.activity(openknxSMLModule.lastReceived);
#elif defined(INFO1_LED_PIN)
    // openknx.info1Led.activity(openknxSMLModule.lastReceived);
#endif

#if defined(DEVICE_PIPICO_BCU_CONNECTOR)

    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, 9, 64U));

    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    openknxSMLModule.getChannel(1)->setSerial(new SerialPIO(SerialPIO::NOPIN, 11, 64U));

    pinMode(26, OUTPUT);
    digitalWrite(26, HIGH);
    openknxSMLModule.getChannel(2)->setSerial(new SerialPIO(SerialPIO::NOPIN, 27, 64U));

#elif defined(DEVICE_REG1_BASE_V0) || defined(DEVICE_REG1_BASE) || defined(DEVICE_REG1_SEN_MULTI)

    pinMode(OKNXHW_REG1_SENSOR_SDA_TX_PIN, OUTPUT);
    digitalWrite(OKNXHW_REG1_SENSOR_SDA_TX_PIN, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, 9, 64U)); // Onboard

    #ifdef DEVICE_REG1_SEN_MULTI
    pinMode(OKNXHW_REG1_APP_SEN_MULTI_SENSOR1_SDA_TX_PIN, OUTPUT);
    digitalWrite(OKNXHW_REG1_APP_SEN_MULTI_SENSOR1_SDA_TX_PIN, HIGH);
    openknxSMLModule.getChannel(1)->setSerial(new SerialPIO(SerialPIO::NOPIN, OKNXHW_REG1_APP_SEN_MULTI_SENSOR1_SCL_RX_PIN, 64U)); // SML Platine A (oben)

    pinMode(OKNXHW_REG1_APP_SEN_MULTI_SENSOR2_SDA_TX_PIN, OUTPUT);
    digitalWrite(OKNXHW_REG1_APP_SEN_MULTI_SENSOR2_SDA_TX_PIN, HIGH);
    openknxSMLModule.getChannel(2)->setSerial(new SerialPIO(SerialPIO::NOPIN, OKNXHW_REG1_APP_SEN_MULTI_SENSOR2_SCL_RX_PIN, 64U)); // SML Platine B (unten)
    #endif

#elif defined(OKNXHW_REG2_PIPICO_V1_BASE)

    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, 4, 64U));

    pinMode(6, OUTPUT);
    digitalWrite(6, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, 6, 64U));

#elif defined(OKNXHW_OPENKNXIAO_RP2040_MINI_V1_METER)

    pinMode(OKNXHW_OPENKNXIAO_MSENS_1_SCL0_RX_PIN, OUTPUT);
    digitalWrite(OKNXHW_OPENKNXIAO_MSENS_1_SCL0_RX_PIN, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, OKNXHW_OPENKNXIAO_MSENS_1_SCL0_RX_PIN, 64U));

    pinMode(OKNXHW_OPENKNXIAO_MSENS_2_SCL1_RX_PIN, OUTPUT);
    digitalWrite(OKNXHW_OPENKNXIAO_MSENS_2_SCL1_RX_PIN, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, OKNXHW_OPENKNXIAO_MSENS_2_SCL1_RX_PIN, 64U));

#elif defined(DEVICE_SMARTMF_1TE_BE_3CH)

    pinMode(SMARTMF_BE_VCC_PIN, OUTPUT);
    digitalWrite(SMARTMF_BE_VCC_PIN, HIGH);

#elif defined(DEVICE_SMARTMF_2SML_3BE)

    pinMode(SMARTMF_SML1_TX_PIN, OUTPUT);
    pinMode(SMARTMF_SML2_TX_PIN, OUTPUT);
    digitalWrite(SMARTMF_SML1_TX_PIN, HIGH);
    digitalWrite(SMARTMF_SML2_TX_PIN, HIGH);
    openknxSMLModule.getChannel(0)->setSerial(new SerialPIO(SerialPIO::NOPIN, SMARTMF_SML1_RX_PIN, 64U));
    openknxSMLModule.getChannel(1)->setSerial(new SerialPIO(SerialPIO::NOPIN, SMARTMF_SML2_RX_PIN, 64U));
#endif
#ifdef W25Q128_FLASH_TEST
    openknx.logger.log("Setup done!");
    openknx.logger.log("Start to test the onboard W25Q128 flash memory");
    _flashTest();
    openknx.logger.log("Start to test the onboard W25Q128 flash memory with LittleFS...");
    delay(1000); // Wait a second
    _littlefsW25Q128Test();
#endif
#ifdef W25Q128_LITTLEFS_WITH_CONFIGURABLE
    testLittleFSWithConfigurable();

#endif

#ifdef EXTERNAL_FLASH_MODULE
    if (extFlashModule.isMounted())
    {
        openknx.logger.log("External Flash mounted. Successfully initialized external LittleFS.");
    }
    else
    {
        openknx.logger.log("Failed to initialize external LittleFS.");
    }
#endif
}

uint32_t _debugCore0 = 0;
uint32_t _debugCore1 = 0;

void loop()
{
    openknx.loop();
    if (delayCheck(_debugCore0, 1000))
    {
#ifndef OPENKNX_DUALCORE
    #ifdef SML_TEST_STRINGS
        for (int i = 0; i < sizeof(smlResponse1); i++)
        {
            openknxSMLModule.getChannel(0)->writeBuffer(smlResponse1[i]);
        }
    #endif
#endif
        _debugCore0 = millis();
    }
}

#ifdef OPENKNX_DUALCORE
void setup1()
{
    openknx.setup1();
}

void loop1()
{
    openknx.loop1();

    if (delayCheck(_debugCore1, 5000))
    {
    #ifdef SML_TEST_STRINGS
        for (int i = 0; i < sizeof(smlResponse1); i++)
        {
            openknxSMLModule.getChannel(0)->writeBuffer(smlResponse1[i]);
        }
    #endif
        _debugCore1 = millis();
    }
}
#endif