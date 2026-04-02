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
#include "hw/intc/sifive_plic.h"
#include "hw/core/platform-bus.h"
#include "chardev/char.h"
#include "system/device_tree.h"
#include "system/system.h"

// Devices
#include "hw/misc/simple_irq_gen.h"

#define log(fmt, ...) qemu_log_mask(LOG_GUEST_ERROR, "%s: " fmt, __func__, ##__VA_ARGS__)


static const MemMapEntry picorv32_memmap[] = {
    [PICORV32_DRAM] =         {        0x0,     0x1000000 },
    [PICORV32_MROM] =         {  0x1000000,        0xf000 },
    [PICORV32_PLATFORM_BUS] = {  0x4000000,     0x2000000 },
    [PICORV32_PLIC] =         {  0xc000000, PICORV32_PLIC_SIZE(2) },
    [PICORV32_UART0] =        { 0x10000000,         0x100 },
    [PICORV32_FW_CFG] =       { 0x10100000,          0x18 },
};

static FWCfgState *create_fw_cfg(const MachineState *ms, hwaddr base)
{
    FWCfgState *fw_cfg;
    fw_cfg = fw_cfg_init_mem_dma(base + 8, base, 8, base + 16, &address_space_memory);
    fw_cfg_add_i16(fw_cfg, FW_CFG_NB_CPUS, (uint16_t)ms->smp.cpus);
    return fw_cfg;
}

static DeviceState *picorv32_create_plic(const MemMapEntry *memmap, int socket,
                                     int base_hartid, int hart_count)
{
    g_autofree char *plic_hart_config = NULL;

    /* Per-socket PLIC hart topology configuration string */
    plic_hart_config = riscv_plic_hart_config_string(hart_count);

    /* Per-socket PLIC */
    return sifive_plic_create(
             memmap[PICORV32_PLIC].base + socket * memmap[PICORV32_PLIC].size,
             plic_hart_config, hart_count, base_hartid,
             PICORV32_IRQCHIP_NUM_SOURCES,
             ((1U << PICORV32_IRQCHIP_NUM_PRIO_BITS) - 1),
             PICORV32_PLIC_PRIORITY_BASE, PICORV32_PLIC_PENDING_BASE,
             PICORV32_PLIC_ENABLE_BASE, PICORV32_PLIC_ENABLE_STRIDE,
             PICORV32_PLIC_CONTEXT_BASE,
             PICORV32_PLIC_CONTEXT_STRIDE,
             memmap[PICORV32_PLIC].size);
}

static void create_platform_bus(RISCVPicorv32State *s, DeviceState *irqchip)
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
    hwaddr start_addr = s->memmap[PICORV32_DRAM].base;
    hwaddr firmware_end_addr;
    vaddr kernel_start_addr;
    const char *firmware_name = riscv_default_firmware_name(&s->soc);
    uint64_t fdt_load_addr;
    uint64_t kernel_entry = 0;
    RISCVBootInfo boot_info;

    firmware_end_addr = riscv_find_and_load_firmware(machine, firmware_name, &start_addr, NULL);

    riscv_boot_info_init(&boot_info, &s->soc);

    if (machine->kernel_filename && !kernel_entry) {
        kernel_start_addr = riscv_calc_kernel_start_addr(&boot_info, firmware_end_addr);
        riscv_load_kernel(machine, &boot_info, kernel_start_addr, true, NULL);
        kernel_entry = boot_info.image_low_addr;
    }

    fdt_load_addr = riscv_compute_fdt_addr(s->memmap[PICORV32_DRAM].base, s->memmap[PICORV32_DRAM].size, machine, &boot_info);
    /* load the reset vector */
    riscv_setup_rom_reset_vec(machine, &s->soc, start_addr, s->memmap[PICORV32_MROM].base, s->memmap[PICORV32_MROM].size, kernel_entry, fdt_load_addr);
    riscv_setup_direct_kernel(kernel_entry, fdt_load_addr);
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
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    DeviceState *mmio_irqchip;
    uint32_t hid = 0, num_harts = 1;

    s->memmap = picorv32_memmap;

    /* Initialize sockets */
    mmio_irqchip = NULL;
    object_initialize_child(OBJECT(machine), "soc0", &s->soc, TYPE_RISCV_HART_ARRAY);
    object_property_set_uint(OBJECT(&s->soc), "hartid-base", hid, &error_abort);
    object_property_set_uint(OBJECT(&s->soc), "num-harts", num_harts, &error_abort);
    object_property_set_str(OBJECT(&s->soc), "cpu-type", machine->cpu_type, &error_abort);
    object_property_set_uint(OBJECT(&s->soc), "resetvec", s->memmap[PICORV32_MROM].base, &error_abort);

    sysbus_realize(SYS_BUS_DEVICE(&s->soc), &error_fatal);

    /* Per-socket interrupt controller */
    s->irqchip = picorv32_create_plic(s->memmap, 0, hid, num_harts);
    mmio_irqchip = s->irqchip;

    /* Set irq vector address in mtvec */
    picorv32_set_irqvec();

    /* register system main memory (actual RAM) */
    memory_region_add_subregion(system_memory, s->memmap[PICORV32_DRAM].base,
                                machine->ram);

    /* boot rom */
    memory_region_init_rom(mask_rom, NULL, "riscv_picorv32_board.mrom",
                           s->memmap[PICORV32_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, s->memmap[PICORV32_MROM].base,
                                mask_rom);

    /*
     * Init fw_cfg. Must be done before riscv_load_fdt, otherwise the
     * device tree cannot be altered and we get FDT_ERR_NOSPACE.
     */
    s->fw_cfg = create_fw_cfg(machine, s->memmap[PICORV32_FW_CFG].base);
    rom_set_fw(s->fw_cfg);

    create_platform_bus(s, mmio_irqchip);

    serial_mm_init(system_memory, s->memmap[PICORV32_UART0].base,
        0, qdev_get_gpio_in(mmio_irqchip, UART0_IRQ), 399193,
        serial_hd(0), DEVICE_LITTLE_ENDIAN);


#define SIMPLE_IRQ_GEN_BASE  0x10001000
#define SIMPLE_IRQ_GEN_IRQ   16

    // Create simple IRQ generator
    DeviceState *irq_gen = qdev_new(TYPE_SIMPLE_IRQ);
    qdev_prop_set_uint32(irq_gen, "default-interval", 2000); // 2 seconds
    sysbus_realize_and_unref(SYS_BUS_DEVICE(irq_gen), &error_fatal);
    // Map to memory
    sysbus_mmio_map(SYS_BUS_DEVICE(irq_gen), 0, SIMPLE_IRQ_GEN_BASE);
    // Connect to interrupt controller
    sysbus_connect_irq(SYS_BUS_DEVICE(irq_gen), 0, qdev_get_gpio_in(mmio_irqchip, SIMPLE_IRQ_GEN_IRQ));
    printf("Simple IRQ Generator mapped at 0x%08x, IRQ %d\n", SIMPLE_IRQ_GEN_BASE, SIMPLE_IRQ_GEN_IRQ);

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
