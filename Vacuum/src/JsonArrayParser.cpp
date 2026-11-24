// JsonArrayParser.cpp
#include "JsonArrayParser.h"
#include <cmath>

#include "States.h"

namespace
{
    enum Symbols : uint8_t
    {
        OPENING_BRACKET                 = '[',
        CLOSING_BRACKET                 = ']',
        COMMA                           = ',',
        EOS                             = '\0',
        SPACE                           = ' '
    };
    
    enum ParserState : uint8_t
    {
        EXPECT_OPEN                     = 0,
        EXPECT_VALUE_OR_CLOSE           = 1,
        EXPECT_COMMA_OR_CLOSE           = 2,
        DONE                            = 3
    };

    constexpr uint8_t UINT32_MAX_LENGTH = 11;

    bool is_char_numeric(const char chr)
    {
        return chr >= '0' && chr <= '9'; 
    }
    // Reverses a string 'str' of length 'len' 
    void reverse(char* str, int len) 
    { 
        int i = 0, j = len - 1, temp; 
        while (i < j) { 
            temp = str[i]; 
            str[i] = str[j]; 
            str[j] = temp; 
            i++; 
            j--; 
        } 
    } 

    // Converts a given integer x to string str[]. 
    // d is the number of digits required in the output. 
    // If d is more than the number of digits in x, 
    // then 0s are added at the beginning. 
    int int_to_str(int x, char str[], int d) 
    { 
        int i = 0; 
        while (x) { 
            str[i++] = (x % 10) + '0'; 
            x = x / 10; 
        } 

        // If number of digits required is more, then 
        // add 0s at the beginning 
        while (i < d) 
            str[i++] = '0'; 

        reverse(str, i); 
        str[i] = '\0'; 
        return i; 
    } 

    // Converts a floating-point/double number to a string. 
    void ftoa(float n, char* res, int afterpoint) 
    { 
        // Extract integer part 
        int ipart = (int)n; 

        // Extract floating part 
        float fpart = n - (float)ipart; 

        // convert integer part to string 
        int i = int_to_str(ipart, res, 0); 

        // check for display option after point 
        if (afterpoint != 0) { 
            res[i] = '.'; // add dot 

            // Get the value of fraction part upto given no. 
            // of points after dot. The third parameter 
            // is needed to handle cases like 233.007 
            fpart = fpart * pow(10, afterpoint); 

            int_to_str((int)fpart, res + i + 1, afterpoint); 
        } 
    } 
}


bool JsonArrayParser::deserialize_json_u32_array(
    uint32_t *dest,
    const size_t dest_length,
    const unsigned char *payload,
    const size_t length
)
{
    char msg_[length + 1];
    memcpy(msg_, payload, length);
    msg_[length] = '\0';

    ParserState state_ = EXPECT_OPEN;
    size_t num_idx_ = 0, num_offset_ = 0;

    if(msg_[length] != EOS) return false;

    if(msg_[0] == EOS) return true;

    for(size_t i = 0; i < length - 1;)
    {
        if(num_idx_ >= dest_length) return false;
        uint32_t num = 0;

        switch(msg_[i])
        {
            default: // msg_[i] is a value
                if(!is_char_numeric(msg_[i])) return false; // JSON is invalid, or has an unsupported value type.

                while(i < length - 1 && is_char_numeric(msg_[i]))
                {
                    // TODO: Protect from overflowing the number
                    if(state_ != EXPECT_COMMA_OR_CLOSE)
                    {
                        state_ = EXPECT_COMMA_OR_CLOSE;
                        num_offset_ = i;
                    }

                    num = num * 10 + (msg_[i++] - '0');
                }

                dest[num_idx_] = num;
                break;
            case SPACE:
                i++;
                break;
            case OPENING_BRACKET:
                if(state_ == EXPECT_VALUE_OR_CLOSE) return false;
                state_ = EXPECT_VALUE_OR_CLOSE;
                i++;
                break;
            case COMMA:
                if(state_ == EXPECT_COMMA_OR_CLOSE)
                {
                    state_ = EXPECT_VALUE_OR_CLOSE;
                    num_idx_++;
                    i++;
                    break;
                }
                else return false; // Comma can't be written twice and it can't be at the start of the array.
                break;
            case CLOSING_BRACKET:
                if(state_ != EXPECT_COMMA_OR_CLOSE) return false;
                state_ = DONE;
            case EOS:
                if(state_ != DONE) return false;
                return true;
        }
    }
    return true;
}

bool JsonArrayParser::serialize_json_u32_array(
    const uint32_t *src,
    const size_t src_length,
    char *buffer,
    const size_t buffer_length
)
{
    for(size_t i = 0; i < buffer_length; i++)
    {
        buffer[i] = 0;
    }
    
    buffer[0] = OPENING_BRACKET;
    size_t buffer_idx = 1;
    
    for(size_t i = 0; i < src_length; i++)
    {
        uint32_t num = src[i];
        
        if (buffer_idx >= buffer_length - 1) return false;
        
        if(src[i] == 0)
        {
            buffer[buffer_idx++] = '0';
        }
        else
        {
            char num_buf[UINT32_MAX_LENGTH];
            uint8_t num_buf_idx = 0;

            while(num > 0)
            {
                if(num_buf_idx > UINT32_MAX_LENGTH - 2) return false;

                uint8_t digit = num % 10;
                num_buf[num_buf_idx++] = digit + '0';
                num = (num - digit) / 10;
            }

            while(num_buf_idx > 0)
            {
                buffer[buffer_idx++] = num_buf[--num_buf_idx];
            }
        }
        
        if (i < src_length - 1)
            buffer[buffer_idx++] = COMMA;
    }

    if (buffer_idx + 2 >= buffer_length) return false;
    buffer[buffer_idx++] = CLOSING_BRACKET;
    buffer[buffer_idx] = EOS;
    
    return true;
}

size_t JsonArrayParser::serialize_feedback_state(char *buffer, size_t buffer_length, struct FeedbackState *fs)
{
    if(buffer_length == 0) return 0;

    for(size_t i = 0; i < buffer_length; i++)
    {
        buffer[i] = 0;
    }

    size_t buf_idx = 0;
    char res[42]; // exact max length for a float with 2 decimal places

    buffer[buf_idx++] = OPENING_BRACKET;

    ftoa(fs->bldc_ammeter_value, res, 2);
    
    for(size_t i = 0; res[i] != '\0' && i < 42; i++)
    {
        buffer[buf_idx++] = res[i];
    }

    buffer[buf_idx++] = COMMA;

    ftoa(fs->compressor_valve_ammeter_value, res, 2);

    for(size_t i = 0; res[i] != '\0' && i < 42; i++)
    {
        buffer[buf_idx++] = res[i];
    }

    buffer[buf_idx++] = COMMA;
    ftoa(fs->load_cell_value, res, 2);

    for(size_t i = 0; res[i] != '\0' && i < 42; i++)
    {
        buffer[buf_idx++] = res[i];
    }
    buffer[buf_idx++] = COMMA;

    uint16_t num = fs->bldc_temperature_value;
    char buf[10];
    size_t i = 0;

    while(num > 0 && i < 4)
    {
        uint8_t digit = num % 10;
        buf[i++] = digit + '0';
        num = (num - digit) / 10;
    }

    while(i > 0)
        buffer[buf_idx++] = buf[--i];

    buffer[buf_idx++] = COMMA;

    buffer[buf_idx++] = fs->vacuum_command + '0';
    buffer[buf_idx++] = COMMA;
    buffer[buf_idx++] = fs->vacuum_state + '0';
    buffer[buf_idx++] = COMMA;
    buffer[buf_idx++] = fs->error_state + '0';
    buffer[buf_idx++] = COMMA;

    buffer[buf_idx++] = fs->is_ready ? '1' : '0'; 
    buffer[buf_idx++] = CLOSING_BRACKET;
    buffer[buf_idx] = '\0';

    return buf_idx;
}