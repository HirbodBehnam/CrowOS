/**
 * Enable FPU at the boot time.
 */
void fpu_enable(void);
/**
 * Saves the FPU state in the given save area. The area must be aligned
 * to 16 bytes. The size of the save location must be at least 512 bytes.
 */
void fpu_save(void *);

/**
 * Loads the FPU state saved with fpu_save to the FPU. The given location
 * must be aligned to 16 bytes.
 */
void fpu_load(const void *);