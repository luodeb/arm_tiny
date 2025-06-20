#ifndef __VIRTIO_INTERRUPT_H__
#define __VIRTIO_INTERRUPT_H__

#include "tiny_types.h"
#include "config.h"

// VirtIO interrupt status flags (from VirtIO specification)
#define VIRTIO_IRQ_VRING_UPDATE (1 << 0)  // Used buffer notification
#define VIRTIO_IRQ_CONFIG_CHANGE (1 << 1) // Configuration change notification

// VirtIO interrupt wait timeout (in milliseconds)
#define VIRTIO_IRQ_TIMEOUT_MS 5000 // 5 second timeout

// Global VirtIO interrupt state tracking
typedef struct
{
    volatile bool interrupt_received;   // Set to true when interrupt occurs
    volatile uint32_t interrupt_status; // Last interrupt status from device
    volatile uint32_t interrupt_count;  // Total number of interrupts received
    volatile uint32_t last_used_idx;    // Last processed used ring index
    bool interrupts_enabled;            // Whether VirtIO interrupts are enabled
    uint32_t active_irq_number;         // Currently active IRQ number (0 if none)
} virtio_interrupt_state_t;

// Global interrupt state (extern declaration)
extern virtio_interrupt_state_t virtio_irq_state;

// Function declarations

/**
 * Initialize VirtIO interrupt system
 * - Registers interrupt handler with GIC
 * - Enables VirtIO interrupt in GIC
 * - Initializes interrupt state tracking
 * @return true if initialization successful, false otherwise
 */
bool virtio_interrupt_init(void);

/**
 * VirtIO interrupt handler (called from IRQ context)
 * - Reads and processes VirtIO interrupt status
 * - Updates global interrupt state
 * - Acknowledges interrupt to device
 * @param ctx Interrupt context (not used)
 */
void virtio_irq_handler(uint64_t *ctx);

/**
 * Wait for VirtIO interrupt with timeout
 * - Replaces polling-based wait mechanism
 * - Uses interrupt-driven notification
 * @param timeout_ms Timeout in milliseconds
 * @return true if interrupt received within timeout, false if timeout
 */
bool virtio_wait_for_interrupt(uint32_t timeout_ms);

/**
 * Get current VirtIO interrupt status from device
 * @return Current interrupt status register value
 */
uint32_t virtio_get_interrupt_status(void);

/**
 * Reset VirtIO interrupt state
 * - Clears all interrupt flags and counters
 * - Used for testing and initialization
 */
void virtio_reset_interrupt_state(void);

/**
 * Print detailed VirtIO interrupt statistics
 * - For debugging and diagnostics
 */
void virtio_print_interrupt_stats(void);

#endif // __VIRTIO_INTERRUPT_H__
