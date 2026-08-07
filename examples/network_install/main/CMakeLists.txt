idf_component_register(
    SRCS
        "main.c"
        "../src/link_check.c"
    INCLUDE_DIRS
        "."
        "../inc"
    REQUIRES
        esp_wiz_toe
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function
    )
