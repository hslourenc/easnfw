#ifdef PLATFORM_H
#define PLATFORM_H

enum {
    NVS_ENTRY_ID__DUMMY = 0,
};

/**
 * @brief Initialize NVS.
 *
 * @return 0 on success. On error, returns negative value of errno.h defined
 * error codes. 
 */
int app_nvs__init(void);


/**
 * @brief Write to default NVS file system
 *
 * @param id Id of the entry to be written
 * @param data Pointer to the data to be written
 * @param len Number of bytes to be written
 *
 * @return 0 on success. On error, returns negative value of errno.h defined
 * error codes. 
 */
int app_nvs__write(uint16_t const id, void * const data, size_t const len);

/**
 * @brief Read an entry from the default file system.
 *
 * @param id Id of the entry to be read
 * @param data Pointer to data buffer
 * @param len Number of bytes to be read
 *
 * @return 0 on success. On error, returns negative value of errno.h defined
 * error codes.
 */
int app_nvs__read(uint16_t const id, void * const data, size_t const len);



#endif