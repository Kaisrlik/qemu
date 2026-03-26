#ifndef HW_RISCV_PICORV32_H
#define HW_RISCV_PICORV32_H

#include "hw/core/boards.h"
#include "hw/riscv/riscv_hart.h"

#define TYPE_RISCV_PICORV32_MACHINE MACHINE_TYPE_NAME("picorv32")
typedef struct RISCVPicorv32State RISCVPicorv32State;
DECLARE_INSTANCE_CHECKER(RISCVPicorv32State, RISCV_PICORV32_MACHINE, TYPE_RISCV_PICORV32_MACHINE)

struct RISCVPicorv32State {
    /*< private >*/
    MachineState parent;

    /*< public >*/
    Notifier machine_done;
    DeviceState *platform_bus_dev;
    RISCVHartArrayState soc[1];
    DeviceState *irqchip[1];
    FWCfgState *fw_cfg;

    int fdt_size;
    const MemMapEntry *memmap;
};

enum {
    PICORV32_MROM,
    PICORV32_RTC,
    PICORV32_PLIC,
    PICORV32_UART0,
    PICORV32_FW_CFG,
    PICORV32_DRAM,
    PICORV32_PLATFORM_BUS,
};

enum {
    UART0_IRQ = 10,
    RTC_IRQ = 11,
    VIRTIO_IRQ = 1, /* 1 to 8 */
    VIRTIO_COUNT = 8,
    PICORV32_PLATFORM_BUS_IRQ = 64, /* 64 to 95 */
};

#define PICORV32_PLATFORM_BUS_NUM_IRQS 32

#define PICORV32_IRQCHIP_NUM_SOURCES 96
#define PICORV32_IRQCHIP_NUM_PRIO_BITS 3

#define PICORV32_PLIC_PRIORITY_BASE 0x00
#define PICORV32_PLIC_PENDING_BASE 0x1000
#define PICORV32_PLIC_ENABLE_BASE 0x2000
#define PICORV32_PLIC_ENABLE_STRIDE 0x80
#define PICORV32_PLIC_CONTEXT_BASE 0x200000
#define PICORV32_PLIC_CONTEXT_STRIDE 0x1000
#define PICORV32_PLIC_SIZE(__num_context) \
    (PICORV32_PLIC_CONTEXT_BASE + (__num_context) * PICORV32_PLIC_CONTEXT_STRIDE)
#endif
