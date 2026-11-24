// JsonArrayParser.h
#ifndef JSON_ARRAY_PARSER_H
#define JSON_ARRAY_PARSER_H

#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "States.h"

using namespace States;

namespace JsonArrayParser
{
    bool deserialize_json_u32_array(
        uint32_t *dest,
        const size_t dest_length,
        const unsigned char *payload,
        const size_t length
    );

    bool serialize_json_u32_array(
        const uint32_t *src,
        const size_t src_length,
        char *buffer,
        const size_t buffer_length
    );

    size_t serialize_feedback_state(
        char *buffer, size_t buffer_length,
        struct FeedbackState *fs
    );
}

#endif