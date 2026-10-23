/**
 * @file    interfw_link_protocol.h
 * @brief   Wire protocol and public API for the Inter-FW Link.
 *
 * EASNFW-SENSOR and EASNFW-CLOUD use this protocol over a full-duplex UART.
 * Common commands may be received by either component. SENSOR-range commands
 * are sent by CLOUD to SENSOR, and CLOUD-range commands are sent by SENSOR to
 * CLOUD. The command identifiers, payload structures, frame layout, status
 * values, and shared helper functions below are the protocol source of truth.
 *
 * Each command frame is acknowledged. The acknowledgement reports frame
 * acceptance or rejection; it does not by itself confirm durable cloud
 * delivery. Long payloads use acknowledged fragments and stop-and-wait retry.
 *
 * Multi-byte fields are little-endian. The physical UART defaults are
 * 1,000,000 baud, 8-N-1, with RTS/CTS flow control. CRC-16/CCITT-FALSE covers
 * TYPE through the payload, excluding SOF and the CRC bytes.
 */

#ifndef EASNFW_INTERFW_LINK_UART_H
#define EASNFW_INTERFW_LINK_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*  Configuration (override from the build system if needed)                  */
/* ========================================================================== */

#ifndef INTERFW_LINK_UART_BAUDRATE
/** @brief Default UART baud rate: 1000000 bits per second. */
#define INTERFW_LINK_UART_BAUDRATE              1000000u
#endif

#ifndef INTERFW_LINK_UART_FLOW_CTRL
/** @brief Enables RTS/CTS hardware flow control; 1 means enabled. */
#define INTERFW_LINK_UART_FLOW_CTRL             1u
#endif

/* Max payload bytes inside one frame (command args or fragment hdr + data). */
#ifndef INTERFW_LINK_UART_MAX_PAYLOAD
/** @brief Maximum payload size: 2048 bytes per frame. */
#define INTERFW_LINK_UART_MAX_PAYLOAD           2048u
#endif

#ifndef INTERFW_LINK_UART_ACK_TIMEOUT_MS
/** @brief Default wait for a matching ACK: 200 milliseconds. */
#define INTERFW_LINK_UART_ACK_TIMEOUT_MS        200u
#endif

#ifndef INTERFW_LINK_UART_MAX_RETRIES
/** @brief Default retries after the first transmission: 3. */
#define INTERFW_LINK_UART_MAX_RETRIES           3u
#endif

#ifndef INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS
/** @brief Inter-byte receive timeout: 50 milliseconds. */
#define INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS 50u
#endif

/* ========================================================================== */
/*  Wire format constants                                                     */
/* ========================================================================== */

/** @brief Start-of-frame marker: 0xA5. */
#define INTERFW_LINK_UART_SOF                   0xA5u
/** @brief Header size through LEN: 7 bytes. */
#define INTERFW_LINK_UART_HDR_SIZE              7u
/** @brief CRC trailer size: 2 bytes. */
#define INTERFW_LINK_UART_CRC_SIZE              2u
#define INTERFW_LINK_UART_MAX_FRAME_SIZE \
    (INTERFW_LINK_UART_HDR_SIZE + INTERFW_LINK_UART_MAX_PAYLOAD + INTERFW_LINK_UART_CRC_SIZE)

/** @brief CRC-16/CCITT-FALSE covers TYPE through PAYLOAD (excluding SOF and CRC): init 0xFFFF, polynomial 0x1021, no reflection, xorout 0. */
#define INTERFW_LINK_UART_CRC_INIT              0xFFFFu

/** Distinguishes a command request from its acknowledgement frame. */
typedef enum {
    EASN_FRAME_CMD = 0x01,  /**< Command request (0x01). */
    EASN_FRAME_ACK = 0x02   /**< Acknowledgement of a command request (0x02). */
} easn_frame_type_t;

/** @name IFL command-frame flags
 *  Bits 4 through 7 are reserved; transmitters set them to zero.
 *  @{
 */
#define EASN_FLAG_FRAGMENT      (1u << 0) /**< Payload starts with easn_frag_hdr_t (0x01). */
#define EASN_FLAG_LAST          (1u << 1) /**< This is the final fragment (0x02). */
#define EASN_FLAG_ABORT         (1u << 2) /**< Abort the active fragmented transfer (0x04). */
#define EASN_FLAG_RETRANSMIT    (1u << 3) /**< Marks a retransmitted command frame (0x08). */
/** @} */

/* ========================================================================== */
/*  Roles                                                                     */
/* ========================================================================== */

typedef enum {
    EASN_ROLE_SENSOR = 0,   /**< This endpoint runs EASNFW-SENSOR (0). */
    EASN_ROLE_CLOUD  = 1    /**< This endpoint runs EASNFW-CLOUD (1). */
} easn_role_t;

/**
 * Runtime state maintained by an IFL endpoint.
 *
 * The state is intentionally transport-independent: UART setup and frame
 * buffering are handled by the platform layer, while these fields track the
 * protocol identity and sequence state needed by both endpoints.
 */
struct ifl_runtime_state {
    /** Role of this endpoint; controls which command range it may receive. */
    easn_role_t role;
    /** Sequence number assigned to the next newly transmitted command. */
    uint8_t next_tx_seq;
    /** Sequence number of the most recently accepted received command. */
    uint8_t last_rx_seq;
    /**
     * Whether last_rx_seq contains a valid previously accepted sequence.
     *
     * This is separate from last_rx_seq because sequence zero is valid. It
     * distinguishes "no command received yet" from "the last command had
     * sequence zero" during duplicate-command detection.
     */
    bool last_rx_seq_valid;
};

/* ========================================================================== */
/*  Command identifiers                                                       */
/* ========================================================================== */
/*
 * One 8-bit command space, split in ranges so a command ID is unambiguous
 * when looking at a logic-analyzer capture:
 *
 *   0x01..0x0F  COMMON  - implemented by BOTH sides
 *   0x10..0x7F  SENSOR  - exposed by EASNFW-SENSOR (sent by EASNFW-CLOUD)
 *   0x80..0xEF  CLOUD   - exposed by EASNFW-CLOUD  (sent by EASNFW-SENSOR)
 *   0xF0..0xFF  reserved
 */


enum ifl_commands {
	/** Commands that may be received by either firmware component. */
	IFL_CMD_COMMON__RANGE_START        = 0x01, /**< Not a command, just helper to check commands ranges */
	IFL_CMD_COMMON__PING               = 0x01, /**< Empty link-liveness check (0x01). */
	IFL_CMD_COMMON__RANGE_END          = 0x0F, /**< Not a command, just helper to check commands ranges */

	/** Commands received by SENSOR and sent by CLOUD. */
	IFL_CMD_SENSOR__RANGE_START  = 0x10, /**<  Not a command, just helper to check commands ranges */
	IFL_CMD_SENSOR__CLOUD_READY  = 0x10, /**< CLOUD startup announcement (0x10); payload is ifl_cmd_payload_cloud_ready_t. */
	IFL_CMD_SENSOR__UTC_TIME     = 0x11, /**< UTC and reset-cause payload (0x11) is ifl_cmd_payload_utc_time_t. */
	IFL_CMD_SENSOR__RANGE_END    = 0x7F, /**<  Not a command, just helper to check commands ranges. */

	/** Commands received by CLOUD and sent by SENSOR. */
	IFL_CMD_CLOUD__RANGE_START   = 0x80, /**<  Not a command, just helper to check commands ranges */
	IFL_CMD_CLOUD__RESET         = 0x80, /**< Empty reset request (0x80); CLOUD resets after acknowledging. */
	IFL_CMD_CLOUD__RECORD_DATA   = 0x81, /**< Transfer record bytes (0x81); large payloads use easn_frag_hdr_t. */
	IFL_CMD_CLOUD__TIME_SYNC_REQUEST = 0x82, /**< Empty asynchronous cellular UTC request (0x82). */
	IFL_CMD_CLOUD__RANGE_END     = 0xEF, /**<  Not a command, just helper to check commands ranges */
};

/* ========================================================================== */
/*  ACK status codes                                                          */
/* ========================================================================== */

/** Status returned by the receiver in an acknowledgement frame. */
typedef enum {
    IFL_STAT__OK              = 0x00, /**< Received and accepted (0x00). */
    IFL_STAT__UNKNOWN_CMD     = 0x01, /**< Command not exposed by this firmware (0x01). */
    IFL_STAT__BAD_LEN         = 0x02, /**< Invalid command payload length (0x02). */
    IFL_STAT__BAD_PARAM       = 0x03, /**< Invalid command payload content (0x03). */
    IFL_STAT__BUSY            = 0x04, /**< Temporarily unavailable; sender may retry (0x04). */
    IFL_STAT__NOT_SUPPORTED   = 0x05, /**< Known command not available now (0x05). */
    IFL_STAT__BAD_OFFSET      = 0x06, /**< Fragment offset differs from expected offset (0x06). */
    IFL_STAT__NO_MEMORY       = 0x07, /**< Receiver cannot store incoming data (0x07). */
    IFL_STAT__TRANSFER_ERR    = 0x08, /**< Transfer validation or assembly failed (0x08). */
    IFL_STAT__INTERNAL_ERR    = 0xFF  /**< Unspecified receiver error (0xFF). */
} ifl_status_t;

/* ========================================================================== */
/*  Wire structures                                                           */
/* ========================================================================== */

/**
 * Ensure protocol structures have no padding so their memory layout matches
 * the byte-level UART wire format. Zephyr applications commonly use GCC or
 * Clang, while the fallback supports other compilers.
 */
#if defined(__GNUC__) || defined(__clang__)
#  define IFL_PACKED __attribute__((packed))
#else
#  define IFL_PACKED
#  pragma pack(push, 1)
#endif

/**
 * @brief Fixed frame header serialized before the payload.
 *
 * Multi-byte members are little-endian on the wire. The trailing CRC is not
 * part of this structure. Fields appear in wire order: one-byte SOF marker,
 * one-byte frame type, one-byte command identifier, one-byte sequence number,
 * one-byte flags, and a two-byte payload length (seven bytes total).
 */
typedef struct IFL_PACKED {
    uint8_t  sof;        /**< INTERFW_LINK_UART_SOF               */
    uint8_t  type;       /**< easn_frame_type_t                   */
    uint8_t  cmd_id;     /**< Command (echoed in the ACK)         */
    uint8_t  seq;        /**< Sequence number (echoed in the ACK) */
    uint8_t  flags;      /**< EASN_FLAG_xxx                       */
    uint16_t len;        /**< Payload length                      */
} easn_frame_hdr_t;

/**
 * @brief Fixed payload returned in an ACK frame.
 *
 * @p next_offset is used by fragmented transfers to report the next expected
 * byte offset; it is zero for non-fragmented commands. The payload contains
 * one status byte, one reserved byte set to zero, and a four-byte offset, for
 * six bytes total.
 */
typedef struct IFL_PACKED {
    uint8_t  status;       /**< ifl_status_t                                    */
    uint8_t  reserved;     /**< 0                                                */
    uint32_t next_offset;  /**< Fragment ACKs: next byte offset the receiver
                                expects (== offset+chunk_len on success, or the
                                resume point on EASN_ST_BAD_OFFSET). 0 otherwise. */
} easn_ack_payload_t;

/**
 * @brief Prefix of a fragmented command payload.
 *
 * The remaining payload bytes are this fragment's data. The sender keeps
 * @p transfer_id and @p total_len constant across the transfer and sets
 * @p offset to the byte position of each fragment. The transfer ID is one
 * byte and the total length and offset are 32-bit little-endian values, for a
 * nine-byte prefix.
 */
typedef struct IFL_PACKED {
    uint8_t  transfer_id;  /**< Identifies the transfer; constant for all of its fragments */
    uint32_t total_len;    /**< Total size in bytes of the whole data blob */
    uint32_t offset;       /**< Offset of this chunk inside the blob       */
    /* followed by chunk bytes (the actual data of the current transfer),
       number of chunk bytes in a given transfer is frame header's len - sizeof(easn_frag_hdr_t) */
} easn_frag_hdr_t;

#define INTERFW_LINK_UART_MAX_FRAG_DATA \
    (INTERFW_LINK_UART_MAX_PAYLOAD - sizeof(easn_frag_hdr_t))

/**
 * @brief Three-byte semantic version sent in ``CLOUD_READY``.
 *
 * Members are serialized as major, minor, and patch version bytes.
 */
typedef struct IFL_PACKED {
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
} ifl_cmd_payload_cloud_ready_t;

/**
 * @brief UTC Unix epoch milliseconds and CLOUD reset-cause bits.
 *
 * The payload is 12 bytes: 64-bit Unix epoch milliseconds followed by
 * 32-bit reset-cause bits, both little-endian.
 */
typedef struct IFL_PACKED {
    uint64_t unix_time_ms;
    uint32_t cloud_reset_cause;
} ifl_cmd_payload_utc_time_t;

#if !(defined(__GNUC__) || defined(__clang__))
#  pragma pack(pop)
#endif

#define INTERFW_LINK_UART_PROTO_VERSION         1u

/* Compile-time sanity checks (C11). */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(easn_frame_hdr_t) == INTERFW_LINK_UART_HDR_SIZE,
               "easn_frame_hdr_t size does not match INTERFW_LINK_UART_HDR_SIZE");

_Static_assert(sizeof(easn_ack_payload_t) == 6u,
               "easn_ack_payload_t size must be 6 bytes");

_Static_assert(sizeof(easn_frag_hdr_t) == 9u,
               "easn_frag_hdr_t size must be 9 bytes");

_Static_assert(sizeof(ifl_cmd_payload_utc_time_t) == 12u,
               "UTC-time payload must be 12 bytes");

_Static_assert(INTERFW_LINK_UART_MAX_PAYLOAD > sizeof(easn_frag_hdr_t),
               "INTERFW_LINK_UART_MAX_PAYLOAD must be larger than easn_frag_hdr_t size");
#endif

/* ========================================================================== */
/*  Command table helpers                                                     */
/* ========================================================================== */

/**
 * Each firmware component should have its own command table to store the
 * commands it can receive alongside other relevant information.
 */

/** Command properties, to be used by the receiver to validate frames. */
#define EASN_CMDF_DATA          (1u << 0) /**< May use fragmentation (large payload)   */
#define EASN_CMDF_NO_PAYLOAD    (1u << 1) /**< Payload must be empty                   */

/**
 * @brief Description of a command accepted by an IFL endpoint.
 *
 * @p cmd_id identifies the command, @p flags contains EASN_CMDF_* validation
 * properties, and @p payload_len gives the required payload length.
 */
typedef struct {
    /** Command identifier. */
    uint8_t cmd_id;
    /** EASN_CMDF_* properties used to validate the command. */
    uint8_t flags;
    /** Required payload length, or zero for commands without a payload. */
    uint16_t payload_len;
} easn_ifl_command_entry_t;

/**
 * @brief  Direction check: can @p cmd_id be received by firmware @p receiver?
 * @return true if the command is either COMMON, or belongs to the receiver's
 *         own exposed range (SENSOR range for SENSOR, CLOUD range for CLOUD).
 */
static inline bool ifl__is_cmd_valid(easn_role_t receiver, uint8_t cmd_id)
{
    if (cmd_id >= IFL_CMD_COMMON__RANGE_START && cmd_id <= IFL_CMD_COMMON__RANGE_END) { return true; }
    if (receiver == EASN_ROLE_SENSOR) { return (cmd_id >= IFL_CMD_SENSOR__RANGE_START && cmd_id <= IFL_CMD_SENSOR__RANGE_END); }
    return (cmd_id >= IFL_CMD_CLOUD__RANGE_START && cmd_id <= IFL_CMD_CLOUD__RANGE_END);
}

/* ========================================================================== */
/*  API                                                                       */
/* ========================================================================== */

/**
 * @brief Get Inter-FW Link initialization status.
 *
 * @return true if Inter-FW Link is initialized, false otherwise.
 */
bool ifl__is_init(void);

/**
 * @brief Initialize the protocol state for an Inter-FW Link endpoint.
 *
 * The platform UART must be initialized separately before sending or
 * receiving frames.
 *
 * @return 0 on success, or a negative EASNFW status code on failure.
 */
int ifl__init(void);

/**
 * @brief Send a command through Inter-FW Link.
 *
 * @param cmd command to be sent, see enum ifl_commands.
 * @param payload command payload, or NULL when payload_len is zero.
 * @param payload_len command payload length in bytes.
 *
 * @return 0 on success, or a negative EASNFW status code on failure.
 */
int ifl__cmd_tx(uint8_t const cmd, uint8_t const *payload,
                uint16_t const payload_len);


#ifdef __cplusplus
}
#endif

#endif /* EASNFW_INTERFW_LINK_UART_H */