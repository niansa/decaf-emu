#ifndef NSYSNET_ENUM_H
#define NSYSNET_ENUM_H

#include <common/enum_start.h>

ENUM_NAMESPACE_BEG(nsysnet)

ENUM_BEG(NSSocketOptions, uint32_t)
   ENUM_VALUE(Broadcast,         0x20)
ENUM_END(NSSocketOptions)

ENUM_NAMESPACE_END(nsysnet)

#include <common/enum_end.h>

#endif // ifdef NSYSNET_ENUM_H
