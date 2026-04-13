#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/core/qdev-properties.h"
#include "hw/char/serial-mm.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/picorv32.h"
#include "hw/riscv/boot.h"
#include "kvm/kvm_riscv.h"
#include "hw/intc/riscv_aplic.h"
#include "hw/core/platform-bus.h"
#include "chardev/char.h"
#include "system/device_tree.h"
#include "system/system.h"

#define log(fmt, ...) qemu_log_mask(LOG_GUEST_ERROR, "%s: " fmt, __func__, ##__VA_ARGS__)


static const MemMapEntry picorv32_memmap[] = {
    [PICORV32_SRAM] = { 0x0, 0x00200000},
    [PICORV32_PLATFORM_BUS] = {  0x4000000,     0x2000000 },
    [PICORV32_UART0] =        { 0x10000000,         0x100 },
};

static void __attribute__((unused)) create_platform_bus(RISCVPicorv32State *s, DeviceState *irqchip)
{
    DeviceState *dev;
    SysBusDevice *sysbus;
    int i;
    MemoryRegion *sysmem = get_system_memory();

    dev = qdev_new(TYPE_PLATFORM_BUS_DEVICE);
    dev->id = g_strdup(TYPE_PLATFORM_BUS_DEVICE);
    qdev_prop_set_uint32(dev, "num_irqs", PICORV32_PLATFORM_BUS_NUM_IRQS);
    qdev_prop_set_uint32(dev, "mmio_size", s->memmap[PICORV32_PLATFORM_BUS].size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    s->platform_bus_dev = dev;

    sysbus = SYS_BUS_DEVICE(dev);
    for (i = 0; i < PICORV32_PLATFORM_BUS_NUM_IRQS; i++) {
        int irq = PICORV32_PLATFORM_BUS_IRQ + i;
        sysbus_connect_irq(sysbus, i, qdev_get_gpio_in(irqchip, irq));
    }

    memory_region_add_subregion(sysmem,
                                s->memmap[PICORV32_PLATFORM_BUS].base,
                                sysbus_mmio_get_region(sysbus, 0));
}

static void picorv32_machine_done(Notifier *notifier, void *data)
{
    RISCVPicorv32State *s = container_of(notifier, RISCVPicorv32State,
                                     machine_done);
    MachineState *machine = MACHINE(s);
    hwaddr start_addr = s->memmap[PICORV32_SRAM].base;
    uint64_t kernel_entry = 0;
    RISCVBootInfo boot_info;

    riscv_boot_info_init(&boot_info, &s->soc);
    riscv_load_kernel(machine, &boot_info, start_addr, true, NULL);
    kernel_entry = boot_info.image_low_addr;
    riscv_setup_direct_kernel(kernel_entry, 0);
}

static void picorv32_set_irqvec(void)
{
    CPUState *cs = qemu_get_cpu(0);
    CPURISCVState *env = &RISCV_CPU(cs)->env;
    env->mtvec = PICORV32_IRQ_VEC;
}

static void picorv32_machine_init(MachineState *machine)
{
    RISCVPicorv32State *s = RISCV_PICORV32_MACHINE(machine);
    MachineState *ms = MACHINE(s);
    MemoryRegion *system_memory = get_system_memory();
    uint32_t hid = 0, num_harts = 1;
    char hmode[] = "M";
    (void) hmode;

    s->memmap = picorv32_memmap;

    /* Initialize sockets */
    object_initialize_child(OBJECT(machine), "soc0", &s->soc, TYPE_RISCV_HART_ARRAY);
    object_property_set_uint(OBJECT(&s->soc), "hartid-base", hid, &error_abort);
    object_property_set_uint(OBJECT(&s->soc), "num-harts", num_harts, &error_abort);
    object_property_set_str(OBJECT(&s->soc), "cpu-type", machine->cpu_type, &error_abort);
    object_property_set_uint(OBJECT(&s->soc), "resetvec", s->memmap[PICORV32_SRAM].base, &error_abort);

    sysbus_realize(SYS_BUS_DEVICE(&s->soc), &error_fatal);

    /* Set irq vector address in mtvec */
    picorv32_set_irqvec();

    /* register system main memory (actual RAM) */
    memory_region_add_subregion(system_memory, s->memmap[PICORV32_SRAM].base,
                                machine->ram);
    s->fw_cfg = NULL;
    serial_mm_init(system_memory, s->memmap[PICORV32_UART0].base, 0, NULL, 115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    ms->fdt = create_device_tree(&s->fdt_size);

    s->machine_done.notify = picorv32_machine_done;
    qemu_add_machine_init_done_notifier(&s->machine_done);
}

static void picorv32_machine_instance_init(Object *obj)
{
}

static void picorv32_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V PicoRV32 emulator";
    mc->init = picorv32_machine_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;
    mc->block_default_type = IF_VIRTIO;
    mc->no_cdrom = 1;
    mc->pci_allow_0_address = true;
    mc->default_ram_id = "riscv_picorv32_board.ram";
}

static const TypeInfo picorv32_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("picorv32"),
    .parent     = TYPE_MACHINE,
    .class_init = picorv32_machine_class_init,
    .instance_init = picorv32_machine_instance_init,
    .instance_size = sizeof(RISCVPicorv32State),
    .interfaces = (const InterfaceInfo[]) {
         { TYPE_HOTPLUG_HANDLER },
         { }
    },
};

static void picorv32_machine_init_register_types(void)
{
    type_register_static(&picorv32_machine_typeinfo);
}

type_init(picorv32_machine_init_register_types)
