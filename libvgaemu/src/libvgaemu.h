#ifndef LIBVGAEMU_H_
# define LIBVGAEMU_H_

/* Opaque type for vga */
typedef struct s_vga *s_vga;

/* This structure describes a range in VGA */
typedef struct
{
    uint64_t start;
    uint64_t length;
    int is_mmio;
} s_vga_range;

/* Resize buffer callback */
typedef void *(*vga_resize_func)(uint32_t width, uint32_t height,
                                 uint32_t linesize, void *priv);

/* Initialize a new vga emulation */
s_vga vga_init(unsigned char *buffer, vga_resize_func resize,
               uint32_t stride_align, void *priv);

/**
 * Retrieve range handle by VGA
 * The array is terminated by 0 in both start and length
 */
const s_vga_range *vga_ranges_get();

/* Release a VGA emulation */
void vga_destroy(s_vga vga);

/* IOPort read/write */
uint32_t vga_ioport_read(s_vga vga, uint64_t addr, uint32_t size);
void vga_ioport_write(s_vga vga, uint64_t addr, uint32_t value,
                      uint32_t size);
/* MMIO read/write */
uint32_t vga_mem_read(s_vga s, uint64_t addr, uint32_t size);
void vga_mem_write(s_vga s, uint64_t addr, uint32_t value, uint32_t size);

/* Update the display */
void vga_update_display(s_vga vga);
/* Do we need to refresh screen? */
int vga_need_refresh(s_vga vga);

#endif /* !LIBVGAEMU_H_ */
