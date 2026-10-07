/**
 * @file    interfw_link_uart.h
 * @brief   UART protocol for the Inter-FW link.
 *
 * ============================================================================
 *  OVERVIEW
 * ============================================================================
 *  Two firmware components on two different MCUs talk over one full-duplex
 *  UART. The link is symmetric: either side can send commands to the other.
 *  The command sets are different per direction:
 *
 *      CLOUD  --(SENSOR_CMD_xxx)-->  SENSOR    (commands exposed by SENSOR)
 *      SENSOR --(CLOUD_CMD_xxx)-->   CLOUD     (commands exposed by CLOUD)
 *
 *  Every command frame MUST be answered by an ACK frame (reception
 *  acknowledgement + status). Commands may carry no payload, a small payload
 *  (single frame) or a large payload (split in fragments, each fragment
 *  individually acknowledged).
 *
 * ============================================================================
 *  PHYSICAL / FRAMING
 * ============================================================================
 *  UART default: 1,000,000 baud 8N1, with flow control (RTS/CTS). All
 *  multi-byte fields are LITTLE ENDIAN on the wire.
 *
 *   +------+------+--------+-----+-------+------+----------+-----------+------+
 *   | SOF  | TYPE | CMD_ID | SEQ | FLAGS | LEN  | PAYLOAD  |   CRC16   |
 *   | | 1 B  | 1 B  |  1 B   | 1 B |  1 B  | 2 B  | 0..LEN B |    2 B    |
 *   |
 *   +------+------+--------+-----+-------+------+----------+-----------+------+
 *   |<------------- header (7 bytes) ---------->|
 *
 *   - SOF     : fixed 0xA5, used for (re)synchronisation.
 *   - TYPE    : EASN_FRAME_CMD or EASN_FRAME_ACK.
 *   - CMD_ID  : command identifier (for ACK: the command being acknowledged).
 *   - SEQ     : sequence number chosen by the sender of a CMD; the ACK echoes
 *               it. Increments by 1 (mod 256) for every NEW CMD frame;
 *               retransmissions reuse the same SEQ.
 *   - FLAGS   : see EASN_FLAG_xxx.
 *   - LEN     : payload length in bytes (<= INTERFW_LINK_UART_MAX_PAYLOAD).
 *   - CRC16   : CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
 *               xorout 0x0000) computed over TYPE..PAYLOAD (everything except
 *               SOF and the CRC itself).
 *
 *  Receiver behaviour: hunt for SOF, read header, reject if LEN is too large,
 *  read payload + CRC. On CRC failure or inter-byte timeout
 *  (INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS) drop the frame, go back to SOF
 *  hunting. Corrupted CMD frames are NOT acked: the sender's ACK timeout
 *  triggers the retransmission.
 *
 * ============================================================================
 *  ACK / RETRANSMISSION (stop-and-wait)
 * ============================================================================
 *  - Only ONE CMD frame per direction may be outstanding (not yet acked).
 *  - If no ACK within INTERFW_LINK_UART_ACK_TIMEOUT_MS, the sender retransmits
 *    the frame (same SEQ) up to INTERFW_LINK_UART_MAX_RETRIES times, then
 *    reports EASN_ERR_TIMEOUT to the application.
 *  - The ACK payload carries a status code (easn_status_t). The status says
 *    whether the command was accepted/executed or rejected (unknown command,
 *    busy, bad parameters, ...).
 *  - Duplicate detection: a receiver that gets a CMD with the same SEQ as the
 *    last one it accepted must re-send the ACK but NOT deliver the command to
 *    the application again (the original ACK was probably lost).
 *  - The ACK only means "received (and accepted/rejected)". Any command result
 *    that takes time is returned by the peer with its own command in the
 *    opposite direction (e.g. CLOUD_CMD_SAMPLE_REPORT).
 *
 * ============================================================================
 *  LARGE DATA (fragmentation)
 * ============================================================================
 *  Commands flagged as "data commands" can carry a payload bigger than one
 *  frame. The sender splits it in chunks of at most
 *  INTERFW_LINK_UART_MAX_FRAG_DATA bytes. Every chunk is a normal CMD frame
 *  with:
 *
 *      FLAGS.FRAGMENT = 1
 *      FLAGS.LAST     = 1 on the final chunk only
 *      PAYLOAD        = easn_frag_hdr_t + chunk bytes
 *
 *  Every chunk is acked individually (stop-and-wait). The receiver validates
 *  that offset == bytes received so far; otherwise it NACKs with
 *  EASN_ST_BAD_OFFSET and reports the offset it expects in the ACK so the
 *  sender can resume. A transfer is aborted with FLAGS.ABORT (sent in a
 *  fragment frame with zero chunk bytes) or when the receiver replies with any
 *  non-OK status.
 *
 *  Small data (<= INTERFW_LINK_UART_MAX_PAYLOAD) is simply sent as an ordinary
 *  CMD frame with no FRAGMENT flag.
 */

#ifndef EASNFW_INTERFW_LINK_UART_H
#define EASNFW_INTERFW_LINK_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========================================================================== */
/*  Configuration (override from the build system if needed)                  */
/* ========================================================================== */

#ifndef INTERFW_LINK_UART_BAUDRATE
#define INTERFW_LINK_UART_BAUDRATE              1000000u
#endif

#ifndef INTERFW_LINK_UART_FLOW_CTRL
#define INTERFW_LINK_UART_FLOW_CTRL             1u
#endif

/* Max payload bytes inside one frame (command args or fragment hdr + data). */
#ifndef INTERFW_LINK_UART_MAX_PAYLOAD
#define INTERFW_LINK_UART_MAX_PAYLOAD           2048u
#endif

#ifndef INTERFW_LINK_UART_ACK_TIMEOUT_MS
#define INTERFW_LINK_UART_ACK_TIMEOUT_MS        200u
#endif

#ifndef INTERFW_LINK_UART_MAX_RETRIES
#define INTERFW_LINK_UART_MAX_RETRIES           3u
#endif

#ifndef INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS
#define INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS 50u
#endif

/* ========================================================================== */
/*  Wire format constants                                                     */
/* ========================================================================== */

#define INTERFW_LINK_UART_SOF                   0xA5u
#define INTERFW_LINK_UART_HDR_SIZE              7u      /* SOF..LEN                   */
#define INTERFW_LINK_UART_CRC_SIZE              2u
#define INTERFW_LINK_UART_MAX_FRAME_SIZE \
    (INTERFW_LINK_UART_HDR_SIZE + INTERFW_LINK_UART_MAX_PAYLOAD + INTERFW_LINK_UART_CRC_SIZE)

#define INTERFW_LINK_UART_CRC_INIT              0xFFFFu

/** Frame types. */
typedef enum {
    EASN_FRAME_CMD = 0x01,  /**< Command (request) frame                     */
    EASN_FRAME_ACK = 0x02   /**< Acknowledgement of a CMD frame              */
} easn_frame_type_t;

/** FLAGS bit field. */
#define EASN_FLAG_FRAGMENT      (1u << 0) /**< Payload starts with easn_frag_hdr_t */
#define EASN_FLAG_LAST          (1u << 1) /**< Final fragment of a transfer        */
#define EASN_FLAG_ABORT         (1u << 2) /**< Abort the ongoing transfer          */
#define EASN_FLAG_RETRANSMIT    (1u << 3) /**< Informative: frame is a retry       */
/* bits 4..7 reserved, must be 0 on TX and ignored on RX */

/* ========================================================================== */
/*  Roles                                                                     */
/* ========================================================================== */

typedef enum {
    EASN_ROLE_SENSOR = 0,   /**< This MCU runs EASNFW-SENSOR */
    EASN_ROLE_CLOUD  = 1    /**< This MCU runs EASNFW-CLOUD  */
} easn_role_t;

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

/** Commands implemented by both firmware components. */
typedef enum {
    EASN_CMD_PING           = 0x01, /**< No payload. Link liveness check.            */
    EASN_CMD_GET_INFO       = 0x02, /**< No payload. Peer answers with EASN_CMD_INFO. */
    EASN_CMD_INFO           = 0x03, /**< easn_info_t. Reply to GET_INFO.             */
    EASN_CMD_RESET_LINK     = 0x04  /**< No payload. Reset seq numbers/transfers.    */
} easn_common_cmd_t;

/** Commands EXPOSED BY EASNFW-SENSOR (CLOUD -> SENSOR). */
typedef enum {
    SENSOR_CMD_START_SAMPLING   = 0x10, /**< easn_start_sampling_t                    */
    SENSOR_CMD_STOP_SAMPLING    = 0x11, /**< No payload                               */
    SENSOR_CMD_SET_CONFIG       = 0x12, /**< Config blob (may be large, fragmented)   */
    SENSOR_CMD_GET_CONFIG       = 0x13, /**< No payload. Reply: CLOUD_CMD_CONFIG_REPORT */
    SENSOR_CMD_SET_TIME         = 0x14, /**< easn_time_t                              */
    SENSOR_CMD_REBOOT           = 0x15, /**< No payload                               */
    SENSOR_CMD_FW_UPDATE_DATA   = 0x16  /**< Firmware image (LARGE, fragmented)       */
} sensor_cmd_t;

/** Commands EXPOSED BY EASNFW-CLOUD (SENSOR -> CLOUD). */
typedef enum {
    CLOUD_CMD_SAMPLE_REPORT     = 0x80, /**< Batch of samples (LARGE, fragmented)     */
    CLOUD_CMD_EVENT_NOTIFY      = 0x81, /**< easn_event_t                             */
    CLOUD_CMD_STATUS_REPORT     = 0x82, /**< easn_sensor_status_t                     */
    CLOUD_CMD_CONFIG_REPORT     = 0x83, /**< Config blob (may be large, fragmented)   */
    CLOUD_CMD_LOG_UPLOAD        = 0x84, /**< Text/binary log (LARGE, fragmented)      */
    CLOUD_CMD_TIME_REQUEST      = 0x85  /**< No payload. Reply: SENSOR_CMD_SET_TIME   */
} cloud_cmd_t;

/* ========================================================================== */
/*  ACK status codes                                                          */
/* ========================================================================== */

typedef enum {
    EASN_ST_OK              = 0x00, /**< Received and accepted                    */
    EASN_ST_UNKNOWN_CMD     = 0x01, /**< Command ID not exposed by this firmware  */
    EASN_ST_BAD_LEN         = 0x02, /**< Payload length invalid for this command  */
    EASN_ST_BAD_PARAM       = 0x03, /**< Payload content invalid                  */
    EASN_ST_BUSY            = 0x04, /**< Cannot handle now, sender may retry later */
    EASN_ST_NOT_SUPPORTED   = 0x05, /**< Known command but not available right now */
    EASN_ST_BAD_OFFSET      = 0x06, /**< Fragment offset != expected (see next_offset) */
    EASN_ST_NO_MEMORY       = 0x07, /**< Cannot store the incoming data           */
    EASN_ST_TRANSFER_ERR    = 0x08, /**< Transfer-level failure (bad total size, id mismatch, CRC of whole image...) */
    EASN_ST_INTERNAL_ERR    = 0xFF  /**< Unspecified error in the receiver        */
} easn_status_t;

/* ========================================================================== */
/*  Wire structures                                                           */
/* ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
#  define EASN_PACKED __attribute__((packed))
#else
#  define EASN_PACKED
#  pragma pack(push, 1)
#endif

/** Frame header as it appears on the wire. */
typedef struct EASN_PACKED {
    uint8_t  sof;        /**< INTERFW_LINK_UART_SOF                       */
    uint8_t  type;       /**< easn_frame_type_t                   */
    uint8_t  cmd_id;     /**< Command (echoed in the ACK)         */
    uint8_t  seq;        /**< Sequence number (echoed in the ACK) */
    uint8_t  flags;      /**< EASN_FLAG_xxx                       */
    uint16_t len;        /**< Payload length                      */
} easn_frame_hdr_t;

/** Payload of an ACK frame. */
typedef struct EASN_PACKED {
    uint8_t  status;       /**< easn_status_t                                    */
    uint8_t  reserved;     /**< 0                                                */
    uint32_t next_offset;  /**< Fragment ACKs: next byte offset the receiver
                                expects (== offset+chunk_len on success, or the
                                resume point on EASN_ST_BAD_OFFSET). 0 otherwise. */
} easn_ack_payload_t;

/** Prefix of the payload in frames with EASN_FLAG_FRAGMENT. */
typedef struct EASN_PACKED {
    uint8_t  transfer_id;  /**< Identifies the transfer; constant for all of its fragments */
    uint8_t  reserved;     /**< 0 */
    uint32_t total_len;    /**< Total size in bytes of the whole data blob */
    uint32_t offset;       /**< Offset of this chunk inside the blob       */
    /* followed by chunk bytes: len - sizeof(easn_frag_hdr_t) */
} easn_frag_hdr_t;

#define INTERFW_LINK_UART_MAX_FRAG_DATA \
    (INTERFW_LINK_UART_MAX_PAYLOAD - sizeof(easn_frag_hdr_t))

/* ---- Example command payloads ------------------------------------------- */

typedef struct EASN_PACKED {          /* EASN_CMD_INFO */
    uint8_t  role;                    /**< easn_role_t                    */
    uint8_t  proto_version;           /**< INTERFW_LINK_UART_PROTO_VERSION        */
    uint8_t  fw_major, fw_minor, fw_patch;
    uint8_t  reserved;
    uint16_t max_payload;             /**< Largest payload this side accepts */
    uint32_t build_id;
} easn_info_t;

typedef struct EASN_PACKED {          /* SENSOR_CMD_START_SAMPLING */
    uint32_t period_ms;               /**< Sampling period            */
    uint32_t duration_ms;             /**< 0 = until STOP_SAMPLING    */
    uint16_t channel_mask;            /**< Bit n = channel n enabled  */
} easn_start_sampling_t;

typedef struct EASN_PACKED {          /* SENSOR_CMD_SET_TIME */
    uint64_t unix_time_ms;
} easn_time_t;

typedef struct EASN_PACKED {          /* CLOUD_CMD_EVENT_NOTIFY */
    uint16_t event_code;
    uint8_t  severity;
    uint8_t  reserved;
    uint32_t timestamp_s;
    uint32_t arg;
} easn_event_t;

typedef struct EASN_PACKED {          /* CLOUD_CMD_STATUS_REPORT */
    uint8_t  state;                   /**< Sensor FSM state          */
    uint8_t  error_flags;
    uint16_t battery_mv;
    uint32_t uptime_s;
    uint32_t samples_pending;
} easn_sensor_status_t;

#if !(defined(__GNUC__) || defined(__clang__))
#  pragma pack(pop)
#endif

#define INTERFW_LINK_UART_PROTO_VERSION         1u

/* Compile-time sanity checks (C11). */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(easn_frame_hdr_t)   == INTERFW_LINK_UART_HDR_SIZE, "hdr size");
_Static_assert(sizeof(easn_ack_payload_t) == 6u,                 "ack size");
_Static_assert(sizeof(easn_frag_hdr_t)    == 10u,                "frag hdr size");
_Static_assert(INTERFW_LINK_UART_MAX_PAYLOAD > sizeof(easn_frag_hdr_t),  "payload too small");
#endif

/* ========================================================================== */
/*  Command table helpers                                                     */
/* ========================================================================== */

/** Command properties, to be used by the receiver to validate frames. */
#define EASN_CMDF_DATA          (1u << 0) /**< May use fragmentation (large payload)   */
#define EASN_CMDF_NO_PAYLOAD    (1u << 1) /**< Payload must be empty                   */

/**
 * @brief  Direction check: can @p cmd_id be received by firmware @p receiver?
 * @return true if the command is either COMMON, or belongs to the receiver's
 *         own exposed range (SENSOR range for SENSOR, CLOUD range for CLOUD).
 */
static inline bool easn_cmd_valid_for(easn_role_t receiver, uint8_t cmd_id)
{
    if (cmd_id >= 0x01u && cmd_id <= 0x0Fu) { return true; }
    if (receiver == EASN_ROLE_SENSOR) { return (cmd_id >= 0x10u && cmd_id <= 0x7Fu); }
    return (cmd_id >= 0x80u && cmd_id <= 0xEFu);
}

/* ========================================================================== */
/*  Port layer (HAL) provided by the integrator                               */
/* ========================================================================== */

typedef struct {
    /** Write @p len bytes to the UART TX (blocking or queued to DMA/ISR ring).
     *  @return number of bytes accepted, <0 on error. Must accept all bytes
     *          or none (the link does not do partial-write bookkeeping). */
    int      (*uart_write)(void *ctx, const uint8_t *data, size_t len);

    /** Monotonic millisecond tick. */
    uint32_t (*now_ms)(void *ctx);

    /** Optional critical-section hooks (may be NULL when single threaded). */
    void     (*lock)(void *ctx);
    void     (*unlock)(void *ctx);

    void      *ctx;     /**< Passed back to every function above */
} interfw_link_uart_port_t;

/* ========================================================================== */
/*  Application callbacks                                                     */
/* ========================================================================== */

/** Result of a command sent by this side. */
typedef enum {
    EASN_TX_ACKED_OK = 0,   /**< Peer ACKed with EASN_ST_OK                         */
    EASN_TX_ACKED_ERR,      /**< Peer ACKed with a non-OK status (see @c status)    */
    EASN_TX_TIMEOUT         /**< No ACK after INTERFW_LINK_UART_MAX_RETRIES retransmissions */
} easn_tx_result_t;

typedef struct {
    /**
     * A complete, single-frame command (no fragmentation) was received.
     * The returned status is sent back in the ACK. Called from
     * interfw_link_uart_rx_feed()/interfw_link_uart_poll() context: keep it short, defer slow
     * work (reply with your own command later).
     */
    easn_status_t (*on_command)(void *user, uint8_t cmd_id,
                                const uint8_t *payload, uint16_t len);

    /**
     * One chunk of a fragmented (large) command was received. Called in order,
     * once per fragment. Return EASN_ST_OK to accept the chunk, anything else
     * aborts the transfer (the status is sent in the ACK).
     * @p is_last is true on the final chunk: the blob is complete after it.
     */
    easn_status_t (*on_data_chunk)(void *user, uint8_t cmd_id, uint8_t transfer_id,
                                   uint32_t offset, uint32_t total_len,
                                   const uint8_t *chunk, uint16_t chunk_len,
                                   bool is_last);

    /** A transfer was aborted by the peer or timed out locally. */
    void (*on_transfer_aborted)(void *user, uint8_t cmd_id, uint8_t transfer_id);

    /** The command (or whole large transfer) sent by this side has finished. */
    void (*on_tx_done)(void *user, uint8_t cmd_id, easn_tx_result_t result,
                       easn_status_t peer_status);

    void *user;
} interfw_link_uart_callbacks_t;

/* ========================================================================== */
/*  Link object                                                               */
/* ========================================================================== */

/** Library error codes (return values of the API, NOT sent on the wire). */
typedef enum {
    EASN_OK            =  0,
    EASN_ERR_PARAM     = -1,  /**< NULL pointer / invalid argument               */
    EASN_ERR_BUSY      = -2,  /**< A command is already waiting for its ACK      */
    EASN_ERR_TOO_BIG   = -3,  /**< Payload exceeds INTERFW_LINK_UART_MAX_PAYLOAD (use interfw_link_uart_send_data) */
    EASN_ERR_CMD       = -4,  /**< Command not valid towards the peer's role     */
    EASN_ERR_IO        = -5,  /**< port->uart_write failed                       */
    EASN_ERR_TIMEOUT   = -6
} easn_err_t;

/** Receive parser states. */
typedef enum {
    EASN_RX_WAIT_SOF = 0,
    EASN_RX_HEADER,
    EASN_RX_PAYLOAD,
    EASN_RX_CRC
} easn_rx_state_t;

/** Source of the bytes of a large transfer being transmitted. */
typedef struct {
    const uint8_t *data;       /**< Must stay valid until on_tx_done()    */
    uint32_t       total_len;
    uint32_t       offset;     /**< Next byte to send                     */
    uint8_t        transfer_id;
    uint8_t        cmd_id;
    bool           active;
} easn_tx_transfer_t;

/** State of a transfer being received. */
typedef struct {
    uint32_t       total_len;
    uint32_t       next_offset;
    uint8_t        transfer_id;
    uint8_t        cmd_id;
    bool           active;
} easn_rx_transfer_t;

/**
 * Link instance. One per UART. Fields are private: treat as opaque and use
 * the API below. It is exposed here only so it can be statically allocated.
 */
typedef struct interfw_link_uart_link {
    easn_role_t            role;          /**< Role of THIS firmware       */
    interfw_link_uart_port_t       port;
    interfw_link_uart_callbacks_t  cb;

    /* RX parser */
    easn_rx_state_t        rx_state;
    uint16_t               rx_idx;
    uint16_t               rx_expected;
    uint32_t               rx_last_byte_ms;
    uint8_t                rx_buf[INTERFW_LINK_UART_MAX_FRAME_SIZE];

    /* Duplicate suppression for received commands */
    bool                   rx_have_last_seq;
    uint8_t                rx_last_seq;
    easn_ack_payload_t     rx_last_ack;   /**< Re-sent verbatim on duplicates */
    easn_rx_transfer_t     rx_xfer;

    /* TX: single outstanding command, stop-and-wait */
    uint8_t                tx_seq;        /**< SEQ of the next NEW command */
    bool                   tx_waiting_ack;
    uint8_t                tx_cur_seq;
    uint8_t                tx_cur_cmd;
    uint8_t                tx_cur_flags;
    uint8_t                tx_retries;
    uint32_t               tx_sent_ms;
    uint16_t               tx_frame_len;
    uint8_t                tx_frame[INTERFW_LINK_UART_MAX_FRAME_SIZE]; /**< Kept for retransmission */
    easn_tx_transfer_t     tx_xfer;
    uint8_t                next_transfer_id;

    /* Statistics */
    struct {
        uint32_t frames_rx, frames_tx;
        uint32_t crc_errors, timeouts, retransmissions, duplicates;
    } stats;
} interfw_link_uart_link_t;

/* ========================================================================== */
/*  API                                                                       */
/* ========================================================================== */

/** CRC-16/CCITT-FALSE helper (also usable to checksum whole transferred images). */
uint16_t easn_crc16(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief  Initialise a link.
 * @param  role  Role of the firmware calling this function.
 */
easn_err_t interfw_link_uart_init(interfw_link_uart_link_t *link, easn_role_t role,
                          const interfw_link_uart_port_t *port,
                          const interfw_link_uart_callbacks_t *callbacks);

/**
 * @brief  Feed bytes received from the UART (call from the main loop, or
 *         from a task draining the RX ring buffer / DMA buffer).
 *         May invoke on_command/on_data_chunk and transmits ACKs.
 *         Also processes ACK frames for the command in flight.
 */
void interfw_link_uart_rx_feed(interfw_link_uart_link_t *link, const uint8_t *data, size_t len);

/**
 * @brief  Periodic service (call at least every ~10 ms): ACK timeouts,
 *         retransmissions, RX inter-byte timeout, and sending the next
 *         fragment of a large transfer after the previous one was acked.
 */
void interfw_link_uart_poll(interfw_link_uart_link_t *link);

/**
 * @brief  Send a command with no payload or a small payload (single frame).
 * @param  cmd_id   A COMMON command or a command in the PEER's range
 *                  (SENSOR sends CLOUD_CMD_xxx, CLOUD sends SENSOR_CMD_xxx).
 * @param  payload  May be NULL when @p len == 0.
 * @param  len      <= INTERFW_LINK_UART_MAX_PAYLOAD.
 * @retval EASN_OK        Frame sent; on_tx_done() will report the outcome.
 * @retval EASN_ERR_BUSY  Previous command still waiting for its ACK.
 */
easn_err_t interfw_link_uart_send_command(interfw_link_uart_link_t *link, uint8_t cmd_id,
                                  const void *payload, uint16_t len);

/**
 * @brief  Send a command with a large data blob, fragmented automatically.
 *         Fragments are sent one by one from interfw_link_uart_poll()/rx_feed() as
 *         the ACKs arrive. @p data must remain valid and unmodified until
 *         on_tx_done() is called (once for the whole transfer).
 * @retval EASN_OK        Transfer started.
 * @retval EASN_ERR_BUSY  Another command/transfer is in progress.
 */
easn_err_t interfw_link_uart_send_data(interfw_link_uart_link_t *link, uint8_t cmd_id,
                               const uint8_t *data, uint32_t total_len);

/** Abort the large transfer being transmitted (sends a FLAGS.ABORT frame). */
easn_err_t interfw_link_uart_abort_transfer(interfw_link_uart_link_t *link);

/** True when a new command can be submitted. */
bool interfw_link_uart_tx_ready(const interfw_link_uart_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* EASNFW_INTERFW_LINK_UART_H */