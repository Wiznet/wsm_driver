idf_component_register(
    SRCS
        "main.c"
        "PPPoE.c"
        "md5.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_wiz_toe
        esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function
    )

# Vendored example sources from WIZnet-PICO-C; keep warnings non-fatal.
set_source_files_properties("PPPoE.c" "md5.c" PROPERTIES COMPILE_OPTIONS "-Wno-error;-w")
