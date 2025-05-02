#include "hx_drv_i2c_m.h"
#include "hx_drv_timer.h"
#include "hx_drv_swreg_aon.h"
#include "WE2_debug.h"

// I2C configuration
#define I2C_FILE_TRANSFER_SLAVE_ADDR     0x42    // Slave address
#define I2C_BUFFER_SIZE                  256     // Buffer size for I2C transfers
#define I2C_FILE_CHUNK_SIZE              128     // Size of each file chunk transfer

// File operation commands
#define CMD_FILE_OPEN                    0x01
#define CMD_FILE_CLOSE                   0x02
#define CMD_FILE_READ                    0x03
#define CMD_FILE_WRITE                   0x04
#define CMD_FILE_STATUS                  0x05

// Status codes
#define FILE_STATUS_SUCCESS              0x00
#define FILE_STATUS_ERROR                0x01
#define FILE_STATUS_EOF                  0x02

// File transfer structures
typedef struct {
    uint8_t cmd;          // Command type
    uint8_t status;       // Status of operation
    uint16_t length;      // Length of data
    uint8_t data[I2C_FILE_CHUNK_SIZE]; // Data buffer
} i2c_file_transfer_t;

// Global variables
static uint8_t g_i2c_init_status = 0;
static i2c_file_transfer_t g_transfer_buffer;
static uint8_t g_file_open = 0;
static char g_filename[32];

/**
 * @brief Initialize I2C master
 * 
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_init(void)
{
    int32_t status = 0;
    
    if (g_i2c_init_status == 0) {
        // Initialize I2C as master
        status = hx_drv_i2cm_init(I2CM_SPEED_FAST);
        if (status != 0) {
            dbg_printf(DBG_LESS_INFO, "I2C master init failed with status %d\r\n", status);
            return status;
        }
        
        g_i2c_init_status = 1;
        dbg_printf(DBG_LESS_INFO, "I2C master initialized successfully\r\n");
    }
    
    return status;
}

/**
 * @brief Send data via I2C
 * 
 * @param data Pointer to data buffer
 * @param len Length of data to send
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_send(uint8_t *data, uint16_t len)
{
    int32_t status = 0;
    
    if (g_i2c_init_status == 0) {
        status = i2c_file_transfer_init();
        if (status != 0) {
            return status;
        }
    }
    
    // Send data to slave
    status = hx_drv_i2cm_write_data(I2C_FILE_TRANSFER_SLAVE_ADDR, data, len);
    if (status != 0) {
        dbg_printf(DBG_LESS_INFO, "I2C write failed with status %d\r\n", status);
    }
    
    return status;
}

/**
 * @brief Receive data via I2C
 * 
 * @param data Pointer to data buffer
 * @param len Length of data to receive
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_receive(uint8_t *data, uint16_t len)
{
    int32_t status = 0;
    
    if (g_i2c_init_status == 0) {
        status = i2c_file_transfer_init();
        if (status != 0) {
            return status;
        }
    }
    
    // Receive data from slave
    status = hx_drv_i2cm_read_data(I2C_FILE_TRANSFER_SLAVE_ADDR, data, len);
    if (status != 0) {
        dbg_printf(DBG_LESS_INFO, "I2C read failed with status %d\r\n", status);
    }
    
    return status;
}

/**
 * @brief Open a file for read/write
 * 
 * @param filename Name of the file
 * @param mode File mode (read, write)
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_open(const char *filename, uint8_t mode)
{
    int32_t status = 0;
    
    if (g_file_open) {
        dbg_printf(DBG_LESS_INFO, "A file is already open\r\n");
        return FILE_STATUS_ERROR;
    }
    
    // Prepare command
    memset(&g_transfer_buffer, 0, sizeof(i2c_file_transfer_t));
    g_transfer_buffer.cmd = CMD_FILE_OPEN;
    g_transfer_buffer.length = strlen(filename) + 1; // +1 for mode
    strcpy((char*)g_transfer_buffer.data, filename);
    g_transfer_buffer.data[g_transfer_buffer.length - 1] = mode;
    
    // Send command
    status = i2c_file_transfer_send((uint8_t*)&g_transfer_buffer, 4 + g_transfer_buffer.length);
    if (status != 0) {
        return status;
    }
    
    // Get response
    status = i2c_file_transfer_receive((uint8_t*)&g_transfer_buffer, 4);
    if (status != 0) {
        return status;
    }
    
    if (g_transfer_buffer.status == FILE_STATUS_SUCCESS) {
        g_file_open = 1;
        strncpy(g_filename, filename, sizeof(g_filename) - 1);
        dbg_printf(DBG_LESS_INFO, "File %s opened successfully\r\n", filename);
    } else {
        dbg_printf(DBG_LESS_INFO, "Failed to open file %s\r\n", filename);
        return FILE_STATUS_ERROR;
    }
    
    return FILE_STATUS_SUCCESS;
}

/**
 * @brief Close the currently open file
 * 
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_close(void)
{
    int32_t status = 0;
    
    if (!g_file_open) {
        dbg_printf(DBG_LESS_INFO, "No file is open\r\n");
        return FILE_STATUS_ERROR;
    }
    
    // Prepare command
    memset(&g_transfer_buffer, 0, sizeof(i2c_file_transfer_t));
    g_transfer_buffer.cmd = CMD_FILE_CLOSE;
    g_transfer_buffer.length = 0;
    
    // Send command
    status = i2c_file_transfer_send((uint8_t*)&g_transfer_buffer, 4);
    if (status != 0) {
        return status;
    }
    
    // Get response
    status = i2c_file_transfer_receive((uint8_t*)&g_transfer_buffer, 4);
    if (status != 0) {
        return status;
    }
    
    if (g_transfer_buffer.status == FILE_STATUS_SUCCESS) {
        g_file_open = 0;
        dbg_printf(DBG_LESS_INFO, "File %s closed successfully\r\n", g_filename);
    } else {
        dbg_printf(DBG_LESS_INFO, "Failed to close file %s\r\n", g_filename);
        return FILE_STATUS_ERROR;
    }
    
    return FILE_STATUS_SUCCESS;
}

/**
 * @brief Write data to the currently open file
 * 
 * @param data Pointer to data buffer
 * @param len Length of data to write
 * @return int32_t Number of bytes written, or error code
 */
int32_t i2c_file_transfer_write(uint8_t *data, uint16_t len)
{
    int32_t status = 0;
    uint16_t remaining = len;
    uint16_t offset = 0;
    uint16_t chunk_size;
    
    if (!g_file_open) {
        dbg_printf(DBG_LESS_INFO, "No file is open for writing\r\n");
        return FILE_STATUS_ERROR;
    }
    
    while (remaining > 0) {
        // Determine chunk size
        chunk_size = (remaining > I2C_FILE_CHUNK_SIZE) ? I2C_FILE_CHUNK_SIZE : remaining;
        
        // Prepare command
        memset(&g_transfer_buffer, 0, sizeof(i2c_file_transfer_t));
        g_transfer_buffer.cmd = CMD_FILE_WRITE;
        g_transfer_buffer.length = chunk_size;
        memcpy(g_transfer_buffer.data, data + offset, chunk_size);
        
        // Send command
        status = i2c_file_transfer_send((uint8_t*)&g_transfer_buffer, 4 + chunk_size);
        if (status != 0) {
            return status;
        }
        
        // Get response
        status = i2c_file_transfer_receive((uint8_t*)&g_transfer_buffer, 4);
        if (status != 0) {
            return status;
        }
        
        if (g_transfer_buffer.status != FILE_STATUS_SUCCESS) {
            dbg_printf(DBG_LESS_INFO, "Failed to write to file %s\r\n", g_filename);
            return FILE_STATUS_ERROR;
        }
        
        // Update counters
        remaining -= chunk_size;
        offset += chunk_size;
    }
    
    return len;
}

/**
 * @brief Read data from the currently open file
 * 
 * @param data Pointer to data buffer
 * @param len Maximum length of data to read
 * @return int32_t Number of bytes read, or error code
 */
int32_t i2c_file_transfer_read(uint8_t *data, uint16_t len)
{
    int32_t status = 0;
    uint16_t remaining = len;
    uint16_t offset = 0;
    uint16_t chunk_size;
    uint16_t total_read = 0;
    
    if (!g_file_open) {
        dbg_printf(DBG_LESS_INFO, "No file is open for reading\r\n");
        return FILE_STATUS_ERROR;
    }
    
    while (remaining > 0) {
        // Determine chunk size
        chunk_size = (remaining > I2C_FILE_CHUNK_SIZE) ? I2C_FILE_CHUNK_SIZE : remaining;
        
        // Prepare command
        memset(&g_transfer_buffer, 0, sizeof(i2c_file_transfer_t));
        g_transfer_buffer.cmd = CMD_FILE_READ;
        g_transfer_buffer.length = chunk_size;
        
        // Send command
        status = i2c_file_transfer_send((uint8_t*)&g_transfer_buffer, 4);
        if (status != 0) {
            return status;
        }
        
        // Get response
        status = i2c_file_transfer_receive((uint8_t*)&g_transfer_buffer, 4 + I2C_FILE_CHUNK_SIZE);
        if (status != 0) {
            return status;
        }
        
        if (g_transfer_buffer.status == FILE_STATUS_EOF) {
            // End of file reached
            break;
        } else if (g_transfer_buffer.status != FILE_STATUS_SUCCESS) {
            dbg_printf(DBG_LESS_INFO, "Failed to read from file %s\r\n", g_filename);
            return FILE_STATUS_ERROR;
        }
        
        // Copy data to output buffer
        memcpy(data + offset, g_transfer_buffer.data, g_transfer_buffer.length);
        
        // Update counters
        total_read += g_transfer_buffer.length;
        remaining -= g_transfer_buffer.length;
        offset += g_transfer_buffer.length;
        
        if (g_transfer_buffer.length < chunk_size) {
            // End of file reached
            break;
        }
    }
    
    return total_read;
}

/**
 * @brief Send JPEG and algorithm results via I2C file transfer
 * 
 * @param jpeg_addr Address of JPEG data
 * @param jpeg_size Size of JPEG data
 * @param algo_result Pointer to algorithm results
 * @param algo_size Size of algorithm results
 * @return int32_t 0 if successful, error code otherwise
 */
int32_t i2c_file_transfer_send_frame_data(uint32_t jpeg_addr, uint32_t jpeg_size, void* algo_result, uint32_t algo_size)
{
    int32_t status = 0;
    char filename[32];
    static uint32_t frame_count = 0;
    
    // Create unique filename for this frame
    sprintf(filename, "frame_%06d.jpg", frame_count);
    
    // Open file for writing
    status = i2c_file_transfer_open(filename, 'w');
    if (status != FILE_STATUS_SUCCESS) {
        return status;
    }
    
    // Write JPEG data
    status = i2c_file_transfer_write((uint8_t*)jpeg_addr, jpeg_size);
    if (status != jpeg_size) {
        i2c_file_transfer_close();
        return FILE_STATUS_ERROR;
    }
    
    // Close JPEG file
    status = i2c_file_transfer_close();
    if (status != FILE_STATUS_SUCCESS) {
        return status;
    }
    
    // If there's algorithm data, write it to a separate file
    if (algo_result != NULL && algo_size > 0) {
        // Create unique filename for the metadata
        sprintf(filename, "meta_%06d.bin", frame_count);
        
        // Open file for writing
        status = i2c_file_transfer_open(filename, 'w');
        if (status != FILE_STATUS_SUCCESS) {
            return status;
        }
        
        // Write algorithm data
        status = i2c_file_transfer_write((uint8_t*)algo_result, algo_size);
        if (status != algo_size) {
            i2c_file_transfer_close();
            return FILE_STATUS_ERROR;
        }
        
        // Close metadata file
        status = i2c_file_transfer_close();
        if (status != FILE_STATUS_SUCCESS) {
            return status;
        }
    }
    
    frame_count++;
    return FILE_STATUS_SUCCESS;
}