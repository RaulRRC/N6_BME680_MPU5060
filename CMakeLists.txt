cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(N6_BME680_MPU5060)


target_include_directories(app PRIVATE src/ui)
target_sources(app PRIVATE src/main.c
                            src/ui/images.c
                            src/ui/screens.c
                            src/ui/styles.c
                            src/ui/ui.c
                            )

