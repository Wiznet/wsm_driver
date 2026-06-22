idf_component_register(
    SRCS
        "main.c"
        "netbios.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_wiz_toe
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function
    )

# Vendored example source from WIZnet-PICO-C; keep warnings non-fatal.
set_source_files_properties("netbios.c" PROPERTIES COMPILE_OPTIONS "-Wno-error;-w")
