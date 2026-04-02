#ifndef HW_SIMPLE_IRQ_H
#define HW_SIMPLE_IRQ_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_SIMPLE_IRQ "simple-irq"
OBJECT_DECLARE_SIMPLE_TYPE(SimpleIRQState, SIMPLE_IRQ)

typedef struct SimpleIRQState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq irq;
    QEMUTimer *timer;

    uint32_t control;       // Control register
    uint32_t status;        // Status register  
    uint32_t interval;      // Timer interval register

    bool irq_enabled;
    bool timer_enabled;
    bool irq_pending;
} SimpleIRQState;

// Register offsets
#define SIMPLE_IRQ_CONTROL   0x00
#define SIMPLE_IRQ_STATUS    0x04

// Control register bits
#define CONTROL_IRQ_ENABLE   (1 << 0)
#define CONTROL_TIMER_ENABLE (1 << 1)
#define CONTROL_MANUAL_IRQ   (1 << 2)
#define CONTROL_RESET        (1 << 7)

// Status register bits
#define STATUS_IRQ_PENDING   (1 << 0)
#define STATUS_TIMER_ACTIVE  (1 << 1)

#endif
