# Install script for directory: H:/esp/esp-idf/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/dongle_s3")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "H:/espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/block_cipher.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_x509.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/lms.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs7.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha3.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/build_info.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_legacy.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "H:/esp/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

