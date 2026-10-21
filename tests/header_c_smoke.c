#include <helium/helium.h>
#include <stddef.h>

_Static_assert(HELIUM_PLUGIN_ABI_VERSION == 1u, "unexpected ABI version");
_Static_assert(sizeof(HeliumStatus) == 4, "status must be 32-bit");
_Static_assert(sizeof(HeliumServiceHeaderV1) == 8, "service header layout changed");
_Static_assert(sizeof(HeliumDinitEventV1) == 40, "Dinit event layout changed");
_Static_assert(offsetof(HeliumDinitServiceV1, header) == 0, "Dinit service header must be first");

int main(void) {
    HeliumStringView empty = {NULL, 0};
    return empty.byte_len == 0 && HELIUM_OK == 0 ? 0 : 1;
}
