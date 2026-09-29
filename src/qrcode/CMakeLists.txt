# QR Code generator library (C) by Project Nayuki, MIT license (see qrcodegen.h)
# https://www.nayuki.io/page/qr-code-generator-library

add_library(qrcode INTERFACE)
target_sources(qrcode INTERFACE ${CMAKE_CURRENT_LIST_DIR}/qrcodegen.c)
target_include_directories(qrcode SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
