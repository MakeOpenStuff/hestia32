#ifndef PROTOCOL_MATTER_H
#define PROTOCOL_MATTER_H

#include "protocols/protocol_interface.h"

/**
 * @brief Get Matter protocol interface
 *
 * @return Pointer to Matter protocol interface
 */
const protocol_interface_t* protocol_get_matter(void);

#endif // PROTOCOL_MATTER_H
