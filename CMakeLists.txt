cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(BME680_project)


target_include_directories(app PRIVATE src/ui)
target_sources(app PRIVATE src/main.c
                            src/ui/images.c
                            src/ui/screens.c
                            src/ui/styles.c
                            src/ui/ui.c
                            )

