/**
 * @file    interfw_link_protocol.h
 * @brief   UART protocol for the Inter-FW link (IFL).
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
 *   | 1 B  | 1 B  |  1 B   | 1 B |  1 B  | 2 B  | 0..LEN B |    2 B    |
 *   +------+------+--------+-----+-------+------+----------+-----------+------+
 *   |<------------- header (7 bytes) ---------->|
 *
 *   - SOF     : start of frame, fixed 0xA5.
 *   - TYPE    : EASN_FRAME_CMD or EASN_FRAME_ACK.
 *   - CMD_ID  : command identifier (for ACK: the command being acknowledged).
 *   - SEQ     : sequence number chosen by the sender of a CMD; the ACK echoes
 *               it. Increments by 1 (mod 256) for every NEW CMD frame;
 *               retransmissions reuse the same SEQ. Used by sender to verify
                 to which command an ACK belongs to. Used by receiver to detect
                 duplicate commands.
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
 *  - The ACK payload carries a status code (ifl_status_t). The status says
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

#ifdef __cplusplus
extern "C" {
#endif

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
#define INTERFW_LINK_UART_HDR_SIZE              7u      /* SOF..LEN */
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
	/** Commands implemented by both firmware components. */
	IFL_CMD_COMMON__RANGE_START        = 0x01, /**< Not a command, just helper to check commands ranges */
	IFL_CMD_COMMON__PING               = 0x01, /**< Link liveness check. */
	IFL_CMD_COMMON__RANGE_END          = 0x0F, /**< Not a command, just helper to check commands ranges */

	/** Commands EXPOSED BY EASNFW-SENSOR (CLOUD -> SENSOR). */
	IFL_CMD_SENSOR__RANGE_START  = 0x10, /**<  Not a command, just helper to check commands ranges */
	IFL_CMD_SENSOR__CLOUD_READY  = 0x10, /**< EASNFW-CLOUD ready after reset. */
	IFL_CMD_SENSOR__RANGE_END    = 0x7F, /**<  Not a command, just helper to check commands ranges. */

	/** Commands EXPOSED BY EASNFW-CLOUD (SENSOR -> CLOUD). */
	IFL_CMD_CLOUD__RANGE_START   = 0x80, /**<  Not a command, just helper to check commands ranges */
	IFL_CMD_CLOUD__RESET         = 0x80, /**< RESET EASNFW-CLOUD. */
	IFL_CMD_CLOUD__RANGE_END     = 0xEF, /**<  Not a command, just helper to check commands ranges */
};

/* ========================================================================== */
/*  ACK status codes                                                          */
/* ========================================================================== */

/* some of the following statuses are placeholders, not all of them will be used
   necessarily and other ones may need to be added */
typedef enum {
    IFL_STAT__OK              = 0x00, /**< Received and accepted                    */
    IFL_STAT__UNKNOWN_CMD     = 0x01, /**< Command ID not exposed by this firmware  */
    IFL_STAT__BAD_LEN         = 0x02, /**< Payload length invalid for this command  */
    IFL_STAT__BAD_PARAM       = 0x03, /**< Payload content invalid                  */
    IFL_STAT__BUSY            = 0x04, /**< Cannot handle now, sender may retry later */
    IFL_STAT__NOT_SUPPORTED   = 0x05, /**< Known command but not available right now */
    IFL_STAT__BAD_OFFSET      = 0x06, /**< Fragment offset != expected (see next_offset) */
    IFL_STAT__NO_MEMORY       = 0x07, /**< Cannot store the incoming data           */
    IFL_STAT__TRANSFER_ERR    = 0x08, /**< Transfer-level failure (bad total size, id mismatch, CRC of whole image...) */
    IFL_STAT__INTERNAL_ERR    = 0xFF  /**< Unspecified error in the receiver        */
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

/** Frame header as it appears on the wire. */
typedef struct IFL_PACKED {
    uint8_t  sof;        /**< INTERFW_LINK_UART_SOF               */
    uint8_t  type;       /**< easn_frame_type_t                   */
    uint8_t  cmd_id;     /**< Command (echoed in the ACK)         */
    uint8_t  seq;        /**< Sequence number (echoed in the ACK) */
    uint8_t  flags;      /**< EASN_FLAG_xxx                       */
    uint16_t len;        /**< Payload length                      */
} easn_frame_hdr_t;

/** Payload of an ACK frame. */
typedef struct IFL_PACKED {
    uint8_t  status;       /**< ifl_status_t                                    */
    uint8_t  reserved;     /**< 0                                                */
    uint32_t next_offset;  /**< Fragment ACKs: next byte offset the receiver
                                expects (== offset+chunk_len on success, or the
                                resume point on EASN_ST_BAD_OFFSET). 0 otherwise. */
} easn_ack_payload_t;

/** Prefix of the payload in frames with EASN_FLAG_FRAGMENT. */
typedef struct IFL_PACKED {
    uint8_t  transfer_id;  /**< Identifies the transfer; constant for all of its fragments */
    uint32_t total_len;    /**< Total size in bytes of the whole data blob */
    uint32_t offset;       /**< Offset of this chunk inside the blob       */
    /* followed by chunk bytes (the actual data of the current transfer),
       number of chunk bytes in a given transfer is frame header's len - sizeof(easn_frag_hdr_t) */
} easn_frag_hdr_t;

#define INTERFW_LINK_UART_MAX_FRAG_DATA \
    (INTERFW_LINK_UART_MAX_PAYLOAD - sizeof(easn_frag_hdr_t))

/* ---- Example command payloads ------------------------------------------- */

typedef struct IFL_PACKED {
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
} ifl_cmd_payload_cloud_ready_t;

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

/** Description of a command accepted by an IFL endpoint. */
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
/*  APÌ                                                                       */
/* ========================================================================== */

/**
 * @brief Get Inter-FW Link initialization status.
 *
 * @return true if Inter-FW Link is initialized, false otherwise.
 */
bool ifl__is_init(void);

/**
 * @brief Initialize Inter-FW Link.
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