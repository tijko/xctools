
#if !defined(__GNUC__)
#pragma pack(push, 1)
#pragma warning(push)
#define XENACPI_PACKED
#define XENACPI_INLINE __inline
#else /* __GNUC__ */
#define XENACPI_PACKED __attribute__ ((packed))
#define XENACPI_INLINE inline
#endif /* __GNUC__ */

/*** XEN ACPI WMI API ***/

/* This API provides access to the platform ACPI WMI layer including
 * reporting on WMI devices and methods and method execution.
 */

/* XEN ACPI WMI defines */
#define XENACPI_WMI_GUID_SIZE       16
#define XENACPI_WMI_GUID_BLOCK_SIZE 20
#define XENACPI_WMI_NAME_SIZE       128

#define XENACPI_WMI_FLAG_NONE      0x0000
#define XENACPI_WMI_FLAG_USE_WMIID 0x0001
#define XENACPI_WMI_FLAG_USE_OBJID 0x0002

#define XENACPI_REGFLAG_EXPENSIVE  0x01
#define XENACPI_REGFLAG_METHOD     0x02
#define XENACPI_REGFLAG_STRING     0x04
#define XENACPI_REGFLAG_EVENT      0x08

#define XENACPI_WMI_MOF_GUID \
    "\x21\x12\x90\x05\x66\xD5\xD1\x11\xB2\xF0\x00\xA0\xC9\x06\x29\x10"

struct xenacpi_wmi_device {
    uint32_t name;
    uint32_t wmiid;
    char _hid[XENACPI_WMI_NAME_SIZE];
    char _uid[XENACPI_WMI_NAME_SIZE];
};

struct xenacpi_wmi_guid_block {
    uint8_t guid[XENACPI_WMI_GUID_SIZE];
    union {
        char object_id[2];
        struct {
            uint8_t notify_id;
            uint8_t reserved;
        } wmi_event;
    } wmi_type;
    uint8_t instance_count;
    uint8_t flags;
};

struct xenacpi_wmi_invocation_data {
    uint8_t  guid[XENACPI_WMI_GUID_SIZE];
    uint32_t wmiid;
    char     objid[2];  /* 2 byte ID */
    uint16_t flags;     /* 0 or XENACPI_WMI_FLAG_USE_WMIID or XENACPI_WMI_FLAG_USE_OBJID */
    uint32_t instance;  /* used for method/get/set */
    uint32_t method_id; /* used for method */
    uint32_t event_id;  /* used for event */
};

#define xenacpi_wmi_extract_name(n, u) {\
    n[0] = (char)(0xFF & (u)); n[1] = (char)(0xFF & (u >> 8));\
    n[2] = (char)(0xFF & (u >> 16)); n[3] = (char)(0xFF & (u >> 24));\
    n[4] = '\0';}

/*** WMI Command Processor Interface ***/

/* This command processor interface is the interface between the guest virtual
 * WMI APCI functionality and the underlying firmware. The guest WMI devices
 * use IO ports to communicate with Qemu which marshals WMI requests back and
 * forth between the guest and the host firmware.
 */

/* XEN ACPI WMI command processor ports */
#define XENACPI_WMI_CMD_PORT        0x96
#define XENACPI_WMI_GPE_EPORT       0x97
#define XENACPI_WMI_DATA_PORTB      0x98
#define XENACPI_WMI_GPE_DPORT       0x99
#define XENACPI_WMI_DATA_PORTL      0x9A

/* WMI/OEM Xenstore paths */
#define XENACPI_XS_OEM_EVENT_PATH       "/oem/event"
#define XENACPI_XS_OEM_WMI_NOTIFY_PATH  "/oem/wmi/notify"
#define XENACPI_XS_OEM_WMI_SSDT_PATH    "/oem/wmi_ssdt"

/* Values written to WMI command port */
enum XENACPI_WMI_COMMAND {
    XENACPI_WMI_CMD_INIT = 100,
    XENACPI_WMI_CMD_GUID,
    XENACPI_WMI_CMD_OBJ_INSTANCE,
    XENACPI_WMI_CMD_METHOD_ID,
    XENACPI_WMI_CMD_IN_BUFFER,
    XENACPI_WMI_CMD_IN_BUFFER_SIZE,
    XENACPI_WMI_CMD_EXECUTE,
    XENACPI_WMI_CMD_OUT_BUFFER,
    XENACPI_WMI_CMD_OUT_BUFFER_SIZE,
    XENACPI_WMI_CMD_EVENT_ID,
    XENACPI_WMI_CMD_OBJID_HINT,
    XENACPI_WMI_CMD_WMIID_HINT,
    XENACPI_WMI_CMD_UNDEFINED
};

/* Values written by INIT specifying invocation type */
enum XENACPI_WMI_OBJ_INVOCATION_TYPE {
    XENACPI_WMI_INV_EXEC_METHOD = 1,
    XENACPI_WMI_INV_QUERY_OBJECT,
    XENACPI_WMI_INV_SET_OBJECT,
    XENACPI_WMI_INV_GET_EVENT_DATA,
    XENACPI_WMI_INV_UNDEFINED
};

/* XEN ACPI WMI SSDT DMA ports */
#define XENACPI_WMI_SSDT_CMD_PORTL  0x3E00
#define XENACPI_WMI_SSDT_DATA_PORTL 0x3E04

/* Command values for starting a DMA transfer of the WMI SSDT */
enum XENACPI_WMI_SSDT_DMA {
    XENACPI_WMI_SSDT_DMA_INIT = 300,
    XENACPI_WMI_SSDT_DMA_COMPLETE,
    XENACPI_WMI_SSDT_DMA_UNDEFINED
};

/*** XEN ACPI Video API ***/

/* This API provides access to the platform Video ACPI layer. This
 * currently only includes brightness related functionality.
 */

/* XEN ACPI Video defines */
struct xenacpi_vid_brightness_levels {
    uint32_t level_count;
    uint32_t levels[0];
};

/*** BCL Command Processor Interface ***/

/* This command processor interface is the interface between the guest virtual
 * LCD BCL APCI functionality and the underlying firmware. The guest ODEV device
 * uses IO ports to communicate with Qemu which marshals brightness valuse back and
 * forth between the guest and the host firmware.
 */

/* XEN ACPI Video Brightness command processor ports */
#define XENACPI_VIDEO_PORTB  0x94
#define XENACPI_BCL_CMD_PORT 0xB04C

/* Values written to VIDEO command port */
enum XENACPI_VIDEO_COMMAND {
    XENACPI_VIDEO_GET_NUM_LEVELS = 100,
    XENACPI_VIDEO_GET_LEVELS,
    XENACPI_VIDEO_CMD_NONE,
};

enum XENACPI_VIDEO_BCL_COMMAND {
    XENACPI_BCL_NONE = 0,
    XENACPI_BCL_UP,
    XENACPI_BCL_DOWN,
};

/*** XEN AML Library ***/

/* The XEN AML library is a general purpose library for creating ACPI AML
 * at runtime.
 */

/* XEN ACPI AML - Operations */
#define XENAML_NAME_SIZE                4
#define XENAML_MAX_ARG_COUNT            7

#define XENAML_SYNC_NO_TIMEOUT               0xFFFF

#define XENAML_ADR_SPACE_SYSTEM_MEMORY       0x00
#define XENAML_ADR_SPACE_SYSTEM_IO           0x01
#define XENAML_ADR_SPACE_PCI_CONFIG          0x02
#define XENAML_ADR_SPACE_EMBEDDED_CONTROLLER 0x03
#define XENAML_ADR_SPACE_SMBUS               0x04
#define XENAML_ADR_SPACE_FIXED_HARDWARE      0x7F

typedef uint8_t xenaml_bool;

enum xenaml_int {
    XENAML_INT_ZERO = 0,
    XENAML_INT_ONE,
    XENAML_INT_ONES,
    XENAML_INT_BYTE,
    XENAML_INT_WORD,
    XENAML_INT_DWORD,
    XENAML_INT_QWORD,
    XENAML_INT_OPTIMIZE,
    XENAML_INT_MAX
};

struct xenaml_args {
    void *arg[XENAML_MAX_ARG_COUNT];
    uint16_t count;
};

enum xenaml_field_acccess_type {
    XENAML_FIELD_ACCESS_TYPE_ANY    = 0x00,
    XENAML_FIELD_ACCESS_TYPE_BYTE   = 0x01,
    XENAML_FIELD_ACCESS_TYPE_WORD   = 0x02,
    XENAML_FIELD_ACCESS_TYPE_DWORD  = 0x03,
    XENAML_FIELD_ACCESS_TYPE_QWORD  = 0x04,
    XENAML_FIELD_ACCESS_TYPE_BUFFER = 0x05
};

enum xenaml_field_lock_rule {
    XENAML_FIELD_LOCK_NEVER  = 0x00,
    XENAML_FIELD_LOCK_ALWAYS = 0x10
};

enum xenaml_field_update_rule {
    XENAML_FIELD_UPDATE_PRESERVE      = 0x00,
    XENAML_FIELD_UPDATE_WRITEASONES   = 0x20,
    XENAML_FIELD_UPDATE_WRITEASZEROES = 0x40
};


#define XENAML_FIELD_TYPE_NAME          0x01
#define XENAML_FIELD_TYPE_OFFSET        0x02

struct xenaml_field_unit {
    union {
        struct {
            char name[XENAML_NAME_SIZE];
            uint8_t size_in_bits;
        } aml_name;
        struct {
            uint8_t bits_to_offset;
        } aml_offset;
    } aml_field;
    uint8_t type;
};

enum xenaml_variable_type {
    XENAML_VARIABLE_TYPE_LOCAL = 0,
    XENAML_VARIABLE_TYPE_ARG
};

enum xenaml_math_func {
    XENAML_MATH_FUNC_ADD = 0,
    XENAML_MATH_FUNC_SUBTRACT,
    XENAML_MATH_FUNC_MULTIPLY,
    XENAML_MATH_FUNC_DIVIDE,
    XENAML_MATH_FUNC_MODULO,
    XENAML_MATH_FUNC_INCREMENT,
    XENAML_MATH_FUNC_DECREMENT,
    XENAML_MATH_FUNC_AND,
    XENAML_MATH_FUNC_NAND,
    XENAML_MATH_FUNC_OR,
    XENAML_MATH_FUNC_NOR,
    XENAML_MATH_FUNC_XOR,
    XENAML_MATH_FUNC_NOT,
    XENAML_MATH_FUNC_SHIFTLEFT,
    XENAML_MATH_FUNC_SHIFTRIGHT,
    XENAML_MATH_FUNC_MAX
};

enum xenaml_logic_func {
    XENAML_LOGIC_FUNC_AND = 0,
    XENAML_LOGIC_FUNC_OR,
    XENAML_LOGIC_FUNC_NOT,
    XENAML_LOGIC_FUNC_EQUAL,
    XENAML_LOGIC_FUNC_GREATERTHAN,
    XENAML_LOGIC_FUNC_LESSTHAN,
    XENAML_LOGIC_FUNC_NOTEQUAL,
    XENAML_LOGIC_FUNC_LESSEQUAL,
    XENAML_LOGIC_FUNC_GREATEREQUAL,
    XENAML_LOGIC_FUNC_MAX
};

enum xenaml_misc_func {
    XENAML_MISC_FUNC_ALIAS = 0,
    XENAML_MISC_FUNC_STORE,
    XENAML_MISC_FUNC_DEREFOF,
    XENAML_MISC_FUNC_NOTIFY,
    XENAML_MISC_FUNC_SIZEOF,
    XENAML_MISC_FUNC_INDEX,
    XENAML_MISC_FUNC_TOBUFFER,
    XENAML_MISC_FUNC_TODECSTRING,
    XENAML_MISC_FUNC_TOHEXSTRING,
    XENAML_MISC_FUNC_TOINTEGER,
    XENAML_MISC_FUNC_TOSTRING,
    XENAML_MISC_FUNC_CONTINUE,
    XENAML_MISC_FUNC_RETURN,
    XENAML_MISC_FUNC_BREAK,
    XENAML_MISC_FUNC_SLEEP,
    XENAML_MISC_FUNC_STALL,
    XENAML_MISC_FUNC_MAX
};

enum xenaml_create_field {
    XENAML_CREATE_FIELD_BIT = 0,
    XENAML_CREATE_FIELD_BYTE,
    XENAML_CREATE_FIELD_WORD,
    XENAML_CREATE_FIELD_DWORD,
    XENAML_CREATE_FIELD_QWORD,
    XENAML_CREATE_FIELD,
    XENAML_CREATE_FIELD_MAX
};

#define XENAML_BUFFER_INIT_RAWDATA      0x00
#define XENAML_BUFFER_INIT_VARLEN       0x01
#define XENAML_BUFFER_INIT_INTLEN       0x02
#define XENAML_BUFFER_INIT_NAMELEN      0x03

struct xenaml_buffer_init {
    union {
        struct {
            const uint8_t *buffer;
            uint32_t raw_length;
        } aml_rawdata;
        struct {
            enum xenaml_variable_type var_type;
            uint8_t var_num;
        } aml_varlen;
        struct {
            uint32_t length;
        } aml_intlen;
        struct {
            char name[XENAML_NAME_SIZE];
        } aml_namelen;
    } aml_buffer;
    uint8_t init_type;
};

/* XEN ACPI AML - Resources */

enum xenaml_irq_mode {
    XENAML_IRQ_MODE_LEVEL = 0,
    XENAML_IRQ_MODE_EDGE,
    XENAML_IRQ_MODE_MAX
};

enum xenaml_irq_active {
    XENAML_IRQ_ACTIVE_HIGH = 0,
    XENAML_IRQ_ACTIVE_LOW,
    XENAML_IRQ_ACTIVE_MAX
};

enum xenaml_dma_type {
    XENAML_DMA_TYPE_COMPAT = 0,
    XENAML_DMA_TYPE_A,
    XENAML_DMA_TYPE_B,
    XENAML_DMA_TYPE_F,
    XENAML_DMA_TYPE_MAX
};

enum xenaml_dma_transfer_size {
    XENAML_DMA_TRANSER_SIZE_8 = 0,
    XENAML_DMA_TRANSER_SIZE_8_16,
    XENAML_DMA_TRANSER_SIZE_16,
    XENAML_DMA_TRANSER_SIZE_MAX
};

enum xenaml_dep_priority {
    XENAML_DEP_PRIORITY_GOOD = 0,
    XENAML_DEP_PRIORITY_ACCEPTABLE,
    XENAML_DEP_PRIORITY_SUBOPTIMAL,
    XENAML_DEP_PRIORITY_MAX
};

enum xenaml_io_decode {
    XENAML_IO_DECODE_10 = 0,
    XENAML_IO_DECODE_16,
    XENAML_IO_DECODE_MAX
};

#define XENAML_RESOURCE_TYPE_MIN 0xC0
#define XENAML_RESOURCE_TYPE_MAX 0xFF

enum xenaml_resource_usage {
    XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER = 0,
    XENAML_RESOURCE_USAGE_CONSUMER,
    XENAML_RESOURCE_USAGE_MAX
};

enum xenaml_decode {
    XENAML_DECODE_POS_DECODE = 0,
    XENAML_DECODE_SUB_DECODE,
    XENAML_DECODE_MAX
};

enum xenaml_isa_ranges {
    XENAML_ISA_RANGE_NONISA_ONLY = 1,
    XENAML_ISA_RANGE_ISA_ONLY,
    XENAML_ISA_RANGE_ENTIRE,
    XENAML_ISA_RANGE_MAX
};

enum xenaml_translation_type {
    XENAML_TRANSLATION_TYPE_STATIC = 0,
    XENAML_TRANSLATION_TYPE_TRANSLATION,
    XENAML_TRANSLATION_TYPE_MAX
};

enum xenaml_translation_density {
    XENAML_TRANSLATION_DENSITY_DENSE = 0,
    XENAML_TRANSLATION_DENSITY_SPARSE,
    XENAML_TRANSLATION_DENSITY_MAX
};

enum xenaml_memory_caching {
    XENAML_MEMORY_CACHING_NON_CACHEABLE = 0,
    XENAML_MEMORY_CACHING_CACHEABLE,
    XENAML_MEMORY_CACHING_WRITECOMBINING,
    XENAML_MEMORY_CACHING_PREFETCH,
    XENAML_MEMORY_CACHING_MAX
};

enum xenaml_memory_type {
    XENAML_MEMORY_TYPE_MEMORY = 0,
    XENAML_MEMORY_TYPE_RESERVED,
    XENAML_MEMORY_TYPE_ACPI,
    XENAML_MEMORY_TYPE_NVS,
    XENAML_MEMORY_TYPE_MAX
};

enum xenaml_register_access_size {
    XENAML_REGISTER_ACCESS_UNDEFINED = 0,
    XENAML_REGISTER_ACCESS_BYTE,
    XENAML_REGISTER_ACCESS_WORD,
    XENAML_REGISTER_ACCESS_DWORD,
    XENAML_REGISTER_ACCESS_QWORD,
    XENAML_REGISTER_ACCESS_MAX
};

struct xenaml_address_space_common_args {
    enum xenaml_resource_usage resource_usage;
    xenaml_bool is_min_fixed;
    xenaml_bool is_max_fixed;
    enum xenaml_decode decode;
};

struct xenaml_address_values_args {
    uint64_t address_granularity;
    uint64_t address_minimum;
    uint64_t address_maximum;
    uint64_t address_translation;
    uint64_t range_length;
};

#define XENAML_QWORD_ADDRESS_ARG(f, d) (f = (0xFFFFFFFFFFFFFFFF & d))
#define XENAML_DWORD_ADDRESS_ARG(f, d) (f = (0x00000000FFFFFFFF & d))
#define XENAML_WORD_ADDRESS_ARG(f, d)  (f = (0x000000000000FFFF & w))

struct xenaml_address_space_source_args {
    xenaml_bool present;
    uint8_t resource_source_index;
    const char *resource_source;
};

struct xenaml_qword_io_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_isa_ranges isa_ranges;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
    enum xenaml_translation_type translation_type;
    enum xenaml_translation_density translation_density;
};

struct xenaml_qword_memory_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_memory_caching cacheable;
    xenaml_bool read_write;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
    enum xenaml_memory_type memory_type;
    enum xenaml_translation_type translation_type;
};

struct xenaml_qword_space_args {
    uint8_t resource_type;
    struct xenaml_address_space_common_args common_args;
    uint8_t type_specific_flags;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
};

struct xenaml_dword_io_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_isa_ranges isa_ranges;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
    enum xenaml_translation_type translation_type;
    enum xenaml_translation_density translation_density;
};

struct xenaml_dword_memory_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_memory_caching cacheable;
    xenaml_bool read_write;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
    enum xenaml_memory_type memory_type;
    enum xenaml_translation_type translation_type;
};

struct xenaml_dword_space_args {
    uint8_t resource_type;
    struct xenaml_address_space_common_args common_args;
    uint8_t type_specific_flags;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
};

struct xenaml_word_bus_args {
    struct xenaml_address_space_common_args common_args;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
};

struct xenaml_word_io_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_isa_ranges isa_ranges;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
    enum xenaml_translation_type translation_type;
    enum xenaml_translation_density translation_density;
};

struct xenaml_word_space_args {
    uint8_t resource_type;
    struct xenaml_address_space_common_args common_args;
    uint8_t type_specific_flags;
    struct xenaml_address_values_args address_args;
    struct xenaml_address_space_source_args source_args;
};

struct xenaml_extended_io_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_isa_ranges isa_ranges;
    struct xenaml_address_values_args address_args;
    uint64_t type_specific_attributes;
    enum xenaml_translation_type translation_type;
    enum xenaml_translation_density translation_density;
};

struct xenaml_extended_memory_args {
    struct xenaml_address_space_common_args common_args;
    enum xenaml_memory_caching cacheable;
    xenaml_bool read_write;
    struct xenaml_address_values_args address_args;
    uint64_t type_specific_attributes;
    enum xenaml_memory_type memory_type;
    enum xenaml_translation_type translation_type;
};

struct xenaml_extended_space_args {
    uint8_t resource_type;
    struct xenaml_address_space_common_args common_args;
    uint8_t type_specific_flags;
    struct xenaml_address_values_args address_args;
    uint64_t type_specific_attributes;
};

struct xenaml_interrupt_args {
    enum xenaml_resource_usage resource_usage;
    enum xenaml_irq_mode edge_level;
    enum xenaml_irq_active active_level;
    xenaml_bool shared;
    struct xenaml_address_space_source_args source_args;
    uint32_t *interrupts;
    uint8_t count;
};

#if !defined(__GNUC__)
#pragma pack(pop)
#endif
