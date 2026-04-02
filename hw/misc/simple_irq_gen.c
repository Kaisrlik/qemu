#include "qemu/osdep.h"
#include "hw/misc/simple_irq_gen.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/irq.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "trace.h"

static void simple_irq_update_irq(SimpleIRQState *s)
{
    bool should_assert = s->irq_enabled && s->irq_pending;
    qemu_log("SimpleIRQ: %s interrupt (enabled=%d, pending=%d)\n", should_assert ? "Asserting" : "Clearing", s->irq_enabled, s->irq_pending);
    qemu_set_irq(s->irq, should_assert ? 1 : 0);
}

static void simple_irq_timer_cb(void *opaque)
{
    SimpleIRQState *s = SIMPLE_IRQ(opaque);

    qemu_log("SimpleIRQ: Timer callback - generating interrupt\n");

    s->irq_pending = true;
    s->status |= STATUS_IRQ_PENDING;

    simple_irq_update_irq(s);

    // Reschedule timer if still enabled
    if (s->timer_enabled && s->interval > 0) {
        // interval in milliseconds
        timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->interval * 1000000ULL);
    }
}

static uint64_t simple_irq_read(void *opaque, hwaddr addr, unsigned size)
{
    SimpleIRQState *s = SIMPLE_IRQ(opaque);
    uint32_t value = 0;

    switch (addr) {
    case SIMPLE_IRQ_CONTROL:
        value = s->control;
        break;
    case SIMPLE_IRQ_STATUS:
        value = s->status;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, 
                      "SimpleIRQ: Invalid read at offset 0x%x\n", (int)addr);
        break;
    }

    qemu_log("SimpleIRQ: Read 0x%08x from offset 0x%x\n", value, (int)addr);
    return value;
}

static void simple_irq_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    SimpleIRQState *s = SIMPLE_IRQ(opaque);

    qemu_log("SimpleIRQ: Write 0x%08x to offset 0x%x\n", (uint32_t)value, (int)addr);

    switch (addr) {
    case SIMPLE_IRQ_CONTROL:
        s->control = value;

        // Handle control bits
        s->irq_enabled = !!(value & CONTROL_IRQ_ENABLE);

        if (value & CONTROL_TIMER_ENABLE) {
            if (!s->timer_enabled && s->interval > 0) {
                s->timer_enabled = true;
                s->status |= STATUS_TIMER_ACTIVE;
                timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->interval * 1000000ULL);
                qemu_log("SimpleIRQ: Timer started with interval %u ms\n", s->interval);
            }
        } else {
            if (s->timer_enabled) {
                s->timer_enabled = false;
                s->status &= ~STATUS_TIMER_ACTIVE;
                timer_del(s->timer);
                qemu_log("SimpleIRQ: Timer stopped\n");
            }
        }

        // Manual IRQ trigger
        if (value & CONTROL_MANUAL_IRQ) {
            qemu_log("SimpleIRQ: Manual IRQ trigger\n");
            s->irq_pending = true;
            s->status |= STATUS_IRQ_PENDING;
        }

        // Reset
        if (value & CONTROL_RESET) {
            qemu_log("SimpleIRQ: Reset\n");
            s->irq_pending = false;
            s->status &= ~STATUS_IRQ_PENDING;
            timer_del(s->timer);
            s->timer_enabled = false;
            s->status &= ~STATUS_TIMER_ACTIVE;
        }

        simple_irq_update_irq(s);
        break;

    case SIMPLE_IRQ_STATUS:
        // Writing 1 to IRQ_PENDING bit clears it (acknowledge)
        if (value & STATUS_IRQ_PENDING) {
            qemu_log("SimpleIRQ: IRQ acknowledged\n");
            s->irq_pending = false;
            s->status &= ~STATUS_IRQ_PENDING;
            simple_irq_update_irq(s);
        }
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "SimpleIRQ: Invalid write at offset 0x%x\n", (int)addr);
        break;
    }
}

static const MemoryRegionOps simple_irq_ops = {
    .read = simple_irq_read,
    .write = simple_irq_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void simple_irq_realize(DeviceState *dev, Error **errp)
{
    SimpleIRQState *s = SIMPLE_IRQ(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &simple_irq_ops, s, TYPE_SIMPLE_IRQ, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->irq);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, simple_irq_timer_cb, s);

    qemu_log("SimpleIRQ: Device realized\n");
}

static const Property simple_irq_properties[] = {
    DEFINE_PROP_UINT32("default-interval", SimpleIRQState, interval, 1000)
};

static void simple_irq_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = simple_irq_realize;
    device_class_set_props(dc, simple_irq_properties);
    dc->desc = "Simple IRQ Generator Device";
}

static const TypeInfo simple_irq_info = {
    .name = TYPE_SIMPLE_IRQ,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SimpleIRQState),
    .class_init = simple_irq_class_init,
};

static void simple_irq_register_types(void)
{
    type_register_static(&simple_irq_info);
}

type_init(simple_irq_register_types)
