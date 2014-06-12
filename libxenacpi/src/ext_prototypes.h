/* XEN ACPI WMI Inteface */
int xenacpi_wmi_get_devices(struct xenacpi_wmi_device **devices_out,
                            uint32_t *count_out,
                            int *error_out);
int xenacpi_wmi_get_device_blocks(uint32_t wmiid,
                                  struct xenacpi_wmi_guid_block **blocks_out,
                                  uint32_t *count_out,
                                  int *error_out);
int xenacpi_wmi_invoke_method(struct xenacpi_wmi_invocation_data *inv_block,
                              void *buffer_in,
                              uint32_t length_in,
                              void **buffer_out,
                              uint32_t *length_out,
                              int *error_out);
int xenacpi_wmi_query_object(struct xenacpi_wmi_invocation_data *inv_block,
                             void **buffer_out,
                             uint32_t *length_out,
                             int *error_out);
int xenacpi_wmi_set_object(struct xenacpi_wmi_invocation_data *inv_block,
                           void *buffer_in,
                           uint32_t length_in,
                           int *error_out);
int xenacpi_wmi_get_event_data(struct xenacpi_wmi_invocation_data *inv_block,
                               void **buffer_out,
                               uint32_t *length_out,
                               int *error_out);

/* XEN ACPI Video Inteface */
int xenacpi_vid_brightness_levels(struct xenacpi_vid_brightness_levels **levels_out,
                                  int *error_out);
int xenacpi_vid_brightness_switch(int disable,
                                  int *error_out);

/* XEN ACPI AML Interface */

/* AML Ops **/
void* xenaml_nullchar(void *pma);
void* xenaml_raw_data(const uint8_t *buffer,
                      uint32_t raw_length,
                      void *pma);
void* xenaml_integer(uint64_t value,
                     enum xenaml_int int_type,
                     void *pma);
void* xenaml_name_declaration(const char *name,
                              void *init,
                              void *pma);
void* xenaml_name_reference(const char *name,
                            void *children,
                            void *pma);
void* xenaml_variable(enum xenaml_variable_type var_type,
                      uint8_t var_num,
                      void *pma);
void* xenaml_string(const char *str, void *pma);
void* xenaml_eisaid(const char *eisaid_str, void *pma);
void* xenaml_math(enum xenaml_math_func math_func,
                  struct xenaml_args *arg_list,
                  void *pma);
void* xenaml_logic(enum xenaml_logic_func logic_func,
                   struct xenaml_args *arg_list,
                   void *pma);
void* xenaml_misc(enum xenaml_misc_func misc_func,
                  struct xenaml_args *arg_list,
                  void *pma);
void *xenaml_create_field(enum xenaml_create_field create_field,
                          const char *field_name,
                          const char *source_buffer,
                          uint32_t bit_byte_index,
                          uint32_t num_bits,
                          void *pma);
void* xenaml_if(void *predicate, void *term_list, void *pma);
void* xenaml_else(void *term_list, void *pma);
void* xenaml_while(void *predicate, void *term_list, void *pma);
void* xenaml_buffer(struct xenaml_buffer_init *buffer_init,
                    void *pma);
void* xenaml_package(uint32_t num_elements,
                     void *package_list,
                     void *pma);
void* xenaml_mutex(const char *mutex_name,
                   uint8_t sync_level,
                   void *pma);
void* xenaml_acquire(const char *sync_object,
                     uint16_t timout_value,
                     void *pma);
void* xenaml_release(const char *sync_object,
                     void *pma);
void* xenaml_event(const char *event_name,
                   void *pma);
void* xenaml_wait(const char *sync_object,
                  uint16_t timout_value,
                  void *pma);
void* xenaml_signal(const char *sync_object,
                    void *pma);
void* xenaml_reset(const char *sync_object,
                   void *pma);
void* xenaml_power_resource(const char *resource_name,
                            uint8_t system_level,
                            uint16_t resource_order,
                            void *object_list,
                            void *pma);
void* xenaml_thermal_zone(const char *thermal_zone_name,
                          void *object_list,
                          void *pma);
void* xenaml_processor(const char *processor_name,
                       uint8_t processor_id,
                       uint32_t pblock_addr,
                       uint8_t pblock_length,
                       void *object_list,
                       void *pma);
void* xenaml_method(const char *method_name,
                    uint8_t num_args,
                    xenaml_bool serialized,
                    void *term_list,
                    void *pma);
void* xenaml_op_region(const char *region_name,
                       uint8_t region_space,
                       uint64_t region_offset,
                       uint64_t region_length,
                       void *pma);
void* xenaml_field(const char *region_name,
                   enum xenaml_field_acccess_type access_type,
                   enum xenaml_field_lock_rule lock_rule,
                   enum xenaml_field_update_rule update_rule,
                   struct xenaml_field_unit *field_unit_list,
                   uint32_t unit_count,
                   void *pma);
void* xenaml_device(const char *device_name,
                    void *object_list,
                    void *pma);
void* xenaml_scope(const char *name,
                   void *object_list,
                   void *pma);

/* AML Res **/
void* xenaml_irq(enum xenaml_irq_mode edge_level,
                 enum xenaml_irq_active active_level,
                 xenaml_bool shared,
                 uint8_t *irqs,
                 uint8_t count,
                 void *pma);
void* xenaml_irq_noflags(uint8_t *irqs,
                         uint8_t count,
                         void *pma);
void* xenaml_dma(enum xenaml_dma_type dma_type,
                 enum xenaml_dma_transfer_size dma_transfer_size,
                 xenaml_bool is_bus_master,
                 uint8_t *channels,
                 uint8_t count,
                 void *pma);
void* xenaml_start_dependent_fn(enum xenaml_dep_priority capability_priority,
                                enum xenaml_dep_priority performance_priority,
                                void *pma);
void* xenaml_start_dependent_fn_nopri(void *pma);
void* xenaml_end_dependent_fn(void *pma);
void* xenaml_io(enum xenaml_io_decode decode,
                uint16_t address_minimum,
                uint16_t address_maximum,
                uint8_t address_alignment,
                uint8_t range_length,
                void *pma);
void* xenaml_fixed_io(uint16_t address_base,
                      uint8_t range_length,
                      void *pma);
void* xenaml_vendor_short(uint8_t *vendor_bytes,
                          uint8_t count,
                          void *pma);
void* xenaml_end(void *pma);
void* xenaml_memory32(xenaml_bool read_write,
                      uint32_t address_minimum,
                      uint32_t address_maximum,
                      uint32_t address_alignment,
                      uint32_t range_length,
                      void *pma);
void* xenaml_memory32_fixed(xenaml_bool read_write,
                            uint32_t address_base,
                            uint32_t range_length,
                            void *pma);
void* xenaml_qword_io(struct xenaml_qword_io_args *args,
                      void *pma);
void* xenaml_qword_memory(struct xenaml_qword_memory_args *args,
                          void *pma);
void* xenaml_qword_space(struct xenaml_qword_space_args *args,
                         void *pma);
void* xenaml_dword_io(struct xenaml_dword_io_args *args,
                      void *pma);
void* xenaml_dword_memory(struct xenaml_dword_memory_args *args,
                          void *pma);
void* xenaml_dword_space(struct xenaml_dword_space_args *args,
                         void *pma);
void* xenaml_word_bus(struct xenaml_word_bus_args *args,
                      void *pma);
void* xenaml_word_io(struct xenaml_word_io_args *args,
                     void *pma);
void* xenaml_word_space(struct xenaml_word_space_args *args,
                        void *pma);
void* xenaml_extended_io(struct xenaml_extended_io_args *args,
                         void *pma);
void* xenaml_extended_memory(struct xenaml_extended_memory_args *args,
                             void *pma);
void* xenaml_extended_space(struct xenaml_extended_space_args *args,
                            void *pma);
void* xenaml_interrupt(struct xenaml_interrupt_args *args,
                       void *pma);
void* xenaml_register(uint8_t address_space_keyword,
                      uint8_t register_bit_width,
                      uint8_t register_bit_offset,
                      uint64_t register_address,
                      enum xenaml_register_access_size access_size,
                      void *pma);
void* xenaml_resource_template(void *resources,
                               void *pma);

/* AML Core **/
void xenaml_delete_node(void *delete_node);
void xenaml_delete_list(void *delete_node);
void* xenaml_next(void *current_node);
void* xenaml_children(void *current_node);
int xenaml_chain_children(void *current_node,
                          void *add_node,
                          int *error_out);
int xenaml_chain_peers(void *current_node,
                       void *add_node,
                       int *error_out);
int xenaml_unchain_node(void *remove_node,
                        int *error_out);
int xenaml_create_ssdt(const char *oem_id,
                       const char *table_id,
                       uint32_t oem_rev,
                       void *pma,
                       void **root_out,
                       int *error_out);
int xenaml_write_ssdt(void *root,
                      uint8_t **buffer_out,
                      uint32_t *length_out,
                      int *error_out);
void* xenaml_create_premem(uint32_t size);
void xenaml_free_premem(void *pma);

/* XEN ACPI Common */
void xenacpi_free_buffer(void *buffer);
